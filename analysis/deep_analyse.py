#!/usr/bin/python3
from __future__ import annotations
import json
import multiprocessing
import subprocess
import matplotlib.pyplot as plt
import os.path as op
import re
import sys
from dataclasses import dataclass, field
from collections import Counter, OrderedDict, defaultdict
import numpy as np
import pandas as pd
from typing import Generator, Union, List, Dict
from scipy.stats import pearsonr, spearmanr
import readline
import os

def get_info_by_id(config_id):
    '''return base_dir, lb_mode, load'''
    base_dir = '/home/zj/ns-allinone-3.19/ns-3.19/mix/output'
    command = f"ls -l {base_dir}"
    # 执行命令
    result = subprocess.run(command, capture_output=True, text=True, shell=True)
    full_config_id = [x.strip().split()[-1] for x in filter(lambda x : f'[{config_id}]' in x, result.stdout.split('\n'))]
    if not len(full_config_id) == 1:
        raise Exception(f'failed to find {config_id} experiment')
    full_config_id = full_config_id[0]
    lb_mode = full_config_id.split('-')[-2]
    load = int(full_config_id.split('-')[-1])
    return op.join(base_dir, full_config_id), lb_mode, load

@dataclass
class Flow:
    sip_id: int
    dip_id: int
    sport: int
    dport: int
    m_size: int
    start_time: float
    elapsed_time: float
    standalone_fct: float
    flow_id: int
    fct_slowdown: Union[float, None] = field(init=False)

    def __post_init__(self):
        if self.standalone_fct == 0:
            self.fct_slowdown = float('inf')
        else:
            self.fct_slowdown = self.elapsed_time / self.standalone_fct

@dataclass
class PathChoiceInfo:
    @dataclass
    class Path:
        nodes:List[int]
        ce:int
        update_time:int
    flow:Flow
    valid_path:List[Path]
    has_unused_path:bool

@dataclass
class PfcEvent:
    timestamp: int
    node_id: int
    node_type: int
    if_index: int
    type: int

@dataclass
class CaverReceivedPath:
    time: int  # timestamp
    switch: int  # Switch ID
    did: int  # Did ID
    update: int  
    m_is_usable: int  
    total_best_ce: int 
    path: List[int]  


