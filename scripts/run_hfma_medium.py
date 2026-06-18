#!/usr/bin/env python3
"""Run HFMA on the medium gen-forest benchmark instances.

The script unpacks GITHUB/instances_medium.zip into a temporary directory,
runs the heap-based forest algorithm, and writes a compact CSV. Temporary
instances and the binary's intermediate CSV log are removed at the end.
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
DEFAULT_INSTANCES_ZIP = ROOT / "GITHUB" / "instances_medium.zip"
DEFAULT_OUT_CSV = CODE_DIR / "outputs" / "runs_hfma_medium.csv"

SIZES_MEDIUM = [100] + list(range(200, 1100, 100))
CLASSES = [
    "uncorr", "weakly_corr", "strongly_corr",
    "neg_uncorr", "neg_weakly_corr", "neg_strongly_corr",
]
DENSITIES = ["sparse", "medium", "dense", "tree"]
SEEDS = list(range(1, 11))


def extract_instances(zip_path: Path, target_dir: Path) -> None:
    if not zip_path.exists():
        raise FileNotFoundError(f"missing medium instance archive: {zip_path}")
    target_dir.mkdir(parents=True, exist_ok=True)
    print(f"Extracting {zip_path} -> {target_dir}", flush=True)
    with zipfile.ZipFile(zip_path) as archive:
        archive.extractall(target_dir)


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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--instances-zip", type=Path, default=DEFAULT_INSTANCES_ZIP)
    parser.add_argument("--out-csv", type=Path, default=DEFAULT_OUT_CSV)
    parser.add_argument("--limit", type=int, default=None,
                        help="optional smoke-test limit on the number of instances")
    args = parser.parse_args()

    exe = args.exe if args.exe.is_absolute() else ROOT / args.exe
    zip_path = args.instances_zip if args.instances_zip.is_absolute() else ROOT / args.instances_zip
    out_csv = args.out_csv if args.out_csv.is_absolute() else ROOT / args.out_csv
    if args.limit is not None and args.out_csv == DEFAULT_OUT_CSV:
        out_csv = CODE_DIR / "outputs" / "smoke_hfma_medium.csv"
    if not exe.exists():
        raise FileNotFoundError(f"missing macroitems executable: {exe}")

    tmp = CODE_DIR / "outputs" / "tmp_hfma_medium"
    if tmp.exists():
        shutil.rmtree(tmp)
    instances_dir = tmp / "instances"
    run_cwd = tmp / "run"
    run_cwd.mkdir(parents=True, exist_ok=True)

    attempted = done = errors = 0
    try:
        extract_instances(zip_path, instances_dir)
        total = len(SIZES_MEDIUM) * len(CLASSES) * len(DENSITIES) * len(SEEDS)
        stop = False
        for n in SIZES_MEDIUM:
            for kp in CLASSES:
                for density in DENSITIES:
                    for seed in SEEDS:
                        attempted += 1
                        inst = instances_dir / f"forest_{kp}_{density}_n{n:07d}_s{seed:02d}.txt"
                        if not inst.exists():
                            errors += 1
                        elif run_one(exe, inst, run_cwd):
                            done += 1
                            if done % 200 == 0:
                                print(f"  {done}/{total} ...", flush=True)
                        else:
                            errors += 1
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
            for n in SIZES_MEDIUM:
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
