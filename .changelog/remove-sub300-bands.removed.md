**Sub-GHz: removed non-functional 150/200/250 MHz bands** — The Si4463 radio
  config and antenna matching network on the M1 are designed for 300–928 MHz.
  The 150/200/250 MHz band options were initialising the radio with the 300 MHz
  config and retuning the VCO, which does not produce correct output at those
  frequencies. The bands have been removed from the firmware UI, the spectrum
  analyser sweep presets, and the README.
