#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""One part of the clang-tidy run of the CI.

clang-tidy was one step of about 23 minutes out of 26 for the whole `lint` job. The CI now runs it as several jobs at once, each with its own
machine; this script gives each one its share of the files and runs `run-clang-tidy` on them.

The set of files is exactly the one the single step used: the entries of the compilation database (`build/compile_commands.json`) whose path
matches PATTERN, with the same pattern, so that nothing is added or dropped. They are put in order and dealt out one by one (the first file to
the first shard, the second to the second...), which spreads the heavy folders over every shard. The parts are tested to cover the whole set,
once each (`tools/test_ci_shard.py`).

    clang_tidy_shard.py INDEX COUNT [--build DIR] [--list]

INDEX is 1 to COUNT. `--list` prints the files of that part and runs nothing. The exit status is that of `run-clang-tidy`.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

# The folders looked at, as the CI has always written them (tools/check.py reads the folders from this line, so the two cannot drift apart).
PATTERN = r"(lib/src|daemon|bosun|tests|media)/[^/]+\.cpp$"


def tidy_files(entries: list[dict], pattern: str = PATTERN) -> list[str]:
    """The files of a compilation database that are to be checked, in order, each once. Pure."""
    found: set[str] = set()
    for entry in entries:
        file = str(entry["file"])
        if not Path(file).is_absolute():
            file = str(Path(entry.get("directory", ".")) / file)
        if re.search(pattern, file):
            found.add(file)
    return sorted(found)


def shard(files: list[str], index: int, count: int) -> list[str]:
    """The files of part `index` (1 to `count`) of the list, dealt out one by one. Pure."""
    if count < 1 or index < 1 or index > count:
        raise ValueError(f"part {index} of {count}: the part is 1 to the number of parts")
    return files[index - 1 :: count]


def regex_of(files: list[str]) -> str:
    """One regular expression that run-clang-tidy matches against each file of its database: exactly these files. Pure."""
    return "(" + "|".join(re.escape(file) for file in files) + ")$"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("index", type=int)
    parser.add_argument("count", type=int)
    parser.add_argument("--build", default="build")
    parser.add_argument("--list", action="store_true", help="print the files of this part and run nothing")
    arguments = parser.parse_args(argv)
    database = Path(arguments.build) / "compile_commands.json"
    try:
        mine = shard(tidy_files(json.loads(database.read_text())), arguments.index, arguments.count)
    except (OSError, ValueError, KeyError) as error:
        print(f"clang_tidy_shard: {error}", file=sys.stderr)
        return 2
    if arguments.list:
        print("\n".join(mine))
        return 0
    print(f"clang-tidy: part {arguments.index} of {arguments.count}, {len(mine)} files", flush=True)
    if not mine:
        return 0
    return subprocess.run(["run-clang-tidy", "-quiet", "-p", arguments.build, regex_of(mine)], check=False).returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
