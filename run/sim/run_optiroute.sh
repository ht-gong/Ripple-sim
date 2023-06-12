#!/bin/sh

../../src/optiroute/datacenter/htsim_dctcp_dynexpTopology -simtime 1.00001 -cutoff 1500000000 -rlbflow 0 -cwnd 20 -q 8 -topfile ../../topologies/general_from_dynexp_N=108_topK=5.txt -flowfile ../traffic_gen/flows_test_traffic.htsim | tee Optiroute_test.txt
