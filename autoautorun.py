#!/usr/bin/env python3
import os
import time
import subprocess

# 预设命令列表（按顺序依次执行）
commands = [
    "./autorun.sh fat_k8_100G_OS2 50",
    "./autorun.sh fat_k8_100G_OS2 70",
    "./autorun.sh fat_k8_100G_bond_OS2 50",
    "./autorun.sh fat_k8_100G_bond_OS2 70",
    "./autorun.sh fat_k8_100G_OS1 50",
    "./autorun.sh fat_k8_100G_OS1 70",
]

# 全局变量：记录上次执行到的命令索引
last_executed_index = 0

def get_process_count():
    """执行命令并返回符合条件的行数"""
    try:
        # 执行命令并获取输出
        result = subprocess.check_output(
            "ps aux | grep load-balance | grep zj | grep -v grep",
            shell=True,
            text=True
        )
        # 统计行数
        return len(result.strip().split("\n")) if result.strip() else 0
    except subprocess.CalledProcessError:
        # 如果命令执行失败，假设行数为0
        return 0

def execute_next_commands():
    """按顺序选择并执行两个命令"""
    global last_executed_index

    # 执行两个命令
    for _ in range(10):
        if last_executed_index >= len(commands):
            print('finished all!!')
            quit(0)  # 循环回到列表开头

        cmd = commands[last_executed_index]
        print(f"Executing: {cmd}")
        os.system(cmd)

        # 更新索引到下一个命令
        last_executed_index += 1

if __name__ == "__main__":
    #print('waiting...')
    #time.sleep(3600 * 4)
    #os.system('rm /home/zj/ns-allinone-3.19/ns-3.19/config/L_30.00_CDF_AliStorage2019_N_256_T_30ms_B_100_flow.txt')
    #os.system('rm /home/zj/ns-allinone-3.19/ns-3.19/config/L_60.00_CDF_AliStorage2019_N_128_T_30ms_B_100_flow.txt')
    #print('awake!')
    while True:
        print("Checking process count...")
        process_count = get_process_count()
        print(f"Current process count: {process_count}")

        if process_count < 5 or True:
            print("Process count less than 5. Executing commands...")
            execute_next_commands()
        else:
            print("Process count is sufficient. No action taken.")

        print("Waiting for 1 minute...")
        time.sleep(60)
