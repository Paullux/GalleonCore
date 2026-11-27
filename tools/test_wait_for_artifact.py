# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""The wait for the Pages artifact before actions/deploy-pages (tools/website/wait_for_artifact.py)."""
import contextlib
import io
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / 'website'))
import wait_for_artifact as w  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
LISTED = {'total_count': 1, 'artifacts': [{'name': 'github-pages', 'expired': False}]}
EMPTY = {'total_count': 0, 'artifacts': []}


class Script:
    """A fetch that gives each answer in turn (an exception is raised), and the last one for ever; and a sleep that only counts."""

    def __init__(self, answers):
        self.answers = list(answers)
        self.looks = 0
        self.sleeps = []

    def fetch(self):
        answer = self.answers[min(self.looks, len(self.answers) - 1)]
        self.looks += 1
        if isinstance(answer, Exception):
            raise answer
        return answer

    def sleep(self, seconds):
        self.sleeps.append(seconds)


class IsListedTests(unittest.TestCase):
    def test_an_artifact_of_the_right_name_is_listed(self):
        self.assertTrue(w.is_listed(LISTED, 'github-pages'))

    def test_another_name_is_not_the_artifact(self):
        self.assertFalse(w.is_listed({'artifacts': [{'name': 'github-pages-old'}, {'name': 'other'}]}, 'github-pages'))

    def test_an_expired_artifact_does_not_count(self):
        self.assertFalse(w.is_listed({'artifacts': [{'name': 'github-pages', 'expired': True}]}, 'github-pages'))

    def test_an_artifact_without_the_expired_key_counts(self):
        self.assertTrue(w.is_listed({'artifacts': [{'name': 'github-pages'}]}, 'github-pages'))

    def test_anything_that_is_not_a_listing_is_not_a_yes(self):
        for odd in (None, [], {}, 'x', 3, {'artifacts': None}, {'artifacts': 'github-pages'}, {'artifacts': [None, 3, 'github-pages']}):
            self.assertFalse(w.is_listed(odd, 'github-pages'), repr(odd))


class WaitTests(unittest.TestCase):
    def test_an_artifact_that_is_already_listed_is_found_at_the_first_look_without_waiting(self):
        s = Script([LISTED])
        self.assertEqual(w.wait_for_artifact(s.fetch, 'github-pages', 5, 2.0, s.sleep, lambda _m: None), 1)
        self.assertEqual(s.sleeps, [])

    def test_the_wait_goes_on_until_the_artifact_is_listed_and_says_at_which_look(self):
        s = Script([EMPTY, EMPTY, EMPTY, LISTED])
        self.assertEqual(w.wait_for_artifact(s.fetch, 'github-pages', 10, 2.0, s.sleep, lambda _m: None), 4)
        self.assertEqual(s.sleeps, [2.0, 2.0, 2.0])  # one pause between two looks
        self.assertEqual(s.looks, 4)                  # and no look after it was found

    def test_an_artifact_that_never_comes_is_given_up_after_the_tries_and_the_answer_is_zero(self):
        s = Script([EMPTY])
        self.assertEqual(w.wait_for_artifact(s.fetch, 'github-pages', 4, 3.0, s.sleep, lambda _m: None), 0)
        self.assertEqual(s.looks, 4)
        self.assertEqual(s.sleeps, [3.0, 3.0, 3.0])  # no pause after the last look

    def test_the_last_look_counts(self):
        s = Script([EMPTY, EMPTY, EMPTY, LISTED])
        self.assertEqual(w.wait_for_artifact(s.fetch, 'github-pages', 4, 1.0, s.sleep, lambda _m: None), 4)
        s = Script([EMPTY, EMPTY, EMPTY, LISTED])
        self.assertEqual(w.wait_for_artifact(s.fetch, 'github-pages', 3, 1.0, s.sleep, lambda _m: None), 0)

    def test_a_look_that_fails_does_not_stop_the_wait(self):
        s = Script([RuntimeError('network'), ValueError('json'), LISTED])
        lines = []
        self.assertEqual(w.wait_for_artifact(s.fetch, 'github-pages', 5, 1.0, s.sleep, lines.append), 3)
        self.assertEqual(len(lines), 2)  # said for each failed look, with the kind of error and nothing of its text
        self.assertTrue(all('could not be read' in line for line in lines))

    def test_a_listing_with_another_artifact_is_still_waiting(self):
        s = Script([{'artifacts': [{'name': 'other'}]}, LISTED])
        self.assertEqual(w.wait_for_artifact(s.fetch, 'github-pages', 5, 1.0, s.sleep, lambda _m: None), 2)


class MainTests(unittest.TestCase):
    def run_main(self, answers, extra=()):
        s = Script(answers)
        lines = []
        code = w.main(['owner/repo', '123', *extra], fetch=s.fetch, sleep=s.sleep, log=lines.append)
        return code, lines, s

    def test_it_succeeds_and_says_at_which_look_the_artifact_was_listed(self):
        code, lines, s = self.run_main([EMPTY, LISTED], ['--interval', '0'])
        self.assertEqual(code, 0)
        self.assertEqual(lines[-1], 'The artifact "github-pages" was listed at look 2 of 24.')
        self.assertFalse(any('::warning::' in line for line in lines))

    def test_when_the_artifact_never_comes_it_warns_and_still_succeeds_so_that_deploy_pages_gives_its_own_error(self):
        code, lines, s = self.run_main([EMPTY], ['--tries', '3', '--interval', '0'])
        self.assertEqual(code, 0)
        self.assertEqual(s.looks, 3)
        self.assertTrue(lines[-1].startswith('::warning::'))

    def test_a_name_can_be_given(self):
        code, lines, _ = self.run_main([{'artifacts': [{'name': 'github-pages'}]}, {'artifacts': [{'name': 'site'}]}], ['--name', 'site', '--tries', '2', '--interval', '0'])
        self.assertEqual(code, 0)
        self.assertIn('listed at look 2 of 2', lines[-1])

    def test_a_number_of_tries_below_one_or_a_negative_interval_is_refused(self):
        for extra in (['--tries', '0'], ['--interval', '-1']):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                self.run_main([LISTED], extra)


class WorkflowTests(unittest.TestCase):
    def test_the_workflow_waits_between_the_upload_and_the_deployment_and_runs_the_script_that_is_tested_here(self):
        text = (ROOT / '.github/workflows/pages.yml').read_text(encoding='utf-8')
        upload = text.index('actions/upload-pages-artifact')
        wait = text.index('run: python3 tools/website/wait_for_artifact.py')  # the step itself, not the paths of the trigger
        deploy = text.index('actions/deploy-pages')
        self.assertLess(upload, wait)
        self.assertLess(wait, deploy)
        self.assertIn('"tools/website/wait_for_artifact.py"', text.split('jobs:')[0])  # a change of the script runs the workflow (the paths of the trigger)


if __name__ == '__main__':
    unittest.main()
