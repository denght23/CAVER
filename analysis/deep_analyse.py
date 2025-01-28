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

@dataclass
class CaverReceivedPath:
    time: int  # 时间戳
    switch: int  # Switch ID
    did: int  # Did ID
    update: int  # 更新状态
    m_is_usable: int  # M_is_usable 标志
    total_best_ce: int  # TotalBestCe 值
    path: List[int]  # Path 内容（节点列表）


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

    def plot_caver_received_path(self, window_size=180 * 1000, step=10, ce_threshold=1.3):
        """
        绘制caver_log中的接收路径数据。
        - 筛选符合条件的路径信息。
        - 使用滑动窗口统计路径数据的总数量和唯一路径数量。
        - 生成折线图和分布直方图，并保存至文件。
        """
        records = []
        pattern = re.compile(
            r"Time:(\d+),\s*Switch:(\d+),\s*Did:(\d+),\s*update:(\d+),\s*M_is_usable:(\d+),\s*totalBestCe:(\d+)\|(.+)"
        )
        with open(op.join(self.base_dir, "caver_log.txt")) as file:
            for line in file.readlines():
                line = line.strip()
                # 匹配正则表达式
                match = pattern.match(line)
                if match:
                    # 提取数据并创建对象
                    time, switch, did, update, m_is_usable, total_best_ce, path = match.groups()
                    record = CaverReceivedPath(
                        time=int(time),
                        switch=int(switch),
                        did=int(did),
                        update=int(update),
                        m_is_usable=int(m_is_usable),
                        total_best_ce=int(total_best_ce),
                        path=tuple(map(int, path.split()))  # 使用tuple以便作为字典键
                    )
                    records.append(record)
        
        # 筛选符合条件的记录
        filtered_records = [
            rec for rec in records
            if rec.switch == 257 and rec.did in [176 + i for i in range(8)] and rec.m_is_usable == 1
        ]
        
        times = [rec.time for rec in filtered_records]
        paths_counter = Counter()  # 用于动态维护当前窗口内的路径
        results = []  # 每个时间窗口内记录数量
        unique_paths_count = []  # 每个时间窗口内唯一路径数量
        ideal_acc_num = []
        start_time = times[0]  # 最小时间戳
        end_time = times[-1]  # 最大时间戳
        n = len(times)
        left, right = 0, 0  # 初始化双指针
        current_start = start_time
        
        data_file = open("data_file.txt", "w")
        while current_start <= end_time:
            current_end = current_start + window_size
            
            # 移动右指针，并将新路径加入计数器
            while right < n and times[right] < current_end:
                paths_counter[filtered_records[right].path] += 1
                right += 1
            
            # 移动左指针，并将移出的路径从计数器中移除
            while left < n and times[left] < current_start:
                path_to_remove = filtered_records[left].path
                if paths_counter[path_to_remove] == 1:
                    del paths_counter[path_to_remove]  # 如果计数为1，则删除路径
                else:
                    paths_counter[path_to_remove] -= 1
                left += 1

            # 当前窗口的记录数量为 [left, right) 范围内的元素数
            results.append(right - left)
            
            # 当前窗口中唯一路径的数量
            unique_paths_count.append(len(paths_counter))

            ideal_acc_num.append(self.query_ideal_acceptable_path_num(current_start, current_end, 257, 177, ce_threshold))
            
            current_start += step
            data_file.write(f'{current_start} {results[-1]} {unique_paths_count[-1]} {ideal_acc_num[-1][0]} {ideal_acc_num[-1][1]} {ideal_acc_num[-1][2]}\n')
        data_file.close()
        
        # 计算统计量
        total_path_mean = np.mean(results)
        total_path_std = np.std(results)
        unique_path_mean = np.mean(unique_paths_count)
        unique_path_std = np.std(unique_paths_count)
        ideal_acc_mean = np.mean(ideal_acc_num)
        ideal_acc_std =  np.std(ideal_acc_num)
        
        # 打印统计信息
        print(f"Total Path - 平均值 (Mean): {total_path_mean:.2f}, 标准差: {total_path_std:.2f}")
        print(f"Unique Path - 平均值 (Mean): {unique_path_mean:.2f}, 标准差: {unique_path_std:.2f}")
        print(f"Ideal Acc Path - 平均值 (Mean): {ideal_acc_mean:.2f}, 标准差: {ideal_acc_std:.2f}")
        
        # 绘制记录数量和唯一路径数量的折线图
        time_ticks = [start_time + i * step for i in range(len(results))]
        plt.figure(figsize=(12, 6))
        #plt.plot(time_ticks, results, linestyle='-', label='Total Path', color='blue')
        plt.plot(time_ticks, unique_paths_count, linestyle='-', label='Unique Path', color='orange')
        plt.plot(time_ticks, ideal_acc_num, linestyle='-', label='Ideal Acc Path', color='red')
        #plt.axhline(total_path_mean, color='blue', linestyle='--', label=f'Total Path Mean: {total_path_mean:.2f}')
        plt.axhline(unique_path_mean, color='orange', linestyle='--', label=f'Unique Path Mean: {unique_path_mean:.2f}')
        plt.axhline(ideal_acc_mean, color='red', linestyle='--', label=f'Ideal Acc Path Mean: {ideal_acc_mean:.2f}')
        plt.title('Received Path (Total and Unique)')
        plt.xlabel('Time')
        plt.ylabel('Path Count')
        plt.legend()
        plt.grid()
        plt.savefig('received_path.png')
        
        # 绘制记录数量分布直方图
        distribution = Counter(results)
        x = list(distribution.keys())  # 不同数值
        y = list(distribution.values())  # 出现次数
        plt.figure(figsize=(10, 5))
        plt.bar(x, y, color='skyblue', edgecolor='black')
        plt.title('Distribution of Received Paths Count')
        plt.xlabel('Received Paths Count')
        plt.ylabel('Frequency')
        plt.grid(axis='y', linestyle='--', alpha=0.7)
        plt.savefig('received_pkt_distribution.png')
        
        # 绘制非重复路径数量分布直方图
        unique_distribution = Counter(unique_paths_count)
        x_unique = list(unique_distribution.keys())  # 不同非重复路径数值
        y_unique = list(unique_distribution.values())  # 出现次数
        plt.figure(figsize=(10, 5))
        plt.bar(x_unique, y_unique, color='lightgreen', edgecolor='black')
        plt.title('Distribution of Unique Paths Count')
        plt.xlabel('Unique Paths Count')
        plt.ylabel('Frequency')
        plt.grid(axis='y', linestyle='--', alpha=0.7)
        plt.savefig('unique_paths_distribution.png')

        ideal_distribution = Counter(ideal_acc_num)
        x_ideal = list(ideal_distribution.keys())  # 不同非重复路径数值
        y_ideal = list(ideal_distribution.values())  # 出现次数
        plt.figure(figsize=(10, 5))
        plt.bar(x_ideal, y_ideal, color='lightgreen', edgecolor='black')
        plt.title('Distribution of ideal acc Paths Count')
        plt.xlabel('ideal acc Paths Count')
        plt.ylabel('Frequency')
        plt.grid(axis='y', linestyle='--', alpha=0.7)
        plt.savefig('ideal_acc_paths_distribution.png')
        
        #print("Received Paths Distribution:", distribution)
        print("Unique Paths Distribution:", unique_distribution)

    def query_ideal_acceptable_path_num(self, start_time, end_time, src, dst, ce_threshold):
        if len(self.ideal_path_ce) == 0:
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
        分析Caver路径选择信息。
        - 解析路径选择相关日志文件。
        - 生成路径选择信息的统计数据，例如是否有未使用路径、FCT减速比等。
        """
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
        """
        计算所有流的平均FCT（Flow Completion Time）减速比。
        - 如果流数据未加载，则先从文件读取流信息。
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        return np.average([f.fct_slowdown for f in self.flows_after2005])

    def get_p99_fct_slowdown(self):
        """
        计算所有流的P99（99th Percentile）FCT减速比。
        - 如果流数据未加载，则先从文件读取流信息。
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        return np.percentile([f.fct_slowdown for f in self.flows_after2005], 99)
    
    def get_small_flow_fct_slowdown(self, threshold=20 * 1024):
        """
        计算小流（m_size < 20 KB）的平均FCT减速比。
        - 如果流数据未加载，则先从文件读取流信息。
        - 如果没有符合条件的流，返回None。
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        # 筛选 m_size < 100 * 1024 的流
        small_flows = [f for f in self.flows_after2005 if f.m_size < threshold]
        if len(small_flows) == 0:
            return float('inf')  # 如果没有符合条件的流，返回 None
        return sum(f.fct_slowdown for f in small_flows) / len(small_flows)

    def get_large_flow_fct_slowdown(self, threshold = 100 * 1024):
        """
        计算大流（m_size > 100 KB）的平均FCT减速比。
        - 如果流数据未加载，则先从文件读取流信息。
        - 如果没有符合条件的流，返回None。
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        # 筛选 m_size > 10 * 1024 * 1024 的流
        large_flows = [f for f in self.flows_after2005 if f.m_size > threshold]
        if len(large_flows) == 0:
            return float('inf')  # 如果没有符合条件的流，返回 None
        return sum(f.fct_slowdown for f in large_flows) / len(large_flows)
    


    def plot_fct_slowdown(self):
        """
        绘制流的FCT减速比图表。
        - 绘制折线图展示每个流的FCT减速比。
        - 绘制直方图展示FCT减速比的分布。
        - 将图表保存到文件中。
        """
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
        """
        绘制PFC事件随时间的分布直方图。
        - 如果PFC事件未加载，则从文件中读取。
        - 将图表保存到文件中。
        """
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
        """
        分析FCT减速比超过指定阈值的长流。
        - 筛选符合条件的流，并打印其详细信息和流的轨迹。
        - 最多分析max_num个流。
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
        分析FCT减速比超过指定阈值的长流。
        - 筛选符合条件的流，并打印其详细信息和流的轨迹。
        - 最多分析max_num个流。
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
        解析路径选择相关的配置信息日志文件。
        - 提取每个流的路径选择信息，包括可用路径数量、是否有未使用路径、路径详情等。
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
        根据包的元组信息查询流ID。
        - 如果映射未加载，则从文件中读取。
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
        解析流轨迹文件，记录每个流在每个时间点的源节点和目标节点信息。
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
        打印指定流ID的轨迹信息。
        - 按时间和节点顺序排序后打印。
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
        从文件中读取流信息。
        - 提取流的源节点、目标节点、流量大小、FCT等信息。
        - 建立流ID与流对象的映射。
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
        从文件中读取PFC（Priority Flow Control）事件信息。
        - 提取每个事件的时间戳、节点ID、接口索引等信息。
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
        print(f'{len(self.pfc_events)}条PFC事件已读取')

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
    # 定义函数来处理每个analyser的任务
    
    # 创建进程池
    with multiprocessing.Pool(processes=multiprocessing.cpu_count()) as pool:
        # 使用pool.starmap并行处理所有analyser任务
        results = pool.starmap(process_analyser, [(analyser, small_flow_threshold, large_flow_threshold) for analyser in analyser_iter(config_ids_str)])
    
    # 将结果转换为DataFrame，并按id排序
    df = pd.DataFrame(results, columns=['id', 'lb', 'avg', 'p99', 'small', 'large']).round(2)
    df_sorted = df.sort_values(by='id')
    
    # 输出排序后的DataFrame
    return df_sorted


