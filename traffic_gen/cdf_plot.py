import matplotlib.pyplot as plt
import numpy as np

# 读取文件内容并解析数据
def read_data(file_path):
    x = []
    y = []
    with open(file_path, 'r') as f:
        for line in f:
            # 跳过空行
            if line.strip():
                columns = line.split()
                x.append(float(columns[0]))
                y.append(float(columns[1])*0.01)
    return np.array(x), np.array(y)

plt.figure(figsize=(10,4))
# 绘制第一个文件的CDF图
file_path_1 = '/home/zj/ns-allinone-3.19/ns-3.19/traffic_gen/FbHdp2015.txt'
x1, y1 = read_data(file_path_1)
plt.plot(x1, y1, linestyle='-', label='FbHdp2015', linewidth=3, color='green')  # 设置线型和标签

# 绘制第二个文件的CDF图
file_path_2 = '/home/zj/ns-allinone-3.19/ns-3.19/traffic_gen/Solar2022.txt'
x2, y2 = read_data(file_path_2)
plt.plot(x2, y2, linestyle='--', label='Solar2022', linewidth=3, color='orange')  # 设置线型和标签

# 绘制第三个文件的CDF图
file_path_3 = '/home/zj/ns-allinone-3.19/ns-3.19/traffic_gen/AliStorage2019.txt'
x3, y3 = read_data(file_path_3)
plt.plot(x3, y3, linestyle='-.', label='AliStorage2019', linewidth=3, color=(102/255, 8/255, 116/255))  # 设置线型和标签

# 绘制第四个文件的CDF图
#file_path_4 = '/home/zj/ns-allinone-3.19/ns-3.19/traffic_gen/GoogleRPC2008.txt'
#x4, y4 = read_data(file_path_4)
#plt.plot(x4, y4, linestyle=':', label='GoogleRPC2008', linewidth=3, color=(210/255, 0, 0))  # 设置线型和标签

plt.xscale('log')
plt.xlim(10, 1e7)
plt.ylim(-0.01, 1)

plt.xticks(fontsize=20)
plt.yticks(fontsize=20)
plt.xlabel('Flow Size(Bytes)', fontsize=22)
plt.ylabel('CDF', fontsize=22)
ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)
plt.grid(True)

handles, labels = plt.gca().get_legend_handles_labels()
sorted_labels, sorted_handles = zip(*sorted(zip(labels, handles), key=lambda x: x[0][-4:]))

# 绘制图例
plt.legend(sorted_handles, sorted_labels, fontsize=20,
    facecolor='white', # 背景色
    framealpha=0,       # 设置透明度
    borderaxespad=0.5,    # 图例与坐标轴的间距
    fancybox=True,      # 使用圆角边框
    loc='upper center', # 图例位置：顶部居中
    bbox_to_anchor=(0.5, 1.35),#调整图例的垂直位置，向上偏移
    ncol=3             # 如果有多个标签，设置为多列显示
    )
plt.savefig('/home/zj/ns-allinone-3.19/ns-3.19/traffic_gen/cdf.pdf', format='pdf', bbox_inches='tight')
