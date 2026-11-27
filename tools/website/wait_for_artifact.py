#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Wait until the Pages artifact of this run is listed by the GitHub API, so that actions/deploy-pages finds it.

actions/deploy-pages asks the API for the artifacts of the run, and it did so one second after actions/upload-pages-artifact had finished: three times in a few
days (runs 79, 82 and 83 of the Website workflow) the list was still empty, "Found 0 artifact(s)", and the deployment failed for no other reason; a re-run was enough.
This waits, up to a limit, for the artifact to be listed. It never fails the workflow itself: when the limit is over it says so (a warning) and lets deploy-pages try,
which then gives its own error if the artifact is really missing.
"""
import argparse
import json
import subprocess
import sys
import time

DEFAULT_NAME = 'github-pages'
DEFAULT_TRIES = 24      # with the interval below: two minutes at most
DEFAULT_INTERVAL = 5.0  # seconds between two looks


def is_listed(listing, name):
    """Whether the listing of the artifacts of a run has an artifact of this name that has not expired. Anything that is not a listing is "no"."""
    if not isinstance(listing, dict) or not isinstance(listing.get('artifacts'), list):
        return False
    return any(isinstance(item, dict) and item.get('name') == name and item.get('expired') is not True for item in listing['artifacts'])


def wait_for_artifact(fetch, name, tries=DEFAULT_TRIES, interval=DEFAULT_INTERVAL, sleep=time.sleep, log=print):
    """The number of the look (1 for the first) at which the artifact was listed, or 0 when it never was within `tries` looks.

    `fetch()` returns the listing (the JSON of the API), and may raise: a look that fails counts as a look without the artifact, so one hiccup of the API does not stop
    the wait."""
    for attempt in range(1, tries + 1):
        try:
            listed = is_listed(fetch(), name)
        except Exception as error:  # noqa: BLE001 - whatever the API or the CLI did, the wait goes on
            log(f'look {attempt}/{tries}: the listing could not be read ({type(error).__name__})')
            listed = False
        if listed:
            return attempt
        if attempt < tries:
            sleep(interval)
    return 0


def gh_listing(repository, run_id):
    """The artifacts of a run, through the gh CLI (the workflow gives it GH_TOKEN)."""
    result = subprocess.run(['gh', 'api', f'repos/{repository}/actions/runs/{run_id}/artifacts'], capture_output=True, text=True, check=True, timeout=60)
    return json.loads(result.stdout)


def main(argv=None, fetch=None, sleep=time.sleep, log=print):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('repository', help='owner/name')
    parser.add_argument('run_id')
    parser.add_argument('--name', default=DEFAULT_NAME)
    parser.add_argument('--tries', type=int, default=DEFAULT_TRIES)
    parser.add_argument('--interval', type=float, default=DEFAULT_INTERVAL)
    args = parser.parse_args(argv)
    if args.tries < 1 or args.interval < 0:
        parser.error('--tries is at least 1 and --interval is not negative')
    if fetch is None:
        def fetch():
            return gh_listing(args.repository, args.run_id)
    found = wait_for_artifact(fetch, args.name, args.tries, args.interval, sleep, log)
    if found:
        log(f'The artifact "{args.name}" was listed at look {found} of {args.tries}.')
    else:
        log(f'::warning::The artifact "{args.name}" was not listed after {args.tries} looks; deploy-pages will try anyway and say if it is really missing.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
