# -*- coding: utf-8 -*-
import re
import sys

running_id = sys.argv[1]
file_path = "./mix/output/{}/config.log".format(running_id)


pattern = r"^flow_passed: switch_id (\d+) flow_id (\d+)$"

# 存储符合条件的行和提取的值
results = []

# 打开文件读取
with open(file_path, "r", encoding="utf-8") as file:
    for line in file:
        match = re.match(pattern, line.strip())
        if match:
            # 提取 switch_id 和 flow_id
            switch_id = int(match.group(1))
            flow_id = int(match.group(2))
            # 存储结果
            results.append((line.strip(), switch_id, flow_id))
flow_path_dict = {}
# 输出符合条件的行及提取的值
for line, switch_id, flow_id in results:
    if flow_id not in flow_path_dict:
        flow_path_dict[flow_id] = []
    else:
        if switch_id not in flow_path_dict[flow_id]:
            flow_path_dict[flow_id].append(switch_id)
with open("./mix/output/{}/flow_paths.txt".format(running_id), "w") as output_file:
    for f in flow_path_dict:
        output_file.write("Flow ID: {}, Path: {}\n".format(f, flow_path_dict[f]))