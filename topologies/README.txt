__author__ = jialong

--update on 20220411--

This is for Sirius routing path file 'sirius_paths_N108_PORT6_WV18.txt'

File format:
	line 1: 648 hosts, 6 uplinks and 6 downlinks for each tor, 108 tors;
	line 2: 18 slices in a whole cycle. 90ns for data and 10ns for reconfiguration;
	line 3-20: connection index.
		(1) There are 18 lines, which are corresponding to the slice indexes. For line 3, it shows the connection in slice 0, and line 4 is for slice 1, and so on.
		(2) Each line has 1296 numbers, indicating the tors that the whole uplinks of 108 tors(108 * 12 = 648) connect to. Take line 3 as an example. The first 12 numbers (0 0 18 0 36 0 54 0 72 0 90 0) belong to the first tor 0. The first 2 numbers indicate that tor 0 q 0 connects to tor 0 q 0, the second 2 numbers mean tor 0 q 1 connect to tor 18 0, and tor 0 q 2 connect to tor 36 q 0...
		(*Different from the opcc/opera path. In opera, any torpair connection will use the same uplink queue index. In Sirius, it is not the case. So we need to specify which queues they use.)

	line 21-end: routing path under 18 slices.
		(1) For each slice, slice number will show in an individual line. Say line 21, line 11588, and so on.
		(2) path format: [src_tor, dst_tor, queue in src_tor connecting to next_hop_tor, queue in next_hop_tor connecting to src_tor, sending slice from src_tor to next_hop_tor]
		(3) Each line will contain exactly Five numbers (except for the slice index line).


*************************************************************
--update on 20220302--

This is for routing path file 'opcc_paths_E_50500_R_10000.txt'.

1. A superslice is only divided into two slices (SLICE_E and SLICE_R). In opera, a superslice is divided into slice_epsilon0, slice_del, and slice_r. In this file, slice_epsilon0 and slice_del are merged into SLICE_E. So in a cycle, only 216 slices remain (324/3*2 = 216).

2. File format:
	line 1: 648 hosts, 6 uplinks and 6 downlinks for each tor, 108 tors;
	line 2: 216 slices in a whole cycle, length of SLICE_E / SLICE_R in picoseconds;
	line 3-218: connection index. 
		(1) There are 216 lines, which are corresponding to the slice index. For line 3, it shows the connection in slice 0, and line 4 is for slice 1, and so on.
		(2) Each line has 648 numbers, indicating the tors that the whole uplinks of 108 tors(108 * 6 = 648) connect to. Take line 3 as an example. The first 6 numbers (46 48 6 0 84 31) belong to the first tor 0. So the 6 uplinks of tor 0 connects to tor [46 48 6 0 84 31] respectively. And tor 2 connects to tor [51 24 97 30 13 82] using its 6 uplinks.
		(3) -1 means the OCS connecting to the uplink is under configuration. No connection. For each slice, only one OCS is under configuration at most.
	line 219-end: routing path under 216 slices.
		(1) For each slice, slice number will show in an individual line. Say line 219, line 11776, and so on.
		(2) path format: [src_tor, dst_tor, queue index connects to next_hop_tor, sending slice from src_tor to next_hop_tor]
		(3) Each line will contain exactly Four numbers (except for the slice index line).
*queue index range: [6, 11]. It is uplink queue. For downlink queue index, it is [0, 5].
