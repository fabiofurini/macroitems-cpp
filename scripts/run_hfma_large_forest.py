#!/usr/bin/env python3
"""Run HFMA on the large gen-forest benchmark instances.

Instances are read from GITHUB/instances_large_forest.zip and extracted one at
a time into a temporary directory, so the large archive is not duplicated on
disk. The script writes a compact CSV with one row per successful run.
"""

from __future__ import annotations

import argparse
import csv
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path


HERE = Path(__file__).resolve().parent
CODE_DIR = HERE.parent
ROOT = CODE_DIR.parent
DEFAULT_EXE = CODE_DIR / "build" / "macroitems_main"
DEFAULT_INSTANCES_ZIP = ROOT / "GITHUB" / "instances_large_forest.zip"
DEFAULT_OUT_CSV = CODE_DIR / "outputs" / "runs_hfma_large_forest.csv"

SIZES_LARGE = list(range(10000, 100001, 10000))
CLASSES = [
    "uncorr", "weakly_corr", "strongly_corr",
    "neg_uncorr", "neg_weakly_corr", "neg_strongly_corr",
]
DENSITIES = ["sparse", "medium", "dense", "tree"]
SEEDS = list(range(1, 11))


def run_one(exe: Path, instance: Path, cwd: Path) -> bool:
    completed = subprocess.run(
        [str(exe), "-i", str(instance), "-a", "forest"],
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        print(f"FAIL {instance.name}: {completed.stderr.strip()}", flush=True)
        return False
    return True


def parse_run_csv(path: Path) -> dict[Path, dict[str, str]]:
    rows_by_instance: dict[Path, dict[str, str]] = {}
    with path.open(newline="") as fin:
        for row in csv.DictReader(fin):
            rows_by_instance[Path(row["instance"])] = row
    return rows_by_instance


def parse_sizes(raw_sizes: list[int] | None) -> list[int]:
    if raw_sizes is None:
        return SIZES_LARGE
    sizes = sorted(set(raw_sizes))
    unknown = [n for n in sizes if n not in SIZES_LARGE]
    if unknown:
        raise ValueError(f"unsupported large sizes: {unknown}")
    return sizes


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--instances-zip", type=Path, default=DEFAULT_INSTANCES_ZIP)
    parser.add_argument("--out-csv", type=Path, default=DEFAULT_OUT_CSV)
    parser.add_argument("--sizes", type=int, nargs="+", default=None,
                        help="optional subset of large n values to run")
    parser.add_argument("--limit", type=int, default=None,
                        help="optional smoke-test limit on the number of instances")
    args = parser.parse_args()

    exe = args.exe if args.exe.is_absolute() else ROOT / args.exe
    zip_path = args.instances_zip if args.instances_zip.is_absolute() else ROOT / args.instances_zip
    out_csv = args.out_csv if args.out_csv.is_absolute() else ROOT / args.out_csv
    if (args.limit is not None or args.sizes is not None) and args.out_csv == DEFAULT_OUT_CSV:
        out_csv = CODE_DIR / "outputs" / "smoke_hfma_large_forest.csv"
    if not exe.exists():
        raise FileNotFoundError(f"missing macroitems executable: {exe}")
    if not zip_path.exists():
        raise FileNotFoundError(f"missing large forest instance archive: {zip_path}")

    sizes = parse_sizes(args.sizes)
    tmp = CODE_DIR / "outputs" / "tmp_hfma_large"
    if tmp.exists():
        shutil.rmtree(tmp)
    instances_dir = tmp / "instances"
    run_cwd = tmp / "run"
    instances_dir.mkdir(parents=True, exist_ok=True)
    run_cwd.mkdir(parents=True, exist_ok=True)

    attempted = done = errors = 0
    try:
        total = len(sizes) * len(CLASSES) * len(DENSITIES) * len(SEEDS)
        stop = False
        with zipfile.ZipFile(zip_path) as archive:
            names = set(archive.namelist())
            for n in sizes:
                for kp in CLASSES:
                    for density in DENSITIES:
                        for seed in SEEDS:
                            attempted += 1
                            filename = f"forest_{kp}_{density}_n{n:07d}_s{seed:02d}.txt"
                            inst = instances_dir / filename
                            if filename not in names:
                                print(f"MISSING {filename}", flush=True)
                                errors += 1
                            else:
                                archive.extract(filename, instances_dir)
                                if run_one(exe, inst, run_cwd):
                                    done += 1
                                    if done % 100 == 0:
                                        print(f"  {done}/{total} ...", flush=True)
                                else:
                                    errors += 1
                                inst.unlink(missing_ok=True)
                            if args.limit is not None and attempted >= args.limit:
                                stop = True
                                break
                        if stop:
                            break
                    if stop:
                        break
                if stop:
                    break
        if stop:
            print(f"\nStopped after --limit={args.limit}.", flush=True)
        run_csv = run_cwd / "results" / "runs.csv"
        if not run_csv.exists():
            raise FileNotFoundError(f"missing generated run CSV: {run_csv}")
        rows_by_instance = parse_run_csv(run_csv)
        out_csv.parent.mkdir(parents=True, exist_ok=True)
        with out_csv.open("w", newline="") as fout:
            fieldnames = [
                "n", "topo_dir", "kp_class", "density", "seed", "n_nodes",
                "n_arcs", "topology", "n_macroitems", "algorithm_ms",
            ]
            writer = csv.DictWriter(fout, fieldnames=fieldnames)
            writer.writeheader()
            for n in sizes:
                for kp in CLASSES:
                    for density in DENSITIES:
                        for seed in SEEDS:
                            inst = instances_dir / f"forest_{kp}_{density}_n{n:07d}_s{seed:02d}.txt"
                            row = rows_by_instance.get(inst)
                            if row is None:
                                continue
                            writer.writerow({
                                "n": n,
                                "topo_dir": "forest",
                                "kp_class": kp,
                                "density": density,
                                "seed": seed,
                                "n_nodes": row["n_nodes"],
                                "n_arcs": row["n_arcs"],
                                "topology": row["topology"],
                                "n_macroitems": row["n_macroitems"],
                                "algorithm_ms": row["algorithm_ms"],
                            })
        print(f"\nDone: {done} ok, {errors} errors.")
        print(f"Wrote {out_csv} ({sum(1 for _ in out_csv.open()) - 1} rows)")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0 if errors == 0 and done > 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # noqa: BLE001
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
