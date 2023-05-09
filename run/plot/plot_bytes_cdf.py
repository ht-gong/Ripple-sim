import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

tot_bytes = 0
size_buckets = defaultdict(int)
with open("../traffic_gen/hadoop_flows_50load_5sec_648hosts_100G.htsim", "r") as f:
    lines = f.readlines()
    for line in lines:
        line = line.split()
        if len(line) <= 0:
            continue
        flowlen = int(line[2])
        tot_bytes += flowlen
        size_buckets[flowlen] += flowlen

x = [flowlen for flowlen,flowbytes in size_buckets.items()]
y = [flowbytes/tot_bytes for flowlen,flowbytes in size_buckets.items()]
x.sort()
y.sort()

percsofar = 0
for i in range(len(y)):
    y[i] += percsofar
    percsofar = y[i]

h, = plt.plot(x, y, label="Hadoop")

#plt.legend(handles=[h, d])
plt.xlabel("Size (bytes)")
plt.ylabel("CDF")
plt.grid()
plt.xscale('log')
#plt.locator_params(axis='x', nbins=20)
plt.savefig("rtt_cmp.pdf")
plt.show()
