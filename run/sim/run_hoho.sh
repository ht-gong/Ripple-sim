#!/bin/sh

../../src/opcc/datacenter/htsim_ndp_dynexpTopology -simtime 0.0101 -cutoff 15000000000 -cwnd 20 -pullrate 1 -q 80 -utiltime 5e-4 -topfile ../../topologies/opcc_paths_E_50500_R_10000.txt -flowfile ../traffic_gen/websearch_20percLoad_1sec_648hosts_100Gbps.htsim > hoho_20perc2.txt