class Analyser:
    def __init__(self, id):
        self.id = id = str(id)
        self.base_dir, self.lb_mode, self.load = get_info_by_id(id)
        self.packetId2FlowId = {}
        self.flow_trace:defaultdict[int, list[tuple]] = defaultdict(list)
        self.flows: list[Flow] = []
        self.flows_after2005:list[Flow] = []
        self.id_to_flow: map[int, Flow] = {}
        self.pfc_events: list[PfcEvent] = []
        self.path_choice_infos:list[PathChoiceInfo] = []
        self.ideal_path_ce:map[int, map[tuple[int, int], list[int]]] = {}#time -> <(src, dst) -> list[ce]>

    def print_info(self):
        print(f'===ID:{self.id}, LB:{self.lb_mode}, LOAD:{self.load}===')

    def plot_caver_received_path(self, window_size=50 * 1000, step=10, ce_threshold=1.3):
        """
        Plot the received path data from the caver_log.
        - Filter paths that meet the criteria.
        - Use a sliding window to count the total number of paths and the number of unique paths.
        - Generate line charts and distribution histograms, and save them to a file.
        """
        records = []
        pattern = re.compile(
            r"Time:(\d+),\s*Switch:(\d+),\s*Did:(\d+),\s*update:(\d+),\s*M_is_usable:(\d+),\s*totalBestCe:(\d+)\|(.+)"
        )
        with open(op.join(self.base_dir, "caver_log.txt")) as file:
            for line in file.readlines():
                line = line.strip()
                # Match the regular expression
                match = pattern.match(line)
                if match:
                    # Extract data and create object
                    time, switch, did, update, m_is_usable, total_best_ce, path = match.groups()
                    record = CaverReceivedPath(
                        time=int(time),
                        switch=int(switch),
                        did=int(did),
                        update=int(update),
                        m_is_usable=int(m_is_usable),
                        total_best_ce=int(total_best_ce),
                        path=tuple(map(int, path.split()))  # Use tuple so it can be used as a dictionary key
                    )
                    records.append(record)
        
        # Filter records that meet the criteria
        filtered_records = [
            rec for rec in records
            if rec.switch == 257 and rec.did in [176 + i for i in range(8)] and rec.m_is_usable == 1
        ]
        
        times = [rec.time for rec in filtered_records]
        paths_counter = Counter()  # Used to dynamically maintain the paths in the current window
        results = []  # Number of records in each time window
        unique_paths_count = []  # Number of unique paths in each time window
        ideal_acc_num = []
        start_time = times[0]  # Minimum timestamp
        end_time = times[-1]  # Maximum timestamp
        n = len(times)
        left, right = 0, 0  # Initialize two pointers
        current_start = start_time
        
        data_file = open("data_file.txt", "w")
        while current_start <= end_time:
            current_end = current_start + window_size
            
            # Move the right pointer and add new paths to the counter
            while right < n and times[right] < current_end:
                paths_counter[filtered_records[right].path] += 1
                right += 1
            
            # Move the left pointer and remove paths that are no longer in the window
            while left < n and times[left] < current_start:
                path_to_remove = filtered_records[left].path
                if paths_counter[path_to_remove] == 1:
                    del paths_counter[path_to_remove]  # If count is 1, delete the path
                else:
                    paths_counter[path_to_remove] -= 1
                left += 1

            # The number of records in the current window is the number of elements in [left, right)
            results.append(right - left)
            
            # The number of unique paths in the current window
            unique_paths_count.append(len(paths_counter))

            ideal_acc_num.append(self.query_ideal_acceptable_path_num(current_start, current_end, 257, 177, ce_threshold))
            
            current_start += step
            data_file.write(f'{current_start} {results[-1]} {unique_paths_count[-1]} {ideal_acc_num[-1][0]} {ideal_acc_num[-1][1]} {ideal_acc_num[-1][2]}\n')
        data_file.close()
        
        # Calculate statistics
        total_path_mean = np.mean(results)
        total_path_std = np.std(results)
        unique_path_mean = np.mean(unique_paths_count)
        unique_path_std = np.std(unique_paths_count)
        ideal_acc_mean = np.mean(ideal_acc_num)
        ideal_acc_std =  np.std(ideal_acc_num)
        
        print(f"Total Path - Mean: {total_path_mean:.2f}, Std: {total_path_std:.2f}")
        print(f"Unique Path - Mean: {unique_path_mean:.2f}, Std: {unique_path_std:.2f}")
        print(f"Ideal Acc Path - Mean: {ideal_acc_mean:.2f}, Std: {ideal_acc_std:.2f}")

    def query_ideal_acceptable_path_num(self, start_time, end_time, src, dst, ce_threshold):
        if len(self.ideal_path_ce) == 0:
            #file_path = op.join(self.base_dir, op.basename(self.base_dir) + '_global_ce_map.txt')
            file_path = op.join(self.base_dir, 'ideal_ce.txt')
            with open(file_path, 'r') as f:
                for line in f.readlines():
                    data = json.loads(line)
                    time = data["timestamp"]
                    data = {(item["i"], item["j"]):item["paths"] for item in data["data"]}
                    self.ideal_path_ce[int(time)] = data
                    print(time)
        start_time = ((start_time + 19999) // 20000) * 20000
        acc_num_list = []
        for time in range(int(start_time), int(end_time), 20000):
            ce_list = self.ideal_path_ce[time][(src, dst)]
            best_ce = min(ce_list)
            acc_num_list.append(len([ce for ce in ce_list if (256 - ce) * ce_threshold >= (256 - best_ce) ]))
        return min(acc_num_list), max(acc_num_list), np.average(acc_num_list)# / len(acc_num_list)
                


    def analyse_caver_choice_info(self):
        """
        Analyze Caver path selection information.
        - Parse path selection related log files.
        - Generate statistics about path selection information, such as whether there are unused paths, FCT slowdown, etc.
        """
        if self.lb_mode != 'caver':
            return
        if len(self.path_choice_infos) == 0:
            self._parse_path_choice_info()
        print(f'Total {len(self.path_choice_infos)} path information')
        data = []
        for info in self.path_choice_infos:
            if info.flow.m_size > 30000:
                continue
            data.append({
                'has_unused_path': info.has_unused_path,
                'has_valid_path': len(info.valid_path) > 0,
                'fct_slowdown': info.flow.fct_slowdown,
                'ce_std': np.var(list(map(lambda x : x.ce, info.valid_path)))**0.5 if len(info.valid_path) else None,
                'ce_avg': sum(x.ce for x in info.valid_path) / len(info.valid_path) if len(info.valid_path) else None,
            })

        df = pd.DataFrame(data)
        unused_path_counts = df.groupby('has_valid_path')['fct_slowdown'].agg(['count', 'mean'])
        print(unused_path_counts)

    def get_avg_fct_slowdown(self):
        """
        Calculate the average FCT (Flow Completion Time) slowdown for all flows.
        - If flow data is not loaded, load the flow information from the file first.
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        return np.average([f.fct_slowdown for f in self.flows_after2005])

    def get_p99_fct_slowdown(self):
        """
        Calculate the P99 (99th Percentile) FCT slowdown for all flows.
        - If flow data is not loaded, load the flow information from the file first.
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        return np.percentile([f.fct_slowdown for f in self.flows_after2005], 99)

    def get_small_flow_fct_slowdown(self, threshold=20 * 1024):
        """
        Calculate the average FCT slowdown for small flows (m_size < 20 KB).
        - If flow data is not loaded, load the flow information from the file first.
        - If there are no flows meeting the condition, return None.
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        # Filter flows with m_size < 100 * 1024
        small_flows = [f for f in self.flows_after2005 if f.m_size < threshold]
        if len(small_flows) == 0:
            return float('inf')  # If no flows meet the condition, return None
        return sum(f.fct_slowdown for f in small_flows) / len(small_flows)

    def get_large_flow_fct_slowdown(self, threshold = 100 * 1024):
        """
        Calculate the average FCT slowdown for large flows (m_size > 100 KB).
        - If flow data is not loaded, load the flow information from the file first.
        - If there are no flows meeting the condition, return None.
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        # Filter flows with m_size > 10 * 1024 * 1024
        large_flows = [f for f in self.flows_after2005 if f.m_size > threshold]
        if len(large_flows) == 0:
            return float('inf')  # If no flows meet the condition, return None
        return sum(f.fct_slowdown for f in large_flows) / len(large_flows)

    def analyse_long_flow_trace(self, threshold=300, max_num = 20):
        """
        Analyze long flows whose FCT slowdown exceeds the specified threshold.
        - Filter flows that meet the condition and print their details and flow trace.
        - Analyze at most max_num flows.
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        i = 0
        for flow in filter(lambda x: x.fct_slowdown > threshold, self.flows):
            if i > 20: return
            print(flow)
            self._print_flow_trace(flow.flow_id)
            i += 1

    def analyse_large_flow_trace(self, threshold=0.95e6, max_num = 1000):
        """
        Analyze long flows whose FCT slowdown exceeds the specified threshold.
        - Filter flows that meet the condition and print their details and flow trace.
        - Analyze at most max_num flows.
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()

        if len(self.flow_trace) == 0:
            self._parse_flow_trace_file()
        i = 0
        for flow in filter(lambda x: x.m_size > threshold, self.flows):
            if i > 20: return
            print(flow)
            trace = self.flow_trace[flow.flow_id]
            trace.sort(key=lambda x: (x[0], x[1] >= x[2], x[1] if x[1] < x[2] else -x[1]))
            full_path = list(OrderedDict.fromkeys([(x[1], x[2]) for x in trace]))
            print(full_path)
            i += 1

    def _parse_path_choice_info(self):
        """
        Parse path selection related configuration log files.
        - Extract path selection information for each flow, including the number of available paths, whether there are unused paths, path details, etc.
        """
        file_path = op.join(self.base_dir, "config.log")

        with open(file_path, "r") as file:
            lines = file.readlines()
            for line in lines:
                if "CHOOSEPATH:num of valid paths" in line:
                    # Parse valid path size and has_unused_path
                    match = re.search(r"CHOOSEPATH:num of valid paths:(\d+), find an unused path:(\d)#", line)
                    if not match:
                        continue
                    valid_path_size = int(match.group(1))
                    has_unused_path = bool(int(match.group(2)))
                    # Parse paths
                    valid_paths = []
                    path_matches = re.findall(r"((?:\d+ )+)\|", line)
                    for path_match in path_matches:
                        elements = list(map(int, path_match.split()))
                        nodes = elements[:-2]  # All except last two are nodes
                        ce = elements[-2]  # Second last is remoteCE
                        #if ce > 255:
                        #    print(line)
                        #    quit(0)
                        update_time = elements[-1]  # Last is updateTime
                        valid_paths.append(PathChoiceInfo.Path(nodes, ce, update_time))
                    # Parse flow ID
                    flow_match = re.search(r"flowid:(\d+)", line)
                    if not flow_match:
                        continue
                    flow_id = int(flow_match.group(1))
                    # Get Flow object
                    if len(self.id_to_flow) == 0:
                        self._read_flows_from_file()
                    flow = self.id_to_flow[flow_id]
                    # Add to path_choice_infos
                    self.path_choice_infos.append(PathChoiceInfo(flow, valid_paths, has_unused_path))

    def _query_flow_id(self, flow_key):
        """
        Query flow ID based on the tuple information of the packet.
        - If the mapping is not loaded, load it from the file.
        """
        if len(self.packetId2FlowId) == 0:
            with open(op.join(self.base_dir, "packetId2FlowId.txt"), 'r') as file:
                for line in file:
                    flow_id = int(line.split(' ')[-1])
                    key = tuple(map(int, line.split('(')[1].split(')')[0].split(' ')))
                    self.packetId2FlowId[key] = flow_id
        return self.packetId2FlowId[flow_key]

    def _parse_flow_trace_file(self):
        """
        Parse the flow trace file, recording the source and destination node information for each flow at each time point.
        """
        current_time = None
        with open(op.join(self.base_dir, "flow_distribution.txt"), 'r') as file:
            for line in file:
                time_match = re.match(r'#####Time\[(\d+)\]#####', line)
                if time_match:
                    current_time = int(time_match.group(1))
                elif current_time is not None:
                    link_match = re.match(r'Link: srcId=(\d+), dstId=(\d+), flowNum=\d+, active flow:([\d, ]+)', line)
                    if link_match:
                        src_id = int(link_match.group(1))
                        dst_id = int(link_match.group(2))
                        flows = [int(flow.strip()) for flow in link_match.group(3).split(',') if flow.strip()]
                        for flow in flows:
                            self.flow_trace[flow].append((current_time, src_id, dst_id))

    def _print_flow_trace(self, flow_id):
        """
        Print the trace information for the specified flow ID.
        - Sort by time and node order, then print.
        """
        if len(self.flow_trace) == 0:
            self._parse_flow_trace_file()
        trace = self.flow_trace[flow_id]
        trace.sort(key=lambda x: (x[0], x[1] >= x[2], x[1] if x[1] < x[2] else -x[1]))
        for time, src, dst in trace:
            print(f'{time}: {src} -> {dst}')
        print('')

    def _read_flows_from_file(self):
        """
        Read flow information from a file.
        - Extract flow's source node, destination node, flow size, FCT, etc.
        - Establish a mapping between flow ID and flow object.
        """
        self.flows = []
        file_name = op.basename(self.base_dir) + "_out_fct.txt"
        file_path = op.join(self.base_dir, file_name)
        with open(file_path, 'r') as file:
            for line in file:
                data = line.split()
                if len(data) != 8:
                    continue
                sip_id = int(data[0])
                dip_id = int(data[1])
                sport = int(data[2])
                dport = int(data[3])
                m_size = int(data[4])
                start_time = int(data[5])
                elapsed_time = int(data[6])
                standalone_fct = int(data[7])
                flow_id = self._query_flow_id((sip_id, dip_id, sport, dport))
                
                flow = Flow(sip_id, dip_id, sport, dport, m_size, start_time, elapsed_time, standalone_fct, flow_id)
                self.flows.append(flow)
                self.id_to_flow[flow_id] = flow
                if start_time > 2.005e9:
                    self.flows_after2005.append(flow)

    def _read_pfc_files(self):
        """
        Read PFC (Priority Flow Control) event information from a file.
        - Extract each event's timestamp, node ID, interface index, and other information.
        """
        file_path = op.join(self.base_dir, op.basename(self.base_dir) + '_out_pfc.txt')
        if not op.exists(file_path):
            raise FileNotFoundError(f"PFC file not found: {file_path}")
        self.pfc_events = []
        with open(file_path, 'r') as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) != 5:
                    print(f"Skipping malformed line: {line.strip()}")
                    continue
                try:
                    event = PfcEvent(
                        timestamp=int(parts[0]),
                        node_id=int(parts[1]),
                        node_type=int(parts[2]),
                        if_index=int(parts[3]),
                        type=int(parts[4])
                    )
                    self.pfc_events.append(event)
                except ValueError as e:
                    print(f"Skipping line due to parsing error: {line.strip()}, Error: {e}")
        print(f'{len(self.pfc_events)} PFC events have been read')


