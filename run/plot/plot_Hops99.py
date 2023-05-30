import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict

flows = defaultdict(list)
with open("../sim/Hoho_10percLoad_10Gbps.txt", "r") as f:
    lines = f.readlines()
    for line in lines:
        line = line.split()
        if len(line) <= 0:
            continue
        if line[0] == "FCT":
            flowsize = int(line[3])
            flowfct = int(line[6])
            flows[flowsize].append(flowfct)

flowsizes = [flowsize for (flowsize, flowfct) in flows.items()]
flowsizes.sort()
fcts = []
for flowsize in flowsizes:
    fcts.append(np.percentile(flows[flowsize], 50))
a, = plt.plot(flowsizes, fcts, marker = "x", label="Hoho")

flows = defaultdict(list)
with open("../sim/General_10percLoad_10Gbps.txt", "r") as f:
    lines = f.readlines()
    for line in lines:
        line = line.split()
        if len(line) <= 0:
            continue
        if line[0] == "FCT":
            flowsize = int(line[3])
            flowfct = int(line[6])
            flows[flowsize].append(flowfct)

flowsizes = [flowsize for (flowsize, flowfct) in flows.items()]
flowsizes.sort()
fcts = []
for flowsize in flowsizes:
    fcts.append(np.percentile(flows[flowsize], 50))
b, = plt.plot(flowsizes, fcts, marker = "o", label="Opera")

plt.xscale("log")
plt.xlabel("Flow Size (bytes)")
plt.ylabel("FCT (us)")
plt.legend(handles=[a, b])

name = "10perc"
plt.title(f"Hop comparison {name}")
plt.savefig(f"./hop_comparison_{name}.png")

plt.show()
