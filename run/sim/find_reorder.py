from collections import defaultdict
import sys

reorders = set()
flow_sizes = defaultdict(int)
tot_flows = 0
reorder_flows = 0
with open(sys.argv[1], 'r') as f:
    lines = f.readlines()
    flows = defaultdict(set)
    for l in lines:
        l = l.split(" ")
        if len(l) < 3:
            continue
        flow_key = l[1]+ ":" + l[2]
        if l[0] == "NACK":
            flows[flow_key].add(int(l[3]))
        elif l[0] == "Filled":
            if int(l[4]) not in flows[flow_key]:
                reorders.add(flow_key)
                print(f"REORDER {l[1]} {l[2]} {l[4]} {l[3]}")
        elif l[0] == "FCT":
            tot_flows += 1
            if flow_key in reorders:
                reorder_flows += 1
                flow_sizes[l[3]] += 1
                
print(f"Reorder ratio: {reorder_flows/tot_flows}")
print(flow_sizes)