_instances = {}
def getAnalyser(id) -> Analyser:
    if id not in _instances.keys():
        _instances[id] = Analyser(str(id))
    return _instances[id]


linestyles = {
    'fecmp': '--',      # 虚线
    'conga': '-.',      # 点划线
    'conweave': ':',    # 点线
    'hula': (0, (3, 1, 1, 1, 1, 1)),       # 虚线
    'caver': '-',       # 实线
    'noshare': '-.',
    'dv': ':',
}
colors = {
    'fecmp': (0, 0, 179/255),        # 暗蓝色 (RGB(0, 0, 139))
    'conga': 'green',                # 绿色保持不变
    'conweave': 'orange',            # 橙色保持不变
    'hula': (179/255, 0, 0),         # 暗红色 (RGB(139, 0, 0))
    'caver': (102/255, 8/255, 116/255),  # 紫色保持不变
    'noshare': (179/255, 0, 0),
    'dv': 'orange',
}
lb_mode_upper = {
    'fecmp': 'ECMP',
    'conga': 'CONGA',
    'conweave': 'ConWeave',
    'hula': 'HULA',
    'caver': 'CAVER',
    'dv': 'dv',
    'noshare': 'noshare',
}
markers = {
    'fecmp': 'o',
    'conga': 's',
    'conweave': '^',
    'hula': 'D',
    'caver': '*',
}
["o", "s", "^", "D", "*"]
def get_config_id(config_ids_str:str)->list:
    config_ids = []
    for part in config_ids_str.split(','):
        if '-' in part:
            start, end = map(int, part.split('-'))
            config_ids.extend(range(start, end + 1))
        else:
            config_ids.append(int(part))
    return config_ids

