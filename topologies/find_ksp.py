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

bucket_shortest = np.zeros(10)
bucket_topk = np.zeros(10)

shortestsize = np.zeros(10)
shortestsizecyc0 = np.zeros(10)

nxthopcount = np.zeros((6, 108, 107, 108), dtype=float)
# index 0: all slice ssp
# index 1: 0th slice ssp
# index 2: all slice ksp
# index 3: 0th slice ksp
# index 4: all slice ecmp
# index 5: 0th slice ecmp

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
    
    for src in range(tor_count):
        for dst in range(tor_count):
            if src == dst:
                continue
            paths = path_dict[(src, dst)]
            counter = 0
            for path in paths:
                if len(path) == len(paths[0]):
                    counter += 1
            shortestsize[counter] += 1
            if cycle == 0:
                shortestsizecyc0[counter] += 1

    for src in range(tor_count):
        countdst = 0
        for dst in range(tor_count):
            if src == dst:
                continue
            path = path_dict[(src, dst)][0]
            for tor in path[1:-1]:
                nxthopcount[0, src, countdst, tor] += 1
                if cycle == 0:
                    nxthopcount[1, src, countdst, tor] += 1
            countdst += 1

    for src in range(tor_count):
        countdst = 0
        for dst in range(tor_count):
            if src == dst:
                continue
            paths = path_dict[(src, dst)]
            for path in paths:
                for tor in path[1:-1]:
                    nxthopcount[2, src, countdst, tor] += 1
                    if cycle == 0:
                        nxthopcount[3, src, countdst, tor] += 1
            countdst += 1
    
    for src in range(tor_count):
        countdst = 0
        for dst in range(tor_count):
            if src == dst:
                continue
            paths = path_dict[(src, dst)]
            countpaths = 0
            eqpaths = [p for p in paths if len(p) == len(paths[0])]
            for path in eqpaths:
                for tor in path[1:-1]:
                    nxthopcount[4, src, countdst, tor] += 1 / len(eqpaths)
                    if cycle == 0:
                        nxthopcount[5, src, countdst, tor] += 1 / len(eqpaths)
            countdst += 1
        

def jain_fairness(arr):
    if np.sum(np.square(arr)) == 0:
        return 0
    N = tor_count - 2
    return np.sum(arr) ** 2 / (N * np.sum(np.square(arr)))

fairness = [[],[],[],[],[],[]]

for src in range(tor_count):
    for dst in range(tor_count - 1):
        for i in range(len(fairness)):
            fairness[i].append(jain_fairness(nxthopcount[i, src, dst, :])) 

forlegend = []
names = ['Expander switching singleshortest', 'Expander static singleshortest', 'Expander switching 5shortest', 'Expander static 5shortest', \
         'Expander switching ECMP', 'Expander static ECMP']
for i in range(len(fairness)):
    count, bins_count = np.histogram(fairness[i], bins=100)
    cdf = np.cumsum(count / sum(count))
    print(cdf, bins_count)
    a, = plt.plot(np.concatenate(([0], bins_count[1:], [1])), np.concatenate(([0], cdf, [1])), label = names[i])
    forlegend.append(a)

c, = plt.plot([0, 0.99999, 1], [0, 0, 1], label = 'VLB')

plt.legend(handles=forlegend + [c], loc='best', bbox_to_anchor=(1.05, 0.3))
plt.title("Jain Fairness CDF")
plt.xlabel("Jain Fairness Index")
plt.savefig(f"./tmp/jain_fairness_cdf", bbox_inches='tight')
plt.clf()


# print(shortestsize, np.sum(shortestsize))
# shortestsize = shortestsize[1:max(np.flatnonzero(shortestsize)) + 1]
# ind = np.arange(1, len(shortestsize) + 1, dtype=int)                
# plt.bar(ind, shortestsize / np.sum(shortestsize), color='g')
# plt.title("PDF of number of shortest paths")
# plt.savefig(f"./tmp/no_shortest_path_hist.png")  
# plt.clf()

# shortestsizecyc0 = shortestsizecyc0[1:max(np.flatnonzero(shortestsize)) + 1]
# ind = np.arange(1, len(shortestsizecyc0) + 1, dtype=int)                
# plt.bar(ind, shortestsizecyc0 / np.sum(shortestsizecyc0), color='g')
# plt.title("PDF of number of shortest paths in 0th slice")
# plt.savefig(f"./tmp/no_shortest_path_hist_0.png")  
# plt.clf()

# bucket_shortest = bucket_shortest[1:max(np.flatnonzero(bucket_shortest)) + 1]
# ind = np.arange(1, len(bucket_shortest) + 1, dtype=int)
# print(f"Shortest path avg hop no. = {np.sum(bucket_shortest * ind) / np.sum(bucket_shortest)}")
# plt.bar(ind, bucket_shortest / np.sum(bucket_shortest), color='g')
# plt.title("PDF of length of shortest path")
# plt.savefig(f"./tmp/shortest_path_hist.png")  

# plt.clf()

# bucket_topk = bucket_topk[1:max(np.flatnonzero(bucket_topk)) + 1]
# ind = np.arange(1, len(bucket_topk) + 1, dtype=int)
# print(f"Top {K} paths avg hop no. = {np.sum(bucket_topk * ind) / np.sum(bucket_topk)}")
# plt.title(f"PDF of length of top {K} paths")
# plt.bar(ind, bucket_topk / np.sum(bucket_topk), color='g')
# plt.savefig(f"./tmp/top{K}_paths_hist.png")  

f.close()


    
    