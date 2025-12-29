# distributed-kvstore

A distributed key-value store in C++20 with TCP networking, primary-replica replication, and write-ahead logging.

## Architecture

3-node cluster: 1 primary, 2 replicas. The primary handles all writes, replicates to replicas via a custom binary protocol, and all nodes serve reads from their local store.

```
┌─────────┐    PUT/DEL    ┌─────────┐
│  Client  │─────────────▶│ Primary │
└─────────┘               │ (7001)  │
                          └────┬────┘
                     REPLICATE │
                   ┌───────────┼───────────┐
                   ▼                       ▼
             ┌──────────┐           ┌──────────┐
             │ Replica  │           │ Replica  │
             │ (7002)   │           │ (7003)   │
             └──────────┘           └──────────┘
```

### Components

- **Binary protocol** — 20-byte request / 16-byte response headers, network byte order, GET/PUT/DELETE/REPLICATE/HEARTBEAT opcodes
- **KV store** — `std::unordered_map` with `std::shared_mutex` (readers never block readers)
- **Write-ahead log** — Append-only binary file with CRC-32 integrity checks and `fsync`. Crash recovery replays the log on startup
- **Thread pool** — Fixed-size pool with `std::mutex` + `std::condition_variable` task queue
- **Replication** — Configurable sync/async modes. Heartbeat-based liveness detection. WAL-based catch-up when a replica reconnects
- **TCP networking** — POSIX sockets, poll-based accept loop, EINTR-safe I/O

## Build

```bash
make          # produces build/kvstore-server and build/kvstore-bench
```

Requires a C++20 compiler (g++ or clang++). No external dependencies.

## Usage

### Start a 3-node cluster

```bash
./scripts/start_cluster.sh
```

This starts the primary on port 7001 and replicas on 7002/7003. Data is stored in `data/node{1,2,3}/`.

### Run the benchmark

```bash
./build/kvstore-bench --host 127.0.0.1 --port 7001 --threads 8 --ops 100000
```

| Flag | Default | Description |
|------|---------|-------------|
| `--host` | 127.0.0.1 | Target host |
| `--port` | 7001 | Target port |
| `--threads` | 4 | Client threads |
| `--ops` | 100000 | Total operations |
| `--key-range` | 10000 | Number of distinct keys |
| `--read-ratio` | 0.8 | Fraction of reads (0.0–1.0) |
| `--value-size` | 64 | Value size in bytes |

### Stop the cluster

```bash
./scripts/stop_cluster.sh
```

## Performance

Tested on localhost (Apple M-series):

| Workload | Throughput | p50 | p99 |
|----------|-----------|-----|-----|
| 10k ops, 4 threads, 50/50 r/w | 63,649 ops/sec | 56μs | 181μs |
| 100k ops, 8 threads, 80/20 r/w | 118,152 ops/sec | 21μs | 125μs |

## Protocol

All integers are network byte order (big-endian).

**Request** (20-byte header + body):
```
[4] magic (0x4B565331)  [1] version  [1] opcode  [4] request_id
[2] key_length  [4] value_length  [4] reserved
[key_length] key  [value_length] value
```

**Response** (16-byte header + body):
```
[4] magic  [1] version  [1] status  [4] request_id
[4] value_length  [2] reserved
[value_length] value
```

## Project Structure

```
src/
  common/       types, protocol serialization, logger
  storage/      kv_store (concurrent hashmap), wal (write-ahead log)
  network/      tcp_server, tcp_client, connection wrapper
  threading/    thread_pool
  replication/  replication_manager (primary→replica forwarding)
  server/       node (orchestrator), config parser
  client/       kv_client (high-level API)
  bench/        load_generator, stats, benchmark CLI
  main.cpp      server entry point
configs/        per-node YAML configs
scripts/        cluster start/stop
```
