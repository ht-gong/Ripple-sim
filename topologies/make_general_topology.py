import pandas as pd 
import numpy as np
import csv
from itertools import islice

# picoseconds
connected_time = 100000000
transmission_delay = 620000 # packets take this long to be pushed onto wire
reconfig_time = 10000

node_count = 648
tor_count = 108
downlink_count = 6
uplink_count = 6
K = 5
# It takes this many cycles for one tor to cycle through every other tor
cycle_count = tor_count // uplink_count

# Odd slices are all connected, even slices are reconfig slices
slice_count = cycle_count * 3

f = open(f'general_from_dynexp_N={tor_count}_topK={K}.txt', 'w')
f.write(f'{node_count} {downlink_count} {uplink_count} {tor_count}\n')
f.write(f'{slice_count} {connected_time - transmission_delay} {transmission_delay} {reconfig_time}\n')

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

    disconnected = [-1] * tor_count
    f.write(' '.join(str(i) for i in disconnected) + '\n')
    f.write(' '.join(str(i) for i in disconnected) + '\n')

f2 = open('ksp.txt', 'r')
f.write(f2.read())

f.close()
f2.close()






    
    