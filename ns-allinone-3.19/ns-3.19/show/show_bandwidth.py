import matplotlib.pyplot as plt
import os

def get_flow_rate_info(id, type):
    file_path = f"mix/output/{id}/{id}_in_flow.txt"
    time_list = []
    info_dict = {}
    bytes_current = {}
    current_time = 2000000000 - 1
    with open(file_path, 'r') as file:
        for line in file:
            numbers = list(map(int, line.split(",")))
            if len(numbers) < 3:
                break
            flow_id = numbers[1]
            time = numbers[0]
            if current_time != time:
                time_list.append(time - 2000000000)
                current_time = time
                for key in info_dict:
                    if len(info_dict[key]) < len(time_list):
                        info_dict[key].append(0)
            if flow_id not in info_dict:
                info_dict[flow_id] = [0] * len(time_list)
            if flow_id in bytes_current:
                rate = (numbers[2] - bytes_current[flow_id]) * 8 / (0.00001 * 1000 * 1000 * 1000)
                info_dict[flow_id].append(rate)
                bytes_current[flow_id] = numbers[2]
            else:
                bytes_current[flow_id] = numbers[2]
                rate = (numbers[2]) * 8 / (0.00001 * 1000 * 1000 * 1000)
                info_dict[flow_id].append(rate)
        for key in info_dict:
            if len(info_dict[key]) < len(time_list):
                info_dict[key].append(0)
    
    # 确保所有流的速率列表长度与 time_list 一致
    for key in info_dict:
        while len(info_dict[key]) < len(time_list):
            info_dict[key].append(0)
        while len(info_dict[key]) > len(time_list):
            info_dict[key].pop()
    
    return time_list, info_dict
def plot_flow_rate(time_list, info_list, flows, title, filename):
    plt.figure(figsize=(10, 6))
    for flow in flows:
        rates = info_list[flow]
        plt.plot([t / 1000 for t in time_list], rates, label=f"Flow {flow}")
    plt.xlabel('Time (us)')
    plt.ylabel('Rate (Gbps)')
    plt.title(title)
    plt.legend()
    plt.grid(True)
    plt.savefig(filename)
    plt.close()
    
    

def get_bandwidth_info(id, type):
    if (type == 0):
        type_str = "out_uplink"
    elif (type == 1):
        type_str = "out_downlink"
    elif (type == 2):
        type_str = "out_uplink_rx"
    elif (type == 3):
        type_str = "out_downlink_rx"
    elif (type == 4):
        type_str = "out_all_link_tx"
    file_path = f"mix/output/{id}/{id}_{type_str}.txt"
    time_list = []
    info_dict = {}
    bytes_current = {}
    current_time = 2000000000 - 1
    with open(file_path, 'r') as file:
        for line in file:
            numbers = list(map(int, line.split(",")))
            link = (numbers[1], numbers[2])
            time = numbers[0]
            if current_time != time:
                time_list.append(time - 2000000000)
                current_time = time
            if link not in info_dict:
                info_dict[link] = []
            if link in bytes_current:
                rate = (numbers[3] - bytes_current[link]) * 8 / (0.00001 * 1000 * 1000 * 1000)
                info_dict[link].append(rate)
                bytes_current[link] = numbers[3]
            else:
                bytes_current[link] = numbers[3]
                rate = (numbers[3]) * 8 / (0.00001 * 1000 * 1000 * 1000)
                info_dict[link].append(rate)
    return time_list, info_dict


def plot_bandwidth(time_list, info_list, links, title, filename):
    plt.figure(figsize=(10, 6))
    for link in links:
        rates = info_list[link]
        plt.plot([t / 1000 for t in time_list], rates, label=f"Link {link}")
    plt.xlabel('Time (us)')
    plt.ylabel('Rate (Gbps)')
    plt.title(title)
    plt.legend()
    plt.grid(True)
    plt.savefig(filename)
    plt.close()

id = 660450198
links = [(32, 41), (41, 51), (51, 47), (47, 39)] 
flows = [0, 1]
up_time_list, info_list = get_bandwidth_info(id, 4)
# 调用get_flow_rate_info函数
flow_up_time_list, flow_info_list = get_flow_rate_info(id, 0)

# print(info_list.keys())
if not os.path.exists(f"motivation/output/{id}"):
    os.makedirs(f"motivation/output/{id}")
plot_bandwidth(up_time_list, info_list, links, "Uplink Bandwidth Over Time", f"motivation/output/{id}/{id}_link_bandwidth.png")
# 调用plot_flow_rate函数
plot_flow_rate(flow_up_time_list, flow_info_list, flows, "Flow Rate Over Time", f"motivation/output/{id}/{id}_flow_rate.png")

