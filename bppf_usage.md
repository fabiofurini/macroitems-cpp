# BPPF — Bounded-Precision Parametric Pseudoflow

This document describes how the **Bounded-Precision Parametric Pseudoflow**
(BPPF) algorithm was used in the computational comparison reported in
Section 6.4 of the paper.

## Source

BPPF is the public implementation of Hochbaum's parametric pseudoflow method:

```
https://github.com/hochbaumGroup/Bounded-precision-simple-parametric
```

It is **not** included in this repository. Download and build it separately
following the instructions in that repository.

## How It Was Used

BPPF was run on the **medium-sized `gen-forest` test bed**:
- n in {100, 200, ..., 1,000} (10 sizes)
- 6 profit-weight classes x 4 arc densities x 10 seeds = **240 instances per size**, 2,400 total
- Precision parameter: eps = 1e-6 (applied to input coefficients and breakpoint ratios)

The script `PARAMETRIC_PSEUDO_FLOW/scripts/hpf_medium_batch.py` automates the runs.
It calls the BPPF binary on each instance, collects CPU times and macroitem
counts, and writes results to `report_performance/timing_hpf_medium.csv`.

## How Instances Are Converted for BPPF

BPPF solves a parametric min-cut problem on a source-sink graph. The PCKP
instance is converted to this format by the function `hpf_input()` in
`PARAMETRIC_PSEUDO_FLOW/scripts/hpf_compare.py`.

The conversion maps the PCKP complement closure to a parametric network:
- node 1 = source `s`, node 2 = sink `t`
- item `i` (1-indexed) becomes node `i + 2`
- for each item `i` with profit `p_i` and weight `w_i`:
  - arc `s -> (i+2)` with capacity `w_i` and parametric cost `-p_i`
  - arc `(i+2) -> t` with capacity `-w_i` and parametric cost `p_i`
- for each precedence arc `(tail, head)` in the instance:
  - arc `(head+2) -> (tail+2)` with large capacity (infinity arc in the complement)
- the parameter list is set to probe values bracketing the macroitem ratios
  found by HFMA (one value between each consecutive pair of ratios)

The resulting file uses the DIMACS-like format expected by the BPPF binary:

```
p sequence <n_nodes> <n_arcs> <decimals> <n_params> <param_1> ... <param_k>
n 1 s
n 2 t
a <from> <to> <lower_capacity> <upper_capacity>
...
```

**Example** (3-item instance, 1 arc, 2 probe parameters):
```
c PCKP complement closure
p sequence 5 8 6 2 0.500000 1.500000
n 1 s
n 2 t
a 1 3 100.000000 -200.000000
a 3 2 -100.000000 200.000000
a 1 4 80.000000 -90.000000
a 4 2 -80.000000 90.000000
a 1 5 60.000000 -50.000000
a 5 2 -60.000000 50.000000
a 4 3 1000000.000000
```

## Output Comparison

For each instance, the number and composition of macroitems found by HFMA
and BPPF were compared. Results:

- **2,152 / 2,160 instances**: full agreement
- **8 instances** (all in `weakly_corr`, medium/dense density,
  700 <= n <= 900): BPPF merged two consecutive macroitems whose
  breakpoint ratios differed by less than 1e-6

This is consistent with the numerical-precision issue discussed in the paper:
BPPF scales integer coefficients internally, and increasing the precision
parameter beyond 1e-6 causes integer overflow in those instances.

## Raw Results

The full comparison data (one row per instance) is in:

```
data/runs_bppf_and_hfma_medium.csv
```

Columns: `n`, `kp_class`, `density`, `seed`, `status`, `n_macroitems`
(HFMA), `n_hpf_groups` (BPPF), `forest_cpu_ms`, `hpf_cpu_ms`, `n_params`.
