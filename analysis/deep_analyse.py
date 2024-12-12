#!/usr/bin/python3
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

def get_info_by_id(config_id):
    '''返回base_dir, lb_mode, load'''
    base_dir = '/home/zj/ns-allinone-3.19/ns-3.19/mix/output'
    command = f"ls -l {base_dir}"
    # 执行命令
    result = subprocess.run(command, capture_output=True, text=True, shell=True)
    full_config_id = [x.strip().split()[-1] for x in filter(lambda x : f'[{config_id}]' in x, result.stdout.split('\n'))]
    if not len(full_config_id) == 1:
        raise Exception(f'查找{config_id}号实验数据出现异常')
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


class Analyser:
    def __init__(self, id):
        print(f'开始分析{id}号实验数据')
        self.base_dir, self.lb_mode, self.load = get_info_by_id(id)
        self.packetId2FlowId = {}
        self.flow_trace:defaultdict[int, list[tuple]] = defaultdict(list)
        self.flows: list[Flow] = []
        self.id_to_flow: map[int, Flow] = {}
        self.pfc_events: list[PfcEvent] = []
        self.path_choice_infos:list[PathChoiceInfo] = []

    def show_caver_choice_info(self):
        if self.lb_mode != 'caver':
            return
        if len(self.path_choice_infos) == 0:
            self._parse_path_choice_info()
        print(f'一共{len(self.path_choice_infos)}路径信息')
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
        # 对于valid_path不为空时，分析ce和fct_slowdown的关系
        # df = df[(df['ce_avg'].notnull())]
        # intervals = [(i, i+20) for i in range(0, 301, 20)]
        # # 遍历每个区间，筛选数据并计算相关性
        # for lower, upper in intervals:
        #     subset = df[(df['ce_avg'] >= lower) & (df['ce_avg'] < upper)]
        #     print(f"区间 {lower} 到 {upper} 的数据行数: {len(subset)}")

    def get_avg_fct_slowdown(self):
        if len(self.flows) == 0:
            self._read_flows_from_file()
        return sum(map(lambda f: f.fct_slowdown, self.flows)) / len(self.flows)

    def get_p99_fct_slowdown(self):
        if len(self.flows) == 0:
            self._read_flows_from_file()
        return np.percentile([f.fct_slowdown for f in self.flows], 99)
    
    def get_small_flow_fct_slowdown(self):
        if len(self.flows) == 0:
            self._read_flows_from_file()
        # 筛选 m_size < 100 * 1024 的流
        small_flows = [f for f in self.flows if f.m_size < 20 * 1024]
        if len(small_flows) == 0:
            return None  # 如果没有符合条件的流，返回 None
        return sum(f.fct_slowdown for f in small_flows) / len(small_flows)

    def get_large_flow_fct_slowdown(self):
        if len(self.flows) == 0:
            self._read_flows_from_file()
        # 筛选 m_size > 10 * 1024 * 1024 的流
        large_flows = [f for f in self.flows if f.m_size > 100 * 1024]
        if len(large_flows) == 0:
            return None  # 如果没有符合条件的流，返回 None
        return sum(f.fct_slowdown for f in large_flows) / len(large_flows)

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

    #def analyse_caver_pathchoice(self, fct_threshold:float):
    #    assert('caver' in self.base_dir)
    #    if len(self.path_choice_infos) == 0:
    #        self._parse_path_choice_info()
    #    for path_choice_info in self.path_choice_infos:
    #        if path_choice_info.flow.fct_slowdown > fct_threshold:
    #            print(f'{path_choice_info}')


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

markers = {
    'fecmp': 'o',
    'conga': 's',
    'conweave': '^',
    'hula': 'd',
    'caver': '+',
}
linestyles = {
    'fecmp': '--',      # 虚线
    'conga': '-.',      # 点划线
    'conweave': ':',    # 点线
    'hula': (0, (3, 1, 1, 1, 1, 1)),       # 虚线
    'caver': '-',       # 实线
}
colors = {
    'fecmp': (0, 0, 179/255),        # 暗蓝色 (RGB(0, 0, 139))
    'conga': 'green',                # 绿色保持不变
    'conweave': 'orange',            # 橙色保持不变
    'hula': (179/255, 0, 0),         # 暗红色 (RGB(139, 0, 0))
    'caver': (102/255, 8/255, 116/255),  # 紫色保持不变
}
lb_mode_upper = {
    'fecmp': 'ECMP',
    'conga': 'Conga',
    'conweave': 'ConWeave',
    'hula': 'HULA',
    'caver': 'Caver',
    'dv': 'dv',
}
def get_config_id(config_ids_str:str)->list:
    config_ids = []
    for part in config_ids_str.split(','):
        if '-' in part:
            start, end = map(int, part.split('-'))
            config_ids.extend(range(start, end + 1))
        else:
            config_ids.append(int(part))
    return config_ids