def analyser_iter(config_ids_str:str) -> Generator[Analyser, None, None]:
    for id in get_config_id(config_ids_str):
        try:
            analyser = getAnalyser(id)
            yield analyser
        except Exception as e:
            print(f'{id}号实验数据出现异常：{e}')

def process_analyser(analyser, small_flow_threshold, large_flow_threshold):
    #analyser.print_info()
    avg = analyser.get_avg_fct_slowdown()
    p99 = analyser.get_p99_fct_slowdown()
    small = analyser.get_small_flow_fct_slowdown(small_flow_threshold)
    large = analyser.get_large_flow_fct_slowdown(large_flow_threshold)
    #print(f'avg:{avg:.2f}, p99:{p99:.2f}, small_flow:{small:.2f}, large_flow:{large:.2f}')
    return [analyser.id, analyser.lb_mode, avg, p99, small, large]

def get_basic_result(config_ids_str:str, small_flow_threshold=20*1024, large_flow_threshold=100*1024) -> pd.DataFrame:
    
    with multiprocessing.Pool(processes=multiprocessing.cpu_count()) as pool:
        results = pool.starmap(process_analyser, [(analyser, small_flow_threshold, large_flow_threshold) for analyser in analyser_iter(config_ids_str)])
    
    df = pd.DataFrame(results, columns=['id', 'lb', 'avg', 'p99', 'small', 'large']).round(2)
    df_sorted = df.sort_values(by='id')
    return df_sorted


