#!/bin/sh

../../src/opera/datacenter/htsim_dctcp_dynexpTopology -simtime 1.1 -cutoff 150000000000 -cwnd 20 -q 300 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_1percLoad_1sec_648hosts_100Gbps.htsim | tee Opera_1percLoad_100Gbps.txt


#../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 1.00001 -cutoff 15000000 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_10percLoad_1sec_648hosts_10Gbps.htsim | tee Opera_10percLoad_10Gbps.txt
