********** Default UCMP paths
To generate default UCMP paths (SLICE_E = 50us, alpha = 0.5), try the following workflow:
Create two empty folders /fully_reconf_one_optimal_path, /fully_reconf_all_optimal_paths
python3 run_1.py
python3 run_2.py
python3 get_joint_paths.py
python3 generate_simu.py

We will have slice50us_portion1_queue0_optiroute_adaptive_paths_alpha_0.5.txt


********** UCMP paths with different time slices
To have UCMP paths with different time slices, change the --SLICE_E parameter in run_1.py, run_2.py, get_joint_paths.py, and generate_simu.py.

For example, parser.add_argument('--SLICE_E', type=int, default=50) ==> parser.add_argument('--SLICE_E', type=int, default=10), we will have:

slice10us_portion1_queue0_optiroute_adaptive_paths_alpha_0.5.txt

********** UCMP paths with different weighting factor alpha
Change --alpha parameter in  generate_simu.py

We will have slice50us_portion1_queue0_optiroute_adaptive_paths_alpha_0.3.txt and _alpha_0.7.txt

********** UCMP paths with different reconfiguration time
The third number of second line in slice50us_portion1_queue0_optiroute_adaptive_paths_alpha_0.5.txt represents the reconfiguration time, with unit of picosecond.

To have 1us reconfiguration time, change it to 1000000. For 10us, it is 10000000.

We will have slice50us_portion1_queue0_optiroute_adaptive_paths_alpha_0.5_reconfig_1us.txt, _reconfig_10us.txt