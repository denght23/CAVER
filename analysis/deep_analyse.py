#!/usr/bin/python3
import subprocess
import matplotlib.pyplot as plt
import os.path as op
import re
import sys
from dataclasses import dataclass, field
from collections import Counter, defaultdict
import numpy as np
import pandas as pd
from typing import Union, List
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
        print(f'开始分析{id}号实验数据')
        self.base_dir, self.lb_mode, self.load = get_info_by_id(id)
        self.packetId2FlowId = {}
        self.flow_trace:defaultdict[int, list[tuple]] = defaultdict(list)
        self.flows: list[Flow] = []
        self.id_to_flow: map[int, Flow] = {}
        self.pfc_events: list[PfcEvent] = []
        self.path_choice_infos:list[PathChoiceInfo] = []

    def plot_caver_received_path(self, window_size=180 * 1000, step=10):
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
        start_time = times[0]  # 最小时间戳
        end_time = times[-1]  # 最大时间戳
        n = len(times)
        left, right = 0, 0  # 初始化双指针
        current_start = start_time
        
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
            
            current_start += step
        
        # 计算统计量
        total_path_mean = np.mean(results)
        total_path_std = np.std(results)
        unique_path_mean = np.mean(unique_paths_count)
        unique_path_std = np.std(unique_paths_count)
        
        # 打印统计信息
        print(f"Total Path - 平均值 (Mean): {total_path_mean:.2f}, 标准差: {total_path_std:.2f}")
        print(f"Unique Path - 平均值 (Mean): {unique_path_mean:.2f}, 标准差: {unique_path_std:.2f}")
        
        # 绘制记录数量和唯一路径数量的折线图
        time_ticks = [start_time + i * step for i in range(len(results))]
        plt.figure(figsize=(12, 6))
        plt.plot(time_ticks, results, linestyle='-', label='Total Path', color='blue')
        plt.plot(time_ticks, unique_paths_count, linestyle='-', label='Unique Path', color='orange')
        plt.axhline(total_path_mean, color='blue', linestyle='--', label=f'Total Path Mean: {total_path_mean:.2f}')
        plt.axhline(unique_path_mean, color='orange', linestyle='--', label=f'Unique Path Mean: {unique_path_mean:.2f}')
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
        
        print("Received Paths Distribution:", distribution)
        print("Unique Paths Distribution:", unique_distribution)


    def analyse_caver_choice_info(self):
        """
        分析Caver路径选择信息。
        - 解析路径选择相关日志文件。
        - 生成路径选择信息的统计数据，例如是否有未使用路径、FCT减速比等。
        """
        return
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
        return sum(map(lambda f: f.fct_slowdown, self.flows)) / len(self.flows)

    def get_p99_fct_slowdown(self):
        """
        计算所有流的P99（99th Percentile）FCT减速比。
        - 如果流数据未加载，则先从文件读取流信息。
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        return np.percentile([f.fct_slowdown for f in self.flows], 99)
    
    def get_small_flow_fct_slowdown(self):
        """
        计算小流（m_size < 20 KB）的平均FCT减速比。
        - 如果流数据未加载，则先从文件读取流信息。
        - 如果没有符合条件的流，返回None。
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        # 筛选 m_size < 100 * 1024 的流
        small_flows = [f for f in self.flows if f.m_size < 20 * 1024]
        if len(small_flows) == 0:
            return None  # 如果没有符合条件的流，返回 None
        return sum(f.fct_slowdown for f in small_flows) / len(small_flows)

    def get_large_flow_fct_slowdown(self):
        """
        计算大流（m_size > 100 KB）的平均FCT减速比。
        - 如果流数据未加载，则先从文件读取流信息。
        - 如果没有符合条件的流，返回None。
        """
        if len(self.flows) == 0:
            self._read_flows_from_file()
        # 筛选 m_size > 10 * 1024 * 1024 的流
        large_flows = [f for f in self.flows if f.m_size > 100 * 1024]
        if len(large_flows) == 0:
            return None  # 如果没有符合条件的流，返回 None
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
        analyser.analyse_caver_choice_info()


#['o', 's', '^', 'd', '*', 'x', '+', 'p', 'h']
def plot_overall_fctslowdown(config_ids_str):

    # Data preparation
    data = defaultdict(lambda: {})
    small_flow_data = defaultdict(lambda: {})
    large_flow_data = defaultdict(lambda: {})
    p99_flow_data = defaultdict(lambda: {})

    for config_id in get_config_id(config_ids_str):
        analyser = Analyser(config_id)

        # 最新数据覆盖旧数据
        avg_slowdown = analyser.get_avg_fct_slowdown()
        data[analyser.lb_mode][analyser.load] = avg_slowdown

        small_avg_slowdown = analyser.get_small_flow_fct_slowdown()
        if small_avg_slowdown is not None:
            small_flow_data[analyser.lb_mode][analyser.load] = small_avg_slowdown

        large_avg_slowdown = analyser.get_large_flow_fct_slowdown()
        if large_avg_slowdown is not None:
            large_flow_data[analyser.lb_mode][analyser.load] = large_avg_slowdown

        p99_slowdown = analyser.get_p99_fct_slowdown()
        p99_flow_data[analyser.lb_mode][analyser.load] = p99_slowdown

    # 移除 'dv' 数据（如果存在）
    data.pop('dv', None)
    small_flow_data.pop('dv', None)
    large_flow_data.pop('dv', None)
    p99_flow_data.pop('dv', None)

    # Plotting helper function
    def plot_data(data, title, ylabel, filename):
        plt.figure(figsize=(6, 4), dpi=300)
        y_max = 0
        for lb_mode, loads_data in data.items():
            loads = sorted(loads_data.keys())
            avg_slowdowns = [loads_data[load] for load in loads]
            plt.plot(loads, avg_slowdowns, label=lb_mode_upper[lb_mode], linewidth=3.5,
                     linestyle=linestyles[lb_mode], color=colors[lb_mode])
            y_max = max([y_max, max(avg_slowdowns)])

        plt.xticks([40, 50, 60, 70, 80], fontsize=18)
        y_max = min(60, y_max)

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
    plot_data(data, "Overall Avg FCT Slowdown vs Load", "Avg FCT Slowdown", "overall_fct_slowdown.png")
    plot_data(small_flow_data, "Small Flow Avg FCT Slowdown vs Load", "Avg FCT Slowdown", "small_flow_fct_slowdown.png")
    plot_data(large_flow_data, "Large Flow Avg FCT Slowdown vs Load", "Avg FCT Slowdown", "large_flow_fct_slowdown.png")
    plot_data(p99_flow_data, "P99 FCT Slowdown vs Load", "P99 FCT Slowdown", "p99_fct_slowdown.png")


def show_caver_setting(config_id_str):
    for id in get_config_id(config_id_str):
        config_id_path = op.join(get_info_by_id(id)[0], 'config.log')
        print(f'================{id}==============')
        os.system(f'cat {config_id_path} | grep caver | tail')

#分析随机流量数据plot_overall_fctslowdown("434-451,488-499")
#分析bond随机流量数据plot_overall_fctslowdown("452-469,512-523")
#plot_overall_fctslowdown('765-794')叶脊
if __name__ == "__main__":
    #get_avg_fct('827-832')
    Analyser(840).plot_caver_received_path()
    #plot_overall_fctslowdown('694-723')
    #plot_overall_fctslowdown('608-611,614-617,620-623,626-629,632-635,638-642')
    #plot_overall_fctslowdown("452-469,512-523")
    #"555,558,560-569" 选路数和阈值对参数的影响
    #get_avg_fct("603-607")
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
    pass