**Web Updater: fix SD file existence probe and ZIP path safety** — replace FILE_READ-based
  existence check with FILE_LIST (single response frame, no SEQ-collision risk); cache
  per-directory listings to avoid redundant RPC round-trips; validate and normalize ZIP
  entry paths before SD writes (reject drive prefixes, `..` traversal, and leading slashes);
  use `subarray()` instead of `slice()` in write loop to avoid unnecessary buffer copies.
