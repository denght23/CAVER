import random
import os

class Flow:
    def __init__(self, src, dst, size, t):
        self.src, self.dst, self.size, self.t = src, dst, size, t
    def __str__(self):
        return "%d %d 3 %d %.9f" % (self.src, self.dst, self.size, self.t)

host_num = 128                  # host number from 0-255
dp_num = 8                      # number of hosts per dp group, group 0-7, group 8-15, etc.
pp_num = host_num // dp_num     # number of layers in the model

dp_flow_size = 6 * 1e6         # size of dp flow for each transmission
pp_flow_size = 1 * 1e6          # size of each pp flow

pp_forward_interval = 0.5e-3      # interval for pp forward propagation
pp_backward_interval = 0.8e-3     # interval for pp backward propagation
pp_flow_num = 4
dp_flow_num = 4

dp_interval = 2.5e-3              # interval for dp communication
minibatch_num = 16               # number of minibatches

size_variance = 0.05 * 1e6      # variance for flow size normal distribution
time_variance = 1e-5          # variance for time normal distribution

cur_time = 2.005
dp_groups = [list(range(i, i + dp_num)) for i in range(0, host_num, dp_num)]
flows: list = []
print('Layer division:', dp_groups)

# Simulate forward propagation
for i in range(minibatch_num + pp_num - 1 - 1):
    print(f'\n{i+1} forward propagation, Time:{cur_time:.4f}')
    for layer in range(max(0, i - minibatch_num + 1), min(pp_num - 1, i + 1)):
        print(f'Layer{layer}->Layer{layer+1}', end=',')
        for layer1_host, layer2_host in zip(dp_groups[layer], dp_groups[layer + 1]):
            for _ in range(pp_flow_num):
                # Introducing random variations
                size = max(1, int(random.normalvariate(pp_flow_size, size_variance)))
                t = max(0, cur_time + random.normalvariate(0, time_variance))
                flows.append(Flow(layer1_host, layer2_host, size, t))
    cur_time += pp_forward_interval

# Simulate backward propagation
for i in range(1, minibatch_num + pp_num - 1):
    print(f'\n{i} backward propagation, Time:{cur_time:.4f}')
    for layer in range(min(pp_num - 1, pp_num - 1 - i + minibatch_num), max(0, pp_num - 1 - i), -1):
        print(f'Layer{layer}->Layer{layer-1}', end=',')
        for layer2_host, layer1_host in zip(dp_groups[layer], dp_groups[layer - 1]):
            for _ in range(pp_flow_num):
                # Introducing random variations
                size = max(1, int(random.normalvariate(pp_flow_size, size_variance)))
                t = max(0, cur_time + random.normalvariate(0, time_variance))
                flows.append(Flow(layer2_host, layer1_host, size, t))
    cur_time += pp_backward_interval

# Simulate dp communication
for step in range(2 * (dp_num - 1)):
    print(f'\n\n===============Step {step+1} dp, Time:{cur_time:.4f}')
    for layer in range(pp_num):  # Each layer requires 2*(dp_num-1) dp communications
        print(f'\nLayer{layer} inner ring dp')
        for src_idx in range(dp_num):
            dst_idx = (src_idx + 1) % dp_num  # Circular propagation: src -> dst
            print(f'Host{dp_groups[layer][src_idx]}->Host{dp_groups[layer][dst_idx]}', end=',')
            for _ in range(dp_flow_num):
                # Introducing random variations
                size = max(1, int(random.normalvariate(dp_flow_size, size_variance)))
                t = max(0, cur_time + random.normalvariate(0, time_variance))
                flows.append(Flow(dp_groups[layer][src_idx], dp_groups[layer][dst_idx], size, t))
    cur_time += dp_interval  # Each propagation step increases the time by dp_interval

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
    Generate a list of random flows
    :param cdf_file: path to the cdf file (default: "Solar2022.txt")
    :param nhost: number of hosts (default: 100)
    :param load: traffic load percentage (default: 0.3)
    :param bandwidth: host link bandwidth (default: "100G")
    :param time: running time (in seconds) (default: 10)
    :return: list of flow objects
    """
    base_t = 2000000000  # Base timestamp
    bandwidth = translate_bandwidth(bandwidth)  # Convert bandwidth format
    time_ns = time * 1e9  # Convert to nanoseconds

    if bandwidth is None:
        raise ValueError("Bandwidth format incorrect")

    # Read the CDF file
    with open(cdf_file, "r") as file:
        lines = file.readlines()
    cdf = []
    for line in lines:
        x, y = map(float, line.strip().split(' '))
        cdf.append([x, y])

    # Create custom random generator
    customRand = CustomRand()
    if not customRand.setCdf(cdf):
        raise ValueError("Error: Not valid cdf")

    avg = customRand.getAvg()  # Average flow size
    avg_inter_arrival = 1 / (bandwidth * load / 8. / avg) * 1e9  # Average inter-arrival time (ns)

    # Initialize the event queue for hosts
    host_list = [(base_t + int(poisson(avg_inter_arrival)), i) for i in range(nhost)]  # (time, host_id)
    heapq.heapify(host_list)

    flows = []  # Store flow objects

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

#flows.extend(generate_flows(nhost=host_num, time=flows[-1].t-2, load=0.3))
# Print the generated flow information
with open(os.path.join(os.path.dirname(__file__), '../config/llm_flow.txt'), 'w') as f:
    f.write(f'{len(flows)}\n')
    for flow in sorted(flows, key=lambda x : x.t):
        f.write(f'{flow}\n')
