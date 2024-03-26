import sys

_, PREFIX, SRC_FILE, DST_FILE = sys.argv

fin = open(SRC_FILE)
fout = open(DST_FILE, "w")
for line in fin.readlines():
  if line.split("\t")[0] == PREFIX:
    print("\t".join(line.split("\t")[1:]), file=fout, end="")