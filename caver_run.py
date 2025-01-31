from collections import defaultdict
from itertools import product
import os
import subprocess
import sys
import time
from datetime import datetime

import psutil
from analysis.deep_analyse import *

all_processes = []
def cleanup_processes(processes):
    for process in processes:
        process.terminate()  # Send SIGTERM to each child process
        process.wait()  # Wait for the process to terminate
# Function to print colored text
def get_cur_id():
    with open('mix/index.txt', 'r') as index_file:
        content = int(index_file.read().strip())
        if content:
            return int(content)
        else:
            return 1
    
def record_history(log:str):
    with open('./mix/autorun_history.txt', 'a') as history_file:
        history_file.write(log)

def run_normal(cdf='AliStorage2019'):
    topo = 'fat_k8_100G_OS2'
    lb_modes = ["caver", "conga", "conweave", "hula", "fecmp"]
    loads = [40, 50, 60, 70, 80]
    runtime = 0.03
    processes = []
    record = defaultdict(dict)  # load->lb->id

    record_history(f"{datetime.now()} fat_k8_100G_OS2 topology Random traffic experiment, cdf={cdf}\n")
    for load in loads:
        record_history(f'LOAD:{load}, INDEX:[')

        for lb_mode in lb_modes:
            if lb_mode == lb_modes[-1]:
                record_history(f"{lb_mode}:{get_cur_id()}]\n")
            else:
                record_history(f"{lb_mode}:{get_cur_id()}, ")
            record[load][lb_mode] = get_cur_id()
            command = f"python3 run.py --lb {lb_mode} --pfc 1 --irn 0 --simul_time {runtime} --netload {load} --topo {topo} --cdf {cdf}"
            process = subprocess.Popen(command, shell=True)
            processes.append(process)
            all_processes.append(process)

            time.sleep(10)
    print(record)
    print('Waiting for all processes to finish...')
    for process in processes:
        process.wait()  # Wait for each child process to complete
    for load in loads:
        print(f"\nResults for LOAD: {load}")
        for lb_mode in lb_modes:
            experiment_id = record[load].get(lb_mode)
            if experiment_id:
                # Retrieve the FCT slowdown for the experiment
                analyser = getAnalyser(experiment_id)
                avg_fct_slowdown = analyser.get_avg_fct_slowdown()
                # Print the result for this combination
                print(f"    lb_mode: {lb_mode}, Experiment ID: {experiment_id}, FCT Slowdown: {avg_fct_slowdown:.2f}")


def run_caver_pathnum_ce_experiments():
    topo = 'fat_k8_100G_OS2'
    loads = [60, 80]
    runtime = 0.03
    processes = []
    record = defaultdict(dict)  # Record for load -> parameter combination -> id

    # Start the experiment
    record_history(f"{datetime.now()} fat_k8_100G_OS2 topology Caver parameter tuning experiment: CE threshold and path choice number\n")

    # Lists of parameters to tune
    caver_pathChoice_num_list = [1, 2, 3, 4, 5]
    caver_ce_threshold_list = [1.1, 1.3, 1.5, 1.8]

    # Generate all combinations of caver_pathChoice_num and caver_ce_threshold
    param_combinations = product(caver_pathChoice_num_list, caver_ce_threshold_list)

    # Iterate through each load value
    for load in loads:
        record_history(f'LOAD:{load}\n')

        # Iterate through each parameter combination
        for pathChoice_num, ce_threshold in param_combinations:
            # Build the command dynamically with adjusted parameters
            command = f"python3 run.py --lb caver --pfc 1 --irn 0 --simul_time {runtime} --netload {load} --topo {topo} --caver_alpha 0.3 "
            command += f"--caver_pathChoice_num {pathChoice_num} --caver_ce_threshold {ce_threshold} "

            # Log the parameters and experiment ID
            param_str = f"caver_pathChoice_num={pathChoice_num}, caver_ce_threshold={ce_threshold}"
            experiment_id = get_cur_id()
            record_history(f"{param_str}, Experiment ID: {experiment_id}\n")
            
            # Record the current experiment setup
            record[load][(pathChoice_num, ce_threshold)] = experiment_id

            # Run the experiment in a subprocess
            process = subprocess.Popen(command, shell=True)
            processes.append(process)
            all_processes.append(process)
            time.sleep(10)  # Optional: pause between starting processes

        # Reset param_combinations for the next load value
        param_combinations = product(caver_pathChoice_num_list, caver_ce_threshold_list)
    
    print("Record of experiments:", record)
    print('Waiting for all processes to finish...')
    # Wait for all processes to complete
    for process in processes:
        process.wait()

    for load in loads:
        # Initialize a table to hold FCT slowdown results for this load
        table = {}
        
        # Iterate over each parameter combination for the current load
        for pathChoice_num, ce_threshold in product(caver_pathChoice_num_list, caver_ce_threshold_list):
            experiment_id = record[load].get((pathChoice_num, ce_threshold))
            if experiment_id:
                # Retrieve the FCT slowdown for the experiment
                analyser = getAnalyser(experiment_id)
                avg_fct_slowdown = analyser.get_avg_fct_slowdown()
                table[(pathChoice_num, ce_threshold)] = avg_fct_slowdown

        # Print the table for the current load
        print(f"\nFCT Slowdown Table for LOAD {load}:")
        print("caver_pathChoice_num \\ caver_ce_threshold", end="  ")
        for ce_threshold in caver_ce_threshold_list:
            print(f"{ce_threshold:.1f}", end="   ")
        print()

        for pathChoice_num in caver_pathChoice_num_list:
            print(f"{pathChoice_num: <20}", end="  ")
            for ce_threshold in caver_ce_threshold_list:
                # Retrieve the value from the table and print it
                avg_fct_slowdown = table.get((pathChoice_num, ce_threshold), None)
                if avg_fct_slowdown is None:
                    print("N/A", end="   ")
                else:
                    print(f"{avg_fct_slowdown:.2f}", end="   ")
            print()

