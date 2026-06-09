**Sub-GHz: KeeLoq learning types 4/5 (#551)** — added Magic XOR Type 1
  (Beninca, type 4) and FAAC SLH (type 5) device-key derivation to the KeeLoq
  cipher engine; widened the manufacturer-key parser to accept types 1–5 in
  both compact and RocketGod formats; type 4 is fully replay-capable, type 5
  is parsed but encoder-deferred (seed dependency).  Documented the
  Flipper-extracted file investigation and MC-expansion plan.
