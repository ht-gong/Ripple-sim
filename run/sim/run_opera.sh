#!/bin/sh

#../../src/opera/datacenter/htsim_dctcp_dynexpTopology -simtime 0.1001 -cutoff 1500000000000 -cwnd 20 -q 300 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_1percLoad_1sec_648hosts_100Gbps.htsim > opera_20perc.txt

../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 0.0101 -cutoff 15000000000 -rlbflow 0 -cwnd 20 -q 80 -utiltime 5e-4 -pullrate 1 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/websearch_20percLoad_1sec_648hosts_100Gbps.htsim > opera_20perc.txt