def run_caver_patchoiceTimeout_experiments():
    topo = 'fat_k8_100G_OS2'
    loads = [60, 80]
    runtime = 0.03
    processes = []
    record = defaultdict(dict)  # Record for load -> parameter combination -> id

    # Start the experiment
    record_history(f"{datetime.now()} fat_k8_100G_OS2 topology Caver parameter tuning experiment: PatchoiceTimeout\n")

    # List of values for caver_patchoiceTimeout to adjust
    caver_patchoiceTimeout_list = [30, 50, 70, 100, 150]

    # Iterate through each load value
    for load in loads:
        record_history(f'LOAD:{load}\n')

        # Iterate through each caver_patchoiceTimeout value
        for patchoiceTimeout in caver_patchoiceTimeout_list:
            # Build the command dynamically with adjusted parameters
            command = f"python3 run.py --lb caver --pfc 1 --irn 0 --simul_time {runtime} --netload {load} --topo {topo} "
            command += f"--caver_patchoiceTimeout {patchoiceTimeout} "

            # Log the parameters and experiment ID
            param_str = f"caver_patchoiceTimeout={patchoiceTimeout}"
            experiment_id = get_cur_id()
            record_history(f"{param_str}, Experiment ID: {experiment_id}\n")
            
            # Record the current experiment setup
            record[load][patchoiceTimeout] = experiment_id

            # Run the experiment in a subprocess
            process = subprocess.Popen(command, shell=True)
            processes.append(process)
            time.sleep(10)  # Optional: pause between starting processes

        # Reset param_combinations for the next load value
    
    print("Record of experiments:", record)
    print('Waiting for all processes to finish...')

    # Wait for all processes to complete
    for process in processes:
        process.wait()

    # For each load, print the result for each caver_patchoiceTimeout value
    for load in loads:
        for patchoiceTimeout in caver_patchoiceTimeout_list:
            experiment_id = record[load].get(patchoiceTimeout)
            if experiment_id:
                # Retrieve the FCT slowdown for the experiment
                analyser = getAnalyser(experiment_id)
                avg_fct_slowdown = analyser.get_avg_fct_slowdown()
                # Print the result for this combination
                print(f"LOAD: {load}, caver_patchoiceTimeout: {patchoiceTimeout}, Experiment ID: {experiment_id}, FCT Slowdown: {avg_fct_slowdown:.2f}")

