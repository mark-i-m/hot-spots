#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt

from datetime import datetime
import re
import sys
import os
import heapq

# Usage: ./script (r|w) (avg|99) <list of directories that contain data>
#
#   r indicates that we are varying r
#   w indicates that we are varying w
#
#   avg indicates that we compute avg throughput
#   99  indicates computing 99%-tile (slowest) throughput
#
# All runs should have the same value for W, B, X, N, and should be run with
# the same implementation; only R varies.
USAGE = """
Usage: ./script (avg|99) <list of directories that contain data>

  r indicates that we are varying r
  w indicates that we are varying w

  avg indicates that we compute avg throughput
  99  indicates computing 99%-tile (slowest) throughput

All runs should have the same value for W (or R), B, X, N, and should be run
with the same implementation; only R (or W) varies.

We always discard the first 10% of data from each thread. This avoids measuring
any warmup period.
"""

if len(sys.argv) < 4:
    print(USAGE)
    sys.exit(1)

if sys.argv[1] == "r":
    is_r = True
elif sys.argv[1] == "w":
    is_r = False
else:
    print(USAGE)
    sys.exit(1)

if sys.argv[2] == "avg":
    is_avg = True
elif sys.argv[2] == "99":
    is_avg = False
else:
    print(USAGE)
    sys.exit(1)

DISCARD_WARMUP = 10
MILLION = 1E6

def thread_avg(thread_file, n, x):
    total = 0
    totaln = 0

    # how many measurements to discard for warmup
    discard_amt = n * DISCARD_WARMUP / 100 / x

    with open(thread_file) as f:
        for line in f.readlines():
            # discard warmup period
            if discard_amt > 0:
                discard_amt -= 1
                continue

            total += int(line)
            totaln += x

    return (total, totaln)

# returns the slowest 99%-tile of data points (per million ops), including
# among any in the pre-existing array `p99`.
#
# NOTE: `p99` contains a the negated versions of everything.
def thread_p99(thread_file, p99, n, x):
    # how many measurements to discard for warmup
    discard_amt = n * DISCARD_WARMUP / 100 / x

    # how many is the top 99%-tile?
    p99_size = n / 100

    # python only has a min-heap, but we want a max-heap, so we will negate
    # everything before putting in the heap and negate again at the end.

    total = 0
    totaln = 0

    with open(thread_file) as f:
        for line in f.readlines():
            # discard warmup period
            if discard_amt > 0:
                discard_amt -= 1
                continue

            # report everything per Mop
            # FIXME: what if MILLION % x != 0?
            # FIXME: what if n % x != 0?
            if totaln < MILLION:
                total += int(line)
                totaln += x
                continue

            if len(p99) < p99_size:
                heapq.heappush(p99, -total)
            else:
                heapq.heappushpop(p99, -total)

def avg_for_threads(threads, n, x):
    per_thread_averages = [thread_avg(t, n, x) for t in threads]
    print(per_thread_averages)
    total = 0
    totaln = 0
    for thread_total, count in per_thread_averages:
        count = float(count) / MILLION
        total += thread_total
        totaln += count
    return total / totaln # per Mop

# see comments on `thread_p99` fn...
def p99_for_threads(threads, n, x):
    p99 = []
    for t in threads:
        thread_p99(t, p99, n, x)

    return -heapq.nlargest(1, p99)[0]

