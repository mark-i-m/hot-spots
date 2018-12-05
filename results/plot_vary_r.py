#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt

import re
import sys
import os

# Usage: ./script (avg|99) <list of directories that contain data>
#   avg indicates that we compute avg throughput
#   99  indicates computing 99%-tile (slowest) throughput
#
# All runs should have the same value for W, B, X, N, and should be run with
# the same implementation; only R varies.
USAGE = """
Usage: ./script (avg|99) <list of directories that contain data>
  avg indicates that we compute avg throughput
  99  indicates computing 99%-tile (slowest) throughput

All runs should have the same value for W, B, X, N, and should be run with
the same implementation; only R varies.
"""

if len(sys.argv) < 3:
    print(USAGE)
    sys.exit(1)
elif sys.argv[1] == "avg":
    is_avg = True
elif sys.argv[1] == "99":
    is_avg = False
else:
    print(USAGE)
    sys.exit(1)

# parse file names and extract parameters
#
# we populate all of these parameters in increasing order of R
class Experiment:
    def __init__(self, directory):
        self.directory = directory

        REGEX = """([0-9-]+)_btree_([a-zA-Z]+)_r([0-9]+)_w([0-9]+)_n([0-9]+)_x([0-9]+)_b([0-9]+)/?"""

        cleaned_dir_name = os.path.basename(os.path.normpath(directory))
        m = re.match(REGEX, cleaned_dir_name)

        if m is None:
            raise ValueError("Unable to parse directory name %s" % directory)

        self.date = "" # TODO parse group 1
        self.impl = m.group(2)
        self.r = int(m.group(3))
        self.w = int(m.group(4))
        self.n = int(m.group(5))
        self.x = int(m.group(6))
        self.b = int(m.group(7))

        # TODO params
        # TODO check dir exists and has enough files
        pass

    def __repr__(self):
        return ("Experiment(R=%d, W=%d, N=%d, X=%d, B=%d, %s)" % 
                (self.r, self.w, self.n, self.x, self.b, self.directory))

experiments = [Experiment(d) for d in sys.argv[2:]]
experiments.sort(key=lambda e: e.r)

print(experiments)


# TODO: parse some data


# TODO Number of reader threads
r_range = np.arange(1, 20 + 1)

# TODO Number of writer threads (constant)
w = 20

# Parameters
BAR_WIDTH = 0.35

# For each value of r, average reader/writer time per million ops
avg_reader_time = [1E6 / r for r in r_range]
avg_writer_time = [2E6 / r for r in r_range]

# Plot
plt.figure(1, figsize=(5, 3.5))
plt.bar(r_range - BAR_WIDTH/2, avg_reader_time, BAR_WIDTH, color="#dbcccc", hatch=r"""////""", edgecolor="black", label='reader')
plt.bar(r_range + BAR_WIDTH/2, avg_writer_time, BAR_WIDTH, color="#8faab3", hatch=r"""\\\\""", edgecolor="black", label='writer')

plt.xlabel('$R$ = Number of Reader Threads')
plt.xticks(r_range)

plt.ylabel(r'%s Throughput (seconds/$R$ Mops)'
        % ('Average' if is_avg else '99%-tile'))

plt.title('Reader/Writer Throughput as Number of\nReader Threads $R$ Varies ($W$ = %d Writer Threads)' % w)

plt.legend()
plt.tight_layout()
plt.savefig("/tmp/figure.png", bbox_inches="tight")
plt.show()
