#!/bin/sh

../../src/opcc/datacenter/htsim_ndp_dynexpTopology -simtime 1.00001 -cutoff 15000000 -cwnd 20 -pullrate 1 -q 8 -topfile ../../topologies/opcc_paths_E_50500_R_10000.txt -flowfile ../traffic_gen/flows_40percLoad_10sec_648hosts.htsim
