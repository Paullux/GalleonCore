# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/linux/hid-input-shape.c shows the shape of the keyboard's input reports and nothing of what was typed.

The program is compiled and run on reports sent through a socket pair that keeps report boundaries, like a hidraw node does. What is held here
is the privacy promise of AGENTS.md ("no collection of key presses"): two runs where different keys were pressed print the same text.
"""
import shutil
import socket
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'tools' / 'linux' / 'hid-input-shape.c'


@unittest.skipUnless(shutil.which('gcc') or shutil.which('cc'), 'no C compiler')
class HidInputShape(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        cls.program = Path(cls.dir.name) / 'hid-input-shape'
        compiler = shutil.which('gcc') or shutil.which('cc')
        subprocess.run([compiler, '-Wall', '-Wextra', '-Werror', '-o', str(cls.program), str(SOURCE)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_on(self, reports, *extra):
        """Runs the program on `reports` (a list of bytes, one per report) and returns (exit status, standard output)."""
        ours, theirs = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        with ours, theirs:
            for report in reports:
                ours.send(report)
            ours.shutdown(socket.SHUT_WR)
            done = subprocess.run([str(self.program), '--input-fd', str(theirs.fileno()), '5', *extra], pass_fds=[theirs.fileno()],
                                  capture_output=True, text=True, timeout=30)
        return done.returncode, done.stdout

    @staticmethod
    def bitmap(payload):
        return bytes([1]) + payload.ljust(17, b'\0')

    def test_two_runs_with_different_keys_pressed_print_the_same_text(self):
        first = [self.bitmap(b'\x00\x10\x20'), self.bitmap(b''), self.bitmap(b'\x00\x01')]
        second = [self.bitmap(b'\x02\xf0\x0f\x55'), self.bitmap(b''), self.bitmap(b'\x00\xa5\x5a\xc3\x3c')]
        self.assertEqual(self.run_on(first), self.run_on(second))

    def test_the_whole_output_is_the_summary_and_no_byte_of_the_report(self):
        report = bytes([8, 0xA5, 0x5A, 0xC3, 0x3C, 0x99, 0x66, 0xE7, 0x81, 0x7E, 0x42, 0x24, 0xBD, 0xDB, 0xF0])
        status, out = self.run_on([report])
        self.assertEqual(status, 0)
        self.assertEqual(out.splitlines()[1:], ['id   length  reports  all zero  not all zero', '08   15      1        0         1'])

    def test_it_counts_reports_by_id_and_length_and_by_whether_they_are_all_zero(self):
        reports = [self.bitmap(b'\x00\x10'), self.bitmap(b''), self.bitmap(b'\x01'), bytes([8]) + bytes(14)]
        status, out = self.run_on(reports)
        self.assertEqual(status, 0)
        lines = out.splitlines()
        self.assertEqual(lines[1].split(), ['id', 'length', 'reports', 'all', 'zero', 'not', 'all', 'zero'])
        self.assertEqual(lines[2].split(), ['01', '18', '3', '1', '2'])
        self.assertEqual(lines[3].split(), ['08', '15', '1', '1', '0'])
        self.assertEqual(len(lines), 4)

    def test_the_same_id_with_another_length_is_another_line(self):
        _, out = self.run_on([bytes([1, 1, 0]), bytes([1, 1, 0, 0])])
        rows = [line.split() for line in out.splitlines()[2:]]
        self.assertEqual(rows, [['01', '3', '1', '0', '1'], ['01', '4', '1', '0', '1']])

    def test_no_report_is_said_in_words(self):
        status, out = self.run_on([])
        self.assertEqual(status, 0)
        self.assertEqual(out.splitlines()[1:], ['No report was seen. Press a key while it runs.'])

    def test_the_table_is_bounded_and_the_rest_is_counted(self):
        reports = [bytes([i + 1, 1]) for i in range(20)]
        _, out = self.run_on(reports)
        self.assertEqual(len(out.splitlines()), 2 + 16 + 1)
        self.assertIn('(and 4 reports of other shapes)', out)

    def test_a_wrong_command_line_is_refused(self):
        for arguments in (['0'], ['61'], ['abc'], ['--device'], ['--input-fd'], ['--input-fd', '2'], ['5', '6'], ['1x']):
            done = subprocess.run([str(self.program), *arguments], capture_output=True, text=True, timeout=30)
            self.assertEqual(done.returncode, 1, arguments)
            self.assertIn('usage', done.stderr)

    def test_a_node_that_is_not_the_keyboard_is_refused(self):
        done = subprocess.run([str(self.program), '--device', '/dev/null', '5'], capture_output=True, text=True, timeout=30)
        self.assertEqual(done.returncode, 2)
        self.assertIn('not the key interface', done.stderr)

    def test_the_program_never_opens_anything_for_writing(self):
        source = SOURCE.read_text()
        self.assertNotIn('O_WRONLY', source)
        self.assertNotIn('O_RDWR', source)
        self.assertNotIn('write(', source)
        self.assertNotIn('fopen', source)


if __name__ == '__main__':
    unittest.main()
