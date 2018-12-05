#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt



# TODO: parse some data


# TODO Number of reader threads
r_range = np.arange(1, 20 + 1)

# TODO Number of writer threads (constant)
w = 20

# TODO Doing average or 99%-tile?
is_avg = True

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
