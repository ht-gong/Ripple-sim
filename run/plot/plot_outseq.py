import sys
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

seq_gaps1 = []
with open("./buftest_10ecn.txt", 'r') as f:
    lines = f.readlines()
    idx = -1
    for line in lines:
        idx += 1
        line = line.split()
        if len(line) <= 0:
            continue
        if line[0] == "OUTOFSEQ":
            gap = int(line[7])
            seq_gaps1.append(gap)

seq_gaps1 = np.array(seq_gaps1)
cnt, b_cnt = np.histogram(seq_gaps1, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
a, = plt.plot(b_cnt[1:], cdf, label="8us")


seq_gaps2 = []
with open("./buftest_20ecn.txt", 'r') as f:
    lines = f.readlines()
    idx = -1
    for line in lines:
        idx += 1
        line = line.split()
        if len(line) <= 0:
            continue
        if line[0] == "OUTOFSEQ":
            gap = int(line[7])
            seq_gaps2.append(gap)

seq_gaps2 = np.array(seq_gaps2)
cnt, b_cnt = np.histogram(seq_gaps2, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
b, = plt.plot(b_cnt[1:], cdf, label="14us")

seq_gaps3 = []
with open("./buftest_45ecn.txt", 'r') as f:
    lines = f.readlines()
    idx = -1
    for line in lines:
        idx += 1
        line = line.split()
        if len(line) <= 0:
            continue
        if line[0] == "OUTOFSEQ":
            gap = int(line[7])
            seq_gaps3.append(gap)

seq_gaps3 = np.array(seq_gaps3)
cnt, b_cnt = np.histogram(seq_gaps3, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
c, = plt.plot(b_cnt[1:], cdf, label="30us")

plt.legend(handles=[a,b,c])
plt.title("Sequence Gaps")
plt.xlabel("Gap (pkts)")
plt.ylabel("CDF")
plt.grid()

plt.show()
