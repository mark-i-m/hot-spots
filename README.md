# HotSpots

This is the repo for our databases class project.

We modify the concurrent B-tree with OLC from https://github.com/wangziqi2016/index-microbench.

# Building

```console
> git clone https://github.com/mark-i-m/hot-spots
> cd hot-spots
> git submodule update --init --recursive
> make
```

You need to have `g++` 7 or newer, along with GNU `make`.

To run tests for B-tree implementations: `make tst.all`. To run a test `foo`:
`make foo.tst`.

# Running workloads

All benchmarks were run using the `benchmarks/run_eval.sh` script in this repo.
It allows varying the number of insertion and lookup threads, the B-tree
implementation, and the number of operations per thread, the initial load of
the B-tree, and the frequency with which to output statistics. See the usage
message for more info:

```console
> cd benchmarks
> bash run_eval.sh # prints help message
```

To run benchmarks, you need to have passwordless `sudo` access. You also need
to have `cpupower` installed. To get this tool, install `apt install
linux-tools-common`.  Then, attempt to run `cpupower`. This will fail with a
message that tells you what else to install (it is kernel-specific). Install
the first package only.

# Plotting graphs

The results of the workloads are produced in the `results` directory. To graph
results, use the `results/plot.py` script. The figures in the `figures`
directory were generated using this script. See the usage message for more
info:

```console
> ./plot.py # prints the help message
```

This script requires `python 3`, `numpy`, and `matplotlib`.

# Organization

The organization of this repository is as follows:

- `README.md`: This README.
- `makefile`: The makefile for building and running everything.
- `benchmarks/`: Implementation of benchmarks and utilities for running.
- `btrees/`: Implementation of B-trees.
- `figures/`: Figures produced from results with the `plot.py` script.
- `results/`: Results produced from the `run_eval.sh` script.
- `tests/`: Correctness tests for the B-trees.
