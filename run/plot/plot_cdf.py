import matplotlib.pyplot as plt
import numpy as np

datamining = np.loadtxt("./datamining.txt")
d, = plt.plot(datamining[:,0], datamining[:,1], label="Datamining")

hadoop = np.loadtxt("./w4_fb_hadoop_cdf.txt")
h, = plt.plot(hadoop[:,0], hadoop[:,1], label="Hadoop")


plt.legend(handles=[h, d])
plt.xlabel("Size (bytes)")
plt.ylabel("CDF")
plt.grid()
plt.xscale('log')
#plt.locator_params(axis='x', nbins=20)
plt.savefig("rtt_cmp.pdf")
plt.show()

