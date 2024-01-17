#!/bin/sh

../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 0.50001 -cutoff 0 -rlbflow 0 -cwnd 20 -q 65 -pullrate 1 -topfile ../../topologies/dynexp_50us_10nsrc.txt -utiltime 2e-05 -flowfile ../traffic_gen/websearch_uniform_40percLoad_1sec_648hosts.htsim > vlb_rotorlb_40perc_ws.txt &

