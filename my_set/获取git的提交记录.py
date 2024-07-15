import subprocess
import os

# 可配置的内容置顶
REPO_PATH = r"D:\new_production\new_production"
SINCE_DATE = "2023-01-01"
UNTIL_DATE = "2024-12-31"

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
        
        # 打印并返回提交记录
        print(result.stdout)
        return result.stdout
    except subprocess.CalledProcessError as e:
        print(f"Error occurred: {e}")
        print(f"Command output: {e.output}")
        print(f"Command stderr: {e.stderr}")
        return None

def save_commits_to_md(commits, file_path):
    with open(file_path, 'w', encoding='utf-8') as file:
        file.write("# Git Commit History\n\n")
        file.write("```\n")
        # 去除每行开头的哈希值，并写入文件
        for line in commits.splitlines():
            if line.strip():  # 如果行不为空
                # 找到第一个空格的位置，跳过哈希值
                start_index = line.index(' ') + 1
                file.write(line[start_index:] + '\n')
        file.write("```\n")

if __name__ == "__main__":
    print(f"Using repository path: {REPO_PATH}")
    
    # 获取 Git 提交记录
    commits = get_git_commits(REPO_PATH, SINCE_DATE, UNTIL_DATE)
    
    if commits:
        # 保存提交记录到 Markdown 文件
        md_file_path = os.path.join(REPO_PATH, "my_set", "git的提交记录.md")
        save_commits_to_md(commits, md_file_path)
        print(f"Commits have been saved to {md_file_path}")