def run_caver_dreTime_alpha_experiments():
    topo = 'fat_k8_100G_OS2'
    loads = [60, 80]
    runtime = 0.02
    processes = []
    record = defaultdict(dict)  # Record for load -> parameter combination -> id

    # Start the experiment
    record_history(f"{datetime.now()} fat_k8_100G_OS2 topology Caver parameter tuning experiment: dreTime and alpha\n")

    # Lists of values for caver_dreTime and caver_alpha to adjust
    caver_dreTime_list = [20, 30, 50]
    caver_alpha_list = [0.2, 0.25, 0.3]

    # Iterate through each load value
    for load in loads:
        record_history(f'LOAD:{load}\n')

        # Iterate through each combination of caver_dreTime and caver_alpha
        for dreTime, alpha in product(caver_dreTime_list, caver_alpha_list):
            # Build the command dynamically with adjusted parameters
            command = f"python3 run.py --lb caver --pfc 1 --irn 0 --simul_time {runtime} --netload {load} --topo {topo} "
            command += f"--caver_dreTime {dreTime} --caver_alpha {alpha} "

            # Log the parameters and experiment ID
            param_str = f"caver_dreTime={dreTime}, caver_alpha={alpha}"
            experiment_id = get_cur_id()
            record_history(f"{param_str}, Experiment ID: {experiment_id}\n")
            
            # Record the current experiment setup
            record[load][(dreTime, alpha)] = experiment_id

            # Run the experiment in a subprocess
            process = subprocess.Popen(command, shell=True)
            processes.append(process)
            time.sleep(10)  # Optional: pause between starting processes

        # Reset param_combinations for the next load value
    
    print("Record of experiments:", record)
    print('Waiting for all processes to finish...')

    # Wait for all processes to complete
    for process in processes:
        process.wait()

    # For each load, print the result for each combination of caver_dreTime and caver_alpha
    for load in loads:
        for dreTime, alpha in product(caver_dreTime_list, caver_alpha_list):
            experiment_id = record[load].get((dreTime, alpha))
            if experiment_id:
                # Retrieve the FCT slowdown for the experiment
                analyser = getAnalyser(experiment_id)
                avg_fct_slowdown = analyser.get_avg_fct_slowdown()
                # Print the result for this combination
                print(f"LOAD: {load}, caver_dreTime: {dreTime}, caver_alpha: {alpha}, Experiment ID: {experiment_id}, FCT Slowdown: {avg_fct_slowdown:.2f}")

def run_experiments_with_topology():
    # Different values for topology, load, and lb_mode
    topologies = ['fat_k4_100G_OS1', 'leaf_spine_128_100G_OS2', 'fat_k8_100G_bond_OS1']
    loads = [40, 50, 60, 70, 80]
    lb_modes = ['caver', 'conga', 'conweave', 'hula', 'fecmp']
    runtime = 0.03
    processes = []
    record = defaultdict(lambda: defaultdict(dict))  # Record for topo -> load -> lb_mode -> id

    # Start the experiment
    record_history(f"{datetime.now()} Random traffic experiment with Topology, Load, and LB Mode\n") 

    # Iterate through each topology
    for topo in topologies:
        # Iterate through each load value
        for load in loads:
            record_history(f"Topology: {topo}, LOAD: {load}, INDEX=[")

            # Iterate through each lb_mode
            for lb_mode in lb_modes:
                if lb_mode == lb_modes[-1]:
                    record_history(f"{lb_mode}:{get_cur_id()}]\n")
                else:
                    record_history(f"{lb_mode}:{get_cur_id()}, ")
                # Build the command dynamically with adjusted parameters
                command = f"python3 run.py --lb {lb_mode} --pfc 1 --irn 0 --simul_time {runtime} --netload {load} --topo {topo} "
                
                # Log the parameters and experiment ID
                param_str = f"lb_mode={lb_mode}, load={load}, topo={topo}"
                experiment_id = get_cur_id()
                
                # Record the current experiment setup
                record[topo][load][lb_mode] = experiment_id

                # Run the experiment in a subprocess
                process = subprocess.Popen(command, shell=True)
                processes.append(process)
                time.sleep(10)  # Optional: pause between starting processes

    print("Record of experiments:", record)
    print('Waiting for all processes to finish...')

    # Wait for all processes to complete
    for process in processes:
        process.wait()

    # For each topo, print the result for each load and lb_mode combination
    for topo in topologies:
        print(f"\nResults for Topology: {topo}")
        for load in loads:
            print(f"  LOAD: {load}")
            for lb_mode in lb_modes:
                experiment_id = record[topo][load].get(lb_mode)
                if experiment_id:
                    # Retrieve the FCT slowdown for the experiment
                    analyser = getAnalyser(experiment_id)
                    avg_fct_slowdown = analyser.get_avg_fct_slowdown()
                    # Print the result for this combination
                    print(f"    lb_mode: {lb_mode}, Experiment ID: {experiment_id}, FCT Slowdown: {avg_fct_slowdown:.2f}")

