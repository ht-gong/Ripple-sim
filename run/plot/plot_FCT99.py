import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict
import os
from datetime import date
import itertools

marker = itertools.cycle((',', '+', '.', 'o', '*', 'x', 'd')) 
dir = date.today().strftime("%Y-%m-%d")
os.makedirs(dir, exist_ok=True)
zoom_thresh = 1E4

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
graph_settings = ["full", "zoomed"]

for gs in graph_settings:
    for load in ["1percLoad", "10percLoad", "20percLoad"]:
        #fig, (ax1, ax2) = plt.subplots(nrows=2, sharex=True, figsize=(10, 10))
        fig, (ax1, ax2) = plt.subplots(nrows=2, figsize=(13, 10))
        forlegend = []
        for i in range(len(all_settings)):
            for expr in all_settings[i]:
                flows = defaultdict(list)
                with open(f"../sim/{prefix[i]}_{load}_{expr}.txt", "r") as f:
                    lines = f.readlines()
                    for line in lines:
                        line = line.split()
                        if len(line) <= 0:
                            continue
                        if line[0] == "FCT":
                            if not (gs == "zoomed" and int(line[3]) > zoom_thresh):
                                flowsize = int(line[3])
                                flowfct = float(line[4])*1E3
                                flows[flowsize].append(flowfct)

                flowsizes = [flowsize for (flowsize, flowfct) in flows.items()]
                flowsizes.sort()
                fcts99 = []
                fcts50 = []
                for flowsize in flowsizes:
                    fcts99.append(np.percentile(flows[flowsize], 99))
                for flowsize in flowsizes:
                    fcts50.append(np.percentile(flows[flowsize], 50))
                m = next(marker)
                if expr[-2:] == '65':
                    a, = ax1.plot(flowsizes, fcts99, marker=m, color=forlegend[-1].get_color(), label=f"{prefix[i]}_{expr}", linestyle='--')
                else:
                    a, = ax1.plot(flowsizes, fcts99, marker=m, label=f"{prefix[i]}_{expr}", linestyle='-')
                ax1.set_title(f'FCT Comparison {load} 99 tail {gs}')
                if expr[-2:] == '65':
                    a, = ax2.plot(flowsizes, fcts50, marker=m, color=forlegend[-1].get_color(), label=f"{prefix[i]}_{expr}", linestyle='--')
                else:
                    a, = ax2.plot(flowsizes, fcts50, marker=m, label=f"{prefix[i]}_{expr}", linestyle='-')
                ax2.set_title(f'FCT Comparison {load} 50 tail {gs}')
                
                for ax in [ax1, ax2]:
                    if gs == "full":
                        ax.set_xscale('log')
                        ax.set_yscale('log')
                    ax.set_ylabel("FCT (us)")
                    ax.set_xlabel("Flow size (bytes)")
                forlegend.append(a)
        
        #plt.xlabel("Flow size (bytes)")
        ax1.legend(handles = forlegend, bbox_to_anchor=(1.3, 1.0), loc = 'upper right')
        fig.tight_layout()
        plt.savefig(f"./{dir}/ExpanderECN_FCT_Comparison_{load}_{gs}.png", bbox_inches='tight')
        plt.show()
        plt.close()