def get_avg_fct(config_ids_str:str)->list:
    for config_id in get_config_id(config_ids_str):
        analyser = Analyser(config_id)
        avg_slowdown = analyser.get_avg_fct_slowdown()
        print(f'====={config_id}=====')
        print(f'avg_fct_slowdown: {avg_slowdown}')
        analyser.show_caver_choice_info()


#['o', 's', '^', 'd', '*', 'x', '+', 'p', 'h']
def plot_overall_fctslowdown(config_ids_str):

    # Data preparation
    data = defaultdict(lambda: defaultdict(list))
    small_flow_data = defaultdict(lambda: defaultdict(list))
    large_flow_data = defaultdict(lambda: defaultdict(list))
    p99_flow_data = defaultdict(lambda: defaultdict(list))

    for config_id in get_config_id(config_ids_str):
        analyser = Analyser(config_id)

        avg_slowdown = analyser.get_avg_fct_slowdown()
        data[analyser.lb_mode][analyser.load].append(avg_slowdown)

        small_avg_slowdown = analyser.get_small_flow_fct_slowdown()
        if small_avg_slowdown is not None:
            small_flow_data[analyser.lb_mode][analyser.load].append(small_avg_slowdown)

        large_avg_slowdown = analyser.get_large_flow_fct_slowdown()
        if large_avg_slowdown is not None:
            large_flow_data[analyser.lb_mode][analyser.load].append(large_avg_slowdown)

        p99_slowdown = analyser.get_p99_fct_slowdown()
        p99_flow_data[analyser.lb_mode][analyser.load].append(p99_slowdown)

    del data['dv']  # Remove 'dv' if present
    del small_flow_data['dv']
    del large_flow_data['dv']
    del p99_flow_data['dv']

    # Plotting helper function
    def plot_data(data, title, ylabel, filename):
        plt.figure(figsize=(6, 4), dpi=300)
        y_max = 0
        for lb_mode, loads_data in data.items():
            loads = sorted(loads_data.keys())
            avg_slowdowns = [sum(loads_data[load]) / len(loads_data[load]) for load in loads]
            plt.plot(loads, avg_slowdowns, label=lb_mode_upper[lb_mode], linewidth=3.5, linestyle=linestyles[lb_mode], color=colors[lb_mode])
            y_max = max([y_max, max(avg_slowdowns)])


        plt.xticks([40, 50, 60, 70, 80], fontsize=18)  # 指定显示的刻度值
        y_max = min(60, y_max)
        # 计算初步的步长
        raw_step = y_max / 10
        if raw_step <= 1:
            step = 0.5 if raw_step > 0.5 else 1
        else:
            step = np.ceil(raw_step * 2) / 2
        all_ticks = np.arange(0, y_max + step, step)  # 所有刻度值
        visible_ticks = [tick if i % 2 != len(all_ticks) % 2 else '' for i, tick in enumerate(all_ticks)]  # 每隔一个显示一次数字        
        plt.yticks(all_ticks, visible_ticks, fontsize=18)  # 指定显示的刻度值
        plt.xlabel("Load(%)", fontsize=22)  # 设置 x 轴标签字体大小
        plt.ylabel(ylabel, fontsize=22)  # 设置 y 轴标签字体大小
        plt.legend(frameon=False, fontsize=20)
        plt.grid(axis='y', alpha=0.3)
        ax = plt.gca()
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.set_ylim(0, all_ticks[-1])
        ax.set_xlim(37, 80)

        plt.savefig(filename, bbox_inches='tight')
        print(f'已保存到{filename}')

    # Plot 1: Overall Avg FCT Slowdown
    plot_data(data, "Overall Avg FCT Slowdown vs Load", "Avg FCT Slowdown", "overall_fct_slowdown.png")
    plot_data(small_flow_data, "Small Flow Avg FCT Slowdown vs Load", "Avg FCT Slowdown", "small_flow_fct_slowdown.png")
    plot_data(large_flow_data, "Large Flow Avg FCT Slowdown vs Load", "Avg FCT Slowdown", "large_flow_fct_slowdown.png")
    plot_data(p99_flow_data, "P99 FCT Slowdown vs Load", "P99 FCT Slowdown", "p99_fct_slowdown.png")


#分析随机流量数据plot_overall_fctslowdown("434-451,488-499")
#分析bond随机流量数据plot_overall_fctslowdown("452-469,512-523")
if __name__ == "__main__":
    #plot_overall_fctslowdown("452-469,512-523")
    #"555,558,560-569" 选路数和阈值对参数的影响
    get_avg_fct("537")
    #if len(sys.argv) >= 2:
    #    analyser = Analyser(int(sys.argv[1]))
    #    #analyser.analyse_long_flow_trace(1000)
    #    #analyser.plot_fct_slowdown()
    #    #analyser.plot_pfc_times()
    #    #analyser.show_caver_choice_info()
    #    print(analyser.get_avg_fct_slowdown())
    #    #code.interact(local=globals())
    #else:
    #    print("Usage: python script.py <id>")