#['o', 's', '^', 'd', '*', 'x', '+', 'p', 'h']
def plot_overall_fctslowdown(config_ids_str):

    # Data preparation
    avg_data = defaultdict(lambda: {})
    small_flow_data = defaultdict(lambda: {})
    large_flow_data = defaultdict(lambda: {})
    p99_flow_data = defaultdict(lambda: {})

    #plot_data = defaultdict(dict)#lb_mode->(load->fct)
    for _, row in get_basic_result(config_ids_str).iterrows():
        lb, avg, p99, small, large, load = row['lb'], row['avg'], row['p99'], row['small'], row['large'], getAnalyser(row['id']).load
        if lb in ['dv', 'noshare']:
            continue
        avg_data[lb][load] = avg
        small_flow_data[lb][load] = small
        large_flow_data[lb][load] = large
        p99_flow_data[lb][load] = p99
    print(f'avg:{avg_data}\nsmall:{small_flow_data}\nlarge:{large_flow_data}\np99:{p99_flow_data}')

    # Plotting helper function
    def plot_data(data, ylabel, filename):
        plt.figure(figsize=(6, 4), dpi=300)
        y_max = 0
        for lb_mode, loads_data in data.items():
            loads = sorted(loads_data.keys())
            avg_slowdowns = [loads_data[load] for load in loads]
            plt.plot(loads, avg_slowdowns, label=lb_mode_upper[lb_mode], linewidth=3.5,
                     linestyle=linestyles[lb_mode], color=colors[lb_mode])
            y_max = max([y_max, max(avg_slowdowns)])

        plt.xticks([40, 50, 60, 70, 80], fontsize=18)
        #y_max = min(60, y_max)

        raw_step = y_max / 10
        step = 0.5 if raw_step <= 1 else np.ceil(raw_step * 2) / 2
        all_ticks = np.arange(0, y_max + step, step)
        visible_ticks = [tick if i % 2 != len(all_ticks) % 2 else '' for i, tick in enumerate(all_ticks)]
        plt.yticks(all_ticks, visible_ticks, fontsize=18)

        plt.xlabel("Load(%)", fontsize=22)
        plt.ylabel(ylabel, fontsize=22)
        plt.legend(frameon=False, fontsize=20, loc='upper left', bbox_to_anchor=(0, 1.1), labelspacing=0.3)
        plt.grid(axis='y', alpha=0.3)
        ax = plt.gca()
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.set_ylim(0, all_ticks[-1])
        ax.set_xlim(37, 80)

        filepath = op.join(op.dirname(__file__), filename)
        plt.savefig(filepath, bbox_inches='tight')
        print(f'3aved to {filepath}')

    plot_data(avg_data, "Avg. FCT Slowdown", "overall_fct_slowdown.pdf")
    plot_data(small_flow_data, "Avg. FCT Slowdown", "small_flow_fct_slowdown.pdf")
    plot_data(large_flow_data, "Avg. FCT Slowdown", "large_flow_fct_slowdown.pdf")
    plot_data(p99_flow_data, "p99. FCT Slowdown", "p99_fct_slowdown.pdf")


