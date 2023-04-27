from math import *

PROP_DELAY = 0.5 #microseconds
LINK_RATE = 100e9 #bits/s
PKT_SIZE = 1500 #bytes
QUEUE_SIZE = 20 #packets
MINMAX_HOPS = 5 #hops
RECONFIG = 3*1e6 #microseconds

pkt_sendtime = PKT_SIZE*8/LINK_RATE #seconds
hop_delay_worst = PROP_DELAY*1e6 + QUEUE_SIZE*pkt_sendtime*1e12 #picoseconds

eps0=(MINMAX_HOPS-1)*hop_delay_worst + (QUEUE_SIZE-1)*pkt_sendtime*1e12
delta=PROP_DELAY*1e6 + pkt_sendtime*1e12

slice_E = eps0+delta
slice_R = RECONFIG
duty_cycle = (6*slice_E+5*slice_R)/(6*(slice_E+slice_R))

print(f"EPS:{ceil(eps0)/1e6} DELTA:{ceil(delta)/1e6} RECONFIG:{ceil(RECONFIG)/1e6} DUTY:{duty_cycle}")

#opera's og duty: 0.9724517906336089
