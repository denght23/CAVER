import ast  # 用于解析字符串为 Python 数据结构
import matplotlib.pyplot as plt
import os
def loadSnapshotsFromFile(filename):
    snapshots = []
    with open(filename, 'r') as file:
        for line in file:
            # 将行解析为列表
            snapshot = ast.literal_eval(line.strip())
            ce_map = {(entry[0], entry[1]): entry[2] for entry in snapshot}
            snapshots.append(ce_map)
    return snapshots

def plotValuesOverTime(snapshots, keys, output_path):
    time_points = [i * 20 for i in range(len(snapshots))]  # 每个数据点之间的间隔为20us
    plt.figure()
    for key in keys:
        values = [snapshot.get(key, 0) for snapshot in snapshots]
        plt.plot(time_points, values, label=f"Key {key}")
    plt.xlabel("Time (us)")
    plt.ylabel("Value")
    plt.title("Values Over Time")
    plt.legend()
    plt.grid(True)
    plt.savefig(output_path)
    plt.close()

def main():
    #修改这里，来决定实验以及要绘制的链路
    id = 444565463
    keys_to_plot = [(32, 41), (41, 51), (51, 47), (47, 39)]  # Replace with the keys you want to plot
    
    
    file_name = "{}{}".format(str(id), "_global_ce_map.txt")
    file_path = os.path.join("mix", "output", str(id), file_name)
    output_path = os.path.join("motivation", "output", "{}".format(str(id)), "{}_global_ce".format(str(id)))
    if not os.path.exists(f"motivation/output/{id}"):
        os.makedirs(f"motivation/output/{id}")

    snapshots = loadSnapshotsFromFile(file_path)
    plotValuesOverTime(snapshots, keys_to_plot, output_path)

if __name__ == "__main__":
    main()