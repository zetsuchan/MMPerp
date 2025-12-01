# Changelog

All notable changes to this project will be documented in this file.

## [2025-10-06] Ingress pipeline & QUIC/SBE scaffolding
- Added SPSC-backed ingress pipeline with per-lane queues (new/cancel/replace), auth hook, and per-account rate limiting stubs.
- Introduced SBE-style message helpers and QUIC transport stub wiring; tradecored now wires the transport into the pipeline.
- Added reusable `tradecore::common::SpscRing` utility and expanded unit tests to exercise message encoding, queue classification, and rate limiting.
- Enhanced telemetry sink with counter/latency helpers and drained summaries; unit tests cover latency quantiles.
- Created `issues.md` to track follow-up work (telemetry scaling, real QUIC/auth, matcher O(1) cancel, etc.).

## [2025-10-05] Risk, funding, liquidation pass (`7b6c620`)
- Implemented cross-margin risk engine with collateral accounting, reduce-only enforcement, and deterministic margin summaries.
- Added liquidation manager to classify accounts (healthy/partial/full) and funding engine with clamped mark, premium rate, and accrual tracking.
- Extended daemon wiring and unit tests to cover new risk/funding behaviour and liquidation states.

## [2025-10-05] Build integration fixes (`f5003eb`)
- Aligned matcher interfaces with new persistence layer, updated daemon bootstrap wiring, and ensured CMake + unit tests succeed end-to-end.

## [2025-10-05] Persistence layer (`1d43677`)
- Added WAL writer/reader with checksums, fsync controls, and sequence recovery plus snapshot store and replay driver.
- Expanded unit harness to snapshot + replay deterministic state transitions.

## [2025-10-04] Initial scaffold + matcher refactor (`c6d8516`)
- Established repository layout, subsystem libraries, CI workflow, and documentation scaffold.
- Rebuilt matching engine around intrusive price levels, multi-market shards, and deterministic order lifecycle (submit/cancel/replace).
