// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Which input reports does the keyboard (USB 1b1c:2b0c) send while someone types? This program answers WITHOUT ever showing, storing or passing
// on what was typed: it reads the reports of the keyboard's key interface for a few seconds and prints only, for each report ID and length, how
// many reports it saw and how many of them had all their bytes at zero (no key down) or not. No byte of a report is ever printed, written to a
// file or kept beyond the one report being counted, and no time is recorded, so the output cannot tell which keys were pressed, in which order,
// or how fast. Two runs where different keys were pressed print the same text if they got the same reports (tests/test_hid_input_shape.py checks it).
//
// Why: the descriptor of the key interface declares two keyboard reports (ID 1, a bitmap, and ID 8, an array of 13 usages; docs/protocol/README.md,
// "Keyboard input reports"), and nothing says which one the keyboard actually sends. The reactive glow must not guess.
//
// Usage: ./hid-input-shape [seconds]          (1 to 60, default 10; type normally meanwhile, or press nothing)
//        ./hid-input-shape --device /dev/hidrawN [seconds]
// The node is found by itself: the hidraw node of 1b1c:2b0c whose descriptor is a keyboard (usage page 1, usage 6). Nothing is opened for writing.
// For the tests only: --input-fd N reads from an already open descriptor (a socket pair that keeps report boundaries) instead of a hidraw node.
//
// Build: gcc -Wall -Wextra -o hid-input-shape hid-input-shape.c

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define MAX_SHAPES 16
#define BUFFER_SIZE 512

struct Shape {
    unsigned id;
    unsigned length;
    unsigned long reports;
    unsigned long allZero;
};

static struct Shape shapes[MAX_SHAPES];
static int shapeCount = 0;
static unsigned long otherShapes = 0;

// Counts one report. Only the ID, the length and whether every byte after the ID is zero are looked at; the bytes themselves are not kept.
static void count(const unsigned char *report, size_t length)
{
    if (length == 0)
        return;
    int zero = 1;
    for (size_t i = 1; i < length; ++i) {
        if (report[i] != 0)
            zero = 0;
    }
    for (int i = 0; i < shapeCount; ++i) {
        if (shapes[i].id == report[0] && shapes[i].length == length) {
            ++shapes[i].reports;
            shapes[i].allZero += (unsigned long)zero;
            return;
        }
    }
    if (shapeCount == MAX_SHAPES) {
        ++otherShapes;
        return;
    }
    shapes[shapeCount].id = report[0];
    shapes[shapeCount].length = (unsigned)length;
    shapes[shapeCount].reports = 1;
    shapes[shapeCount].allZero = (unsigned long)zero;
    ++shapeCount;
}

static int isKeyboardNode(int fd)
{
    struct hidraw_devinfo info;
    if (ioctl(fd, HIDIOCGRAWINFO, &info) < 0 || (info.vendor & 0xFFFF) != 0x1b1c || (info.product & 0xFFFF) != 0x2b0c)
        return 0;
    int size = 0;
    struct hidraw_report_descriptor descriptor;
    memset(&descriptor, 0, sizeof descriptor);
    if (ioctl(fd, HIDIOCGRDESCSIZE, &size) < 0 || size < 4 || size > HID_MAX_DESCRIPTOR_SIZE)
        return 0;
    descriptor.size = (unsigned)size;
    if (ioctl(fd, HIDIOCGRDESC, &descriptor) < 0)
        return 0;
    return descriptor.value[0] == 0x05 && descriptor.value[1] == 0x01 && descriptor.value[2] == 0x09 && descriptor.value[3] == 0x06;
}

// The first hidraw node of the keyboard whose descriptor is a keyboard; the path is written to `path`. -1 when there is none.
static int openKeyboardNode(char *path, size_t pathSize)
{
    DIR *dir = opendir("/dev");
    if (!dir)
        return -1;
    int found = -1;
    struct dirent *entry;
    while (found < 0 && (entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "hidraw", 6) != 0)
            continue;
        char candidate[300];
        snprintf(candidate, sizeof candidate, "/dev/%s", entry->d_name);
        const int fd = open(candidate, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;
        if (isKeyboardNode(fd)) {
            snprintf(path, pathSize, "%s", candidate);
            found = fd;
        } else {
            close(fd);
        }
    }
    closedir(dir);
    return found;
}

static int parseSeconds(const char *text, long *seconds)
{
    char *end = NULL;
    errno = 0;
    const long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 1 || value > 60)
        return 0;
    *seconds = value;
    return 1;
}

static void usage(const char *program)
{
    fprintf(stderr, "usage: %s [--device /dev/hidrawN] [seconds 1 to 60]\n", program);
}

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    long seconds = 10;
    int haveSeconds = 0;
    const char *device = NULL;
    int inputFd = -1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--device") == 0 && i + 1 < argc && device == NULL) {
            device = argv[++i];
        } else if (strcmp(argv[i], "--input-fd") == 0 && i + 1 < argc && inputFd < 0) {
            char *end = NULL;
            const long value = strtol(argv[++i], &end, 10);
            if (end == argv[i] || *end != '\0' || value < 3 || value > 1023) {
                usage(argv[0]);
                return 1;
            }
            inputFd = (int)value;
        } else if (haveSeconds || !parseSeconds(argv[i], &seconds)) {
            usage(argv[0]);
            return 1;
        } else {
            haveSeconds = 1;
        }
    }

    char path[300] = "(a descriptor given to the program)";
    int fd = inputFd;
    if (fd < 0) {
        if (device != NULL) {
            fd = open(device, O_RDONLY | O_NONBLOCK);
            if (fd >= 0 && !isKeyboardNode(fd)) {
                fprintf(stderr, "%s is not the key interface of 1b1c:2b0c\n", device);
                close(fd);
                return 2;
            }
            snprintf(path, sizeof path, "%s", device);
        } else {
            fd = openKeyboardNode(path, sizeof path);
        }
        if (fd < 0) {
            fprintf(stderr, "No readable hidraw node for the keyboard's keys (is it plugged in, and is its udev rule installed?)\n");
            return 3;
        }
    }

    printf("Read %s for up to %ld s. Nothing typed is shown: only the report ID, its length and a count.\n", path, seconds);
    fflush(stdout);

    unsigned char report[BUFFER_SIZE];
    const double end = now() + (double)seconds;
    for (;;) {
        const double left = end - now();
        if (left <= 0)
            break;
        struct pollfd wait = {fd, POLLIN, 0};
        const int ready = poll(&wait, 1, (int)(left * 1000) + 1);
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready <= 0)
            break;
        const ssize_t got = read(fd, report, sizeof report);
        if (got <= 0)
            break; // the end of the input, or an error: stop
        count(report, (size_t)got);
    }
    memset(report, 0, sizeof report); // nothing of the last report stays in memory
    close(fd);

    if (shapeCount == 0) {
        printf("No report was seen. Press a key while it runs.\n");
        return 0;
    }
    printf("%-4s %-7s %-8s %-9s %s\n", "id", "length", "reports", "all zero", "not all zero");
    for (int i = 0; i < shapeCount; ++i)
        printf("%02x   %-7u %-8lu %-9lu %lu\n", shapes[i].id, shapes[i].length, shapes[i].reports, shapes[i].allZero, shapes[i].reports - shapes[i].allZero);
    if (otherShapes > 0)
        printf("(and %lu reports of other shapes)\n", otherShapes);
    return 0;
}
