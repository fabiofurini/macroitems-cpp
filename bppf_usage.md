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
- $n \in \{100, 200, \ldots, 1\,000\}$
- 6 profit-weight classes × 4 arc densities × 10 seeds = **240 instances per size**, 2 400 total
- Precision parameter: $\varepsilon = 10^{-6}$ (applied to input coefficients and breakpoint ratios)

The script `PARAMETRIC_PSEUDO_FLOW/scripts/hpf_medium_batch.py` automates the runs.
It calls the BPPF binary on each instance, collects CPU times and macroitem
counts, and writes results to `report_performance/timing_hpf_medium.csv`.

## Output Comparison

For each instance, the number and composition of macroitems found by HFMA
and BPPF were compared. Results:

- **2 152 / 2 160 instances**: full agreement
- **8 instances** (all in `weakly_corr`, medium/dense density,
  $700 \le n \le 900$): BPPF merged two consecutive macroitems whose
  breakpoint ratios differed by less than $10^{-6}$

This is consistent with the numerical-precision issue discussed in
Remark (rem:numerical_precision) of the paper: BPPF scales integer
coefficients internally, and increasing the precision parameter beyond
$10^{-6}$ causes integer overflow in those instances.

## Raw Results

The full comparison data (one row per instance) is in:

```
data/runs_bppf_and_hfma_medium.csv
```

Columns: `n`, `kp_class`, `density`, `seed`, `status`, `n_macroitems`
(HFMA), `n_hpf_groups` (BPPF), `forest_cpu_ms`, `hpf_cpu_ms`, `n_params`.