#['o', 's', '^', 'd', '*', 'x', '+', 'p', 'h']
def plot_overall_fctslowdown(config_ids_str):

    # Data preparation
    avg_data = defaultdict(lambda: {})
    small_flow_data = defaultdict(lambda: {})
    large_flow_data = defaultdict(lambda: {})
    p99_flow_data = defaultdict(lambda: {})

    plot_data = defaultdict(dict)#格式：lb_mode->(load->fct)
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
        print(f'已保存到 {filepath}')

    # 绘制图表
    plot_data(avg_data, "Avg. FCT Slowdown", "overall_fct_slowdown.pdf")
    plot_data(small_flow_data, "Avg. FCT Slowdown", "small_flow_fct_slowdown.pdf")
    plot_data(large_flow_data, "Avg. FCT Slowdown", "large_flow_fct_slowdown.pdf")
    plot_data(p99_flow_data, "p99. FCT Slowdown", "p99_fct_slowdown.pdf")

def plot_data(y_values:np.array, xticks, line_names, psave_path, line_colors=None, line_markers=None):
    x = [1, 2, 3, 4, 5]
    custom_xticks = ["5", "10", "20", "40", "100"]
    # Line properties
    colors = ["red", "blue", "green", "orange", "purple"]
    line_widths = [3.5, 3.5, 3.5, 3.5, 3.5]
    markers = ["o", "s", "^", "D", "*"]
    marker_sizes = [9, 9, 9, 9, 9]

    # Plot the lines
    plt.figure(figsize=(6, 4), dpi=300)
    for i, y in enumerate(y_values):
        plt.plot(
            x,
            y,
            label=line_names[i],
            color=colors[i],
            linewidth=line_widths[i],
            marker=markers[i],
            markersize=marker_sizes[i],
        )

    # Customize x-axis
    plt.xticks(ticks=x, labels=custom_xticks, fontsize=18)

    # Customize y-axis
    plt.yticks(fontsize=18)
    plt.xlabel("Concurency rate", fontsize=22)
    plt.ylabel("Avg FCT Slowdown", fontsize=22)
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
        columnspacing=0.5,  # 调整列之间的间距
        handletextpad=0.2   # 调整图例标记与文字之间的间距
    )
    # Save the plot as PNG and PDF
    plt.savefig("concurrency.png", format="png", bbox_inches="tight")
    plt.savefig("concurrency.pdf", format="pdf", bbox_inches="tight")

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
    ax.set_ylim(0, 15)
    ax.set_xlim(0.7, len(y_values)+0.1)

    # Add legend

    plt.legend(
        frameon=False, 
        fontsize=20, 
        loc='upper center', 
        bbox_to_anchor=(0.5, 1.3), 
        ncol=3, 
        labelspacing=0.01, 
        columnspacing=0.5,  # 调整列之间的间距
        handletextpad=0.2   # 调整图例标记与文字之间的间距
    )
    # Save the plot as PNG and PDF
    #plt.savefig("ack80.png", format="png", bbox_inches="tight")
    plt.savefig("ack80.pdf", format="pdf", bbox_inches="tight")

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
    plt.xlabel("Flow Number", fontsize=22)
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
        columnspacing=0.5,  # 调整列之间的间距
        handletextpad=0.2   # 调整图例标记与文字之间的间距
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
    plt.xlabel("Concurrency rate", fontsize=22)
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
        columnspacing=0.5,  # 调整列之间的间距
        handletextpad=0.2   # 调整图例标记与文字之间的间距
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

#分析随机流量数据plot_overall_fctslowdown("434-451,488-499")
#分析bond随机流量数据plot_overall_fctslowdown("452-469,512-523")
#plot_overall_fctslowdown('765-794')叶脊
if __name__ == "__main__":
    #plot_ack_data('1671,1673,1675,1677,1680,1681', [1,2,4,10,20,40])
    #plot_ack_data('1672,1674,1676,1678,1679,1682', [1,2,4,10,20,40])
    #plot_incast_data('1603-1627')
    
    #plot_all_to_all_data('1628-1647, 1803-1807')
    #plot_overall_fctslowdown('1683-1707') #fat k4
    
    #plot_overall_fctslowdown('1733-1757') #leaf spine
    #plot_overall_fctslowdown('1683-1707') #bond
    #plot_overall_fctslowdown('1818-1842')

    plot_incast_data('1900-1924')


    
    pass