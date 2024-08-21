import subprocess
import os
import re
from datetime import datetime

# 可配置的内容置顶
REPO_PATH = r"D:\new_production\new_production"
SINCE_DATE = "2024-08-15"
UNTIL_DATE = "2024-12-31"

# 定义头文件路径
HEADERS_FILE = r"D:\new_production\new_production\my_set\AbIni.h"

# 初始化提交消息映射字典
COMMIT_DEFINITIONS = {}

def parse_header_file(header_file):
    definitions = {}
    pattern = r'^#define\s+(\w+VER)\s+"([^"]+)"'
    with open(header_file, 'r', encoding='utf-8') as file:
        for line in file:
            match = re.match(pattern, line.strip())
            if match:
                key = match.group(1)
                value = match.group(2)
                definitions[key] = f"{value}"
    return definitions

# 解析头文件中的宏定义
COMMIT_DEFINITIONS = parse_header_file(HEADERS_FILE)

def get_git_commits(repo_path, since, until):
    try:
        # 设置环境变量以使用 UTF-8 编码
        env = os.environ.copy()
        env['PYTHONIOENCODING'] = 'utf-8'

        # 构建 git log 命令，不包含哈希值
        command = [
            "git", "-C", repo_path, "log", 
            "--pretty=format:%an, %ar : %s", 
            f"--since={since}", f"--until={until}"
        ]
        
        # 运行 git log 命令并捕获输出
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=True,
            env=env,
            encoding='utf-8'  # 强制使用 UTF-8 编码
        )
        
        # 返回提交记录
        return result.stdout
    except subprocess.CalledProcessError as e:
        print(f"Error occurred: {e}")
        print(f"Command output: {e.output}")
        print(f"Command stderr: {e.stderr}")
        return None
    
def save_commits_to_md(commits, file_path):
    print(commits)
    with open(file_path, 'a', encoding='utf-8') as file:  # 使用 'a' 模式打开文件，追加内容
        # 获取当前日期
        current_date = datetime.now().strftime("%Y-%m-%d")

        # 构建标题中的日期信息
        date_info = f"日期:从 {SINCE_DATE} 到 {UNTIL_DATE}，发布日期 {current_date}"
        
        # 写入版本发布信息，包含日期信息
        file.write(f"##### 上位机版本 ({date_info})\n")
        file.write("内置版本内容（需要配合新的固件使用）:\n")
        
        # 初始化版本更新信息列表
        version_updates = {key: [] for key in COMMIT_DEFINITIONS}
        
        # 按照定义将提交消息格式化为 Markdown
        for line in commits.splitlines():
            for tag, definition in COMMIT_DEFINITIONS.items():
                if tag in line:
                    # 提取包含该标签的所有部分
                    updates = re.findall(rf"\[{tag}\]\s*(.*?)(?=\s*\[\w+_VER\]|$)", line)
             
                    for update_info in updates:
                        # print(update_info)
                        version_updates[tag].append(update_info.strip())
        
        # 写入每个版本号及其对应的更新信息
        for index, (tag, definition) in enumerate(COMMIT_DEFINITIONS.items(), start=1):
            # 计算需要的填充空格数，确保括号对齐
            max_def_length = max(len(definition) for definition in COMMIT_DEFINITIONS.values()) + 1  # 加1是为了留出空格
            padding_length = max_def_length - len(definition)
            
            # 写入版本号及其更新信息
            file.write(f"{index:2}. {definition}{padding_length * ' '}")
            if version_updates[tag]:
                file.write(f" ({', '.join(version_updates[tag])})")
            else:
                file.write(" (-----)")
            file.write("\n")
        file.write("\n")

if __name__ == "__main__":
    print(f"使用的git本地库为: {REPO_PATH}")
    
    # 获取 Git 提交记录
    commits = get_git_commits(REPO_PATH, SINCE_DATE, UNTIL_DATE)
    
    if commits:
        # 保存提交记录到 Markdown 文件
        md_file_path = os.path.join(REPO_PATH, "my_set", "上位机版本发布.md")
        save_commits_to_md(commits, md_file_path)
        print(f"保存到 {md_file_path}")
