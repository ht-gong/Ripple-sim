#!/bin/sh

# ../../src/optiroute/datacenter/htsim_dctcp_dynexpTopology -simtime 1.001 -cutoff 1500000000 -rlbflow 0 -cwnd 20 -q 300 -topfile ../../topologies/general_from_dynexp_N=108_topK=5.txt -flowfile ../traffic_gen/flows_20percLoad_1sec_648hosts_100Gbps.htsim | tee Optiroute_test.txt 

# ../../src/optiroute/datacenter/htsim_dctcp_dynexpTopology -simtime 1.1 -cutoff 1500000000 -rlbflow 0 -cwnd 20 -q 300 -topfile ../../topologies/general_from_dynexp_N=108_topK=5.txt -flowfile ../traffic_gen/flows_1percLoad_1sec_648hosts_100Gbps.htsim | tee Optiroute_1percLoad_100Gbps_noswitch.txt

../../src/optiroute/datacenter/htsim_dctcp_dynexpTopology -simtime 1.001 -cutoff 150000000000 -rlbflow 0 -cwnd 20 -q 300 -topfile ../../topologies/general_from_dynexp_N=108_topK=5.txt -flowfile ../traffic_gen/flows_1percLoad_1sec_648hosts_100Gbps.htsim > Optiroute_1percLoad_100Gbps_noswitch_1path.txt &

../../src/optiroute/datacenter/htsim_dctcp_dynexpTopology -simtime 1.001 -cutoff 150000000000 -rlbflow 0 -cwnd 20 -q 300 -topfile ../../topologies/general_from_dynexp_N=108_topK=5.txt -flowfile ../traffic_gen/flows_10percLoad_1sec_648hosts_100Gbps.htsim > Optiroute_10percLoad_100Gbps_noswitch_1path.txt &

../../src/optiroute/datacenter/htsim_dctcp_dynexpTopology -simtime 1.001 -cutoff 150000000000 -rlbflow 0 -cwnd 20 -q 300 -topfile ../../topologies/general_from_dynexp_N=108_topK=5.txt -flowfile ../traffic_gen/flows_20percLoad_1sec_648hosts_100Gbps.htsim > Optiroute_20percLoad_100Gbps_noswitch_1path.txt &
