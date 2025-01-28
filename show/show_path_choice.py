def parse_path_choice_details(file_path):
    results = []

    with open(file_path, 'r', encoding='utf-8') as file:
        for line in file:
            line = line.strip()

            # 检测以 "Path_choice_detail info:" 开头的行
            if line.startswith("Path_choice_detail info:"):
                # 去掉 "Path_choice_detail info: " 部分
                details = line[len("Path_choice_detail info:"):].strip()

                # 按逗号分隔字段
                fields = details.split(", ")
                parsed = {}
                
                for field in fields:
                    # 按 ": " 分割字段名称和值
                    if ": " in field:
                        key, value = field.split(": ", 1)
                        parsed[key.strip()] = value.strip()

                # 检查字段逻辑是否完整
                if parsed.get("has_valid_paths") == "true":
                    if parsed.get("choose_newest_path") == "false":
                        if parsed.get("choose_srcRoute") == "true":
                            # 确保解析了 time_gap 或 newest_time
                            parsed["newest_time"] = parsed.get("newest_time", "N/A")
                            parsed["choose_time"] = parsed.get("choose_time", "N/A")
                
                results.append(parsed)

    return results

# 示例调用
id = 871178477
file_path = f"../mix/output/{id}/config.log"
parsed_data = parse_path_choice_details(file_path)

total_entry = len(parsed_data)
total_has_valid_paths = 0
total_choose_newest_path = 0
total_choose_srcRoute = 0
time_gap = []
for entry in parsed_data:
    if entry["has_valid_paths"] == "true":
        total_has_valid_paths += 1
        if entry["choose_newest_path"] == "true":
            total_choose_newest_path += 1
        else:
            if entry["choose_srcRoute"] == "true":
                total_choose_srcRoute += 1
                time_gap.append(int(entry["newest_time"]) - int(entry["choose_time"]))

print(f"Total entries: {total_entry}")
print(f"Total has_valid_paths: {total_has_valid_paths}")
print(f"Total choose_newest_path: {total_choose_newest_path}")
print(f"Total choose_srcRoute: {total_choose_srcRoute}")
print(f"Average time gap: {sum(time_gap) / len(time_gap)}")