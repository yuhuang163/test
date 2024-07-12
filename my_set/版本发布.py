import re
from datetime import datetime

# 获取当前日期
current_date = datetime.now().strftime("%Y-%m-%d")

# 读取AbIni.h文件
with open('AbIni.h', 'rb') as file:
    content = file.read().decode('utf-8', errors='ignore')

# 匹配版本定义和更新内容
pattern = r'#define\s+(\w+_VER)\s+"([^"]+)"(?:\s+\/\/\s+(.*))?'
matches = re.findall(pattern, content)

# 写入版本发布.md文件
with open('版本发布.md', 'w', encoding='utf-8') as file:
    file.write(f"# 版本发布 ({current_date})\n\n")
    file.write("内置版本内容（需要配合新的固件使用）:\n")
    for i, (version, ver_num, update_content) in enumerate(matches, start=1):
        if update_content:
            file.write(f"{i}. {ver_num}({update_content.strip()})\n")
        else:
            file.write(f"{i}. {ver_num}\n")

