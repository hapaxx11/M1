**WiFi: MAC Track, Wardrive, and Station Wardrive now reachable** — three fully-implemented
  WiFi tools (`wifi_mac_track`, `wifi_wardrive`, `wifi_station_wardrive`) were accidentally
  omitted from the top-level WiFi scene menu during the SiN360 port; they had handlers and
  implementations but were completely unreachable via the UI. All three are now listed in
  the WiFi menu between "Station Scan" and "Sniffers".
