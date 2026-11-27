# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""The deployment steps of the Website workflow (.github/workflows/pages.yml).

actions/deploy-pages sometimes finds no artifact right after its upload ("Found 0 artifact(s)"), for a cause that is not known, and a re-run of the failed job cannot fix it
(it uploads a second artifact of the same name: "Found 2 artifact(s)"). The workflow therefore tries the deployment again in the same run. These tests keep that shape:
a first try that may fail, a pause, a second try that runs only after a failure, and the address of the deployment taken from whichever of the two succeeded.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / '.github/workflows/pages.yml'
READY = "steps.release.outputs.ready == 'true'"


def steps_of(text):
    """The steps of the job as text blocks, in order (the lines from one "      - " to the next)."""
    body = text.split('    steps:\n', 1)[1]
    blocks = re.split(r'(?m)^      - ', body)
    return ['      - ' + block for block in blocks[1:]]


def step_with(blocks, needle):
    found = [block for block in blocks if needle in block]
    assert len(found) == 1, f'{needle!r}: {len(found)} steps'
    return found[0]


class PagesWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.text = WORKFLOW.read_text(encoding='utf-8')
        self.blocks = steps_of(self.text)

    def position(self, needle):
        return self.blocks.index(step_with(self.blocks, needle))

    def test_the_file_is_valid_yaml_when_a_reader_is_there(self):
        try:
            import yaml
        except ImportError:
            self.skipTest('PyYAML is not installed')
        data = yaml.safe_load(self.text)
        steps = data['jobs']['deploy']['steps']
        self.assertEqual([s.get('id') for s in steps if s.get('id') in ('deployment_first', 'deployment')], ['deployment_first', 'deployment'])

    def test_the_deployment_is_tried_twice_and_only_twice(self):
        self.assertEqual(self.text.count('uses: actions/deploy-pages@v4'), 2)

    def test_the_first_try_may_fail_without_failing_the_job(self):
        first = step_with(self.blocks, 'id: deployment_first')
        self.assertIn('uses: actions/deploy-pages@v4', first)
        self.assertIn('continue-on-error: true', first)
        self.assertIn(f'if: {READY}', first)

    def test_the_second_try_runs_only_after_a_failure_of_the_first_and_is_allowed_to_fail_the_job(self):
        second = step_with(self.blocks, 'id: deployment\n')
        self.assertIn('uses: actions/deploy-pages@v4', second)
        self.assertIn(f"if: {READY} && steps.deployment_first.outcome == 'failure'", second)
        self.assertNotIn('continue-on-error', second)  # the second failure is the failure of the deployment

    def test_the_steps_are_in_the_order_upload_settle_first_try_pause_second_try(self):
        order = [self.position('actions/upload-pages-artifact'), self.position('Let the artifact settle'), self.position('id: deployment_first'),
                 self.position('After a failed first try'), self.position('id: deployment\n')]
        self.assertEqual(order, sorted(order))
        self.assertEqual(len(set(order)), 5)

    def test_there_is_a_pause_before_the_first_try_and_a_longer_one_before_the_second(self):
        settle = step_with(self.blocks, 'Let the artifact settle')
        pause = step_with(self.blocks, 'After a failed first try')
        before = int(re.search(r'sleep (\d+)', settle).group(1))
        between = int(re.search(r'sleep (\d+)', pause).group(1))
        self.assertGreaterEqual(before, 5)
        self.assertGreaterEqual(between, 10)
        self.assertGreater(between, before)

    def test_the_pause_after_a_failure_runs_only_after_a_failure(self):
        pause = step_with(self.blocks, 'After a failed first try')
        self.assertIn("steps.deployment_first.outcome == 'failure'", pause)

    def test_the_steps_that_only_print_never_fail_the_job(self):
        for needle in ('Let the artifact settle', 'After a failed first try'):
            block = step_with(self.blocks, needle)
            self.assertIn('continue-on-error: true', block, needle)
            self.assertIn('gh api', block, needle)
            self.assertIn('GH_TOKEN: ${{ github.token }}', block, needle)

    def test_the_address_of_the_deployment_comes_from_whichever_try_succeeded(self):
        self.assertIn('url: ${{ steps.deployment.outputs.page_url || steps.deployment_first.outputs.page_url }}', self.text)

    def test_the_wait_script_that_did_not_help_is_gone(self):
        self.assertNotIn('wait_for_artifact', self.text)
        self.assertFalse((ROOT / 'tools/website/wait_for_artifact.py').exists())
        self.assertFalse((ROOT / 'tools/test_wait_for_artifact.py').exists())

    def test_every_deployment_step_waits_for_the_release_like_the_rest(self):
        for needle in ('actions/upload-pages-artifact', 'id: deployment_first', 'id: deployment\n', 'Let the artifact settle', 'After a failed first try'):
            self.assertIn(READY, step_with(self.blocks, needle), needle)


if __name__ == '__main__':
    unittest.main()
