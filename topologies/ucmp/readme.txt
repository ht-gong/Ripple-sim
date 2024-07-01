********** Default UCMP paths
To generate default UCMP paths (SLICE_E = 50us, alpha = 0.5), try the following workflow:
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

We will have 

********** UCMP paths with different reconfiguration time
