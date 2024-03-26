import numpy as np
import matplotlib.pyplot as plt
import os
from datetime import date

dir = date.today().strftime("%Y-%m-%d")
os.makedirs(dir, exist_ok=True)

files = (
  ("../sim/simple6/opera.txt", "Opera"),
  ("../sim/simple6/optiroute.txt", "Optiroute"),
)

for fname, label in files:
  f = open(fname)

  fcts = []

  for line in f.readlines():
    tokens = line.split(" ")
    type = tokens[0]
    if type != "FCT":
      continue
    
    src, dst, time = int(tokens[1]), int(tokens[2]), float(tokens[4])
    fcts.append(time)

  fcts.sort()
  pdf = fcts / np.sum(fcts)
  cdf = np.cumsum(pdf)

  f.close()
  plt.plot(fcts, cdf, label=label)

plt.title(f"FCT CDF")
plt.xlabel("FCT")
plt.ylabel("CDF")
plt.legend()
plt.savefig(f"./{dir}/fct_cdf.png")
plt.show()
plt.close()