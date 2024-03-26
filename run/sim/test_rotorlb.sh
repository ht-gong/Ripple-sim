echo $1
mkdir $2
echo "Using ../traffic_gen/$2.htsim"
if [ $1 = "optiroute" ]; then
  echo "Logging Optiroute to $2/optiroute.txt ..."
  time ../../src/optiroute/datacenter/htsim_ndp_dynexpTopology -simtime 0.801 -rlbflow 0 -cwnd 20 -cutoff 0 -q 65 -utiltime 5e-04 -pullrate 1 -topfile ../../topologies/slice50us_portion1_queue0_optiroute_adaptive_paths_alpha_0.5.txt -routing OptiRoute -flowfile ../traffic_gen/$2.htsim  -slicedur 50000000 > $2/optiroute.txt
  # time ../../src/optiroute/datacenter/htsim_ndp_dynexpTopology -simtime 0.801 -rlbflow 0 -cwnd 20 -cutoff 0 -q 65 -utiltime 5e-04 -pullrate 1 -topfile ../../topologies/slice50us_portion1_queue0_optiroute_adaptive_paths_alpha_0.5.txt -routing OptiRoute -flowfile ../traffic_gen/$2.htsim  -slicedur 300000000 > $2/optiroute.txt
fi
if [ $1 = "opera" ]; then
  echo "Logging Opera to $2.htsim/opera.txt ..."
  time ../../src/opera/datacenter/htsim_ndp_dynexpTopology -simtime 0.80001 -cutoff 0 -rlbflow 0 -cwnd 20 -q 8 -pullrate 1 -topfile ../../topologies/dynexp_50us_10nsrc.txt -flowfile ../traffic_gen/$2.htsim > $2/opera.txt
fi
# cd ../plot
# echo "Plotting FCT percentiles ..."
# python3 plot_FCT99.py $1
echo "Condensing to $2/$1.txt ..."
python3 ./../traffic_gen/split_into_condensed_logs.py DEBUG1 $2/$1.txt $2/"$1"_condensed_1.txt
python3 ./../traffic_gen/split_into_condensed_logs.py DEBUG2 $2/$1.txt $2/"$1"_condensed_2.txt