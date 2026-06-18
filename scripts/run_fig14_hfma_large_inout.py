#!/usr/bin/env python3
"""Run general HFMA on the large in-forest and out-forest benchmarks."""

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
DEFAULT_OUT_CSV = CODE_DIR / "outputs" / "runs_fig14_hfma_inout.csv"

SIZES_LARGE = list(range(10000, 100001, 10000))
CLASSES = [
    "uncorr", "weakly_corr", "strongly_corr",
    "neg_uncorr", "neg_weakly_corr", "neg_strongly_corr",
]
DENSITIES = ["sparse", "medium", "dense", "tree"]
SEEDS = list(range(1, 11))
ARCHIVES = {
    "inforest": ROOT / "GITHUB" / "instances_large_inforest.zip",
    "outforest": ROOT / "GITHUB" / "instances_large_outforest.zip",
}


def instance_name(topo: str, n: int, kp_class: str, density: str, seed: int) -> str:
    return f"{topo}_{kp_class}_{density}_n{n:07d}_s{seed:02d}.txt"


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
    parser.add_argument("--out-csv", type=Path, default=DEFAULT_OUT_CSV)
    parser.add_argument("--topology", choices=sorted(ARCHIVES), default=None)
    args = parser.parse_args()

    exe = args.exe if args.exe.is_absolute() else ROOT / args.exe
    out_csv = args.out_csv if args.out_csv.is_absolute() else ROOT / args.out_csv
    if not exe.exists():
        raise FileNotFoundError(f"missing macroitems executable: {exe}")

    topologies = [args.topology] if args.topology else list(ARCHIVES)
    tmp = CODE_DIR / "outputs" / "tmp_fig14_hfma"
    if tmp.exists():
        shutil.rmtree(tmp)
    instances_dir = tmp / "instances"
    instances_dir.mkdir(parents=True, exist_ok=True)
    all_rows: list[dict[str, str]] = []
    total_errors = 0
    try:
        for topo in topologies:
            archive_path = ARCHIVES[topo]
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
                                if run_one(exe, inst, run_cwd):
                                    done += 1
                                    if done % 200 == 0:
                                        print(f"  {topo}: {done} ...", flush=True)
                                else:
                                    errors += 1
                                inst.unlink(missing_ok=True)
            rows_by_instance = parse_run_csv(run_cwd / "results" / "runs.csv")
            for n in SIZES_LARGE:
                for kp_class in CLASSES:
                    for density in DENSITIES:
                        for seed in SEEDS:
                            inst = instances_dir / instance_name(topo, n, kp_class, density, seed)
                            row = rows_by_instance.get(inst)
                            if row is None:
                                continue
                            all_rows.append({
                                "n": str(n),
                                "topo_dir": topo,
                                "kp_class": kp_class,
                                "density": density,
                                "seed": str(seed),
                                "n_nodes": row["n_nodes"],
                                "n_arcs": row["n_arcs"],
                                "topology": row["topology"],
                                "n_macroitems": row["n_macroitems"],
                                "algorithm_ms": row["algorithm_ms"],
                            })
            print(f"{topo}: {done} ok, {errors} errors")
            total_errors += errors
        out_csv.parent.mkdir(parents=True, exist_ok=True)
        fieldnames = [
            "n", "topo_dir", "kp_class", "density", "seed", "n_nodes",
            "n_arcs", "topology", "n_macroitems", "algorithm_ms",
        ]
        with out_csv.open("w", newline="") as fout:
            writer = csv.DictWriter(fout, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(all_rows)
        print(f"wrote {out_csv} ({len(all_rows)} rows)")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0 if total_errors == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # noqa: BLE001
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
