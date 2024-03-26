import sys
import random


_OUTPUT_FILE = sys.argv[1]
_NUM_SRC_HOSTS = 200
_FLOW_SIZE = int(1e8)

def generate_dst(src):
  while True:
    dst = random.randint(0, 647)
    if src // 6 != dst // 6:
      return dst

with open(_OUTPUT_FILE, "w") as f:
  for src in range(_NUM_SRC_HOSTS):
    dst = generate_dst(src)
    print(f"{src} {dst} {_FLOW_SIZE}", file=f)