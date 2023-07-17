import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
from collections import defaultdict
import os
from datetime import date
import itertools

marker = itertools.cycle((',')) 
dir = date.today().strftime("%Y-%m-%d")
os.makedirs(dir, exist_ok=True)
N_TOR = 108
N_UPLINK = 6
QUEUE_SIZE = 300
PKT_SIZE = 1500

# vlb_settings = ["10us"]
# expander_settings = ["10us_1path", "1000us_1path", "static_1path", "static_5path", "static_ECMP"]
expander_settings = ["10us_1path_ECNK=32"]
optiroute_settings = ["10us_portion2_queue0_lookup_1path",  
                      "10us_portion2_queue8_lookup_1path",
                      "10us_portion5_queue0_lookup_1path",
                      "10us_portion5_queue8_lookup_1path"]
# opera_settings = ["regular_1path", "static_1path", "static_slice60", "static_slice120", "static_slice240", "regular_1path_slowswitching"]
# all_settings = [expander_settings, opera_settings, vlb_settings]
all_settings = [expander_settings, optiroute_settings]
# prefix = ["Expander", "Opera", "VLB"]

prefix = ["Expander", "Optiroute"]

def jain_fairness(arr):
    if np.sum(np.square(arr)) == 0:
        return 0
    N = N_TOR * N_UPLINK - 2
    return np.sum(arr) ** 2 / (N * np.sum(np.square(arr)))

for load in ["20percLoad"]:
    
    for i in range(len(all_settings)):
        for expr in all_settings[i]:
            forlegend = []
            fig, (ax1, ax2, ax3, ax4) = plt.subplots(nrows=4, figsize=(7, 10))
            with open(f"../sim/{prefix[i]}_{load}_{expr}.txt", "r") as f:
                lines = f.readlines()
                queues = defaultdict(list)
                times = np.zeros(0)

                for line in lines:
                    line = line.split()
                    if len(line) <= 0:
                        continue
                    if line[0] == "Queue":
                        tor = int(line[1])
                        port = int(line[2])
                        queues[(tor, port)].append(int(line[3]))
                    if line[0] == "Util":
                        times = np.append(times, [float(line[2])])
                print(len(queues[(0, 6)]))
                jfs = np.zeros(len(times))
                avgs = np.zeros(len(times))
                print(f"Total samples :{len(times)}")
                for t in range(len(times)):
                    arr = []
                    for tor in range(N_TOR):
                        for port in range(6, 6 + N_UPLINK):
                            arr.append(queues[(tor, port)][t])
                    jfs[t] = jain_fairness(np.array(arr))
                    avgs[t] = np.average(np.array(arr) / PKT_SIZE)
                
                m = next(marker)
                a, = ax1.plot(times, jfs, marker=m, label=f"{prefix[i]}_{expr}", linestyle='')
                a, = ax2.plot(times, avgs, marker=m, label=f"{prefix[i]}_{expr}", linestyle='')
                a, = ax3.plot(times[400000 // 20 : 402000 // 20], avgs[400000 // 20 : 402000 // 20], marker=m, label=f"{prefix[i]}_{expr}", linestyle='-')
                a, = ax4.plot(times[600000 // 20 : 604000 // 20], avgs[600000 // 20 : 604000 // 20], marker=m, label=f"{prefix[i]}_{expr}", linestyle='-')  
                forlegend.append(a)
            ax1.set_ylabel("Jain Fairness Index")
            ax2.set_ylabel("Average Queue Size (Packets)")
            ax3.set_ylabel("Average Queue Size (Packets)")
            ax4.set_ylabel("Average Queue Size (Packets)")
            for ax in [ax1, ax2, ax3, ax4]:
                ax.set_xlabel("Time since Start (ms)")
            ax1.legend(handles=forlegend)
            fig.suptitle(f"Jain Fairness and Avg Queue Size {load}")
            fig.tight_layout()
            plt.savefig(f"./{dir}/JF_Qsize_{prefix[i]}_{load}_{expr}.png", bbox_inches='tight')
            plt.show()
            plt.clf()
            plt.close()


