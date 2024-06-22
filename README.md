# Ripple-sim
Packet-level simulation code to model Ripple and other networks from the 2024 SIGCOMM paper "#506 Uniform-Cost Multi-Path Routing for Reconfigurable Data Center Networks"

## Requirements:

- C++ files: g++-7 compiler

## Description:

- The /FigureX directories contain example Matlab scripts to generate traffic and post-process simulation data as well as execution scripts for running simulations.
- The /src directory contains the packet simulator source code. There is a separate simulator for each network type (e.g. Ripple, Opera, Clos). The packet simulator is an extension of the htsim NDP simulator (https://github.com/nets-cs-pub-ro/NDP/tree/master/sim)

## Build instructions:

- To build any simulator, take Ripple for example, from the top level directory run:
  ```
  cd /src/optiroute
  make
  cd /datacenter
  make
  ```
- The executable will be found in the /datacenter directory and named htsim_...

## Typical workflow:

- Compile the simulator as described above (for Ripple, expander, Opera, etc.).
- Generate/use our pre-existing topology file related to that network type
	- Ripple -- /topologies/sigcomm/slice50us_portion1_queue0_optiroute_adaptive_paths_alpha_0.5.txt 
	- Opera -- /topologies/sigcomm/dynexp_50us_10nsrc_5paths.txt 
	- VLB -- /topologies/dynexp_50us_10nsrc_1path.txt
	- Expander (k-shortest paths) -- /topologies/sigcomm/general_from_dynexp_N=108_topK=1.txt
- Generate/use our pre-generated files specifying the traffic 
	- Our pre-generated traffic files are in /run/traffic_gen/sigcomm/
	- If you want to generate them manually you should run the corresponding script.  (e.g. run /Figure7_datamining/opera/traffic_gen/generate_traffic.m). The file format is (where src_host and dst_host are indexed from zero)
  ```
  <src_host> <dst_host> <flow_size_bytes> <flow_start_time_nanosec> /newline
  ```
- Specify the simulation parameters and run (e.g. run /Figure7_datamining/opera/sim/run.sh).
- Post-process the simulation data (e.g. run /Figure7_datamining/opera/plot/process_FCT_and_UTIL.m).
- Plot the post-processed data (e.g. run /Figure7_datamining/opera/plot/plotter.m)
