# Changelog

All notable changes to this project will be documented in this file.

## [2025-10-05]
- Restructured matching engine with per-market shards, intrusive price levels, true cancel/replace, and TIF/post-only enforcement.
- Expanded unit tests to cover maker/taker interaction and cancel flows.
- Introduced canonical order enums and flags in common types for engine-wide use.
- Added WAL writer/reader with checksum + fsync controls, snapshot store, and replay driver with persistence-focused unit coverage.

## [2025-10-04]
- Initial C++ scaffolding for TradeCore subsystems, CI workflow, and integration docs.
