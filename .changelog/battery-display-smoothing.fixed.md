**Battery: smooth display percentage during charge/discharge transitions** — the
  battery level shown on screen no longer jumps by several percent at once when
  plugging in or unplugging.  A 1 %/s rate-limiter (`battery_soc_smooth()`) is
  now applied to the displayed value; the underlying fuel-gauge reading is
  unchanged and still used for firmware-update and low-battery decisions.
