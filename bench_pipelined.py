#!/usr/bin/env python3
import argparse, socket, threading, time

HOST = "127.0.0.1"


def warmup(port, clients, keys_per_client):
    def warm(seed):
        s = socket.create_connection((HOST, port), timeout=10)
        payload = b"".join(
            f"SET key{seed}_{i} v{i}\n".encode() for i in range(keys_per_client)
        )
        s.sendall(payload)
        buf = b""
        while buf.count(b"\n") < keys_per_client:
            chunk = s.recv(65536)
            if not chunk:
                break
            buf += chunk
        s.close()
    ts = [threading.Thread(target=warm, args=(c,)) for c in range(clients)]
    for t in ts: t.start()
    for t in ts: t.join()


def worker(port, cid, n, pipeline, use_get, results, errors):
    try:
        s = socket.create_connection((HOST, port), timeout=10)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        cmds = [
            (f"GET key{cid}_{i}\n" if use_get else f"SET key{cid}_{i} v{i}\n").encode()
            for i in range(n)
        ]

        t0 = time.perf_counter()
        i = 0
        while i < n:
            batch = min(pipeline, n - i)
            s.sendall(b"".join(cmds[i:i + batch]))

            need = batch
            buf = b""
            while buf.count(b"\n") < need:
                chunk = s.recv(65536)
                if not chunk:
                    raise ConnectionError("server closed")
                buf += chunk
            i += batch

        elapsed = time.perf_counter() - t0
        results.append((cid, elapsed, n))
        s.close()
    except Exception as e:
        errors.append(f"c{cid}: {e}")


def run(port, clients, requests, pipeline, use_get):
    if use_get:
        print("Warming up keys...")
        warmup(port, clients, requests)

    results, errors = [], []
    ts = [
        threading.Thread(
            target=worker,
            args=(port, c, requests, pipeline, use_get, results, errors),
        )
        for c in range(clients)
    ]

    t0 = time.perf_counter()
    for t in ts: t.start()
    for t in ts: t.join()
    wall = time.perf_counter() - t0

    total = sum(r[2] for r in results)
    print(f"Op:            {'GET' if use_get else 'SET'}")
    print(f"Clients:       {clients}")
    print(f"Pipeline:      {pipeline}")
    print(f"Per client:    {requests}")
    print(f"Total ops:     {total}")
    print(f"Errors:        {len(errors)}")
    if errors:
        print(f"First error:   {errors[0]}")
    print(f"Wall time:     {wall:.2f}s")
    print(f"Throughput:    {total/wall:.0f} ops/sec")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("port", type=int)
    p.add_argument("--clients", type=int, default=50)
    p.add_argument("--requests", type=int, default=5000)
    p.add_argument("--pipeline", type=int, default=100)
    p.add_argument("--op", choices=["set", "get"], default="set")
    a = p.parse_args()
    run(a.port, a.clients, a.requests, a.pipeline, a.op == "get")
