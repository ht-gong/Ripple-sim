import sys

flows = []
for file in sys.argv[1:]:
    with open(file, 'r') as f:
        data = list(filter(None, f.readlines()))
        data = list([d.strip('\n') for d in data])
        flows += data

flows = sorted(flows, key=lambda flow: int(flow.split()[3]))

with open('merge_out.txt', 'w') as f:
    for i in range(len(flows)):
        if i == len(flows)-1:
            f.write(f"{flows[i]}")
        else:
            f.write(f"{flows[i]}\n")
