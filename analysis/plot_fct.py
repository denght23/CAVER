#!/usr/bin/python3

import re
import subprocess
import os
import sys
import argparse
import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.ticker as tick
import math
from cycler import cycler


from datetime import datetime

allowed_config_id = {
}
index_limit = '186-190'
overlap = False

# LB/CC mode matching
cc_modes = {
    1: "dcqcn",
    3: "hp",
    7: "timely",
    8: "dctcp",
}
lb_modes = {
    0: "fecmp",
    2: "drill",
    3: "conga",
    6: "letflow",
    9: "conweave",
    20: "caver",
    12: 'hula',
    10: 'dv',
    21: 'noshare',
}
topo2bdp = {
    "leaf_spine_128_100G_OS2": 104000,  # 2-tier
    "fat_k8_100G_OS2": 153000, # 3-tier -> core 400G
    "fat_k8_100G_OS1": 153000, # 3-tier -> core 400G
    "fat_k4_100G_OS2": 153000,
    "fat_k16_100G_OS1": 153000, 
    'fat_k_4_OS1': 153000,
    'fat_k4_100G_OS1': 153000,
    'fat_k_4_nobond_OS1': 153000,
    'fat_k8_100G_bond_OS2': 153000,
    "fat_k8_100G_bond_OS1": 153000,
}

C = [
    'xkcd:grass green',
    'xkcd:blue',
    'xkcd:purple',
    'xkcd:orange',
    'xkcd:teal',
    'xkcd:brick red',
    'xkcd:black',
    'xkcd:brown',
    'xkcd:grey',
]

LS = [
    'solid',
    'dashed',
    'dashdot',
    'dotted',
]

M = [
    'o',
    's',
    'x',
    'v',
    'D'
]