def plot_ack_data(config_ids_str, ack_interval):
    y_values = []
    for analyser in analyser_iter(config_ids_str):
        y_values.append(analyser.get_avg_fct_slowdown())
    assert(len(y_values) == len(ack_interval))
    x = list(range(1, len(y_values) + 1))

    # Plot the lines
    plt.figure(figsize=(6, 4), dpi=300)
    plt.plot(x, y_values, label='CAVER',color="purple",
        linewidth=3.5,
        marker='*',
        markersize=9,
    )
    ecmp_y_value = getAnalyser(1375).get_avg_fct_slowdown()
    plt.plot([0, len(y_values) + 1], [ecmp_y_value, ecmp_y_value], label='ECMP',color=(0, 0, 179/255),
        linewidth=2,
        linestyle='--',
        alpha=0.6,
        
    )
    print(x, y_values)

    # Customize x-axis
    plt.xticks(ticks=x, labels=ack_interval, fontsize=18)

    # Customize y-axis
    plt.yticks(fontsize=18)
    plt.xlabel("Ack Interval", fontsize=22)
    plt.ylabel("Avg. FCT Slowdown", fontsize=22)
    plt.grid(axis="y", linewidth=0.8, alpha=0.6)
    ax = plt.gca()
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_linewidth(1.5)
    ax.spines['bottom'].set_linewidth(1.5)
    ax.set_ylim(0, 8)
    ax.set_xlim(0.7, len(y_values)+0.1)

    # Add legend

    plt.legend(
        frameon=False, 
        fontsize=20, 
        loc='upper center', 
        bbox_to_anchor=(0.5, 1.3), 
        ncol=3, 
        labelspacing=0.01, 
        columnspacing=0.5, 
        handletextpad=0.2  
    )
    # Save the plot as PNG and PDF
    #plt.savefig("ack80.png", format="png", bbox_inches="tight")
    plt.savefig("ack.pdf", format="pdf", bbox_inches="tight")

