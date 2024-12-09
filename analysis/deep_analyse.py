#!/usr/bin/python3import os
import code
import subprocess
import matplotlib.pyplot as plt
import os.path as op
import re
import sys
from dataclasses import dataclass, field
from collections import defaultdict
import numpy as np
import pandas as pd
from typing import Union, List
from scipy.stats import pearsonr, spearmanr
import readline

def get_dir_by_id(config_id):
    base_dir = '/home/zj/ns-allinone-3.19/ns-3.19/mix/output'
    command = f"ls -l {base_dir}"
    # 执行命令
    result = subprocess.run(command, capture_output=True, text=True, shell=True)
    full_config_id = [x.strip().split()[-1] for x in filter(lambda x : f'[{config_id}]' in x, result.stdout.split('\n'))]
    assert(len(full_config_id) == 1)
    return op.join(base_dir, full_config_id[0])

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


class Analyser:
    def __init__(self, id):
        self.base_dir = get_dir_by_id(id)
        self.packetId2FlowId = {}
        self.flow_trace:defaultdict[int, list[tuple]] = defaultdict(list)
        self.flows: list[Flow] = []
        self.id_to_flow: map[int, Flow] = {}
        self.pfc_events: list[PfcEvent] = []
        self.path_choice_infos:list[PathChoiceInfo] = []

    def plot_fct_slowdown(self):
        if len(self.flows) == 0:
            self._read_flows_from_file()
        slowdown = [flow.fct_slowdown for flow in self.flows]
        plt.figure(figsize=(10, 5))
        plt.plot(slowdown, marker='o', linestyle='-')
        plt.title('FCT Ratios')
        plt.xlabel('Flow Index')
        plt.ylabel('Actual FCT / Standalone FCT')
        plt.grid(True)
        script_directory = op.dirname(op.abspath(__file__))
        settings = op.basename(self.base_dir)
        save_path = op.join(script_directory, f'Fct-slowdown-{settings}-hist.png')
        plt.savefig(save_path)
        print(f'已经保存至 {save_path}')

        bins = np.arange(0, max(slowdown) + 50, 50)  # 区间: [0, 50), [50, 100), [100, 150)
        hist, bin_edges = np.histogram(slowdown, [0, 10, 20, 30, 40, 50, 100, 150, 200, 300, 1000, 5000])
        for i in range(len(bin_edges) - 1):
            if hist[i] == 0:
                continue
            print(f"区间 {bin_edges[i]} - {bin_edges[i+1]}: {hist[i]} 个")
        

    def plot_pfc_times(self):
        if len(self.pfc_events) == 0:
            self._read_pfc_files()
        pfc_times = list(map(lambda x: x.timestamp, self.pfc_events))
        plt.figure(figsize=(10, 6))
        plt.hist(pfc_times, bins=30, label='PFC Events', alpha=0.7)
        plt.title("PFC Events Over Time")
        plt.xlabel("Timestamp")
        plt.ylabel("Times")
        plt.legend()
        script_directory = op.dirname(op.abspath(__file__))
        settings = op.basename(self.base_dir)
        save_path = op.join(script_directory, f'PFC-{settings}.png')
        plt.savefig(save_path)
        print(f'已经保存至 {save_path}')

    def analyse_long_flow_trace(self, threshold=300, max_num = 20):
        if len(self.flows) == 0:
            self._read_flows_from_file()
        i = 0
        for flow in filter(lambda x: x.fct_slowdown > threshold, self.flows):
            if i > 20: return
            print(flow)
            self._print_flow_trace(flow.flow_id)
            i += 1

    def analyse_caver_pathchoice(self, fct_threshold:float):
        assert('caver' in self.base_dir)
        if len(self.path_choice_infos) == 0:
            self._parse_path_choice_info()
        for path_choice_info in self.path_choice_infos:
            if path_choice_info.flow.fct_slowdown > fct_threshold:
                print(f'{path_choice_info}')


    def _parse_path_choice_info(self):
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
        if len(self.packetId2FlowId) == 0:
            with open(op.join(self.base_dir, "packetId2FlowId.txt"), 'r') as file:
                for line in file:
                    flow_id = int(line.split(' ')[-1])
                    key = tuple(map(int, line.split('(')[1].split(')')[0].split(' ')))
                    self.packetId2FlowId[key] = flow_id
        return self.packetId2FlowId[flow_key]

    def _parse_flow_trace_file(self):
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
        if len(self.flow_trace) == 0:
            self._parse_flow_trace_file()
        trace = self.flow_trace[flow_id]
        trace.sort(key=lambda x: (x[0], x[1] >= x[2], x[1] if x[1] < x[2] else -x[1]))
        for time, src, dst in trace:
            print(f'{time}: {src} -> {dst}')
        print('')

    def _read_flows_from_file(self):
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

    def _read_pfc_files(self):
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
        print(f'{len(self.pfc_events)}条PFC事件已读取')


if __name__ == "__main__":
    if len(sys.argv) >= 2:

        analyser = Analyser(int(sys.argv[1]))
        analyser.analyse_long_flow_trace(1000)
        #analyser.plot_fct_slowdown()
        #analyser.plot_pfc_times()
        code.interact(local=globals())
    else:
        print("Usage: python script.py <id>")