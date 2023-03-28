import sys

N_SLICES = 216

ToRs = {}

occupancy = []
with open(sys.argv[1], 'r') as f:
    txt = f.readlines()
    for line in txt:
        line = line.split()
        if line[0] == 'Occupancy':
            occupancy.append(line)

#only take last sample
data = occupancy[len(occupancy)-1][1:]
tor_idx = [i for i, el in enumerate(data) if 'ToR' in el]
tor_lists = []
for i in tor_idx:
    if tor_idx.index(i) == len(tor_idx)-1:
        tor_lists.append(data[i:])
    else:
        idx = tor_idx.index(i)
        tor_lists.append(data[i:tor_idx[idx+1]])
for tor in tor_lists:
    n_tor = int(tor[0].split(':')[1])
    ToRs[n_tor] = {}
    p = tor[1:]
    port_idx = [i for i, el in enumerate(p) if 'Port' in el]
    port_lists = []
    for i in port_idx:
        if port_idx.index(i) == len(port_idx)-1:
            port_lists.append(p[i:])
        else:
            idx = port_idx.index(i)
            port_lists.append(p[i:port_idx[idx+1]])
    for port in port_lists:
        n_port = int(port[0].split(':')[1])
        ToRs[n_tor][n_port] = (int(port[0].split(':')[2]),{})
        s = port[1:]
        for queue in s:
            queue = queue.split(':')
            ToRs[n_tor][n_port][1][int(queue[1])] = int(queue[2])

#MAX SIZE PER TOR PER QUEUE PER PORT
'''
for tor in ToRs:
    for port in ToRs[tor]:
        print(ToRs[tor][port][0])
'''

#MAX SIZE PER CALENDAR SLICE
'''
for tor in ToRs:
    for port in ToRs[tor]:
        for slice in range(N_SLICES):
            print(ToRs[tor][port][1][slice])
'''


#MAX TOT SIZE PER SLICE
'''
size_per_slice = [0 for i in range(N_SLICES)]
for i in range(N_SLICES):
    for tor in ToRs:
        for port in ToRs[tor]:
            size_per_slice[i] += ToRs[tor][port][1][i]
    print(size_per_slice[i])
'''
