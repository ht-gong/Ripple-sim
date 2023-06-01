import pandas as pd 
import numpy as np
import networkx as nx
import csv
from itertools import islice

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
        top[tor, :, cycle] = raw_top[tor][uplink_count * cycle : uplink_count * (cycle + 1)]

f = open('ksp.txt', 'w')

for cycle in range(cycle_count):
    print(f"Calculating KSP for slice {cycle}:")
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

    f.write(str(cycle) + '\n')
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





    
    