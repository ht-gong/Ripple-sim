import pandas as pd 
import numpy as np
import csv
from itertools import islice

# picoseconds
connected_time = 100000000
reconfig_time = 10000

node_count = 648
tor_count = 108
downlink_count = 6
uplink_count = 6
K = 5
# It takes this many cycles for one tor to cycle through every other tor
cycle_count = tor_count // uplink_count

# Odd slices are all connected, even slices are reconfig slices
slice_count = cycle_count

towrite = []

towrite.append(open(f'general_from_dynexp_N={tor_count}_topK={K}.txt', 'w'))
towrite.append(open(f'ecmp_from_dynexp_N={tor_count}_topK={K}.txt', 'w'))
towrite.append(open(f'vlb_from_dynexp_N={tor_count}_topK={K}.txt', 'w'))

for f in towrite:
    f.write(f'{node_count} {downlink_count} {uplink_count} {tor_count}\n')
    f.write(f'{slice_count} {connected_time} {reconfig_time}\n')

    df = pd.read_csv('topology_matrix.txt', delim_whitespace=True, header=None)
    raw_top = df.to_numpy()

    top = np.zeros((tor_count, uplink_count, cycle_count), dtype=int)
    for tor in range(tor_count):
        for cycle in range(cycle_count):
            top[tor, :, cycle] = raw_top[tor, cycle::cycle_count]

    for cycle in range(cycle_count):
        line = np.zeros((0), dtype=int)
        for tor in range(tor_count):
            line = np.concatenate((line, top[tor, :, cycle]))
        f.write(' '.join(str(i) for i in line) + '\n')

with open('ksp.txt', 'r') as f:
    towrite[0].write(f.read())

with open('ksp.txt', 'r') as f:
    for cycle in range(cycle_count):
        towrite[1].write(f.readline())
        for _ in range(tor_count * (tor_count - 1)):
            paths = []
            paths.extend(f.readline() for i in range(K))
            paths = list(filter(lambda x: len(x.split()) == len(paths[0].split()), paths))
            for path in paths:
                towrite[1].write(path)

with open('ksp.txt', 'r') as f:
    for cycle in range(cycle_count):
        towrite[2].write(f.readline())
        for _ in range(tor_count * (tor_count - 1)):
            paths = []
            paths.extend(f.readline() for i in range(K))
            paths = list(filter(lambda x: len(x.split()) == 3, paths))
            print(paths)
            if len(paths) >= 1:
                towrite[2].write(paths[0])

for f in towrite:
    f.close()






    
    