**ESP32 SPI: BUSY retry** — `m1_esp32_cmd.c` wraps `spi_tx_64()` and `spi_rx_64()` with 150 ms retry loops to recover from `HAL_BUSY` errors on rapid back-to-back commands.
