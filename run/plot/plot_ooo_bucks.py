import sys
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

buckets = [5, 10, 25, 50, 100, 250, 500]
buckets = list(map(lambda x: x*10**3, buckets))
tot_flows = list(range(0, len(buckets)))
ooo_flows = list(range(0, len(buckets)))
rtx_flows = list(range(0, len(buckets)))

def find_bucket(size):
    for i in range(len(buckets)):
        if size < buckets[i]:
            return i

with open(sys.argv[1], 'r') as f:
    lines = f.readlines()
    for line in lines:
        line = line.split(" ")
        if len(line) <= 0:
            continue
        if line[0] == "FCT":
            tot_flows[find_bucket(int(line[3]))] += 1
            if int(line[6]) > 0:
                ooo_flows[find_bucket(int(line[3]))] += 1
            if int(line[7]) > 0 and int(line[6]) > 0:
                rtx_flows[find_bucket(int(line[3]))] += 1

ooo_ratio = list(map(lambda p: p[0]/p[1], zip(ooo_flows, tot_flows)))
rtx_ratio = list(map(lambda p: p[0]/p[1], zip(rtx_flows, tot_flows)))

bar_width = 0.4
fig = plt.figure()

pos_ooo = np.arange(len(buckets))
pos_rtx = [x + bar_width for x in pos_ooo]

plt.bar(pos_ooo, ooo_ratio, width = bar_width, label = "OOO Ratio")
plt.bar(pos_rtx, rtx_ratio, width = bar_width, label="RTX Ratio")
plt.ylim([0,1.0])
plt.xticks([r+bar_width/2 for r in range(len(buckets))],list(map(str, buckets)))
plt.grid()
plt.legend()
if len(sys.argv) > 2:
    name = sys.argv[2]
    plt.title(name)
    plt.savefig(f"{name}.png")
plt.show()



