import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
from collections import defaultdict
import os
from datetime import date
import itertools
import random

marker = itertools.cycle((',')) 
dir = date.today().strftime("%Y-%m-%d")
os.makedirs(dir, exist_ok=True)
N_TOR = 108
N_UPLINK = 6
QUEUE_SIZE = 300
PKT_SIZE = 1500
N_SAMPLES = 25

expander_settings = ["1000us_LongShort", "1000us_1path"]
optiroute_settings = []
# expander_settings = ["10us_1path", "1000us_1path", "static_1path", "static_5path", "static_ECMP"]
# expander_settings = ["1us_1path_ECNK=32", "1us_1path_ECNK=65", "10us_1path_ECNK=32", "10us_1path_ECNK=65", "100us_1path_ECNK=32", "100us_1path_ECNK=65", "1000us_1path_ECNK=32", "1000us_1path_ECNK=65", \
#                       "10000us_1path_ECNK=32", "10000us_1path_ECNK=65", "static_1path_ECNK=32", "static_1path_ECNK=65"]
# opera_settings = ["regular_allshort_1path"]
# opera_settings = ["regular_allshort_1path"]
hoho_settings = []
# all_settings = [expander_settings, opera_settings, vlb_settings]
all_settings = [expander_settings, hoho_settings, optiroute_settings]
# prefix = ["Expander", "Opera", "VLB"]
prefix = ["Expander", "Hoho", "Optiroute"]

for load in ["20percLoad_websearch"]:
    for i in range(len(all_settings)):
        for expr in all_settings[i]:
            forlegend = []
            fig, axs = plt.subplots(nrows=N_SAMPLES, figsize=(8, 50))
            with open(f"../sim/{prefix[i]}_{load}_{expr}.txt", "r") as f:
                lines = f.readlines()
                queues = defaultdict(list)
                queuesatchange = defaultdict(list)
                times = np.zeros(0)
                timesatchange = np.zeros(0)

                for line in lines:
                    line = line.split()
                    if len(line) <= 0:
                        continue
                    if line[0] == "Queue":
                        tor = int(line[1])
                        port = int(line[2])
                        queues[(tor, port)].append(int(line[3]))
                    elif line[0] == "Queuesize":
                        line = line[1].split(',')
                        tor = int(line[0])
                        port = int(line[1])
                        queuesatchange[(tor, port)].append(int(line[2]))
                    elif line[0] == "Util":
                        if line[1] == "Curtime":
                            if len(timesatchange) == 0 or timesatchange[-1] != float(line[2]):
                                timesatchange = np.append(timesatchange, [float(line[2])])
                        else:
                            times = np.append(times, [float(line[2])])

                random.seed(42)
                sampled = []
                links = list(range(6,12))
                tors = list(range(108))
                for q in range(N_SAMPLES):
                    sample_q = (random.choice(tors), random.choice(links))
                    tors.remove(sample_q[0])
                    sampled.append(sample_q)
                print(sampled)

                for j in range(N_SAMPLES):
                    a, = axs[j].plot(times[300000 // 20: 310000 // 20], [x / PKT_SIZE for x in queues[sampled[j]][300000 // 20: 310000 // 20]] , marker = '.', linestyle = '-', color = 'blue')
                    a, = axs[j].plot(timesatchange[300000 // 1001: 310000 // 1001], [x / PKT_SIZE for x in queuesatchange[sampled[j]][300000 // 1001: 310000 // 1001]] , marker = '.', linestyle = '', color='red')
            for ax in axs:
                ax.set_xlabel("Time since Start (ms)")
                ax.set_ylabel("Queue Size (PKTs)")

            fig.suptitle(f"Sampled Queue Size {load}")
            fig.tight_layout()
            plt.savefig(f"./{dir}/Queuedetails_{prefix[i]}_{load}_{expr}.png", bbox_inches='tight')
            plt.show()
            plt.clf()
            plt.close()


