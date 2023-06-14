#!/bin/sh

# ../../src/optiroute/datacenter/htsim_dctcp_dynexpTopology -simtime 1.00001 -cutoff 1500000000 -rlbflow 0 -cwnd 20 -q 60 -topfile ../../topologies/general_from_dynexp_N=108_topK=5.txt -flowfile ../traffic_gen/flows_test_traffic.htsim | tee Optiroute_test.txt

../../src/optiroute/datacenter/htsim_dctcp_dynexpTopology -simtime 1.1 -cutoff 1500000000 -rlbflow 0 -cwnd 20 -q 300 -topfile ../../topologies/general_from_dynexp_N=108_topK=5.txt -flowfile ../traffic_gen/flows_1percLoad_1sec_648hosts_100Gbps.htsim | tee Optiroute_1percLoad_100Gbps_1000us_10ns.txt
