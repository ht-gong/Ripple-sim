#!/bin/sh

# Test runs of 0.2s for preliminary eval
echo "Running Opera"
../../src/general/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_1percLoad_1sec_648hosts_10Gbps.htsim > General_10Gbps_1perc.txt &
../../src/general/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_10percLoad_1sec_648hosts_10Gbps.htsim > General_10Gbps_10perc.txt &
../../src/general/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_25percLoad_1sec_648hosts_10Gbps.htsim > General_10Gbps_25perc.txt &
../../src/general/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_30percLoad_1sec_648hosts_10Gbps.htsim > General_10Gbps_30perc.txt &
../../src/general/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_40percLoad_1sec_648hosts_10Gbps.htsim > General_10Gbps_40perc.txt &
wait

echo "Running HOHO"
../../src/opcc/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -cwnd 20 -pullrate 1 -q 8 -topfile ../../topologies/opcc_paths_E_50500_R_10000.txt -flowfile ../traffic_gen/flows_1percLoad_1sec_648hosts_10Gbps.htsim > Hoho_10Gbps_1perc.txt &
../../src/opcc/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -cwnd 20 -pullrate 1 -q 8 -topfile ../../topologies/opcc_paths_E_50500_R_10000.txt -flowfile ../traffic_gen/flows_10percLoad_1sec_648hosts_10Gbps.htsim > Hoho_10Gbps_10perc.txt &
../../src/opcc/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -cwnd 20 -pullrate 1 -q 8 -topfile ../../topologies/opcc_paths_E_50500_R_10000.txt -flowfile ../traffic_gen/flows_25percLoad_1sec_648hosts_10Gbps.htsim > Hoho_10Gbps_25perc.txt &
../../src/opcc/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -cwnd 20 -pullrate 1 -q 8 -topfile ../../topologies/opcc_paths_E_50500_R_10000.txt -flowfile ../traffic_gen/flows_30percLoad_1sec_648hosts_10Gbps.htsim > Hoho_10Gbps_30perc.txt &
../../src/opcc/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -cwnd 20 -pullrate 1 -q 8 -topfile ../../topologies/opcc_paths_E_50500_R_10000.txt -flowfile ../traffic_gen/flows_40percLoad_1sec_648hosts_10Gbps.htsim > Hoho_10Gbps_40perc.txt &

echo "Running Opera_Original"
../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -cwnd 20 -pullrate 1 -q 8 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_1percLoad_1sec_648hosts_10Gbps.htsim > OperaOrg_10Gbps_1perc.txt &
../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -cwnd 20 -pullrate 1 -q 8 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_10percLoad_1sec_648hosts_10Gbps.htsim > OperaOrg_10Gbps_10perc.txt &
../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -cwnd 20 -pullrate 1 -q 8 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_25percLoad_1sec_648hosts_10Gbps.htsim > OperaOrg_10Gbps_25perc.txt &
../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -cwnd 20 -pullrate 1 -q 8 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_30percLoad_1sec_648hosts_10Gbps.htsim > OperaOrg_10Gbps_30perc.txt &
../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 1.000001 -cutoff 15000000 -cwnd 20 -pullrate 1 -q 8 -topfile ../../topologies/dynexp_N=108_k=12_1path.txt -flowfile ../traffic_gen/flows_40percLoad_1sec_648hosts_10Gbps.htsim > OperaOrg_10Gbps_40perc.txt &