# parse file names and extract parameters
#
# we populate all of these parameters in increasing order of R
class Experiment:
    def __init__(self, directory):
        self.directory = directory

        #               1         2        3         4       5          6          7
        REGEX = """([0-9-]+)_r([0-9]+)_w([0-9]+)_t([1-3])_b([0-9]+)_n([0-9]+)_x([0-9]+)/?"""
        DATETIMEFMT = """%Y-%m-%d-%H-%M-%S"""

        cleaned_dir_name = os.path.basename(os.path.normpath(directory))
        m = re.match(REGEX, cleaned_dir_name)

        if m is None:
            raise ValueError("Unable to parse directory name %s" % directory)

        self.date = datetime.strptime(m.group(1), DATETIMEFMT)
        self.r = int(m.group(2))
        self.w = int(m.group(3))
        impl = int(m.group(4))
        if impl == 1:
            self.impl = "olc"
        elif impl == 2:
            self.impl = "hybrid"
        elif impl == 3:
            self.impl = "rb"
        else:
            raise ValueError("Invalid btree type %s" % impl)
        self.b = int(m.group(5))
        self.n = int(m.group(6))
        self.x = int(m.group(7))

        # check dir exists
        if not os.path.exists(directory):
            raise ValueError("No such directory: %s" % directory)

        # get list of files and make sure there are enough of them
        files = os.listdir(directory)
        self.reader_files = []
        self.writer_files = []
        for f in files:
            if str(f)[0] == 'R':
                self.reader_files.append(f)
            elif str(f)[0] == 'W':
                self.writer_files.append(f)
            elif str(f) == "expt.log":
                pass # skip the log
            else:
                raise ValueError("Directory contains odd file: %s" % f)

        if len(self.reader_files) != self.r:
            raise ValueError("Directory has wrong number of reader files. Expected %d, found %d" % (
                self.r, len(self.reader_files)))
        if len(self.writer_files) != self.w:
            raise ValueError("Directory has wrong number of writer files. Expected %d, found %d" % (
                self.w, len(self.writer_files)))

    def __repr__(self):
        return ("Experiment(impl=%s, R=%d, W=%d, N=%d, X=%d, B=%d, %s)" %
                (self.impl, self.r, self.w, self.n, self.x, self.b, self.directory))

    # Computes average cycles per `R` million elements
    def avg_reader_time(self):
        reader_files = [os.path.join(self.directory, r) for r in self.reader_files]
        return avg_for_threads(reader_files, self.n, self.x) / self.r

    def avg_writer_time(self):
        writer_files = [os.path.join(self.directory, w) for w in self.writer_files]
        return avg_for_threads(writer_files, self.n, self.x) / self.w

    def p99_reader_time(self):
        reader_files = [os.path.join(self.directory, r) for r in self.reader_files]
        return p99_for_threads(reader_files, self.n, self.x)

    def p99_writer_time(self):
        writer_files = [os.path.join(self.directory, w) for w in self.writer_files]
        return p99_for_threads(writer_files, self.n, self.x)

experiments = [Experiment(d) for d in sys.argv[3:]]
experiments.sort(key=lambda e: e.r if is_r else e.w)

print(experiments)

# Check that only R varies
if is_r:
    rs = []
    w = None
else:
    ws = []
    r = None
x = None
b = None
n = None

for e in experiments:
    if is_r:
        if w is None:
            w = e.w
        else:
            if w != e.w:
                raise ValueError("W varies")
    else:
        if r is None:
            r = e.r
        else:
            if r != e.r:
                raise ValueError("R varies")

    if x is None:
        x = e.x
    else:
        if x != e.x:
            raise ValueError("X varies")

    if b is None:
        b = e.b
    else:
        if b != e.b:
            raise ValueError("B varies")

    if n is None:
        n = e.n
    else:
        if n != e.n:
            raise ValueError("N varies")

    if is_r:
        rs.append(e.r)
    else:
        ws.append(e.w)

# Parameters
BAR_WIDTH = 0.35
if is_r:
    dom = np.arange(min(rs), max(rs)+1)
else:
    dom = np.arange(min(ws), max(ws)+1)

# For each value in dom, average reader/writer time per million ops
avg_reader_time = [e.avg_reader_time() if is_avg else e.p99_reader_time() for e in experiments]
avg_writer_time = [e.avg_writer_time() if is_avg else e.p99_writer_time() for e in experiments]

# Plot
plt.figure(1, figsize=(5, 3.5))
plt.bar(dom - BAR_WIDTH/2, avg_reader_time, BAR_WIDTH, color="#dbcccc", hatch=r"""////""", edgecolor="black", label='reader')
plt.bar(dom + BAR_WIDTH/2, avg_writer_time, BAR_WIDTH, color="#8faab3", hatch=r"""\\\\""", edgecolor="black", label='writer')

plt.xlabel('$%s$ = Number of %s Threads' % ('R' if is_r else 'W', 'Reader' if is_r else 'Writer'))
plt.xticks(dom)

plt.ylabel(r'%s Time per Million ops (cycles/Mop)'
        % ('Average' if is_avg else '99%-tile'))

plt.title('Reader/Writer Throughput as Number of\n%s Threads $%s$ Varies ($%s$ = %d Writer Threads)' %
        ('Reader' if is_r else 'Writer', 'R' if is_r else 'W', 'W' if is_r else 'R',w if is_r else r))

plt.legend()
plt.tight_layout()
plt.savefig("/tmp/figure.png", bbox_inches="tight")
plt.show()
