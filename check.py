#!/usr/bin/env python3
import os
from datetime import datetime
import sys
import re

def check_folders_for_log(n=5):
    # 获取当前目录下的所有子文件夹
    current_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), './mix/output')
    subfolders = [f.path for f in os.scandir(current_dir) if f.is_dir()]

    # 按照修改时间降序排序
    def get_id(path):
        match = re.search(r'\[(\d+)\]', path)
        if match:
            return int(match.group(1))
        else:
            return 0
    subfolders.sort(key=lambda x: get_id(x), reverse=True)

    latest_folders = subfolders[n-1::-1]

    # 检查每个子文件夹中的 config.log 文件
    for folder in latest_folders:
        config_log_path = os.path.join(folder, "config.log")
        if os.path.exists(config_log_path):
            # 检查文件内容是否包含指定字符串
            with open(config_log_path, "r") as log_file:
                log_content = log_file.read()
                if "Simulator is enforced to be finished" in log_content:
                    print(f"{os.path.basename(folder)}: \tFinished!")
                else:
                    print(f"{os.path.basename(folder)}: \tNot finished.\t{log_content.count('已导入') * 1000}")                    
        else:
            print(f"{os.path.basename(folder)}: \tconfig.log file not found.")

def check_history(n=5):
    os.system(f'tail mix/autorun_history.txt -n {n}')

if __name__ == "__main__":
    command = sys.argv[1]
    if 'state'.startswith(command):
        if len(sys.argv) == 3:
            check_folders_for_log(int(sys.argv[2]))
        else:
            check_folders_for_log()
    elif 'history'.startswith(command):
        if len(sys.argv) == 3:
            check_history(int(sys.argv[2]))
        else:
            check_history()
    quit(0)
