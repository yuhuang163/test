#!/usr/bin/env sh
# 将本仓库的 Git 钩子目录设为 .githooks。每个克隆只需执行一次。
# 用法（在仓库根目录）：  sh scripts/setup-git-hooks.sh
set -e
root=$(git rev-parse --show-toplevel)
cd "$root"
git config core.hooksPath .githooks
val=$(git config --get core.hooksPath)
if [ "$val" != ".githooks" ]; then
  echo "设置失败，当前 core.hooksPath=$val" >&2
  exit 1
fi
echo "已设置 core.hooksPath=.githooks，提交时将运行 .githooks/commit-msg"

if [ -f "$root/.gitmessage" ]; then
  git config commit.template ".gitmessage"
  echo "已设置 commit.template=.gitmessage"
fi
