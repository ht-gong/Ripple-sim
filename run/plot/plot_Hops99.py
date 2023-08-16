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

expander_settings = ["300us_1path"]
optiroute_settings = ["300us_portion2_queue0_ECMP"]
# expander_settings = ["10us_1path", "1000us_1path", "static_1path", "static_5path", "static_ECMP"]
# expander_settings = ["1us_1path_ECNK=32", "1us_1path_ECNK=65", "10us_1path_ECNK=32", "10us_1path_ECNK=65", "100us_1path_ECNK=32", "100us_1path_ECNK=65", "1000us_1path_ECNK=32", "1000us_1path_ECNK=65", \
#                       "10000us_1path_ECNK=32", "10000us_1path_ECNK=65", "static_1path_ECNK=32", "static_1path_ECNK=65"]
# opera_settings = ["regular_allshort_1path"]
# opera_settings = ["regular_allshort_1path"]
operacalendarq_settings = []
# all_settings = [expander_settings, opera_settings, vlb_settings]
all_settings = [expander_settings, optiroute_settings, operacalendarq_settings]
# prefix = ["Expander", "Opera", "VLB"]
prefix = ["Expander", "Optiroute", "Operacalendarq"]
graph_settings = ["full", "zoomed"]


for load in ["20percLoad_websearch_more_skew"]:
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
    plt.savefig(f"./{dir}/50us_Hops_CDF_{load}.png")
    plt.clf()
    plt.show()
    plt.close()


