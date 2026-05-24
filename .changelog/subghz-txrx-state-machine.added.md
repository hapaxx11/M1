**Sub-GHz: TX/RX lifecycle state machine (Phase 1 of Momentum parity work)** —
introduce `subghz_txrx_state` as a pure-logic, host-tested state machine that
models the SI4463 radio lifecycle (OFF / IDLE / RX_PASSIVE / RX_ACTIVE /
TX_BLOCK / TX_ASYNC) and enforces legal transitions.  Foundation for
subsequent phases that migrate scenes onto a centralised radio wrapper.