def plot_incast_data(config_ids_str):
    x = [1, 2, 3, 4, 5]
    custom_xticks = ["100", "150", "200", "250", "300"]
    # Line properties

    y_values = get_basic_result(config_ids_str)['avg'].to_numpy().reshape(5, 5).T
    lb_mode = ['caver', 'conga', 'conweave', 'hula', 'fecmp']
    # Plot the lines
    plt.figure(figsize=(6, 4), dpi=300)
    for i, y in enumerate(y_values):
        plt.plot(
            x,
            y,
            label=lb_mode_upper[lb_mode[i]],
            color=colors[lb_mode[i]],
            linewidth=3.5,
            marker=markers[lb_mode[i]],
            markersize=9,
        )

    # Customize x-axis
    plt.xticks(ticks=x, labels=custom_xticks, fontsize=18)

    # Customize y-axis
    plt.yticks(fontsize=18)
    plt.xlabel("Flow Group Size", fontsize=22)
    plt.ylabel("Avg. FCT Slowdown", fontsize=22)
    plt.grid(axis="y", linewidth=0.8, alpha=0.6)
    ax = plt.gca()
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_linewidth(1.5)
    ax.spines['bottom'].set_linewidth(1.5)
    ax.set_ylim(0, 20)
    ax.set_xlim(0.7, 5.1)

    # Add legend

    plt.legend(
        frameon=False, 
        fontsize=20, 
        loc='upper center', 
        bbox_to_anchor=(0.5, 1.3), 
        ncol=3, 
        labelspacing=0.01, 
        columnspacing=0.5, 
        handletextpad=0.2 
    )
    # Save the plot as PNG and PDF
    plt.savefig("incast.png", format="png", bbox_inches="tight")
    plt.savefig("incast.pdf", format="pdf", bbox_inches="tight")

def plot_all_to_all_data(config_ids_str):
    x = [1, 2, 3, 4, 5]
    custom_xticks = ["5", "10", "20", "40", "100"]
    # Line properties
    print(get_basic_result(config_ids_str))
    y_values = get_basic_result(config_ids_str)['avg'].to_numpy().reshape(5, 5).T
    lb_mode = ['caver', 'conga', 'conweave', 'hula', 'fecmp']
    # Plot the lines
    plt.figure(figsize=(6, 4), dpi=300)
    for i, y in enumerate(y_values):
        plt.plot(
            x,
            y,
            label=lb_mode_upper[lb_mode[i]],
            color=colors[lb_mode[i]],
            linewidth=3.5,
            marker=markers[lb_mode[i]],
            markersize=9,
        )

    # Customize x-axis
    plt.xticks(ticks=x, labels=custom_xticks, fontsize=18)

    # Customize y-axis
    plt.yticks(fontsize=18)
    plt.xlabel("Concurrency Rate", fontsize=22)
    plt.ylabel("Avg. FCT Slowdown", fontsize=22)
    plt.grid(axis="y", linewidth=0.8, alpha=0.6)
    ax = plt.gca()
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_linewidth(1.5)
    ax.spines['bottom'].set_linewidth(1.5)
    ax.set_ylim(0, 20)
    ax.set_xlim(0.7, 5.1)

    # Add legend

    plt.legend(
        frameon=False, 
        fontsize=20, 
        loc='upper center', 
        bbox_to_anchor=(0.5, 1.3), 
        ncol=3, 
        labelspacing=0.01, 
        columnspacing=0.5,
        handletextpad=0.2 
    )
    # Save the plot as PNG and PDF
    #plt.savefig("all-to-all.png", format="png", bbox_inches="tight")
    plt.savefig("all-to-all.pdf", format="pdf", bbox_inches="tight")


def show_caver_setting(config_ids_str):
    for analyser in analyser_iter(config_ids_str):
        config_id_path = op.join(analyser.base_dir, 'config.log')
        print(f'================{analyser.id}==============')
        os.system(f'cat {config_id_path} | grep caver_ | tail')

def clear_data(config_ids_str):
    removed_dirs = []
    for analyser in analyser_iter(config_ids_str):
        base_dir = analyser.base_dir
        print(base_dir)
        removed_dirs.append(base_dir)
    op = input('press y to delete these data')
    if op == 'y':
        for dir in removed_dirs:
            os.system(f'rm -r {dir}') 


if __name__ == "__main__":
    #plot_ack_data('1671,1673,1675,1677,1680,1681', [1,2,4,10,20,40])
    #plot_ack_data('1672,1674,1676,1678,1679,1682', [1,2,4,10,20,40])
    #plot_incast_data('1603-1627')
    
    #plot_all_to_all_data('1628-1647, 1803-1807')
    #plot_overall_fctslowdown('1683-1707') #fat k4
    
    #plot_overall_fctslowdown('1733-1757') #leaf spine
    #plot_overall_fctslowdown('1683-1707') #bond
    #plot_overall_fctslowdown('1818-1842')

    #plot_incast_data('1900-1924')
    pass