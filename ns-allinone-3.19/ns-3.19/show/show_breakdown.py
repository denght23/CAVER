import subprocess

def read_fct_slowdowns(file_path, time_start, time_end):
    """
    读取符合时间范围要求的流完成时间 (FCT) slowdown，并计算平均值。

    :param file_path: 文件路径
    :param time_start: 起始时间（纳秒）
    :param time_end: 结束时间（纳秒）
    :return: 平均 slowdown 和所有 slowdown 的列表
    """
    slowdown_data = []

    try:
        with open(file_path, 'r') as file:
            for line in file:
                fields = line.split()
                if len(fields) < 8:
                    continue

                start_time = int(fields[5])
                duration = int(fields[6])
                ideal_duration = int(fields[7])

                if start_time > time_start and start_time + duration < time_end:
                    slowdown = duration / ideal_duration
                    slowdown_data.append(max(slowdown, 1.0))

        # 计算平均值
        if slowdown_data:
            avg_slowdown = sum(slowdown_data) / len(slowdown_data)
        else:
            avg_slowdown = 0.0  # 如果没有数据，返回 0.0

        return avg_slowdown, slowdown_data
    except IOError as e:
        print("Error reading file:", e)
        return 0.0, []
import matplotlib.pyplot as plt

name_list = ["Caver w/o Information sharing", "Caver w/o acceptable path", "Caver"]
id_list = [826081416, 790046701, 265452215]
avg_slowdowns = []

time_start = int(2.000 * 1e9)  # 起始时间（纳秒）
time_end = int(3.0 * 1e9)      # 结束时间（纳秒）

# 计算每个 id 的 slowdown 平均值
for id in id_list:
    file_path = f"../mix/output/{id}/{id}_out_fct.txt"
    avg_slowdown, slowdowns = read_fct_slowdowns(file_path, time_start, time_end)
    avg_slowdowns.append(avg_slowdown)

# 画柱状图
plt.figure(figsize=(10, 6))
plt.bar(name_list, avg_slowdowns, color=['blue', 'orange', 'green'])
plt.xlabel('Configuration')
plt.ylabel('Average Slowdown')
plt.title('Average FCT Slowdown for Different Configurations')
plt.legend(name_list)
plt.show()
# 保存图像为 PNG 文件
plt.savefig('average_slowdown.png')