H = [
    '//',
    'o',
    '***',
    'x',
    'xxx',
]
linestyles = {
    'fecmp': '--',      # 虚线
    'conga': '-.',      # 点划线
    'conweave': ':',    # 点线
    'hula': (0, (3, 1, 1, 1, 1, 1)),       # 虚线
    'caver': '-',       # 实线
    'dv': '-',       # 实线
    'noshare': '-',       # 实线
}
colors = {
    'fecmp': (0, 0, 179/255),        # 暗蓝色 (RGB(0, 0, 139))
    'conga': 'green',                # 绿色保持不变
    'conweave': 'orange',            # 橙色保持不变
    'hula': (179/255, 0, 0),         # 暗红色 (RGB(139, 0, 0))
    'caver': (102/255, 8/255, 116/255),  # 紫色保持不变
    'dv': (102/255, 8/255, 116/255),  # 紫色保持不变
    'noshare': (102/255, 8/255, 116/255),  # 紫色保持不变
}
lb_mode_upper = {
    'fecmp': 'ECMP',
    'conga': 'Conga',
    'conweave': 'ConWeave',
    'hula': 'HULA',
    'caver': 'Caver',
    'dv': 'dv',
    'noshare': 'noshare'
}
def setup():
    """Called before every plot_ function"""

    def lcm(a, b):
        return abs(a*b) // math.gcd(a, b)

    def a(c1, c2):
        """Add cyclers with lcm."""
        l = lcm(len(c1), len(c2))
        c1 = c1 * (l//len(c1))
        c2 = c2 * (l//len(c2))
        return c1 + c2

    def add(*cyclers):
        s = None
        for c in cyclers:
            if s is None:
                s = c
            else:
                s = a(s, c)
        return s

    #plt.rc('axes', prop_cycle=(add(cycler(color=C),
    #                               cycler(linestyle=LS),
    #                               cycler(marker=M))))
    plt.rc('lines', markersize=5)
    plt.rc('legend', handlelength=3, handleheight=1.5, labelspacing=0.25)
    plt.rcParams["font.family"] = "sans"
    plt.rcParams["font.size"] = 10
    plt.rcParams['pdf.fonttype'] = 42
    plt.rcParams['ps.fonttype'] = 42
    plt.rcParams['lines.linewidth'] = 0.2


def getFilePath():
    dir_path = os.path.dirname(os.path.realpath(__file__))
    print("File directory: {}".format(dir_path))
    return dir_path

def get_pctl(a, p):
	i = int(len(a) * p)
	return a[i]

def size2str(steps):
    result = []
    for step in steps:
        if step < 10000:
            result.append("{:.1f}K".format(step / 1000))
        elif step < 1000000:
            result.append("{:.0f}K".format(step / 1000))
        else:
            result.append("{:.1f}M".format(step / 1000000))

    return result


def get_steps_from_raw(filename, time_start, time_end, step=5):
    # time_start = int(2.005 * 1000000000)
    # time_end = int(3.0 * 1000000000) 
    cmd_slowdown = "cat %s"%(filename)+" | awk '{ if ($6>"+"%d"%time_start+" && $6+$7<"+"%d"%(time_end)+") { slow=$7/$8; print slow<1?1:slow, $5} }' | sort -n -k 2"    
    output_slowdown = subprocess.check_output(cmd_slowdown, shell=True)
    aa = output_slowdown.decode("utf-8").split('\n')[:-2]
    if len(aa) == 0:
        raise Exception(f'something wrong in {filename}')
    nn = len(aa)

    # CDF of FCT
    res = [[i/100.] for i in range(0, 100, step)]
    for i in range(0,100,step):
        l = int(i * nn / 100)
        r = int((i+step) * nn / 100)
        fct_size = aa[l:r]
        fct_size = [[float(x.split(" ")[0]), int(x.split(" ")[1])] for x in fct_size]
        fct = sorted(map(lambda x: x[0], fct_size))
        
        res[int(i/step)].append(fct_size[-1][1]) # flow size
        
        res[int(i/step)].append(sum(fct) / len(fct)) # avg fct
        res[int(i/step)].append(get_pctl(fct, 0.5)) # mid fct
        res[int(i/step)].append(get_pctl(fct, 0.95)) # 95-pct fct
        res[int(i/step)].append(get_pctl(fct, 0.99)) # 99-pct fct
        res[int(i/step)].append(get_pctl(fct, 0.999)) # 99-pct fct
    
    # ## DEBUGING ###
    # print("{:5} {:10} {:5} {:5} {:5} {:5} {:5}  <<scale: {}>>".format("CDF", "Size", "Avg", "50%", "95%", "99%", "99.9%", "us-scale"))
    # for item in res:
    #     line = "%.3f %3d"%(item[0] + step/100.0, item[1])
    #     i = 1
    #     line += "\t{:.3f} {:.3f} {:.3f} {:.3f} {:.3f}".format(item[i+1], item[i+2], item[i+3], item[i+4], item[i+5])
    #     print(line)

    result = {"avg": [], "p99": [], "size": []}
    for item in res:
        result["avg"].append(item[2])
        result["p99"].append(item[5])
        result["size"].append(item[1])

    return result

def main():
    global index_limit
    parser = argparse.ArgumentParser(description='Plotting FCT of results')
    parser.add_argument('-sT', dest='time_limit_begin', action='store', type=int, default=2005000000, help="only consider flows that finish after T, default=2005000000 ns")
    parser.add_argument('-fT', dest='time_limit_end', action='store', type=int, default=10000000000, help="only consider flows that finish before T, default=10000000000 ns")
    parser.add_argument('-id', dest='index_limit', action='store', type=str, default='', help="only consider specific experiment results")
    parser.add_argument('-lb', dest='lb_limit', action='store', type=str, default='all', help="only consider specific lb results")
    
    args = parser.parse_args()
    time_start = args.time_limit_begin
    time_end = args.time_limit_end
    index_limit = args.index_limit
    if args.lb_limit == 'all':
        lb_limit = ['caver', 'dv', 'conweave', 'conga', 'fecmp', 'hula', 'noshare']
    else:
        lb_limit = [lb.strip() for lb in args.lb_limit.split()]

    print(index_limit)
    STEP = 5 # 5% step

    file_dir = getFilePath()
    fig_dir = file_dir + "/figures"
    output_dir = file_dir + "/../mix/output"
    history_filename = file_dir + "/../mix/.history"

    # read history file
    map_key_to_id = dict()

    # test_n = 10
    with open(history_filename, "r") as f:
        for line in f.readlines():
            for topo in topo2bdp.keys():
                if topo not in line:
                    continue
                parsed_line = line.replace("\n", "").split(',')
                config_id = parsed_line[1]

                # if len(allowed_config_id) != 0 and config_id not in allowed_config_id.keys():
                #     continue
                # 
                # if len(time_limit) != 0:
                #     try:
                #         experiment_time = datetime.strptime(config_id[0:14], "%m-%d-%H:%M:%S")
                #     except:
                #         continue
                #     time_limit_s = datetime.strptime(time_limit[0], "%m-%d-%H:%M:%S")
                #     time_limit_e = datetime.strptime(time_limit[1], "%m-%d-%H:%M:%S")
                #     if experiment_time < time_limit_s or experiment_time > time_limit_e:
                #         continue
                
                if index_limit != '':
                    match = re.search(r'\[(\d+)\]-(\d{2}-\d{2}-\d{2}:\d{2}:\d{2})-(.*?)-(.*)', config_id)
                    if match:
                        index = int(match.group(1))
                        for cond in index_limit.split(','):
                            if '-' in cond:
                                l = int(cond.split('-')[0])
                                r = int(cond.split('-')[1])
                                if index >= l and index <= r:
                                    break
                            elif int(cond) == index:
                                break
                        else:
                            continue
                    else:
                        #print(f'{config_id} 被抛弃，因为格式不符')
                        continue


                cc_mode = cc_modes[int(parsed_line[2])]
                lb_mode = lb_modes[int(parsed_line[3])]
                if lb_mode not in lb_limit:
                    continue
                # if lb_mode not in ['caver', 'dv', 'conweave']:
                #     continue
                encoded_fc = (int(parsed_line[9]), int(parsed_line[10]))
                if encoded_fc == (0, 1):
                    flow_control = "IRN"
                elif encoded_fc == (1, 0):
                    flow_control = "Lossless"
                else:
                    continue
                topo = parsed_line[13]
                netload = parsed_line[16]
                key = (topo, netload, flow_control)
                if key not in map_key_to_id:
                    map_key_to_id[key] = [[config_id, lb_mode]]
                else:
                    map_key_to_id[key].append([config_id, lb_mode])

    if overlap:#如果设置overlap，则相同设置下相同lb_mode只会保留最后一个
        for key, values in map_key_to_id.items():
            # 创建一个字典，用于记录最后一个 lb_mode 对应的元素
            unique_modes = {}
            for item in values:
                config_id, lb_mode = item
                unique_modes[lb_mode] = item  # 每次更新同一 lb_mode，保留最后一个

            # 更新原字典值为去重后的列表
            map_key_to_id[key] = list(unique_modes.values())


    for k, v in map_key_to_id.items():

        ################## AVG plotting ##################
        fig = plt.figure(figsize=(5, 4), dpi=300)
        ax = fig.add_subplot(111)
        fig.tight_layout()

        ax.set_xlabel("Flow Size (Bytes)", fontsize=14)
        ax.set_ylabel("Avg FCT Slowdown", fontsize=14)

        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.yaxis.set_ticks_position('left')
        ax.xaxis.set_ticks_position('bottom')
        
        xvals = [i for i in range(STEP, 100 + STEP, STEP)]

        lbmode_order = ["fecmp", "conga", "letflow", "conweave", 'hula', 'dv', 'caver', 'noshare']
        for tgt_lbmode in lbmode_order:
            for vv in v:
                config_id = vv[0]
                lb_mode = vv[1]

                if lb_mode == tgt_lbmode:
                    # plotting
                    fct_slowdown = output_dir + "/{id}/{id}_out_fct.txt".format(id=config_id)
                    try:
                        result = get_steps_from_raw(fct_slowdown, int(time_start), int(time_end), STEP)
                    except Exception as e:
                        print(e.args[0])
                        continue
                    label = lb_mode_upper[lb_mode] + config_id[1:config_id.find(']')]
                    ax.plot(xvals,
                        result["avg"],
                        markersize=1.0,
                        linewidth=1.5,
                        label=label,
                        linestyle=linestyles[lb_mode],
                        #color=colors[lb_mode]
                        )
                
        ax.legend(bbox_to_anchor=(0.0, 1.2), loc="upper left", borderaxespad=0,
                frameon=False, fontsize=13, facecolor='white', ncol=3, handlelength=2.0, handletextpad=0.4,
                labelspacing=0.2, columnspacing=0.4)
        
        ax.tick_params(axis="x", rotation=40)
        ax.set_xticks(([0] + xvals)[::2])
        ax.set_xticklabels(([0] + size2str(result["size"]))[::2], fontsize=13)
        ax.set_xlim(xmax=xvals[-1])

        ax.tick_params(axis='y', labelsize=14)
        y_ticks = list(ax.get_yticks())
        ax.set_yticks(y_ticks, [tick if i % 2 != len(y_ticks) % 2 else '' for i, tick in enumerate(y_ticks)])
        ax.tick_params(axis='y', labelsize=14)  # 设置刻度字体大小
        ax.set_ylim(bottom=1)  # 设置最小值
        # ax.set_yscale("log")

        fig.tight_layout()
        ax.grid(which='major', alpha=0.3, axis='y')
        #ax.grid(which='major', alpha=0.5)
        fig_filename = fig_dir + "/{}.png".format("AVG_TOPO_{}_LOAD_{}_FC_{}".format(k[0], k[1], k[2]))
        print(fig_filename)
        plt.savefig(fig_filename, transparent=False, bbox_inches='tight')
        plt.close()


        ################## P99 plotting ##################
        fig = plt.figure(figsize=(6, 4), dpi=300)
        ax = fig.add_subplot(111)
        fig.tight_layout()

        ax.set_xlabel("Flow Size (Bytes)", fontsize=11.5)
        ax.set_ylabel("p99 FCT Slowdown", fontsize=11.5)

        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.yaxis.set_ticks_position('left')
        ax.xaxis.set_ticks_position('bottom')
        
        xvals = [i for i in range(STEP, 100 + STEP, STEP)]

        lbmode_order = ["fecmp", "conga", "letflow", "conweave", 'hula', 'dv', 'caver', 'noshare']
        for tgt_lbmode in lbmode_order:
            for vv in v:
                config_id = vv[0]
                lb_mode = vv[1]

                if lb_mode == tgt_lbmode:
                    # plotting
                    fct_slowdown = output_dir + "/{id}/{id}_out_fct.txt".format(id=config_id)
                    try:
                        result = get_steps_from_raw(fct_slowdown, int(time_start), int(time_end), STEP)
                    except Exception as e:
                        print(e.args[0])
                        continue

                    label = lb_mode + config_id[1:config_id.find(']')]
                    ax.plot(xvals,
                        result["p99"],
                        markersize=1.0,
                        linewidth=1.5,
                        label=label,
                        linestyle=linestyles[lb_mode],
                        #color=colors[lb_mode]
                        )
                
        ax.legend(bbox_to_anchor=(0.0, 1.2), loc="upper left", borderaxespad=0,
                frameon=False, fontsize=12, facecolor='white', ncol=2,
                labelspacing=0.4, columnspacing=0.8)
        
        ax.tick_params(axis="x", rotation=40)
        ax.set_xticks(([0] + xvals)[::2])
        ax.set_xticklabels(([0] + size2str(result["size"]))[::2], fontsize=10.5)
        ax.set_ylim(bottom=1)
        # ax.set_yscale("log")

        fig.tight_layout()
        ax.grid(which='minor', alpha=0.2)
        ax.grid(which='major', alpha=0.5)
        fig_filename = fig_dir + "/{}.png".format("P99_TOPO_{}_LOAD_{}_FC_{}".format(k[0], k[1], k[2]))
        print(fig_filename)
        plt.savefig(fig_filename, transparent=False, bbox_inches='tight')
        plt.close()

if __name__=="__main__":
    setup()
    main()
