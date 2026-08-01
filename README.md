# mini-redis-cpp

A minimal in-memory key-value store with a TCP server, inspired by Redis.
Built to demonstrate systems programming fundamentals: sockets, concurrency,
and a simple custom protocol.

## Features (current)
- TCP server, one thread per connected client
- Commands: `SET`, `GET`, `DEL`, `EXISTS`, `EXPIRE`, `PING`
- Thread-safe store (mutex-protected)
- Basic TTL expiry check on read

## Roadmap (see TODOs in code)
- [ ] Sharded store to reduce lock contention (day 2)
- [ ] Background reaper thread for TTL expiry (day 2)
- [ ] AOF (append-only file) persistence + replay on startup (day 3)
- [ ] LRU eviction once memory cap is hit (day 3)
- [ ] PUBLISH/SUBSCRIBE pub-sub channels (day 4)
- [ ] CLI client (`mini-redis-cli`) (day 4)
- [ ] Benchmark tool (ops/sec, latency) (day 4)
- [ ] Unit tests with Catch2/GoogleTest (day 5)

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Run

```bash
./mini_redis 6380
```

## Try it

```bash
# in another terminal
nc localhost 6380
SET foo bar
GET foo
EXISTS foo
DEL foo
GET foo
```

## Why this project
Shows understanding of:
- POSIX sockets and TCP networking
- Multithreading and mutex-based synchronization
- Designing a simple wire protocol
- Systems-level tradeoffs (thread-per-client vs thread pool, persistence, eviction)
