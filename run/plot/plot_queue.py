import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
from collections import defaultdict
import os
from datetime import date
import itertools

marker = itertools.cycle((',', '+', '.', 'o', '*', 'x', 'd')) 
dir = date.today().strftime("%Y-%m-%d")
os.makedirs(dir, exist_ok=True)
QUEUE_SIZE = 300
PKT_SIZE = 1500

vlb_settings = ["1us", "10us"]
# vlb_settings = ["10us"]
# expander_settings = ["10us_1path", "1000us_1path", "static_1path", "static_5path", "static_ECMP"]
expander_settings = ["10us_1path_ECNK=32", "10us_1path_ECNK=65", "100us_1path_ECNK=32", "100us_1path_ECNK=65", "1000us_1path_ECNK=32", "1000us_1path_ECNK=65", \
                     "static_1path_ECNK=32", "static_1path_ECNK=65"]
# opera_settings = ["regular_1path", "static_1path", "static_slice60", "static_slice120", "static_slice240", "regular_1path_slowswitching"]
opera_settings = ["regular_1path"]
# all_settings = [expander_settings, opera_settings, vlb_settings]
all_settings = [expander_settings]
# prefix = ["Expander", "Opera", "VLB"]
prefix = ["Expander"]

for load in ["1percLoad", "10percLoad", "20percLoad"]:
    forlegend = []
    for i in range(len(all_settings)):
        for expr in all_settings[i]:
            with open(f"../sim/{prefix[i]}_{load}_{expr}.txt", "r") as f:
                lines = f.readlines()
                queues = defaultdict(list)
                data = dict.fromkeys(range(0, QUEUE_SIZE + 1), 0)
                
                for line in lines:
                    line = line.split()
                    if len(line) <= 0:
                        continue
                    if line[0] == "Queue":
                        tor = int(line[1])
                        port = int(line[2])
                        queues[f"{tor},{port}"].append(int(line[3]))
                        data[int(line[3]) // PKT_SIZE] += 1
                        
                bins = list(data.keys())
                vals = list(data.values())
                sortedvals = [x for _,x in sorted(zip(bins, vals))]
                sortedbins = sorted(bins)
                print(sortedvals, sortedbins)
                sortedvals /= np.sum(sortedvals)
                sortedvals = np.cumsum(sortedvals)
                if expr[-2:] == '65':
                    # a, = plt.plot(sortedbins, sortedvals, color=forlegend[-1].get_color(), label=f"{prefix[i]}_{expr}", linestyle='--') 
                    a, = plt.plot(sortedbins[:51], sortedvals[:51], color=forlegend[-1].get_color(), label=f"{prefix[i]}_{expr}", linestyle='--') 
                else:
                    # a, = plt.plot(sortedbins, sortedvals, label=f"{prefix[i]}_{expr}", linestyle='-')
                    a, = plt.plot(sortedbins[:51], sortedvals[:51], label=f"{prefix[i]}_{expr}", linestyle='-') 
                forlegend.append(a)
        plt.xlabel("QueueSize")
        plt.title(f"QueueSize CDF {load}")
        plt.legend(handles=forlegend)
        plt.savefig(f"./{dir}/QueueSize_CDF_{load}.png")
        plt.show()
        plt.close()


