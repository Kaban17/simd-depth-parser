#!/usr/bin/env python3
"""Generates synthetic Binance futures depthUpdate events as JSONL.

usage: gen.py <count> [seed] > events.jsonl
"""

import json
import random
import sys

# symbol, price decimals, qty decimals, price magnitude
SYMBOLS = [
    ("BTCUSDT", 2, 3, 60000),
    ("ETHUSDT", 2, 3, 3000),
    ("SOLUSDT", 4, 2, 150),
    ("DOGEUSDT", 6, 0, 0.15),
    ("DOGSUSDT", 8, 0, 0.00005),
    ("1000PEPEUSDT", 7, 0, 0.01),
]


def level(rng, price, pdec, qdec):
    qty = rng.uniform(0.001, 10**rng.randint(1, 8))
    # occasional long numbers exercise the scalar fallback path
    if rng.random() < 0.01:
        return [f"{rng.randint(10**8, 10**9)}.{rng.randint(0, 10**8 - 1):08d}",
                f"{qty:.{qdec}f}"]
    return [f"{price:.{pdec}f}", f"{qty:.{qdec}f}"]


def side(rng, mid, pdec, qdec, sign):
    n = rng.choice([0, 1, 2, 3, 5, 10, 20, 50, 200])
    tick = 10**-pdec
    return [level(rng, mid + sign * (i + 1) * tick * rng.randint(1, 5), pdec, qdec)
            for i in range(n)]


def main():
    count = int(sys.argv[1])
    rng = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 42)
    t = 1777999159441
    uid = 10478199613493
    for _ in range(count):
        sym, pdec, qdec, mag = rng.choice(SYMBOLS)
        mid = mag * rng.uniform(0.9, 1.1)
        t += rng.randint(0, 5)
        first = uid + rng.randint(1, 1000)
        uid = first + rng.randint(0, 5000)
        event = {
            "e": "depthUpdate",
            "E": t + rng.randint(0, 10),
            "T": t,
            "s": sym,
            "U": first,
            "u": uid,
            "pu": first - rng.randint(1, 100),
            "b": side(rng, mid, pdec, qdec, -1),
            "a": side(rng, mid, pdec, qdec, +1),
        }
        print(json.dumps(event, separators=(",", ":")))


if __name__ == "__main__":
    main()
