from collections import defaultdict
import sys

reorders = set()
flow_sizes = defaultdict(int)
tot_flows = 0
reordered_flows = 0

with open(sys.argv[1], 'r') as f:
    lines = f.readlines()
    flows = defaultdict(set)
    for l in lines:
        l = l.split(" ")
        if len(l) < 1:
            continue
        if l[0] == "REORDER" and int(l[1]) not in reorders:
            reordered_flows += 1
            flow_sizes[l[2]] += 1
            reorders.add(int(l[1]))
        elif l[0] == "FCT":
            tot_flows += 1

print(f"Reorder ratio: {reordered_flows/tot_flows}")
print(f"Tot flows: {tot_flows} Reordered flows: {reordered_flows}")
print(flow_sizes)
