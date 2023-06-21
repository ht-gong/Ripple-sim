import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict
import os
from datetime import date
import itertools

markers = itertools.cycle(('D', '+', '.', 'o', '*', 'x', 'd')) 
dir = date.today().strftime("%Y-%m-%d")
os.makedirs(dir, exist_ok=True)
flows = defaultdict(list)
expander_settings = ["10us_1path", "100us_1path", "1000us_1path", "static_1path", "static_5path", "static_ECMP"]
opera_settings = ["regular_1path", "static_1path"]
all_settings = [expander_settings, opera_settings]
prefix = ["Expander", "Opera"]

for load in ["1percLoad", "10percLoad", "20percLoad"]:
    forlegend = []
    for i in range(len(all_settings)):
        for expr in all_settings[i]:
            data = {}
            with open(f"../sim/{prefix[i]}_{load}_{expr}.txt", "r") as f:
                lines = f.readlines()
                for line in lines:
                    line = line.split()
                    if len(line) <= 0:
                        continue
                    if line[0] == "FCT":
                        hopcount = int(line[9])
                        if hopcount not in data:
                            data[hopcount] = 1
                        else:
                            data[hopcount] += 1
            bins = list(data.keys())
            vals = list(data.values())
            sortedvals = [x for _,x in sorted(zip(bins, vals))]
            sortedbins = sorted(bins)
            print(sortedvals, sortedbins)
            sortedvals /= np.sum(sortedvals)
            sortedvals = np.cumsum(sortedvals)
            a, = plt.plot(sortedbins, sortedvals, label=f"{prefix[i]}_{expr}", marker=next(markers)) 
            forlegend.append(a)

    plt.xlabel("Hops")
    plt.title(f"Hop CDF {load}")
    plt.legend(handles=forlegend)
    plt.savefig(f"./{dir}/hop_CDF_{load}.png")

    plt.show()
    plt.close()


