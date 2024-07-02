1. Each runs.sh contains several simulator runs, which take ~10 hours to execute each. Each would generate an output ``.txt file`` with each line starting with ``<src ToR> <dst ToR> <Flow Size> <Flow Completion Time> <Flow Start Time> ...``, where extra network statistics follows.
    - You may want to execute the following bash command in the `/run/FigureX` folders, and substitute ``$SIMULATOR_OUTPUT`` with the output name to examine the simulation progress, when the script is still running. 
    ``tail -100000 $SIMULATOR_OUTPUT.txt | grep FCT |  awk '{print $6;}'``. 
    The output would be the time elapsed from the start of the simulation in miliseconds. Each simulation should finish when this output approaches 800ms.

2. One may want to execute them in parallel to save time, but be careful about the memory usage -- the memory consumption grew up to 50GBs for certain runs.

3. All settings for plotting figures are in first created in set_config.py. They are configured to work out-of-the box. We specify the settings first then use others scripts, i.e., FCT.py, BE_BAR.py to generate the figures.

