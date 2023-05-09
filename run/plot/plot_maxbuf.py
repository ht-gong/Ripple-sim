import sys
import matplotlib.pyplot as plt
import numpy as np

maxbufs1 = {}
baddst = set()
with open("./buftest_10ecn.txt", 'r') as f:
    lines = f.readlines()
    idx = -1
    for line in lines:
        idx += 1
        line = line.split()
        if len(line) <= 0:
            continue
        if line[0] == "MAXBUF":
            host = int(line[1])
            pkts = int(line[2])
            if host not in baddst:
                maxbufs1[host] = pkts
        elif line[0] == "ToR":
            dst = int(line[11])
            baddst.add(dst)
        elif line[0] == "src":
            dst = int(line[5])
            baddst.add(dst)

maxbufs1 = [buf for (host,buf) in maxbufs1.items()]
maxbufs1 = np.array(maxbufs1)
cnt, b_cnt = np.histogram(maxbufs1, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
a, = plt.plot(b_cnt[1:], cdf, label="8us")

maxbufs2 = {}
baddst = set()
with open("./buftest_20ecn.txt", 'r') as f:
    lines = f.readlines()
    idx = -1
    for line in lines:
        idx += 1
        line = line.split()
        if len(line) <= 0:
            continue
        if line[0] == "MAXBUF":
            host = int(line[1])
            pkts = int(line[2])
            if host not in baddst:
                maxbufs2[host] = pkts
        elif line[0] == "ToR":
            dst = int(line[11])
            baddst.add(dst)
        elif line[0] == "src":
            dst = int(line[5])
            baddst.add(dst)

maxbufs2 = [buf for (host,buf) in maxbufs2.items()]
maxbufs2 = np.array(maxbufs2)
cnt, b_cnt = np.histogram(maxbufs2, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
b, = plt.plot(b_cnt[1:], cdf, label="14us")

maxbufs3 = {}
baddst = set()
with open("./buftest_45ecn.txt", 'r') as f:
    lines = f.readlines()
    idx = -1
    for line in lines:
        idx += 1
        line = line.split()
        if len(line) <= 0:
            continue
        if line[0] == "MAXBUF":
            host = int(line[1])
            pkts = int(line[2])
            if host not in baddst:
                maxbufs3[host] = pkts
        elif line[0] == "ToR":
            dst = int(line[11])
            baddst.add(dst)
        elif line[0] == "src":
            dst = int(line[5])
            baddst.add(dst)

maxbufs3 = [buf for (host,buf) in maxbufs3.items()]
maxbufs3 = np.array(maxbufs3)
cnt, b_cnt = np.histogram(maxbufs3, bins=10000)
pdf = cnt/sum(cnt)
cdf = np.cumsum(pdf)
c, = plt.plot(b_cnt[1:], cdf, label="30us")

plt.legend(handles=[a,b,c])
plt.title("Max buffer size per host")
plt.xlabel("Buffer Size (Pkts)")
plt.ylabel("CDF")
plt.grid()

plt.show()
