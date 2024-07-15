import subprocess
import os
import re
from datetime import datetime

# 可配置的内容置顶
REPO_PATH = r"D:\new_production\new_production"
SINCE_DATE = "2023-01-01"
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
    with open(file_path, 'w', encoding='utf-8') as file:
        # 获取当前日期
        current_date = datetime.now().strftime("%Y-%m-%d")
        
        # 写入版本发布信息
        file.write(f"# 版本发布 ({current_date})\n\n")
        file.write("内置版本内容（需要配合新的固件使用）:\n")
        
        # 按照定义将提交消息格式化为 Markdown
        index = 1
        for tag, definition in COMMIT_DEFINITIONS.items():
            file.write(f"{index}. {definition}")
            found_tag = False
            for line in commits.splitlines():
                if tag in line:
                    start_index = line.index(tag) + len(tag) + 1
                    update_info = line[start_index:].strip()
                    if update_info:
                        file.write(f" ({update_info})")
                    found_tag = True
            if not found_tag:
                file.write(" (无更新内容)")
            file.write("\n")
            index += 1

if __name__ == "__main__":
    print(f"Using repository path: {REPO_PATH}")
    
    # 获取 Git 提交记录
    commits = get_git_commits(REPO_PATH, SINCE_DATE, UNTIL_DATE)
    
    if commits:
        # 保存提交记录到 Markdown 文件
        md_file_path = os.path.join(REPO_PATH, "my_set", "git的提交记录.md")
        save_commits_to_md(commits, md_file_path)
        print(f"Commits have been saved to {md_file_path}")
