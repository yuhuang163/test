import subprocess
import os
import re
from datetime import datetime

# 定义脚本目录
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# 可配置项
# 为空时自动识别当前脚本所在仓库；也可手动写死仓库路径
REPO_PATH = ""
SINCE_DATE = "2026-4-14"
UNTIL_DATE = "2027-12-31"

# 初始化提交消息映射字典
COMMIT_DEFINITIONS = {}


def resolve_repo_path(configured_repo_path):
    if configured_repo_path and configured_repo_path.strip():
        return configured_repo_path.strip()

    try:
        result = subprocess.run(
            ["git", "-C", SCRIPT_DIR, "rev-parse", "--show-toplevel"],
            capture_output=True,
            text=True,
            check=True,
            encoding="utf-8"
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        # 非 git 仓库场景下，回退到脚本上级目录
        return os.path.dirname(SCRIPT_DIR)

def resolve_header_file(repo_path):
    candidates = [
        os.path.join(repo_path, "my_set", "AbIni.h"),
        os.path.join(SCRIPT_DIR, "AbIni.h"),
        os.path.join(os.path.dirname(SCRIPT_DIR), "my_set", "AbIni.h"),
    ]
    for path in candidates:
        if os.path.exists(path):
            return path
    raise FileNotFoundError(f"头文件不存在，已尝试: {', '.join(candidates)}")

def parse_header_file(header_file):
    definitions = {}
    pattern = r'^#define\s+(\w+VER)\s+"([^"]+)"'
    if not os.path.exists(header_file):
        raise FileNotFoundError(f"头文件不存在: {header_file}")
    with open(header_file, 'r', encoding='utf-8') as file:
        for line in file:
            match = re.match(pattern, line.strip())
            if match:
                key = match.group(1)
                value = match.group(2)
                definitions[key] = f"{value}"
    return definitions

# 用于分隔 git log 中每条提交（含标题与正文），便于解析多行 [XXX_VER]
_GIT_LOG_RECORD_MARK = ">>>GIT_LOG_RECORD<<<"

def get_git_commits(repo_path, since, until):
    try:
        # 设置环境变量以使用 UTF-8 编码
        env = os.environ.copy()
        env['PYTHONIOENCODING'] = 'utf-8'

        # %s 标题 + %b 正文：公共模块可在正文中为每个工站各写一行 [XXX_VER]
        command = [
            "git", "-C", repo_path, "log",
            f"--pretty=format:{_GIT_LOG_RECORD_MARK}%n%an, %ar : %s%n%b",
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
    
def _split_git_log_records(raw):
    """按记录分隔符拆成单条提交全文（标题行 + 正文）。"""
    parts = raw.split(_GIT_LOG_RECORD_MARK)
    return [p.strip() for p in parts if p.strip()]


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

        # 每条提交全文（含正文）中解析 [XXX_VER]；lookahead 兼容「换行接下一标签」或「同一行 ；/; 后接下一标签」
        for commit_text in _split_git_log_records(commits):
            for tag, definition in COMMIT_DEFINITIONS.items():
                updates = re.findall(
                    rf"\[{re.escape(tag)}\]\s*(.*?)(?=(?:\s*\[\w+_VER\]|\s*[；;]\s*\[\w+_VER\])|$)",
                    commit_text,
                    flags=re.DOTALL,
                )
                for update_info in updates:
                    version_updates[tag].append(update_info.strip())

        # 写入每个版本号及其对应的更新信息
        for index, (tag, definition) in enumerate(COMMIT_DEFINITIONS.items(), start=1):
            # 计算需要的填充空格数，确保括号对齐
            max_def_length = max(len(definition) for definition in COMMIT_DEFINITIONS.values()) + 1  # 加1是为了留出空格
            padding_length = max_def_length - len(definition)
            
            # 行首统一「空格 + 序号」，避免 1～9 为「 1.」而 10+ 为「10.」导致 Markdown 预览在 9 条后列表断裂
            file.write(f" {index}. {definition}{padding_length * ' '}")
            if version_updates[tag]:
                file.write(f" ({', '.join(version_updates[tag])})")
            else:
                file.write(" (-----)")
            file.write("\n")
        file.write("\n")

if __name__ == "__main__":
    REPO_PATH = resolve_repo_path(REPO_PATH)
    if not os.path.isdir(REPO_PATH):
        raise FileNotFoundError(f"git本地库路径不存在: {REPO_PATH}")
    HEADERS_FILE = resolve_header_file(REPO_PATH)
    COMMIT_DEFINITIONS = parse_header_file(HEADERS_FILE)

    print(f"使用的git本地库为: {REPO_PATH}")
    
    # 获取 Git 提交记录
    commits = get_git_commits(REPO_PATH, SINCE_DATE, UNTIL_DATE)
    
    if commits:
        # 保存提交记录到 Markdown 文件
        md_file_path = os.path.join(REPO_PATH, "docs", "上位机版本发布.md")
        save_commits_to_md(commits, md_file_path)
        print(f"保存到 {md_file_path}")
