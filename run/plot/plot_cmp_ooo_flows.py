import sys
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

flows = []
flows_ooo1 = []
flows_ooo2 = []
with open(sys.argv[1], 'r') as f:
    lines = f.readlines()
    for line in lines:
        line = line.split(" ")
        if len(line) <= 0:
            continue
        if line[0] == "FCT":
            flows.append(int(line[3]))
            if int(line[6]) > 0:
                flows_ooo1.append(int(line[3]))

with open(sys.argv[2], 'r') as f:
    lines = f.readlines()
    for line in lines:
        line = line.split(" ")
        if len(line) <= 0:
            continue
        if line[0] == "FCT":
            flows.append(int(line[3]))
            if int(line[6]) > 0:
                flows_ooo2.append(int(line[3]))

flows_dist = np.array(flows)
cnt, b_cnt = np.histogram(flows_dist, bins=10000)
logbin = np.logspace(np.log10(b_cnt[0]), np.log10(b_cnt[-1]), len(b_cnt))
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
a, = plt.plot(b_cnt[1:], cdf, label="All Flows")

flows_ooo_dist1 = np.array(flows_ooo1)
cnt, b_cnt = np.histogram(flows_ooo_dist1, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
b, = plt.plot(b_cnt[1:], cdf, label="OOO Flows 10%")

flows_ooo_dist2 = np.array(flows_ooo2)
cnt, b_cnt = np.histogram(flows_ooo_dist2, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
c, = plt.plot(b_cnt[1:], cdf, label="OOO Flows 50%")

plt.legend(handles=[a, b, c])
plt.xlabel("Size (Bytes)")
plt.ylabel("CDF")
plt.xscale('log')
plt.grid()
if len(sys.argv) > 3:
    name = sys.argv[3]
    plt.title(name)
    plt.savefig(f"{name}.png")
plt.show()

