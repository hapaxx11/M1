WiFi HAL_Delay elimination (Phase A-6): replaced all 23 `HAL_Delay` calls in
`m1_wifi.c` with button-responsive alternatives — status screens now use
`wifi_wait_dismiss()` (Back/OK dismissible), ESP32 reset waits use
`vTaskDelay(pdMS_TO_TICKS(N))`, and the station-scan countdown is abortable with
Back. Other RTOS tasks (WDT, battery, LED) are no longer frozen during WiFi
status displays.
