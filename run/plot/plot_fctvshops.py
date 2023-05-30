import sys
import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict
import itertools
import scipy.stats as sts

marker = itertools.cycle(('x', '+', '.', 'o', '*')) 
for perc_load in [30]:
    flows = defaultdict(list)
    with open(f"../sim/General_{perc_load}percLoad_10Gbps.txt", "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split()
            if len(line) <= 0:
                continue
            if line[0] == "FCT":
                flows[line[1] + line[2] + line[5]] = [float(line[4])*1E3, int(line[6])]

    with open(f"../sim/Hoho_{perc_load}percLoad_10Gbps.txt", "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split()
            if len(line) <= 0:
                continue
            if line[0] == "FCT":
                if line[1] + line[2] + line[5] in flows:
                    flows[line[1] + line[2] + line[5]].append(float(line[4])*1E3)
                    flows[line[1] + line[2] + line[5]].append(int(line[6]))
                else:
                    print("Flow not found")
    x = []
    y = []
    for (_ , flowarray) in flows.items():
        if len(flowarray) == 4:
            x.append(flowarray[1] - flowarray[3])
            y.append(flowarray[0] - flowarray[2])
    # xy = np.vstack([x, y])
    # z = sts.gaussian_kde(xy)(xy)                                 
    # plt.scatter(                                                     
    # x, y, c=z, s=50, cmap='jet',
    # edgecolor=['none'], label=None, picker=True, zorder=2                
    # )  
    plt.hist2d(x, y, (50,1000000), cmap="Blues")

plt.axhline(0, color='black')
plt.axvline(0, color='black')
plt.xlabel("Change in hops")
plt.ylabel("Change in FCT")
plt.yscale('symlog')

name = "opera_minus_hoho"
plt.title(f"Hops vs. FCT {name}")
plt.savefig(f"./hops_versus_fct_{name}.png")

plt.show()
