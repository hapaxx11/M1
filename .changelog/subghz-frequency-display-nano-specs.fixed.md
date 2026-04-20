**Sub-GHz: frequency display now shows correctly on device** — The frequency value (e.g.
  "433.92 MHz") was blank in the Signal Info screen, Decode Results detail, hopper status
  bar, and Receiver Info screen because `%.2f` format is not supported by newlib-nano
  (`--specs=nano.specs`) without `-u _printf_float`. All four sites now use integer
  arithmetic formatting (`%lu.%02lu MHz`) so frequency always renders correctly.
