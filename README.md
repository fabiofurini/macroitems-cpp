# Macroitems — Source Code

Implementation of the algorithms described in:

> **"Optimal Macroitem Sequences in the Precedence Constrained Knapsack Problem"**  
> Valerio Dose, Fabio Furini, Marco Locatelli

---

## Requirements

- **C++20** compiler (GCC ≥ 11, Clang ≥ 14)
- **CMake** ≥ 3.16
- **HiGHS** LP solver (vendored under `third_party/HiGHS/`)

HiGHS is only needed for the LP-based algorithms (`lp`, `dinkelbach`). The
four main macroitem algorithms (`forest`, `forest_noheap`, `inforest`,
`outforest`) have no external dependencies.

---

## Build

```bash
# Clone HiGHS into third_party/ (if not already present):
git clone https://github.com/ERGO-Code/HiGHS.git third_party/HiGHS

# Configure and build (Release mode by default):
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

This produces the `macroitems_main` binary in `build/`.

**CMake options:**

| Option | Default | Description |
|---|---|---|
| `HIGHS_SOURCE_DIR` | `third_party/HiGHS` | Path to the HiGHS source tree |
| `MACROITEMS_ENABLE_IPO` | `ON` | Enable link-time optimization |
| `MACROITEMS_ENABLE_NATIVE_ARCH` | `ON` | Compile for the native CPU (`-march=native`) |

---

## Usage

```
macroitems_main -i <instance> -a <algorithm> [-c <capacity>] [-r <result_file>] [-g <graphml>]
```

| Flag | Description |
|---|---|
| `-i` | Path to the instance file (required) |
| `-a` | Algorithm: `forest` \| `forest_noheap` \| `inforest` \| `outforest` \| `dinkelbach` \| `lp` |
| `-c` | Capacity: absolute integer (e.g. `4`) or percentage (e.g. `50%`). Defaults to total item weight. |
| `-r` | Write text result to this file (optional) |
| `-g` | Write yEd GraphML visualization to this file (optional) |

**Examples:**

```bash
# Run HFMA on a forest instance
./build/macroitems_main -i instances/forest/forest_uncorr_sparse_n0000100_s01.txt \
    -a forest -c 50%

# Run HIMA on an in-tree instance
./build/macroitems_main -i instances/inforest/inforest_uncorr_sparse_n0000100_s01.txt \
    -a inforest -c 50%
```

---

## Algorithms

| Flag | Name in paper | Description | Complexity |
|---|---|---|---|
| `forest` | **HFMA** | Heap-based Forest Macroitem Algorithm | O(n² log n) |
| `forest_noheap` | **FMA** | Forest Macroitem Algorithm (linear scan, no heap) | O(n²) worst case |
| `inforest` | **HIMA** | Heap-based In-tree Macroitem Algorithm | O(n log n) |
| `outforest` | **HOMA** | Heap-based Out-tree Macroitem Algorithm | O(n log n) |
| `dinkelbach` | — | Dinkelbach parametric algorithm via LP (HiGHS) | — |
| `lp` | — | Direct PCKP LP relaxation via HiGHS | — |

---

## Source Layout

```
CMakeLists.txt              Build configuration
src/
  algo_forest.cpp           HFMA: heap-based directed-forest contraction, O(n² log n)
  algo_forest_noheap.cpp    FMA: linear-scan directed-forest contraction
  algo_in_forest.cpp        HIMA: O(n log n) in-tree max-heap algorithm
  algo_out_forest.cpp       HOMA: O(n log n) out-tree min-heap algorithm
  algo_dinkelbach.cpp       Dinkelbach parametric algorithm (via HiGHS)
  instance.cpp              Graph loading, validation, stream printing
  solution.cpp              Primal solution from macroitem sequence
  lp_highs.cpp              HiGHS wrapper
  pckp_lp.cpp               Direct PCKP LP relaxation
  result_writer.cpp         Text result output and timing
  yed_writer.cpp            yEd GraphML visualizations
include/
  macroitems.hpp            Single public header (includes all below)
  macroitems/
    core.hpp                Core data structures and ratio comparison
    instance.hpp            Instance struct
    solution.hpp            Solution struct
    algo_forest.hpp         HFMA interface
    algo_in_forest.hpp      HIMA interface
    algo_out_forest.hpp     HOMA interface
    lp.hpp                  LP interface
    pckp_lp.hpp             PCKP LP interface
    result_writer.hpp       Result writer interface
    yed_writer.hpp          yEd writer interface
examples/
  macroitems_main.cpp       Command-line driver
scripts/
  gen_instances.py          Benchmark instance generator (see instances/)
  run_benchmark.py          Run HFMA/FMA on small+medium test bed
  run_benchmark_large.py    Run HFMA/HIMA/HOMA on large test bed
```

---

## Reproducing the Benchmarks

**Step 1 — Generate instances** (or unzip the provided archives):
```bash
python3 scripts/gen_instances.py
```
This creates `instances/forest/`, `instances/inforest/`, `instances/outforest/`
with 20 880 instances total (6 classes × 4 densities × 10 seeds × 87 sizes).

**Step 2 — Run the benchmarks:**
```bash
# Small + medium test bed (n = 10..1000), all algorithms
python3 scripts/run_benchmark.py

# Large test bed (n = 10 000..100 000), heap-based algorithms only
python3 scripts/run_benchmark_large.py
```

Results are appended to `results/runs.csv`.

**Step 3 — BPPF comparison** (requires the Bounded-Precision Pseudoflow binary):  
See `bppf_usage.md` in this folder.

---

## Instance Format

Plain text, comments start with `#`:

```
n 100
profits  770 655 676 ...
weights  10 673 626 ...
arcs 27
1 4
2 7
...
```

- **`n`**: number of items  
- **`profits`**: integer profit of each item (1-indexed)  
- **`weights`**: integer weight of each item (1-indexed)  
- **`arcs k`**: followed by `k` lines, each `u v` meaning item `u` must be selected before item `v` (precedence arc)
