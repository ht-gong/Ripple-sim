import sys

tot_short = 0
tot_long = 0
done_short = 0
done_long = 0
bytes_short = 0
bytes_long = 0
dropped = 0
skipped = 0
cutoff = 15000000
FCTs={}

with open(sys.argv[1], 'r') as f:
    txt = f.readlines()
    for line in txt:
        line = line.split()
        if line[0] == 'FCT':
            if int(line[3]) >= cutoff:
                bytes_long += int(line[3])
                done_long += 1
                tot_long += float(line[4])
            else:
                bytes_short += int(line[3])
                done_short += 1
                tot_short += float(line[4])
            if int(line[3]) not in FCTs:
                FCTs[int(line[3])] = []
            FCTs[int(line[3])].append(float(line[4]))
        elif line[0] == '!!!':
            dropped += 1
        elif line[0] == 'N':
            skipped += 1

tot = tot_short+tot_long
done = done_short+done_long
tot_avg = None if done==0 else tot/done
short_avg = None if done_short==0 else tot_short/done_short
long_avg = None if done_long==0 else tot_long/done_long
bytes_tot = bytes_short+bytes_long
print(f"FCT_TOT: {tot_avg}ms FCT_SH: {short_avg}ms FCT_LO: {long_avg}ms")
print(f"DONE_TOT: {done} DONE_SH: {done_short} DONE_LO: {done_long}")
print(f"BYTES_TOT {bytes_tot} BYTES_SH: {bytes_short} BYTES_LO: {bytes_long}")
print(f"Drop due to mismatch/pipe clip: {dropped}")

times = [t for t in FCTs]
times.sort()
for t in times:
    print(f"{t} : Avg:{sum(FCTs[t])/len(FCTs[t])} Max:{max(FCTs[t])} Min:{min(FCTs[t])}")
