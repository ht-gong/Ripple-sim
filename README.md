# Ripple-sim
Packet-level simulation code for SIGCOMM 2024 paper (UCMP) "Uniform-Cost Multi-Path Routing for Reconfigurable Data Center Networks"

## Requirements:

- C++ files: g++-7 compiler

## Description:

- The /src directory contains the packet simulator source code. There is a separate simulator for each network type (e.g. UCMP, Opera). The packet simulator is an extension of the htsim NDP simulator (https://github.com/nets-cs-pub-ro/NDP/tree/master/sim)

## Repo Structure:
- /routing: scripts replated to pre-computing UCMP paths
- /src: source for the htsim simulator
  - optiroute: for UCMP, ksp
  - opera: for Opera(NSDI '20), VLB (SIGCOMM '17)
- /run: where simulator runs are initiated, results and plotting scripts are stored
- /topologies: network topology files
- /traffic: for generating synthetic traffic traces

## Build instructions:

- To build any simulator, take Ripple for example, from the top level directory run:
  ```
  cd /src/$SIMULATOR_BRANCH
  make
  cd /datacenter
  make
  ```
- The executable will be found in the /datacenter directory and named htsim_...

## Typical workflow:

- Compile the simulator as described above.
- Generate/use our pre-existing topology file related to that network type
	- UCMP -- /topologies/slice50us_portion1_queue0_optiroute_adaptive_paths_alpha_0.5.txt 
	- Opera -- /topologies/dynexp_50us_10nsrc_1path.txt 
	- VLB -- /topologies/dynexp_50us_10nsrc_1path.txt
	- ksp -- /topologies/general_from_dynexp_N=108_topK=1.txt
  If you want to generate them manually, go to the corresponding folder and run the script.
- Generate/use our pre-generated files specifying the traffic 
	- Our pre-generated traffic files are in /traffic
	- If you want to generate them manually you should run the corresponding script.  (e.g. /traffic/generate_traffic.m). The file format is (where src_host and dst_host are indexed from zero)
  ```
  <src_host> <dst_host> <flow_size_bytes> <flow_start_time_nanosec> /newline
  ```
  - If you want to generate them manually, run /traffic/generate_websearch_traffic.m, then run write_to_htsim_file.m
- Specify the simulation parameters and run (e.g. run /Figure8/run.sh).
- Plot the post-processed data (e.g. run /Figure8/set_config.py and then run FCT.py)
