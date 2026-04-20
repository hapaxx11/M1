**Sub-GHz: removed non-functional 150/200/250 MHz band presets** — The SI4463
  synthesiser requires outdiv values of 16–24 for frequencies below ~283 MHz, but
  the firmware only implements outdiv=12 (valid down to ~283 MHz). The three presets
  were loading the 300 MHz radio config and retuning with an out-of-range VCO target,
  so the PLL could not lock at those frequencies. Custom frequency input range updated
  from 142–1050 MHz to 300–928 MHz to match hardware capability.
