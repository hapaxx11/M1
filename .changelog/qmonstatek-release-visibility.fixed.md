**CI: Hapax releases now visible in qMonstatek** — removed the automatic pre-release flag
  for versions with MAJOR < 1. The GitHub `/releases/latest` API silently skips pre-releases,
  which caused qMonstatek to show "No release candidates found" for `hapaxx11/M1`. Releases
  are now published as full releases; the manual `prerelease` workflow input can still be used
  to mark a release as pre-release when explicitly needed.
