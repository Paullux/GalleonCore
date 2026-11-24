// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
//
// READ-ONLY listing of the HID report descriptors of the Galleon 100 SD keyboard (USB 1b1c:2b0c), to learn the exact layout of its input reports
// (report IDs, which bit is which key) WITHOUT reading a single key press: the program opens each /dev/hidraw node and asks the kernel for the
// device's name and its report descriptor (ioctls HIDIOCGRAWINFO, HIDIOCGRAWNAME, HIDIOCGRDESCSIZE, HIDIOCGRDESC). It never calls read() or
// write() on a node, so no report, and so no key press, is ever seen. A descriptor describes the device; it holds no data typed on it.
//
// Why: the reactive glow (ROADMAP.md, "Effet réactif minimal") needs to know which key went down. The keyboard's own input reports carry it, and
// their format is not invented here: this prints the descriptor that declares it, so the layout is read from the device and recorded in
// docs/protocol/ with its source (a real capture of a descriptor, not of typing).
//
// Output, one block per node:
//   /dev/hidrawN  bus=0x0003 vendor=1b1c product=2b0c  name="..."
//   descriptor (N bytes): hex bytes, 16 per line
//
// Usage: ./hid-descriptors            (only 1b1c:2b0c)
//        ./hid-descriptors 1b1c:2b18  (another vendor:product, e.g. the Stream Deck module)
//
// Build: gcc -Wall -Wextra -o hid-descriptors hid-descriptors.c

#include <dirent.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int parseId(const char *text, unsigned *vendor, unsigned *product)
{
    char *end = NULL;
    const unsigned long v = strtoul(text, &end, 16);
    if (end == text || *end != ':' || v > 0xFFFF)
        return 0;
    const char *rest = end + 1;
    const unsigned long p = strtoul(rest, &end, 16);
    if (end == rest || *end != '\0' || p > 0xFFFF)
        return 0;
    *vendor = (unsigned)v;
    *product = (unsigned)p;
    return 1;
}

static void dumpNode(const char *path, unsigned vendor, unsigned product, int *shown)
{
    const int fd = open(path, O_RDONLY | O_NONBLOCK); // opened only for the ioctls below; nothing is read from it
    if (fd < 0)
        return;
    struct hidraw_devinfo info;
    if (ioctl(fd, HIDIOCGRAWINFO, &info) < 0 || (unsigned)(info.vendor & 0xFFFF) != vendor || (unsigned)(info.product & 0xFFFF) != product) {
        close(fd);
        return;
    }
    char name[256] = "";
    if (ioctl(fd, HIDIOCGRAWNAME(sizeof name), name) < 0)
        snprintf(name, sizeof name, "(unknown)");
    int size = 0;
    struct hidraw_report_descriptor descriptor;
    memset(&descriptor, 0, sizeof descriptor);
    if (ioctl(fd, HIDIOCGRDESCSIZE, &size) < 0 || size <= 0 || size > HID_MAX_DESCRIPTOR_SIZE) {
        printf("%s  no usable descriptor (size %d)\n\n", path, size);
        close(fd);
        return;
    }
    descriptor.size = (unsigned)size;
    if (ioctl(fd, HIDIOCGRDESC, &descriptor) < 0) {
        printf("%s  the descriptor could not be read\n\n", path);
        close(fd);
        return;
    }
    printf("%s  bus=0x%04x vendor=%04x product=%04x  name=\"%s\"\n", path, (unsigned)info.bustype, (unsigned)(info.vendor & 0xFFFF),
           (unsigned)(info.product & 0xFFFF), name);
    printf("descriptor (%d bytes):", size);
    for (int i = 0; i < size; ++i)
        printf("%s%02x", i % 16 == 0 ? "\n  " : " ", descriptor.value[i]);
    printf("\n\n");
    ++*shown;
    close(fd);
}

int main(int argc, char **argv)
{
    unsigned vendor = 0x1b1c;
    unsigned product = 0x2b0c;
    if (argc == 2 && !parseId(argv[1], &vendor, &product)) {
        fprintf(stderr, "usage: %s [vendor:product]   (hexadecimal, e.g. 1b1c:2b0c)\n", argv[0]);
        return 1;
    }
    if (argc > 2) {
        fprintf(stderr, "usage: %s [vendor:product]\n", argv[0]);
        return 1;
    }
    DIR *dir = opendir("/dev");
    if (!dir) {
        perror("/dev");
        return 2;
    }
    int shown = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "hidraw", 6) != 0)
            continue;
        char path[300];
        snprintf(path, sizeof path, "/dev/%s", entry->d_name);
        dumpNode(path, vendor, product, &shown);
    }
    closedir(dir);
    if (shown == 0) {
        fprintf(stderr, "No /dev/hidraw node of %04x:%04x was readable (is the keyboard plugged in, and is its udev rule installed?)\n", vendor, product);
        return 3;
    }
    return 0;
}
