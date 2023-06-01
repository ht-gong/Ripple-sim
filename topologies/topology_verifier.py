import pandas as pd 
import numpy as np
import csv

tor_count = 108
uplink_count = 6
slice_count = 324
# It takes this many cycles for one tor to cycle through every other tor
cycle_count = tor_count // uplink_count

df = pd.read_csv("dynexp_N=108_k=12_1path.txt", skiprows=2, nrows=slice_count, delim_whitespace=True, header=None)
assert tor_count * uplink_count == df.shape[1], "tor/uplink count mismatch" 

top = np.zeros((tor_count, uplink_count, cycle_count), dtype=int)

for tor in range(tor_count):
    connected = []
    for uplink in range(uplink_count): 
        col = df.iloc[:, tor * uplink_count + uplink].unique()
        col = col[col != -1]
        assert col.size == cycle_count, "cycle count mismatch"
        top[tor][uplink][:] = col
        connected = np.concatenate((connected, col))
    assert np.logical_and(connected >= 0, connected < tor_count).all(), "range error"
    assert len(np.unique(connected)) == tor_count, f"not all connected for tor {tor}"

for tor in range(tor_count):
    for uplink in range(uplink_count):
        for cycle in range(cycle_count):
            connected = top[tor][uplink][cycle]
            assert tor in top[connected, :, cycle], f"connection not bidirectional for tor {tor} and tor {connected}"


with open('topology_matrix.txt', 'w') as f:
    writer = csv.writer(f, delimiter=' ')
    for tor in range(tor_count):
        row = np.zeros((0), dtype=int)
        for uplink in range(uplink_count):
            row = np.concatenate((row, top[tor][uplink][:]))
        writer.writerow(row)
        