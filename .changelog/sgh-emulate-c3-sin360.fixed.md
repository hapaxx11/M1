**Sub-GHz: Fix "Memory error" when emulating .sgh files saved by C3.12 / SiN360** — M1 native
  NOISE recordings (`.sgh`) produced by other firmware variants (C3.12 fw 0.8.x, SiN360 0.9.0.5)
  now emulate correctly.  Previously these files were routed through the Flipper conversion path
  which failed with error code 5.  Files are now identified as M1 native via the `Filetype:`
  header and streamed directly without any conversion.
