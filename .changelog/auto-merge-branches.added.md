**CI: Auto-merge main into agent branches** — New `update-branches.yml`
  GitHub Actions workflow that triggers on every push to `main` and
  automatically merges main into all active `copilot/*` branches, keeping
  long-running agent branches up-to-date and reducing merge conflicts.
