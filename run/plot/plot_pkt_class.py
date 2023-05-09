import sys
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

hop_diffs = []
queue_diffs = []
with open(sys.argv[1], 'r') as f:
    lines = f.readlines()
    for line in lines:
        line = line.split(" ")
        if len(line) <= 0:
            continue
        if line[0] == "REORDER":
            hops1 = int(line[6])
            hops2 = int(line[10])
            hop_diff = hops2-hops1
            hop_diffs.append(hop_diff)
            queue1 = int(line[7])
            queue2 = int(line[11])
            queue_diff = queue2-queue1
            queue_diffs.append(queue_diff)

hop_diffs = np.array(hop_diffs)
cnt, b_cnt = np.histogram(hop_diffs, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
plt.figure("Hops")
plt.xlabel("Hops diff")
plt.ylabel("CDF")
plt.grid()
a, = plt.plot(b_cnt[1:], cdf, label="Hop diff")
plt.savefig("hops_diff.png")

queue_diffs = np.array(queue_diffs)
cnt, b_cnt = np.histogram(queue_diffs, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
plt.figure("Queues")
plt.xlabel("Queueing diff")
plt.ylabel("CDF")
plt.grid()
b, = plt.plot(b_cnt[1:], cdf, label="Queue diff")
plt.savefig("queueing_diff.png")

plt.show()

