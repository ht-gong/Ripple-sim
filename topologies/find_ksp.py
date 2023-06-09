import pandas as pd 
import numpy as np
import networkx as nx
import csv
from itertools import islice
import matplotlib.pyplot as plt

tor_count = 108
uplink_count = 6
K = 5
# Uplink port number starts at 6
uplink_portno = 6
# It takes this many cycles for one tor to cycle through every other tor
cycle_count = tor_count // uplink_count

df = pd.read_csv("topology_matrix.txt", delim_whitespace=True, header=None)
raw_top = df.to_numpy()

top = np.zeros((tor_count, uplink_count, cycle_count), dtype=int)
for tor in range(tor_count):
    for cycle in range(cycle_count):
        top[tor, :, cycle] = raw_top[tor, cycle::cycle_count]

f = open('ksp.txt', 'w')
f2 = open('ksp_org.txt', 'w')

bucket_shortest = np.zeros(10)
bucket_topk = np.zeros(10)

for cycle in range(cycle_count):
    print(f"Calculating KSP for slice {cycle}")
    lines = []
    for tor in range(tor_count):
        line = str(tor) + ' ' + ' '.join(str(i) for i in top[tor, : , cycle])
        lines.append(line)

    G = nx.parse_adjlist(lines, nodetype=int)
    path_dict = {}

    for i in range(0, tor_count):
        for j in range(i + 1, tor_count):
            path_gen = nx.shortest_simple_paths(G, i, j)
            paths = list(islice(path_gen, K))
            rev_paths = [list(reversed(path)) for path in paths]
            path_dict[(i, j)] = paths
            path_dict[(j, i)] = rev_paths

            bucket_shortest[len(paths[0]) - 1] += 1
            for path in paths:
                bucket_topk[len(path) - 1] += 1
    
    f.write(str(cycle * 3) + '\n')
    for src in range(tor_count):
        for dst in range(tor_count):
            if src == dst:
                continue
            paths = path_dict[(src, dst)]
            for path in paths:
                mapped_uplink = []
                for i in range(len(path) - 1):
                    uplink = np.argwhere(top[path[i], :, cycle] == path[i + 1])[0][0] + uplink_portno
                    mapped_uplink.append(uplink)
                mapped_uplink = [src, dst] + mapped_uplink
                f.write(' '.join(str(i) for i in mapped_uplink) + '\n')
    
    for src in range(tor_count):
        for dst in range(tor_count):
            if src == dst:
                continue
            paths = path_dict[(src, dst)]
            for path in paths:
                mapped_uplink = []
                for i in range(len(path) - 1):
                    uplink = paths[i]
                    mapped_uplink.append(uplink)
                mapped_uplink = [src, dst] + mapped_uplink
                f2.write(' '.join(str(i) for i in mapped_uplink) + '\n')
                
    f.write(str(cycle * 3 + 1) + '\n')
    f.write(str(cycle * 3 + 2) + '\n')

bucket_shortest = bucket_shortest[1:max(np.flatnonzero(bucket_shortest)) + 1]
ind = np.arange(1, len(bucket_shortest) + 1, dtype=int)
print(f"Shortest path avg hop no. = {np.sum(bucket_shortest * ind) / np.sum(bucket_shortest)}")
plt.bar(ind, bucket_shortest / np.sum(bucket_shortest), color='g')
plt.title("PDF of length of shortest path")
plt.savefig(f"./tmp/shortest_path_hist.png")  

plt.clf()

bucket_topk = bucket_topk[1:max(np.flatnonzero(bucket_topk)) + 1]
ind = np.arange(1, len(bucket_topk) + 1, dtype=int)
print(f"Top {K} paths avg hop no. = {np.sum(bucket_topk * ind) / np.sum(bucket_topk)}")
plt.title(f"PDF of length of top {K} paths")
plt.bar(ind, bucket_topk / np.sum(bucket_topk), color='g')
plt.savefig(f"./tmp/top{K}_paths_hist.png")  

f.close()
f2.close()


    
    