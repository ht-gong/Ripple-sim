#!/bin/sh

../../src/opcc/datacenter/htsim_dctcp_dynexpTopology -simtime 1.00001 -cutoff 500000 -topfile ../../topologies/100G_opcc_paths_E_6868_R_1360.txt -flowfile ../traffic_gen/hadoop_flows_50percLoad_1sec_648hosts_100G.htsim > hoho65ecn.txt
