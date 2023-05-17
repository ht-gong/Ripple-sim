#!/bin/sh

../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 1.00001 -cutoff 15000000 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_40percLoad_10sec_648hosts.htsim
