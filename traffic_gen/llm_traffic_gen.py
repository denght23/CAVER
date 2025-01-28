import random

class Flow:
    def __init__(self, src, dst, size, t):
        self.src, self.dst, self.size, self.t = src, dst, size, t
    def __str__(self):
        return "%d %d 3 %d %.9f" % (self.src, self.dst, self.size, self.t)

host_num = 128                  # host编号为0-255
dp_num = 8                      # 多少个host被分在一个dp组内, 0-7为一组，8-15为1组...
pp_num = host_num // dp_num     # 模型的层数

dp_flow_size = 12 * 1e6         # dp流一次发送的流大小
pp_flow_size = 2 * 1e6          # pp每条流的大小

pp_forward_interval = 1e-3      # pp前向传播的间隔
pp_backward_interval = 1.6e-3     # pp反向传播的间隔
pp_flow_num = 4
dp_flow_num = 4

dp_interval = 5e-3              # dp传播的间隔
minibatch_num = 16               # 被切分为多少minibatch

size_variance = 0.05 * 1e6      # 流大小的正态分布方差
time_variance = 1e-5          # 时间的正态分布方差

cur_time = 2.005
dp_groups = [list(range(i, i + dp_num)) for i in range(0, host_num, dp_num)]
flows: list = []
print('层划分:', dp_groups)

# 模拟前向传播
for i in range(minibatch_num + pp_num - 1 - 1):
    print(f'\n{i+1}次前向传播, Time:{cur_time:.4f}')
    for layer in range(max(0, i - minibatch_num + 1), min(pp_num - 1, i + 1)):
        print(f'Layer{layer}->Layer{layer+1}', end=',')
        for layer1_host, layer2_host in zip(dp_groups[layer], dp_groups[layer + 1]):
            for _ in range(pp_flow_num):
                # 引入随机差异
                size = max(1, int(random.normalvariate(pp_flow_size, size_variance)))
                t = max(0, cur_time + random.normalvariate(0, time_variance))
                flows.append(Flow(layer1_host, layer2_host, size, t))
    cur_time += pp_forward_interval

# 模拟反向传播
for i in range(1, minibatch_num + pp_num - 1):
    print(f'\n{i}次反向传播, Time:{cur_time:.4f}')
    for layer in range(min(pp_num - 1, pp_num - 1 - i + minibatch_num), max(0, pp_num - 1 - i), -1):
        print(f'Layer{layer}->Layer{layer-1}', end=',')
        for layer2_host, layer1_host in zip(dp_groups[layer], dp_groups[layer - 1]):
            for _ in range(pp_flow_num):
                # 引入随机差异
                size = max(1, int(random.normalvariate(pp_flow_size, size_variance)))
                t = max(0, cur_time + random.normalvariate(0, time_variance))
                flows.append(Flow(layer2_host, layer1_host, size, t))
    cur_time += pp_backward_interval

# 模拟dp通信
for step in range(2 * (dp_num - 1)):
    print(f'\n\n===============第{step+1}次dp, Time:{cur_time:.4f}')
    for layer in range(pp_num):  # 每层内需要 2*(dp_num-1) 次通信
        print(f'\nLayer{layer}内环形dp')
        for src_idx in range(dp_num):
            dst_idx = (src_idx + 1) % dp_num  # 环形传播：src -> dst
            print(f'Host{dp_groups[layer][src_idx]}->Host{dp_groups[layer][dst_idx]}', end=',')
            for _ in range(dp_flow_num):
                # 引入随机差异
                size = max(1, int(random.normalvariate(dp_flow_size, size_variance)))
                t = max(0, cur_time + random.normalvariate(0, time_variance))
                flows.append(Flow(dp_groups[layer][src_idx], dp_groups[layer][dst_idx], size, t))
    cur_time += dp_interval  # 每次传播增加 dp_interval 时间

import random
import math
import heapq
import numpy as np
from custom_rand import CustomRand

def translate_bandwidth(b):
    if b is None:
        return None
    if type(b) != str:
        return None
    if b[-1] == 'G':
        return float(b[:-1]) * 1e9
    if b[-1] == 'M':
        return float(b[:-1]) * 1e6
    if b[-1] == 'K':
        return float(b[:-1]) * 1e3
    return float(b)


def poisson(lam):
    return -math.log(1 - random.random()) * lam


def generate_flows(cdf_file="AliStorage2019.txt", nhost=128, load=0.2, bandwidth="100G", time=10):
    """
    生成随机流量列表
    :param cdf_file: cdf 文件路径 (默认值: "Solar2022.txt")
    :param nhost: 主机数量 (默认值: 100)
    :param load: 流量负载百分比 (默认值: 0.3)
    :param bandwidth: 主机链路带宽 (默认值: "10G")
    :param time: 运行时间（秒）(默认值: 10)
    :return: 流量对象列表
    """
    base_t = 2000000000  # 基础时间戳
    bandwidth = translate_bandwidth(bandwidth)  # 转换带宽格式
    time_ns = time * 1e9  # 转换为纳秒

    if bandwidth is None:
        raise ValueError("Bandwidth format incorrect")

    # 读取 CDF 文件
    with open(cdf_file, "r") as file:
        lines = file.readlines()
    cdf = []
    for line in lines:
        x, y = map(float, line.strip().split(' '))
        cdf.append([x, y])

    # 创建自定义随机生成器
    customRand = CustomRand()
    if not customRand.setCdf(cdf):
        raise ValueError("Error: Not valid cdf")

    avg = customRand.getAvg()  # 平均流大小
    avg_inter_arrival = 1 / (bandwidth * load / 8. / avg) * 1e9  # 平均流间到达时间 (ns)

    # 初始化主机的事件队列
    host_list = [(base_t + int(poisson(avg_inter_arrival)), i) for i in range(nhost)]  # (时间, 主机ID)
    heapq.heapify(host_list)

    flows = []  # 存储流量对象

    while len(host_list) > 0:
        t, src = host_list[0]
        inter_t = int(poisson(avg_inter_arrival))
        dst = random.randint(0, nhost - 1)
        while dst == src:
            dst = random.randint(0, nhost - 1)
        if t + inter_t > time_ns + base_t:
            heapq.heappop(host_list)
        else:
            size = int(customRand.rand())
            if size <= 0:
                size = 1
            flows.append(Flow(src, dst, size, t * 1e-9))
            heapq.heapreplace(host_list, (t + inter_t, src))

    return flows

flows.extend(generate_flows(nhost=host_num, time=flows[-1].t-2, load=0.3))
# 打印生成的流信息
with open('/home/zj/ns-allinone-3.19/ns-3.19/config/llm_flow2.txt', 'w') as f:
    f.write(f'{len(flows)}\n')
    for flow in sorted(flows, key=lambda x : x.t):
        f.write(f'{flow}\n')