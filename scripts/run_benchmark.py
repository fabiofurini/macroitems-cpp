#!/usr/bin/env python3
"""
Benchmark script for macroitem algorithms.

Runs all instances and writes every result to results/runs.csv (appended by
the binary on each invocation).  After all runs, rebuilds the four timing CSVs
in report_performance/ with the full output schema.

Output CSVs (schema = all columns from results/runs.csv + derived columns
topo_dir, kp_class, density, seed_int parsed from the instance path):

  timing_forest.csv         forest algo, all 3 topologies, n=10..50000
  timing_forest_noheap.csv  forest_noheap algo, forest topology, n=10..50000
  timing_inforest.csv       inforest algo, inforest topology, n=10..50000
  timing_outforest.csv      outforest algo, outforest topology, n=10..50000

Run from the project root:
    python3 report_performance/run_benchmark.py
"""
import subprocess, csv, os, sys, re
from collections import defaultdict

EXE     = "./build/macroitems_main"
RUNS    = "results/runs.csv"
OUT_DIR = "report_performance"

SIZES_SMALL  = list(range(10, 110, 10))
SIZES_MEDIUM = SIZES_SMALL + list(range(200, 1100, 100))
SIZES_LARGE  = [10000, 20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000, 100000]
SIZES_ALL    = SIZES_MEDIUM + SIZES_LARGE
SEEDS        = list(range(1, 11))
CLASSES      = [
    "uncorr", "weakly_corr", "strongly_corr",
    "neg_uncorr", "neg_weakly_corr", "neg_strongly_corr",
]
DENSITIES    = ["sparse", "medium", "dense", "tree"]
TOPOS        = ["forest", "inforest", "outforest"]

# ── Step 1: run the binary for every required (topo, algo, n) combination ─────
# forest_noheap is O(n^2): only run on medium-sized instances (n <= 1000)

RUNS_PLAN = [
    # (topo, algo, sizes)
    ("forest",    "forest",        SIZES_ALL),
    ("forest",    "forest_noheap", SIZES_MEDIUM),   # O(n^2): medium only
    ("inforest",  "forest",        SIZES_ALL),
    ("inforest",  "inforest",      SIZES_ALL),
    ("outforest", "forest",        SIZES_ALL),
    ("outforest", "outforest",     SIZES_ALL),
]

total = sum(len(sz)*len(CLASSES)*len(DENSITIES)*len(SEEDS) for _,_,sz in RUNS_PLAN)
done = errors = 0

for topo, algo, sizes in RUNS_PLAN:
    print(f"\n=== {algo} on {topo} instances ({len(sizes)} sizes) ===", flush=True)
    for n in sizes:
        for kp in CLASSES:
            for d in DENSITIES:
                for s in SEEDS:
                    inst = f"instances/{topo}/{topo}_{kp}_{d}_n{n:07d}_s{s:02d}.txt"
                    if not os.path.exists(inst):
                        errors += 1
                        continue
                    r = subprocess.run([EXE, "-i", inst, "-a", algo],
                                       capture_output=True)
                    done += 1
                    if r.returncode != 0:
                        errors += 1
                        print(f"  FAILED: {inst}", flush=True)
                    if done % 500 == 0:
                        print(f"  {done}/{total} ...", flush=True)
        print(f"  n={n} done", flush=True)

print(f"\nRuns complete: {done} ok, {errors} errors.")

# ── Step 2: rebuild timing CSVs from results/runs.csv ─────────────────────────

def parse_instance(inst):
    parts = inst.replace("\\", "/").split("/")
    for i, p in enumerate(parts):
        if p in TOPOS:
            topo_dir = p
            basename = parts[i + 1]
            break
    else:
        return None
    name = basename[:-4] if basename.endswith(".txt") else basename
    m = re.search(r"_n(\d+)_s(\d+)$", name)
    if not m:
        return None
    n, seed = int(m.group(1)), int(m.group(2))
    prefix = name[: m.start()]
    for d in DENSITIES:
        if prefix.endswith("_" + d):
            before = prefix[: -(len(d) + 1)]
            if before.startswith(topo_dir + "_"):
                return topo_dir, before[len(topo_dir) + 1 :], d, n, seed
    return None

print("\nRebuilding timing CSVs ...", flush=True)

rows = list(csv.DictReader(open(RUNS)))
FULL_FIELDS = list(rows[0].keys())
OUT_FIELDS  = FULL_FIELDS + ["topo_dir", "kp_class", "density", "seed_int"]

# Keep last run per unique (algo, topo, kp_class, density, n, seed)
best = {}
for r in rows:
    parsed = parse_instance(r["instance"])
    if not parsed:
        continue
    topo, kp, d, n, seed = parsed
    key = (r["algorithm"], topo, kp, d, n, seed)
    best[key] = (r, topo, kp, d, n, seed)

def write_csv(path, filters, sizes):
    written = missing = 0
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=OUT_FIELDS)
        w.writeheader()
        for algo, topo in filters:
            for n in sizes:
                for kp in CLASSES:
                    for d in DENSITIES:
                        for s in SEEDS:
                            key = (algo, topo, kp, d, n, s)
                            if key in best:
                                r, topo2, kp2, d2, n2, s2 = best[key]
                                row = dict(r)
                                row["topo_dir"]  = topo2
                                row["kp_class"]  = kp2
                                row["density"]   = d2
                                row["seed_int"]  = s2
                                w.writerow(row)
                                written += 1
                            else:
                                missing += 1
    return written, missing

configs = [
    (f"{OUT_DIR}/timing_forest.csv",
     [("forest", t) for t in TOPOS], SIZES_ALL),
    (f"{OUT_DIR}/timing_forest_noheap.csv",
     [("forest_noheap", "forest")], SIZES_ALL),
    (f"{OUT_DIR}/timing_inforest.csv",
     [("inforest", "inforest")], SIZES_ALL),
    (f"{OUT_DIR}/timing_outforest.csv",
     [("outforest", "outforest")], SIZES_ALL),
]

for path, filters, sizes in configs:
    n_written, n_missing = write_csv(path, filters, sizes)
    status = "OK" if not n_missing else f"MISSING {n_missing}"
    print(f"  {os.path.basename(path)}: {n_written} rows  [{status}]")

print("\nDone.")
