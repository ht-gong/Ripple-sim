import sys
import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict
import itertools

marker = itertools.cycle(('x', '+', '.', 'o', '*')) 
for perc_load in [1, 10, 25, 30]:
    flows = defaultdict(list)
    with open(f"../sim/General_{perc_load}percLoad_10Gbps.txt", "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split()
            if len(line) <= 0:
                continue
            if line[0] == "FCT":
                flowsize = int(line[3])
                flowfct = float(line[4])*1E3
                flows[flowsize].append(flowfct)

    flowsizes = [flowsize for (flowsize, flowfct) in flows.items()]
    flowsizes.sort()
    fcts_base = []
    for flowsize in flowsizes:
        fcts_base.append(np.percentile(flows[flowsize], 99))
    fcts_base = np.array(fcts_base)

    flows = defaultdict(list)
    with open(f"../sim/Hoho_{perc_load}percLoad_10Gbps.txt", "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split()
            if len(line) <= 0:
                continue
            if line[0] == "FCT":
                flowsize = int(line[3])
                flowfct = float(line[4])*1E3
                flows[flowsize].append(flowfct)

    flowsizes = [flowsize for (flowsize, flowfct) in flows.items()]
    flowsizes.sort()
    fcts_diff = []
    for flowsize in flowsizes:
        fcts_diff.append(np.percentile(flows[flowsize], 99))
    fcts_diff = np.array(fcts_diff)
    
    min_size = min(fcts_base.size, fcts_diff.size)
    fcts_base = fcts_base[:min_size]
    fcts_diff = fcts_diff[:min_size]
    plt.plot(flowsizes, fcts_base/fcts_diff, marker = next(marker), linestyle = ' ', label=perc_load)

ratios = [0.9, 1, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7]
labels = ["-10%", "0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%"]
plt.yticks(ratios, labels)

plt.axhline(1, color="black", linestyle='dashed')
plt.legend(loc="upper left")
plt.xscale("log")
plt.xlabel("Flow Size (bytes)")
plt.ylabel("FCT reduction (%)")

name = "opera_hoho"
plt.title(f"FCT reduction {name}")
plt.savefig(f"./fct_reduction_{name}.png")

plt.show()
