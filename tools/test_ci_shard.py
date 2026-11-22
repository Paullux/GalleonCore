# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later

"""The clang-tidy parts of the CI: every file is checked, once, and the CI file wires them so that a failed part cannot pass."""

import importlib.util
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
_spec = importlib.util.spec_from_file_location("clang_tidy_shard", ROOT / "tools" / "ci" / "clang_tidy_shard.py")
shard_tool = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(shard_tool)


def entries(*files: str) -> list[dict]:
    return [{"directory": "/repo/build", "file": file, "command": "c++ -c"} for file in files]


class Selection(unittest.TestCase):
    def test_only_the_folders_of_the_pattern_are_checked(self) -> None:
        found = shard_tool.tidy_files(entries(
            "/repo/daemon/a.cpp", "/repo/tests/t.cpp", "/repo/bosun/b.cpp", "/repo/lib/src/l.cpp", "/repo/media/m.cpp",
            "/repo/build/daemon/galleond_autogen/mocs_compilation.cpp",  # generated, deeper
            "/repo/lib/include/h.cpp", "/repo/daemon/x.h", "/repo/fuzz/f.cpp", "/repo/tools/t.cpp"))
        self.assertEqual(found, ["/repo/bosun/b.cpp", "/repo/daemon/a.cpp", "/repo/lib/src/l.cpp", "/repo/media/m.cpp", "/repo/tests/t.cpp"])

    def test_a_file_in_the_database_twice_is_checked_once(self) -> None:
        self.assertEqual(shard_tool.tidy_files(entries("/repo/daemon/a.cpp", "/repo/daemon/a.cpp")), ["/repo/daemon/a.cpp"])

    def test_a_relative_path_is_taken_from_the_directory_of_its_entry(self) -> None:
        self.assertEqual(shard_tool.tidy_files([{"directory": "/repo", "file": "daemon/a.cpp"}]), ["/repo/daemon/a.cpp"])


class Parts(unittest.TestCase):
    FILES = [f"/repo/{folder}/f{n:03d}.cpp" for folder in ("bosun", "daemon", "tests") for n in range(37)]

    def test_the_parts_cover_every_file_exactly_once(self) -> None:
        for count in (1, 2, 3, 4, 7):
            parts = [shard_tool.shard(self.FILES, index, count) for index in range(1, count + 1)]
            joined = [file for part in parts for file in part]
            self.assertEqual(sorted(joined), sorted(self.FILES), f"{count} parts")
            self.assertEqual(len(joined), len(set(joined)), f"a file in two parts, {count} parts")

    def test_the_parts_are_as_even_as_they_can_be(self) -> None:
        sizes = [len(shard_tool.shard(self.FILES, index, 4)) for index in range(1, 5)]
        self.assertLessEqual(max(sizes) - min(sizes), 1)

    def test_each_folder_is_spread_over_the_parts(self) -> None:
        # Dealt out one by one: a heavy folder does not end up whole in one part.
        for index in range(1, 5):
            part = shard_tool.shard(self.FILES, index, 4)
            self.assertTrue(any("/tests/" in file for file in part), index)
            self.assertTrue(any("/daemon/" in file for file in part), index)

    def test_a_part_out_of_range_is_refused(self) -> None:
        for index, count in ((0, 4), (5, 4), (1, 0), (-1, 3)):
            with self.assertRaises(ValueError):
                shard_tool.shard(self.FILES, index, count)

    def test_more_parts_than_files_leaves_some_empty_and_loses_none(self) -> None:
        files = self.FILES[:3]
        parts = [shard_tool.shard(files, index, 5) for index in range(1, 6)]
        self.assertEqual(sorted(file for part in parts for file in part), sorted(files))
        self.assertEqual(sum(1 for part in parts if not part), 2)

    def test_the_regular_expression_given_to_run_clang_tidy_matches_those_files_and_no_other(self) -> None:
        mine = ["/repo/daemon/a+b.cpp", "/repo/tests/t.cpp"]
        regex = re.compile(shard_tool.regex_of(mine))
        for file in mine:
            self.assertTrue(regex.search(file))
        for other in ("/repo/daemon/a_b.cpp", "/repo/daemon/aab.cpp", "/repo/tests/t.cpp.orig", "/repo/bosun/b.cpp"):
            self.assertIsNone(regex.search(other), other)


class CiFile(unittest.TestCase):
    def setUp(self) -> None:
        self.ci = (ROOT / ".github" / "workflows" / "ci.yml").read_text()

    def test_the_parts_are_numbered_from_one_without_a_gap(self) -> None:
        found = re.search(r"^\s+shard: \[([0-9, ]+)\]\s*$", self.ci, re.M)
        self.assertIsNotNone(found, "no matrix `shard: [1, 2, ...]` in the CI file")
        numbers = [int(part) for part in found.group(1).split(",")]
        self.assertGreaterEqual(len(numbers), 2)
        self.assertEqual(numbers, list(range(1, len(numbers) + 1)))

    def test_every_part_is_told_how_many_there_are_by_the_matrix_itself(self) -> None:
        # The count comes from the matrix, so that adding a part cannot leave some files to nobody.
        self.assertIn('clang_tidy_shard.py "${{ matrix.shard }}" "${{ strategy.job-total }}"', self.ci)

    def test_lint_waits_for_the_parts_and_fails_when_one_failed(self) -> None:
        lint = self.ci[self.ci.index("\n  lint:\n"):]
        self.assertIn("needs: clang-tidy", lint)
        self.assertIn("if: ${{ always() }}", lint)  # without it a failed part would skip `lint`, which counts as passed
        self.assertIn("needs.clang-tidy.result != 'success'", lint)
        self.assertRegex(lint, r"needs\.clang-tidy\.result != 'success'\s*\n\s+run: \|\s*\n[^\n]*\n\s+exit 1")

    def test_lint_keeps_the_quick_steps_and_no_longer_runs_clang_tidy_itself(self) -> None:
        lint = self.ci[self.ci.index("\n  lint:\n"):]
        for step in ("qmllint", "Decisions of tools/check.py", "REUSE licensing"):
            self.assertIn(f"- name: {step}", lint)
        self.assertNotIn("run-clang-tidy", lint)


if __name__ == "__main__":
    unittest.main()
