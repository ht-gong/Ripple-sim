import sys
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

flows = []
flows_ooo = []
with open(sys.argv[1], 'r') as f:
    lines = f.readlines()
    for line in lines:
        line = line.split(" ")
        if len(line) <= 0:
            continue
        if line[0] == "FCT":
            flows.append(int(line[3]))
            if int(line[6]) > 0:
                flows_ooo.append(int(line[3]))

flows_dist = np.array(flows)
cnt, b_cnt = np.histogram(flows_dist, bins=10000)
logbin = np.logspace(np.log10(b_cnt[0]), np.log10(b_cnt[-1]), len(b_cnt))
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
a, = plt.plot(b_cnt[1:], cdf, label="All Flows")

flows_ooo_dist = np.array(flows_ooo)
cnt, b_cnt = np.histogram(flows_ooo_dist, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
b, = plt.plot(b_cnt[1:], cdf, label="OOO Flows")

plt.legend(handles=[a, b])
plt.xlabel("Size (Bytes)")
plt.ylabel("CDF")
plt.xscale('log')
plt.grid()
if len(sys.argv) > 2:
    name = sys.argv[2]
    plt.title(name)
    plt.savefig(f"{name}.png")
plt.show()

