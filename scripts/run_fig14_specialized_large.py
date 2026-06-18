#!/usr/bin/env python3
"""Run the specialized large benchmark used in Figure 14."""

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
DEFAULT_OUT_DIR = CODE_DIR / "outputs"

SIZES_LARGE = list(range(10000, 100001, 10000))
CLASSES = [
    "uncorr", "weakly_corr", "strongly_corr",
    "neg_uncorr", "neg_weakly_corr", "neg_strongly_corr",
]
DENSITIES = ["sparse", "medium", "dense", "tree"]
SEEDS = list(range(1, 11))
CONFIGS = {
    "inforest": {
        "archive": ROOT / "GITHUB" / "instances_large_inforest.zip",
        "algorithm": "inforest",
        "output": "runs_fig14_hima.csv",
    },
    "outforest": {
        "archive": ROOT / "GITHUB" / "instances_large_outforest.zip",
        "algorithm": "outforest",
        "output": "runs_fig14_homa.csv",
    },
}


def instance_name(topo: str, n: int, kp_class: str, density: str, seed: int) -> str:
    return f"{topo}_{kp_class}_{density}_n{n:07d}_s{seed:02d}.txt"


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


def write_output(path: Path, topo: str, rows_by_instance: dict[Path, dict[str, str]], instances_dir: Path) -> int:
    fieldnames = [
        "n", "topo_dir", "kp_class", "density", "seed", "n_nodes",
        "n_arcs", "topology", "n_macroitems", "algorithm_ms",
    ]
    count = 0
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as fout:
        writer = csv.DictWriter(fout, fieldnames=fieldnames)
        writer.writeheader()
        for n in SIZES_LARGE:
            for kp_class in CLASSES:
                for density in DENSITIES:
                    for seed in SEEDS:
                        inst = instances_dir / instance_name(topo, n, kp_class, density, seed)
                        row = rows_by_instance.get(inst)
                        if row is None:
                            continue
                        writer.writerow({
                            "n": n,
                            "topo_dir": topo,
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
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--topology", choices=sorted(CONFIGS), default=None)
    args = parser.parse_args()

    exe = args.exe if args.exe.is_absolute() else ROOT / args.exe
    out_dir = args.out_dir if args.out_dir.is_absolute() else ROOT / args.out_dir
    if not exe.exists():
        raise FileNotFoundError(f"missing macroitems executable: {exe}")

    topologies = [args.topology] if args.topology else list(CONFIGS)
    tmp = CODE_DIR / "outputs" / "tmp_fig14"
    if tmp.exists():
        shutil.rmtree(tmp)
    instances_dir = tmp / "instances"
    instances_dir.mkdir(parents=True, exist_ok=True)

    total_errors = 0
    try:
        for topo in topologies:
            cfg = CONFIGS[topo]
            archive_path = cfg["archive"]
            if not archive_path.exists():
                raise FileNotFoundError(f"missing instance archive: {archive_path}")
            run_cwd = tmp / f"run_{topo}"
            run_cwd.mkdir(parents=True, exist_ok=True)
            done = errors = 0
            with zipfile.ZipFile(archive_path) as archive:
                names = set(archive.namelist())
                for n in SIZES_LARGE:
                    for kp_class in CLASSES:
                        for density in DENSITIES:
                            for seed in SEEDS:
                                filename = instance_name(topo, n, kp_class, density, seed)
                                inst = instances_dir / filename
                                if filename not in names:
                                    print(f"MISSING {filename}", flush=True)
                                    errors += 1
                                    continue
                                archive.extract(filename, instances_dir)
                                if run_one(exe, cfg["algorithm"], inst, run_cwd):
                                    done += 1
                                    if done % 200 == 0:
                                        print(f"  {topo}: {done} ...", flush=True)
                                else:
                                    errors += 1
                                inst.unlink(missing_ok=True)
            run_csv = run_cwd / "results" / "runs.csv"
            if not run_csv.exists():
                raise FileNotFoundError(f"missing generated run CSV: {run_csv}")
            out_csv = out_dir / cfg["output"]
            rows = write_output(out_csv, topo, parse_run_csv(run_csv), instances_dir)
            print(f"{topo}: {done} ok, {errors} errors; wrote {out_csv} ({rows} rows)")
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
