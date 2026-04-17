**Infrared: ESL Tag Tinker** — IR-based tool for Pricer Electronic Shelf Label tags,
  inspired by github.com/i12bp8/TagTinker. Accessible via Infrared → ESL Tags.
  Features: broadcast page flip (pages 0–7), broadcast diagnostic screen, and
  targeted ping of a specific tag by 17-digit barcode. Uses the tag's native
  ~1.25 MHz PP4 carrier (TIM1_CH4N, PC5) with DWT cycle-accurate symbol timing
  scaled for the STM32H573's 250 MHz core clock.
