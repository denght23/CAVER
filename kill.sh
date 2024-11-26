#!/bin/bash

# 获取所有不是SS+状态的进程ID，并排除标题行
pids=$(ps aux | awk '$8 != "Ss" && NR > 1 {print $2}')

# 检查是否有进程ID被获取到
if [ -z "$pids" ]; then
  echo "没有找到需要终止的进程。"
else
  # 打印找到的进程ID
  echo "找到以下进程ID需要终止：$pids"
  
  # 杀掉所有获取到的进程ID
  echo "$pids" | xargs kill -9
  echo "已终止以下进程：$pids"
fi