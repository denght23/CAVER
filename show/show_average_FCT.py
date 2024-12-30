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



time_start = int(2.000 * 1e9)  # 起始时间（纳秒）
time_end = int(2.003 * 1e9)      # 结束时间（纳秒）
name_list = ["Caver w/o Information sharing", "Caver w/o acceptable path", "Caver"] # noshare, dv, caver
load_list = ["40%", "60%", "80%"]
load_id_lists = [
    [856436130, 269122553, 414208792],  # 40% 负载
    [100313750, 317335163, 518352743],  # 60% 负载
    [410532519, 279368860, 656727120]   # 80% 负载
]

# 初始化存储所有负载下的平均 slowdown 的列表
all_avg_slowdowns = []

# 计算每个负载下的 slowdown 平均值
for load_ids in load_id_lists:
    avg_slowdowns = []
    for id in load_ids:
        file_path = f"../mix/output/{id}/{id}_out_fct.txt"
        avg_slowdown, slowdowns = read_fct_slowdowns(file_path, time_start, time_end)
        avg_slowdowns.append(avg_slowdown)
    all_avg_slowdowns.append(avg_slowdowns)

# 画多组柱状图
x = range(len(load_list))
total_width = 0.8
n = len(name_list)
width = total_width / n

plt.figure(figsize=(12, 8))

width = 0.15  # Reduce width to make the bars thinner
spacing = 0.02  # Add spacing between groups

colors = ['#EDB679', '#BAE4C5', '#660874']
hatches = ['x', '/', '']  # First pattern: crosshatch, second: diagonal lines, third: no fill pattern

for i in range(n):
    plt.bar([p + i * (width + spacing) for p in x], 
            [avg_slowdowns[i] for avg_slowdowns in all_avg_slowdowns], 
            width=width, label=name_list[i],
            color=colors[i], hatch=hatches[i])

plt.xlabel('Load', fontsize=28)
plt.ylabel('Avg. FCT Slowdown', fontsize=28)
# plt.title('Average FCT Slowdown for Different Loads and Configurations')
plt.xticks([p + (n - 1) * (width + spacing) / 2 for p in x], load_list, fontsize=28)
plt.yticks(fontsize=28)  # Adjust y-axis font size
plt.legend(fontsize=28, frameon=False, loc='upper center', bbox_to_anchor=(0.4, 1.2))  # Adjust legend position

# Remove right and top borders
ax = plt.gca()
ax.spines['right'].set_visible(False)
ax.spines['top'].set_visible(False)

# Add horizontal dashed reference lines
for y in range(1, 5):
    plt.axhline(y, color='gray', linestyle='--', linewidth=0.5)

plt.show()

# 添加选项，选择保存为 PNG 还是 PDF
save_as_pdf = False  # 如果为 True，则保存为 PDF，否则保存为 PNG

if save_as_pdf:
    plt.savefig('average_slowdown_by_load.pdf')
else:
    plt.savefig('average_slowdown_by_load.png')