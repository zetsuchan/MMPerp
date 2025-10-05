# TradeCore ↔ Execution Extension Integration

TradeCore is designed to run alongside two execution extensions (ExExs):

- `monmouth-svm-exex`: owns express lane quotas, block-builder hooks, and ensures deterministic inclusion of TradeCore messages.
- `monmouth-rag-memory-exex`: maintains enriched trade metadata and retrieval primitives for agents.

## Integration Contracts

### Express Lane Feed (SVM ExEx)

- **Source**: `tradecore::ingest` publishes a hash-chained queue of pre-admitted frames.
- **Transport**: The queue root is exposed via `tradecore::api::ApiRouter` on the `/express-feed` endpoint.
- **Expectations**:
  - ExEx polls `ApiRouter::has_endpoint("/express-feed")` and consumes frames using a binary protocol defined under `tradecore::api`.
  - TradeCore guarantees deterministic ordering (ingress order → risk checks → matcher outcomes → ledger updates) before the root is exposed.
  - ExEx commits the queue root to consensus within the same block, ensuring replay safety during reorgs.

### Trade Metadata Stream (RAG Memory ExEx)

- **Source**: `tradecore::telemetry` and `tradecore::ledger` cooperate to emit metadata records per fill/cancel.
- **Transport**: `tradecore::api::ApiRouter` exposes `/trade-metadata` for streaming in protobuf/SBE format.
- **Expectations**:
  - ExEx issues deterministic requests referencing `common::OrderId::value()` so retrieval remains stable across replays.
  - TradeCore ensures WAL offsets are included with each record so the ExEx can reconcile gaps after reorgs or failover.

### State Commitments

- `tradecored` (see `apps/tradecored/main.cpp`) orchestrates subsystem commits at the end of each block.
- After ledger and risk modules settle a block, TradeCore computes a Merkle/IPA root and publishes it via `/state-root`.
- The SVM ExEx must record the state root alongside the general EVM root to keep the combined state deterministic.

### Error Semantics

- All public API routes return deterministic error codes (to be defined in `libs/api`).
- ExEx clients should treat any non-success code as a consensus-critical event and halt inclusion for the block.

## Integration Checklist

| Task | Owner | Notes |
|------|-------|-------|
| Wire `monmouth-svm-exex` client to `/express-feed` | SVM ExEx | Requires SBE schema finalization.
| Persist TradeCore WAL offsets into consensus events | SVM ExEx | Enables replay after reorg.
| Implement `/trade-metadata` subscription | RAG Memory ExEx | Use deterministic pagination by WAL offset.
| Configure shared telemetry sink for ExEx metrics | Both ExExs | `telemetry::TelemetrySink` exposes `drain()` for batch export.
| Define protobuf/SBE schemas for API routes | TradeCore | Live under `libs/api` once finalized.

## Local Development Linking

The ExEx repositories live at:

- `/Users/danchou/Documents/Monmouth/monmouth-svm-exex`
- `/Users/danchou/Documents/Monmouth/monmouth-rag-memory-exex`

When hacking locally:

1. Build TradeCore with `cmake -S . -B build && cmake --build build`.
2. In each ExEx repo, add a local dependency path to TradeCore headers (e.g., `-I../MMPerp/include`).
3. Link against the relevant static libraries in `build/libs/*` until the packaging flow is ready.
4. Use the deterministic test harness in `tests/unit` as a template for ExEx integration tests.

## Future Work

- Generate SBE schemas for order ingress/egress (`libs/api`).
- Embed QUIC server bootstrap inside `apps/tradecored` once networking is wired up.
- Publish gRPC-style debug APIs for ExEx operators (non-consensus-critical).
