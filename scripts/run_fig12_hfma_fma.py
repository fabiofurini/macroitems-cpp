#!/usr/bin/env python3
"""Run the reproducible HFMA/FMA benchmark used in Figure 12.

The published archives contain the gen-forest instances for n=100,...,1000
and n=10000,20000. They do not contain the small n=10,...,90 instances, so this
script refreshes the part of Figure 12 that is reproducible from GITHUB.
Instances are extracted one at a time to avoid duplicating the archives.
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
DEFAULT_MEDIUM_ZIP = ROOT / "GITHUB" / "instances_medium.zip"
DEFAULT_LARGE_ZIP = ROOT / "GITHUB" / "instances_large_forest.zip"
DEFAULT_OUT_DIR = CODE_DIR / "outputs"

MEDIUM_SIZES = list(range(100, 1100, 100))
LARGE_SIZES = [10000, 20000]
CLASSES = [
    "uncorr", "weakly_corr", "strongly_corr",
    "neg_uncorr", "neg_weakly_corr", "neg_strongly_corr",
]
DENSITIES = ["sparse", "medium", "dense", "tree"]
SEEDS = list(range(1, 11))
ALGORITHMS = {"forest": "runs_fig12_hfma.csv", "forest_noheap": "runs_fig12_fma.csv"}


def instance_name(n: int, kp_class: str, density: str, seed: int) -> str:
    return f"forest_{kp_class}_{density}_n{n:07d}_s{seed:02d}.txt"


def run_one(exe: Path, algo: str, instance: Path, cwd: Path) -> bool:
    completed = subprocess.run(
        [str(exe), "-i", str(instance), "-a", algo],
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        print(f"FAIL {algo} {instance.name}: {completed.stderr.strip()}", flush=True)
        return False
    return True


def parse_run_csv(path: Path) -> dict[Path, dict[str, str]]:
    rows_by_instance: dict[Path, dict[str, str]] = {}
    with path.open(newline="") as fin:
        for row in csv.DictReader(fin):
            rows_by_instance[Path(row["instance"])] = row
    return rows_by_instance


def run_archive(
    *,
    archive: zipfile.ZipFile,
    archive_names: set[str],
    sizes: list[int],
    exe: Path,
    algo: str,
    instances_dir: Path,
    run_cwd: Path,
) -> tuple[int, int]:
    done = errors = 0
    for n in sizes:
        for kp_class in CLASSES:
            for density in DENSITIES:
                for seed in SEEDS:
                    filename = instance_name(n, kp_class, density, seed)
                    inst = instances_dir / filename
                    if filename not in archive_names:
                        print(f"MISSING {filename}", flush=True)
                        errors += 1
                        continue
                    archive.extract(filename, instances_dir)
                    if run_one(exe, algo, inst, run_cwd):
                        done += 1
                        if done % 200 == 0:
                            print(f"  {algo}: {done} ...", flush=True)
                    else:
                        errors += 1
                    inst.unlink(missing_ok=True)
    return done, errors


def write_output(path: Path, rows_by_instance: dict[Path, dict[str, str]], instances_dir: Path) -> int:
    fieldnames = [
        "n", "topo_dir", "kp_class", "density", "seed", "n_nodes",
        "n_arcs", "topology", "n_macroitems", "algorithm_ms",
    ]
    count = 0
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as fout:
        writer = csv.DictWriter(fout, fieldnames=fieldnames)
        writer.writeheader()
        for n in MEDIUM_SIZES + LARGE_SIZES:
            for kp_class in CLASSES:
                for density in DENSITIES:
                    for seed in SEEDS:
                        inst = instances_dir / instance_name(n, kp_class, density, seed)
                        row = rows_by_instance.get(inst)
                        if row is None:
                            continue
                        writer.writerow({
                            "n": n,
                            "topo_dir": "forest",
                            "kp_class": kp_class,
                            "density": density,
                            "seed": seed,
                            "n_nodes": row["n_nodes"],
                            "n_arcs": row["n_arcs"],
                            "topology": row["topology"],
                            "n_macroitems": row["n_macroitems"],
                            "algorithm_ms": row["algorithm_ms"],
                        })
                        count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--medium-zip", type=Path, default=DEFAULT_MEDIUM_ZIP)
    parser.add_argument("--large-zip", type=Path, default=DEFAULT_LARGE_ZIP)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--algorithm", choices=sorted(ALGORITHMS), default=None)
    args = parser.parse_args()

    exe = args.exe if args.exe.is_absolute() else ROOT / args.exe
    medium_zip = args.medium_zip if args.medium_zip.is_absolute() else ROOT / args.medium_zip
    large_zip = args.large_zip if args.large_zip.is_absolute() else ROOT / args.large_zip
    out_dir = args.out_dir if args.out_dir.is_absolute() else ROOT / args.out_dir
    if not exe.exists():
        raise FileNotFoundError(f"missing macroitems executable: {exe}")
    for path in [medium_zip, large_zip]:
        if not path.exists():
            raise FileNotFoundError(f"missing instance archive: {path}")

    algos = [args.algorithm] if args.algorithm else list(ALGORITHMS)
    tmp = CODE_DIR / "outputs" / "tmp_fig12"
    if tmp.exists():
        shutil.rmtree(tmp)
    instances_dir = tmp / "instances"
    instances_dir.mkdir(parents=True, exist_ok=True)

    total_errors = 0
    try:
        for algo in algos:
            run_cwd = tmp / f"run_{algo}"
            run_cwd.mkdir(parents=True, exist_ok=True)
            done = errors = 0
            with zipfile.ZipFile(medium_zip) as archive:
                names = set(archive.namelist())
                d, e = run_archive(
                    archive=archive, archive_names=names, sizes=MEDIUM_SIZES,
                    exe=exe, algo=algo, instances_dir=instances_dir, run_cwd=run_cwd,
                )
                done += d
                errors += e
            with zipfile.ZipFile(large_zip) as archive:
                names = set(archive.namelist())
                d, e = run_archive(
                    archive=archive, archive_names=names, sizes=LARGE_SIZES,
                    exe=exe, algo=algo, instances_dir=instances_dir, run_cwd=run_cwd,
                )
                done += d
                errors += e
            run_csv = run_cwd / "results" / "runs.csv"
            if not run_csv.exists():
                raise FileNotFoundError(f"missing generated run CSV: {run_csv}")
            out_csv = out_dir / ALGORITHMS[algo]
            rows = write_output(out_csv, parse_run_csv(run_csv), instances_dir)
            print(f"{algo}: {done} ok, {errors} errors; wrote {out_csv} ({rows} rows)")
            total_errors += errors
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0 if total_errors == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # noqa: BLE001
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
