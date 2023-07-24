import numpy as np
import matplotlib.pyplot as plt
import seaborn as sea
import pandas as pd
from collections import defaultdict
import random
import os
from datetime import date

plt.rc('axes', titlesize=24)     # fontsize of the axes title
plt.rc('axes', labelsize=24)    # fontsize of the x and y labels
plt.rc('xtick', labelsize=24)    # fontsize of the tick labels
plt.rc('ytick', labelsize=24)    # fontsize of the tick labels
plt.rc('legend', fontsize=24)    # legend fontsize
plt.rc('figure', titlesize=17)  # fontsize of the figure title
plt.grid(which='major', linewidth=0.5, linestyle='--')
plt.margins(x=0.01,y=0.01)


SAMPLES=200
QUEUES =100

dir = date.today().strftime("%Y-%m-%d")
os.makedirs(dir, exist_ok=True)

expander_settings = ["static_1path_websearch", "static_operapaths"]
optiroute_settings = []
# expander_settings = ["10us_1path", "1000us_1path", "static_1path", "static_5path", "static_ECMP"]
# expander_settings = ["1us_1path_ECNK=32", "1us_1path_ECNK=65", "10us_1path_ECNK=32", "10us_1path_ECNK=65", "100us_1path_ECNK=32", "100us_1path_ECNK=65", "1000us_1path_ECNK=32", "1000us_1path_ECNK=65", \
#                       "10000us_1path_ECNK=32", "10000us_1path_ECNK=65", "static_1path_ECNK=32", "static_1path_ECNK=65"]
# opera_settings = ["regular_allshort_1path"]
# opera_settings = ["regular_allshort_1path"]
opera_calendarq_settings = []
# all_settings = [expander_settings, opera_settings, vlb_settings]
all_settings = [expander_settings, opera_calendarq_settings, optiroute_settings]
# prefix = ["Expander", "Opera", "VLB"]
prefix = ["Expander", "Operacalendarq", "Optiroute"]

for load in ["20percLoad"]:
    for i in range(len(all_settings)):
        for expr in all_settings[i]:
            with open(f"../sim/{prefix[i]}_{load}_{expr}.txt", "r") as f:
                queues_opera = defaultdict(list)
                lines = f.readlines()
                for line in lines:
                    line = line.split()
                    if len(line) <= 0:
                        continue
                    if line[0] == "Queue":
                        tor = int(line[1])
                        port = int(line[2])
                        queues_opera[f"{tor},{port}"].append(int(line[3])/1E3)

                qheatmap = np.zeros((QUEUES,SAMPLES))
                fheatmap = np.zeros((QUEUES,SAMPLES))
                links = list(range(6,12))
                tors = list(range(108))

                sampled = set()
                random.seed(42)
                start_t = 1000

                for q in range(QUEUES):
                    sample_q = (random.choice(tors), random.choice(links))
                    while sample_q in sampled:
                        sample_q = (random.choice(tors), random.choice(links))
                    sampled.add(sample_q)
                    print(sample_q)
                    for t in range(SAMPLES):
                        qheatmap[q][t] = queues_opera[f"{sample_q[0]},{sample_q[1]}"][t+start_t]

                time = [round(x,1) for x in list(np.arange(0, SAMPLES*0.02, 0.02))]

                pd_qhm = pd.DataFrame(qheatmap, columns=time)
                pd_fhm = pd.DataFrame(fheatmap, columns=time)
                plt.figure()
                sea.heatmap(pd_qhm, linewidths=0.0, rasterized=True, vmax=450, cmap="rocket_r", 
                            xticklabels=22,yticklabels=0)
                plt.savefig(f"./{dir}/{prefix[i]}_{load}_{expr}_queue_hm.png")
                plt.xlabel("Time (ms)")
                plt.ylabel("Uplink")
                plt.show()