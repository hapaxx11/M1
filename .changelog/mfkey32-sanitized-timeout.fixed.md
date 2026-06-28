Host tests: build `test_mfkey32` with optimization under ASan/UBSan so the
  MFKey32 recovery test stays within the existing GitHub Actions timeout on
  slower runners.
