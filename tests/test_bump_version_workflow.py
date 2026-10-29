"""Regression tests for the protected-main version bump workflow."""

import pathlib
import unittest


WORKFLOW = (
    pathlib.Path(__file__).resolve().parents[1]
    / ".github"
    / "workflows"
    / "bump-version.yml"
).read_text()


class TestBumpVersionWorkflow(unittest.TestCase):
    def test_publishes_version_bump_through_pull_request(self):
        self.assertIn('git push origin "HEAD:${BRANCH}"', WORKFLOW)
        self.assertIn("gh pr create", WORKFLOW)
        self.assertIn("--base main", WORKFLOW)
        self.assertNotIn("git push origin HEAD:main", WORKFLOW)

    def test_uses_app_token_for_branch_push_and_pull_request(self):
        self.assertIn("token: ${{ steps.app-token.outputs.token }}", WORKFLOW)
        self.assertIn("GH_TOKEN: ${{ steps.app-token.outputs.token }}", WORKFLOW)
        self.assertIn("permission-contents: write", WORKFLOW)
        self.assertIn("permission-pull-requests: write", WORKFLOW)


if __name__ == "__main__":
    unittest.main()