def run_with_traffic_patterns():
    lb_modes = ["caver", "conga", "conweave", "hula", "fecmp"]
    traffic_patterns = [
        #('fat_k8_100G_OS2', 'incast100'),
        #('fat_k8_100G_OS2', 'incast150'),
        #('fat_k8_100G_OS2', 'incast200'),
        #('fat_k8_100G_OS2', 'incast250'),
        #('fat_k8_100G_OS2', 'incast300'),
        #('fat_k8_100G_OS2', 'alltoall0.2'),
        #('fat_k8_100G_OS2', 'alltoall0.1'),
        #('fat_k8_100G_OS2', 'alltoall0.05'),
        #('fat_k8_100G_OS2', 'alltoall0.025'),
        #('fat_k8_100G_OS2', 'alltoall0.01'),
        #('fat_k8_100G_OS1', 'llm_flow'),
        ##('fat_k8_100G_OS1', 'llm_flow2'),
        #('fat_k8_100G_OS2', 'alltoall0.05-100'),
        #('fat_k8_100G_OS2', 'alltoall0.05-150'),
        #('fat_k8_100G_OS2', 'alltoall0.05-200'),
        #('fat_k8_100G_OS2', 'alltoall0.05-250'),
        #('fat_k8_100G_OS2', 'alltoall0.05-300'),
        ('fat_k8_100G_OS2', 'incast0.2-200'),
        ('fat_k8_100G_OS2', 'incast0.1-200'),
        ('fat_k8_100G_OS2', 'incast0.05-200'),
        ('fat_k8_100G_OS2', 'incast0.025-200'),
        ('fat_k8_100G_OS2', 'incast0.01-200'),
    ]
    
    processes = []
    record = defaultdict(dict)  # topo -> lb_mode -> id

    # Start the experiment
    record_history(f"{datetime.now()} Running experiments with different traffic patterns\n")

    for topo, flow_file in traffic_patterns:
        record_history(f"Topology: {topo}, Flow File: {flow_file}, INDEX=[")

        # Iterate through each lb_mode
        for lb_mode in lb_modes:
            experiment_id = get_cur_id()  # Simulate getting an experiment ID
            if lb_mode == lb_modes[-1]:
                record_history(f"{lb_mode}:{get_cur_id()}]\n")
            else:
                record_history(f"{lb_mode}:{get_cur_id()}, ")

            # Record the current experiment setup
            record[(topo, flow_file)][lb_mode] = experiment_id

            # Build the command dynamically with the traffic pattern and lb_mode
            command = f"python3 run.py --lb {lb_mode} --pfc 1 --irn 0 --my_flow {flow_file} --topo {topo}"
            
            # Run the experiment in a subprocess
            process = subprocess.Popen(command, shell=True)
            processes.append(process)
            time.sleep(10)  # Optional: pause between starting processes

    print("Waiting for all processes to finish...")
    
    # Wait for all processes to complete
    for process in processes:
        process.wait()

    # Now print the results for each topo and lb_mode
    for topo, flow_file in traffic_patterns:
        print(f"\nResults for Topology: {topo}, Flow File: {flow_file}")
        for lb_mode in lb_modes:
            experiment_id = record[(topo, flow_file)].get(lb_mode)
            if experiment_id:
                # Retrieve the FCT slowdown for the experiment
                analyser = getAnalyser(experiment_id)
                avg_fct_slowdown = analyser.get_avg_fct_slowdown()
                # Print the result for this combination
                print(f"    lb_mode: {lb_mode}, Experiment ID: {experiment_id}, FCT Slowdown: {avg_fct_slowdown:.2f}")



try:
    #run_caver_pathnum_ce_experiments()
    #run_caver_dreTime_alpha_experiments()
    #run_with_traffic_patterns()
    run_normal(cdf='GoogleRPC2008')
    #run_with_traffic_patterns()
    #run_caver_patchoiceTimeout_experiments()
    
finally:
    cleanup_processes(all_processes)