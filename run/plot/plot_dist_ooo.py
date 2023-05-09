import sys
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

time_diffs = []
seq_gaps = []
with open(sys.argv[1], 'r') as f:
    lines = f.readlines()
    idx = -1
    for line in lines:
        idx += 1
        line = line.split()
        if len(line) <= 0:
            continue
        if line[0] == "REORDER":
            time1 = int(line[5])/1e6
            time2 = int(line[10])/1e6
            time_diff = time2-time1
            time_diffs.append(time_diff)

time_diffs = np.array(time_diffs)
cnt, b_cnt = np.histogram(time_diffs, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
plt.figure("Time distance")
plt.xlabel("Time distance (us)")
plt.ylabel("CDF")
plt.grid()
a, = plt.plot(b_cnt[1:], cdf)

plt.show()

