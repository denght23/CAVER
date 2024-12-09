# -*- coding: utf-8 -*-
import re
import sys
import argparse
import os
import numpy as np
import matplotlib.pyplot as plt

def parse_file(file_path, start_line, end_line, k):
    results = []
    with open(file_path, "r") as file:
        for i, line in enumerate(file):
            if i < start_line:
                continue
            if i > end_line:
                break
            parts = list(map(int, line.strip().split(',')))
            if len(parts) - 2 == k:
                results.append(parts)
    return results

def analyze_data(data, alpha):
    optimal_ce_values = []
    all_ce_values = []
    next_best_ce_values = []
    ratio_all_paths = []
    left_ratio_all_paths = []
    alpha_counts = []

    for row in data:
        ce_values = row[2:]
        optimal_ce = min(ce_values)
        optimal_ce_values.append(optimal_ce)
        all_ce_values.extend(ce_values)

        sorted_ce_values = sorted(ce_values)
        next_best_ce = sorted_ce_values[1] if len(sorted_ce_values) > 1 else sorted_ce_values[0]
        next_best_ce_values.append(next_best_ce)

        ratios = [(ce-optimal_ce) / optimal_ce if optimal_ce != 0 else ce for ce in sorted_ce_values]
        ratio_all_paths.append(ratios)
        
        left_ratios = [(256 - ce) / (256 - optimal_ce) for ce in sorted_ce_values]
        left_ratio_all_paths.append(left_ratios)

        alpha_count = sum(1 for ce in ce_values if ce < optimal_ce * alpha)
        alpha_counts.append(alpha_count)

    return optimal_ce_values, all_ce_values, next_best_ce_values, ratio_all_paths, alpha_counts, left_ratio_all_paths

def plot_cdf(data_list, labels, title, output_path, xlabel='CE Value'):
    plt.figure()
    for data, label in zip(data_list, labels):
        sorted_data = np.sort(data)
        yvals = np.arange(len(sorted_data)) / float(len(sorted_data) - 1)
        plt.plot(sorted_data, yvals, label=label)
    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel('CDF')
    plt.legend()
    plt.grid(True)
    plt.savefig(output_path)
    plt.close()

def main():
    parser = argparse.ArgumentParser(description="Analyze path CE values.")
    parser.add_argument("file_id", type=int, help="The ID of the file to analyze")
    parser.add_argument("suffix", choices=["_out_pathce.txt", "_out_pathce_exclude_last_hop.txt"], help="The suffix of the file to analyze")
    parser.add_argument("k", type=int, help="The number of paths to consider")
    parser.add_argument("start_line", type=int, help="The starting line number (0-indexed)")
    parser.add_argument("end_line", type=int, help="The ending line number (0-indexed)")
    parser.add_argument("alpha", type=float, help="The alpha value for analysis")
    args = parser.parse_args()

    file_name = "{}{}".format(args.file_id, args.suffix)
    file_path = os.path.join("..", "mix", "output", str(args.file_id), file_name)
    output_dir = os.path.join("output", "{}_pathce".format(args.file_id) if args.suffix == "_out_pathce.txt" else "{}_pathce_exclude_last_hop".format(args.file_id))

    print("File path:", file_path)  # 添加调试信息
    print("Output directory:", output_dir)  # 添加调试信息

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    if not os.path.exists(file_path):
        print("Error: File not found:", file_path)
        return

    data = parse_file(file_path, args.start_line, args.end_line, args.k)
    optimal_ce_values, all_ce_values, next_best_ce_values, ratio_all_paths, alpha_counts,left_ratio_all_paths = analyze_data(data, args.alpha)

    # Flatten the list of ratios for plotting
    flattened_ratios = [item for sublist in ratio_all_paths for item in sublist]

    plot_cdf([optimal_ce_values, all_ce_values, next_best_ce_values], ["Optimal CE Values", "All CE Values", "Next Best CE Values"], "CE Values CDF", os.path.join(output_dir, "ce_values_cdf.png"))

    # Prepare data for CE Ratio CDF
    ratio_labels = [f"Path {i+2} / Optimal CE Ratio" for i in range(args.k - 1)]
    ratio_data = [[] for _ in range(args.k - 1)]
    for ratios in ratio_all_paths:
        for i in range(len(ratios) - 1):
            ratio_data[i].append(ratios[i + 1])

    plot_cdf(ratio_data, ratio_labels, "CE Ratio CDF", os.path.join(output_dir, "ce_ratio_cdf.png"), xlabel='Ratio')
    #添加left_ratio_all_paths的CDF图
    left_ratio_labels = [f"Path {i+2} / Optimal CE Ratio" for i in range(args.k - 1)]
    left_ratio_data = [[] for _ in range(args.k - 1)]
    for left_ratios in left_ratio_all_paths:
        for i in range(len(left_ratios) - 1):
            left_ratio_data[i].append(left_ratios[i + 1])
            
    plot_cdf(left_ratio_data, left_ratio_labels, "Left CE Ratio CDF", os.path.join(output_dir, "left_ce_ratio_cdf.png"), xlabel='Ratio')
    
    plot_cdf([alpha_counts], ["Alpha Count Distribution"], "Alpha Count Distribution (alpha={})".format(args.alpha), os.path.join(output_dir, "alpha_count_distribution.png"), xlabel='Path Number')

    print("Output written to {}".format(output_dir))

if __name__ == "__main__":
    main()