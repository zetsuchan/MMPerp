# TradeCore Engine Scaffold

TradeCore is the central limit order book and risk engine that powers Monmouth's perpetuals and spot venues. This repository hosts the C++ implementation scaffold used by the execution extensions (`monmouth-rag-memory-exex` and `monmouth-svm-exex`) to coordinate flow into the on-chain stack.

## Repository Layout

```
.
├── apps/                # Executables (tradecored daemon)
├── include/             # Shared public headers
├── libs/                # Engine subsystems (static libraries)
├── tests/               # Deterministic unit/integration harnesses
├── docs/                # Integration and design notes
└── CMakeLists.txt       # Root CMake build entry point
```

Key subsystems already scaffolded:

- `ingest`: network ingress and pre-admission QoS.
- `matcher`: price-time matching engine.
- `risk`: cross-margin and rate-limit checks.
- `funding`: index/mark/funding calculations.
- `ledger`: account and balance bookkeeping.
- `wal`: append-only write-ahead logging.
- `snapshot`: deterministic snapshotting.
- `replay`: replay harness for deterministic verification.
- `telemetry`: latency/perf sampling surfaces.
- `api`: internal RPC surface for ExEx integration.

Each module is compiled into a static library that the `tradecored` binary links together.

## Building

```bash
cmake -S . -B build -DTRADECORE_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure
```

Set `TRADECORE_ENABLE_SANITIZERS=ON` during configuration to build with Address and Undefined Behaviour sanitizers.

## Next Steps

- Flesh out deterministic data structures inside `libs/matcher` and `libs/risk`.
- Implement QUIC ingress transport and frame schema inside `libs/ingest`.
- Flesh out WAL framing and snapshot delta encoding in `libs/wal` and `libs/snapshot`.

See `docs/INTEGRATION.md` for details on how TradeCore exposes state to the ExEx layer.
