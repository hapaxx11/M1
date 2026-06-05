**STM32 firmware update: progress bar now reaches 100%** — Progress is
  initialised with the full image size before flashing begins and is updated
  only after each chunk is successfully written, so the final iteration
  correctly displays 100% instead of stopping at the pre-final state.
