#!/usr/bin/env python3
"""Benchmark instance generator for Precedence-Constrained Knapsack Problems.

Run:
    python3 instances/gen_instances.py

Output layout
-------------
instances/
  toy/      hand-crafted examples (unchanged)
  inforest/ inforest_{class}_{density}_n{n:07d}_s{s:02d}.txt
  outforest/ outforest_{class}_{density}_n{n:07d}_s{s:02d}.txt
  forest/   forest_{class}_{density}_n{n:07d}_s{s:02d}.txt
  catalog.csv   one row per generated instance

KP coefficient classes (Pisinger 1994, r=1000):
  uncorr         w ~ U[1,r],   p ~ U[1,r]
  weakly_corr    w ~ U[1,r],   p ~ U[max(1,w-r/10), w+r/10]
  strongly_corr  w ~ U[1,r],   p = w + r/10
  neg_uncorr         as uncorr, with controlled negative profits
  neg_weakly_corr    as weakly_corr, with controlled negative profits
  neg_strongly_corr  as strongly_corr, with controlled negative profits

Arc density rho in {0.3, 0.6, 0.9, 1.0}  <->  sparse / medium / dense / tree
  (rho=1.0 always yields a single spanning tree over all n nodes)

Sizes:
  10 20 30 ... 100  200 300 ... 1000  10000 20000 ... 50000

Instances per combination: 10 independent seeds.

Capacity is NOT stored in the file.
Pass it to macroitems_main as a percentage, e.g.:  25%  50%  75%
"""

import csv
import os
import random
import time

BASE = os.path.dirname(os.path.abspath(__file__))
for sub in ("inforest", "outforest", "forest"):
    os.makedirs(os.path.join(BASE, sub), exist_ok=True)

R = 1000

TYPES = {
    "uncorr": 1,
    "weakly_corr": 2,
    "strongly_corr": 3,
    "neg_uncorr": 4,
    "neg_weakly_corr": 5,
    "neg_strongly_corr": 6,
}

NEGATIVE_PROFIT_RATE = 0.25

DENSITIES = {"sparse": 0.3, "medium": 0.6, "dense": 0.9, "tree": 1.0}

SIZES = [
    10, 20, 30, 40, 50, 60, 70, 80, 90, 100,
    200, 300, 400, 500, 600, 700, 800, 900, 1000,
    10000, 20000, 30000, 40000, 50000,
    60000, 70000, 80000, 90000, 100000,
]
INFOREST_SIZES = SIZES
OUTFOREST_SIZES = SIZES
FOREST_SIZES = SIZES
N_SEEDS = 10


def base_profit(w, base_type_id, rng):
    r1 = R // 10
    if base_type_id == 1:
        return rng.randint(1, R)
    if base_type_id == 2:
        return rng.randint(max(1, w - r1), w + r1)
    return w + r1


def pisinger(n, type_id, rng):
    profits, weights = [], []
    is_negative_family = type_id > 3
    base_type_id = type_id - 3 if is_negative_family else type_id
    for _ in range(n):
        w = rng.randint(1, R)
        p = base_profit(w, base_type_id, rng)
        if is_negative_family and rng.random() < NEGATIVE_PROFIT_RATE:
            p = -p
        profits.append(p)
        weights.append(w)
    if is_negative_family:
        if all(p > 0 for p in profits):
            idx = rng.randrange(n)
            profits[idx] = -profits[idx]
        if all(p < 0 for p in profits):
            idx = rng.randrange(n)
            profits[idx] = -profits[idx]
    return profits, weights


def gen_inforest(n, rho, rng):
    """Out-degree <= 1: arc (i,j) with j > i drawn with prob rho."""
    arcs = []
    for i in range(1, n):
        if rng.random() < rho:
            arcs.append((i, rng.randint(i + 1, n)))
    return arcs


def gen_outforest(n, rho, rng):
    """In-degree <= 1: arc (j,i) with j < i drawn with prob rho."""
    arcs = []
    for i in range(2, n + 1):
        if rng.random() < rho:
            arcs.append((rng.randint(1, i - 1), i))
    return arcs


def gen_forest(n, rho, rng):
    """Random spanning forest, then each edge oriented 50/50."""
    arcs = []
    for i in range(2, n + 1):
        if rng.random() < rho:
            j = rng.randint(1, i - 1)
            if rng.random() < 0.5:
                arcs.append((j, i))
            else:
                arcs.append((i, j))
    return arcs


def write_instance(path, n, profits, weights, arcs):
    if os.path.exists(path):
        return
    tmp = f"{path}.tmp.{os.getpid()}"
    for attempt in range(5):
        try:
            with open(tmp, "w") as f:
                f.write(f"n {n}\n")
                f.write("profits  " + " ".join(map(str, profits)) + "\n")
                f.write("weights  " + " ".join(map(str, weights)) + "\n")
                f.write(f"arcs {len(arcs)}\n")
                for t, h in arcs:
                    f.write(f"{t} {h}\n")
            os.replace(tmp, path)
            return
        except OSError:
            try:
                if os.path.exists(tmp):
                    os.remove(tmp)
            finally:
                if attempt == 4:
                    raise
                time.sleep(0.25 * (attempt + 1))

catalog_rows = []


def generate(topo, sizes, gen_fn):
    total = 0
    for n in sizes:
        for tname, tid in TYPES.items():
            for dname, rho in DENSITIES.items():
                for s in range(1, N_SEEDS + 1):
                    rng = random.Random((topo, tid, dname, n, s))
                    profits, weights = pisinger(n, tid, rng)
                    arcs = gen_fn(n, rho, rng)
                    fname = f"{topo}_{tname}_{dname}_n{n:07d}_s{s:02d}.txt"
                    fpath = os.path.join(BASE, topo, fname)
                    write_instance(fpath, n, profits, weights, arcs)
                    catalog_rows.append({
                        "topology": topo,
                        "kp_class": tname,
                        "density": dname,
                        "rho": rho,
                        "n": n,
                        "m": len(arcs),
                        "seed": s,
                        "file": os.path.join(topo, fname),
                    })
                    total += 1
        per_size = len(TYPES) * len(DENSITIES) * N_SEEDS
        print(f"  [{topo}] n={n:>7d} done ({per_size} files)", flush=True)
    return total


print("Generating in-forest instances...")
c1 = generate("inforest", INFOREST_SIZES, gen_inforest)

print("\nGenerating out-forest instances...")
c2 = generate("outforest", OUTFOREST_SIZES, gen_outforest)

print("\nGenerating forest instances...")
c3 = generate("forest", FOREST_SIZES, gen_forest)

catalog_path = os.path.join(BASE, "catalog.csv")
with open(catalog_path, "w", newline="") as cf:
    writer = csv.DictWriter(
        cf,
        fieldnames=["topology", "kp_class", "density", "rho", "n", "m", "seed", "file"],
    )
    writer.writeheader()
    writer.writerows(catalog_rows)

print(f"\nDone. Total instances: {c1 + c2 + c3}")
print(f"Catalog written to   {catalog_path}")
