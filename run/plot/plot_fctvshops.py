import sys
import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict
import itertools
import scipy.stats as sts

marker = itertools.cycle(('x', '+', '.', 'o', '*')) 
prop_index = {'fct': 4, 'max_hop': 6, 'last_hop': 7}

for perc_load in [1, 10, 25, 30, 40]:
    flows = defaultdict(list)
    with open(f"../sim/General_{perc_load}percLoad_10Gbps.txt", "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split()
            if len(line) <= 0:
                continue
            if line[0] == "FCT":
                flows[line[1] + line[2] + line[5]] = [{'fct': float(line[4])*1E3, 'max_hop': int(line[6]), 'last_hop': int(line[7])}]

    with open(f"../sim/Hoho_{perc_load}percLoad_10Gbps.txt", "r") as f:
        lines = f.readlines()
        for line in lines:
            line = line.split()
            if len(line) <= 0:
                continue
            if line[0] == "FCT":
                if line[1] + line[2] + line[5] in flows:
                    flows[line[1] + line[2] + line[5]].append({'fct': float(line[4])*1E3, 'max_hop': int(line[6]), 'last_hop': int(line[7])})
                else:
                    print(f"Flow {line[1] + line[2] + line[5]} not found")
                
    # Remove flows that does not match or have the same fct
    flows = {key: val for key, val in flows.items() if len(val) == 2 and val[0]['fct'] - val[1]['fct'] != 0}

    x_max = []
    x_last = []
    y = []
    x_max_nonzero = []
    y_max_nonzero = []
    x_last_nonzero = []
    y_last_nonzero = []

    for (_ , flowarray) in flows.items():
        # if we can find flows in both routings
        d_max_hop = flowarray[0]['max_hop'] - flowarray[1]['max_hop']
        d_fct = flowarray[0]['fct'] - flowarray[1]['fct']
        d_last_hop = flowarray[0]['last_hop'] - flowarray[1]['last_hop']
        x_max.append(d_max_hop)
        x_last.append(d_last_hop)
        y.append(d_fct)
        if d_max_hop != 0:
            x_max_nonzero.append(d_max_hop)
            y_max_nonzero.append(d_fct)
        if d_last_hop != 0:
            x_last_nonzero.append(d_last_hop)
            y_last_nonzero.append(d_fct)
    
    fig, (ax1, ax2, ax3, ax4) = plt.subplots(1, 4, figsize=(25, 15))
    fig.tight_layout()
    
    ax1.hist2d(x_max, y, (50,100000), cmap="Blues")
    ax1.set_title("Delta Max Hop vs. Delta FCT")
    ax2.hist2d(x_last, y, (50,100000), cmap="Blues")
    ax2.set_title("Delta Last Hop vs. Delta FCT")
    ax3.hist2d(x_max_nonzero, y_max_nonzero, (50,100000), cmap="Blues")
    ax3.set_title("Delta Max Hops (nonzero) vs. Delta FCT")
    ax4.hist2d(x_last_nonzero, y_last_nonzero, (50,100000), cmap="Blues")
    ax4.set_title("Delta Last Hops (nonzero) vs. Delta FCT")

    for ax in [ax1, ax2, ax3, ax4]:
        ax.axhline(0, color='black')
        ax.axvline(0, color='black')
        fig.supxlabel("Change in hops")
        fig.supylabel("Change in FCT")
        ax.set_yscale('symlog')

    name = f"opera_minus_hoho_1s_{perc_load}percLoad"
    plt.savefig(f"./hops_versus_fct_{name}.png")
    plt.show()
