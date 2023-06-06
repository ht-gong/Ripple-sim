import pandas as pd 
import numpy as np
import csv
import matplotlib.pyplot as plt

tor_count = 108
uplink_count = 6
slice_count = 324
slice_per_supslice = 3 
supslice_count = slice_count // slice_per_supslice

# We simulate and cycle through the entire topology 4 times
n_cycles = 4

df = pd.read_csv("dynexp_N=108_k=12_1path.txt", skiprows=2, nrows=slice_count, delim_whitespace=True, header=None)

top = np.zeros((tor_count, uplink_count, supslice_count * n_cycles), dtype=int)
port_connection = np.zeros((tor_count, tor_count), dtype=int)
src_dst_connection = {}

df = df.iloc[::slice_per_supslice, :].reset_index(drop=True)

for slice in range(supslice_count):
    for tor in range(tor_count):
        for uplink in range(uplink_count):
            #print(df.iloc[slice, tor * uplink_count + uplink])
            dst = df.iloc[slice, tor * uplink_count + uplink]
            top[tor, uplink, slice::supslice_count] = dst
            port_connection[tor, dst] = uplink
            if (tor, dst) not in src_dst_connection:
                src_dst_connection[(tor, dst)] = np.zeros(0, dtype=int)
            src_dst_connection[(tor, dst)] = \
                np.concatenate((src_dst_connection[(tor, dst)], list(range(slice, supslice_count * n_cycles, supslice_count))))
RTT = np.zeros((tor_count, tor_count, supslice_count * 2))        
for src in range(tor_count):
    for dst in range(tor_count):
        for n in range(supslice_count * 2):
            if src == dst:
                continue
            curtime = n
            inter_tor_1 = top[src, port_connection[src, dst], curtime]
            if inter_tor_1 != dst:
                conn = src_dst_connection[(inter_tor_1, dst)]
                curtime = np.min(conn[conn >= curtime])
            
            inter_tor_2 = top[dst, port_connection[dst, src], curtime]
            if inter_tor_2 != src:
                conn = src_dst_connection[(inter_tor_2, src)]
                curtime = np.min(conn[conn >= curtime])
            RTT[src, dst, n] = curtime - n


for (src, dst) in [(0, 46), (23, 103), (26, 95)]:

    plt.plot(list(range(0, supslice_count * 2)), RTT[src, dst, :])
    plt.axvline(x=107, color='black')
    plt.ylabel("RTT (timeslices)")
    plt.xlabel("Index of sending timeslice")
    plt.title(f"RTT SrcTor = {src}, DstTor = {dst}")
    plt.show()
    plt.savefig(f"./tmp/RTT_per_timeslice_src{src}dst_{dst}.png")  
    plt.clf()

    shifted = np.concatenate((np.zeros(1), RTT[src, dst, :]))

    plt.plot(list(range(0, supslice_count * 2)), RTT[src, dst, :] - shifted[:-1])
    plt.axvline(x=107, color='black')
    plt.ylabel("Change in RTT (timeslices)")
    plt.xlabel("Index of sending timeslice")
    plt.title(f"Delta RTT SrcTor = {src}, DstTor = {dst}")
    plt.show()
    plt.savefig(f"./tmp/RTT_delta_per_timeslice_src{src}dst_{dst}.png") 
    plt.clf() 
