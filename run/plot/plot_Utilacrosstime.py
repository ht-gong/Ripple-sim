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
SAMPLE_TIME1 = 300000
SAMPLE_TIME2 = 500000
SAMPLE_INTERVAL = 20
 
expander_settings = ["300us_1path"]
optiroute_settings = ["300us_portion2_queue0_ECMP"]
# expander_settings = ["10us_1path", "1000us_1path", "static_1path", "static_5path", "static_ECMP"]
# expander_settings = ["1us_1path_ECNK=32", "1us_1path_ECNK=65", "10us_1path_ECNK=32", "10us_1path_ECNK=65", "100us_1path_ECNK=32", "100us_1path_ECNK=65", "1000us_1path_ECNK=32", "1000us_1path_ECNK=65", \
#                       "10000us_1path_ECNK=32", "10000us_1path_ECNK=65", "static_1path_ECNK=32", "static_1path_ECNK=65"]
# opera_settings = ["regular_allshort_1path"]
# opera_settings = ["regular_allshort_1path"]
operacalendarq_settings = ["1path"]
# all_settings = [expander_settings, opera_settings, vlb_settings]
all_settings = [expander_settings, optiroute_settings, operacalendarq_settings]
# prefix = ["Expander", "Opera", "VLB"]
prefix = ["Expander", "Optiroute", "Operacalendarq"]
graph_settings = ["full", "zoomed"]


for load in ["20percLoad_websearch_more_skew"]:
    fig, (ax1) = plt.subplots(nrows=1)
    forlegend = []
    for i in range(len(all_settings)):
        for expr in all_settings[i]:
            
            with open(f"../sim/{prefix[i]}_{load}_{expr}.txt", "r") as f:
                lines = f.readlines()
                util = np.zeros(0)
                times = np.zeros(0)

                for line in lines:
                    line = line.split()
                    if len(line) <= 0:
                        continue
                    if line[0] == "Util":
                        util = np.append(util, [float(line[1])])
                        times = np.append(times, [float(line[2])])
            print(f"Total samples :{len(times)}")
            a, = ax1.plot(times, util, marker=next(marker), label=f"{prefix[i]}_{expr}", linestyle='') 
            forlegend.append(a)
    ax1.set_ylabel("Utilization (%)")
    for ax in [ax1]:
        ax.set_xlabel("Time since Start (ms)")
    ax1.legend(handles=forlegend)
    fig.suptitle(f"Utilization {load} ")
    fig.tight_layout()
    plt.savefig(f"./{dir}/Util_{load}.png", bbox_inches='tight')
    plt.show()
    plt.clf()
    plt.close()


