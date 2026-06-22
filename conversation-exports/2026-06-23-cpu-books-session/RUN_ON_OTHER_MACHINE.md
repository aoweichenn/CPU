# 在另一台机器运行

下面命令用于在另一台机器拉取仓库、检查本轮会话导出、重新构建第一册。

## 1. 克隆或更新仓库

如果机器上还没有仓库：

```bash
git clone git@github.com:aoweichenn/CPU.git
cd CPU
```

如果 SSH 默认 22 端口不可用，可以用 443：

```bash
git clone ssh://git@ssh.github.com/aoweichenn/CPU.git
cd CPU
```

如果已经有仓库：

```bash
cd /path/to/CPU
git fetch origin
git checkout main
git pull --ff-only
```

如果远端默认分支是 `master`，但本地要用 `main`：

```bash
git fetch origin
git checkout -B main origin/master
```

## 2. 查看本轮会话导出

```bash
cd /path/to/CPU
ls conversation-exports/2026-06-23-cpu-books-session
sed -n '1,220p' conversation-exports/2026-06-23-cpu-books-session/TRANSCRIPT.md
sed -n '1,220p' conversation-exports/2026-06-23-cpu-books-session/SESSION_STATE.md
```

## 3. 检查第一册导出产物

```bash
find "book-exports/从C++到计算系统第一册" -maxdepth 1 -type f -printf '%p %s\n' | sort
```

预期大小：

```text
main.pdf 4264081
main.epub 623738
从C++到计算系统第一册.pdf 4264081
从C++到计算系统第一册.epub 623738
```

## 4. 重新构建第一册

需要本机有 `xelatex`、`latexmk` 和 Python 3。

```bash
cd books/cpu-volume-1/source/latex
make check
make pdf
make epub
```

检查 Overfull：

```bash
rg -n -F 'Overfull \hbox' main.log || true
```

检查 EPUB 是否没有图片资源：

```bash
python3 - <<'PY'
from pathlib import Path
from zipfile import ZipFile
path = Path('main.epub')
image_exts = {'.png', '.jpg', '.jpeg', '.gif', '.webp', '.svg'}
with ZipFile(path) as zf:
    images = [name for name in zf.namelist() if Path(name).suffix.lower() in image_exts]
print('image_entries:', len(images))
for name in images:
    print(name)
PY
```

## 5. 重新导出第一册

```bash
cd /path/to/CPU
mkdir -p "book-exports/从C++到计算系统第一册"
cp books/cpu-volume-1/source/latex/main.pdf "book-exports/从C++到计算系统第一册/main.pdf"
cp books/cpu-volume-1/source/latex/main.epub "book-exports/从C++到计算系统第一册/main.epub"
cp books/cpu-volume-1/source/latex/main.pdf "book-exports/从C++到计算系统第一册/从C++到计算系统第一册.pdf"
cp books/cpu-volume-1/source/latex/main.epub "book-exports/从C++到计算系统第一册/从C++到计算系统第一册.epub"
```

验证大小：

```bash
stat -c '%n %s' \
  books/cpu-volume-1/source/latex/main.pdf \
  books/cpu-volume-1/source/latex/main.epub \
  "book-exports/从C++到计算系统第一册/main.pdf" \
  "book-exports/从C++到计算系统第一册/main.epub" \
  "book-exports/从C++到计算系统第一册/从C++到计算系统第一册.pdf" \
  "book-exports/从C++到计算系统第一册/从C++到计算系统第一册.epub"
```

## 6. 继续修改后的提交推送

```bash
cd /path/to/CPU
git status --short
git diff --check
git add <changed-files>
git commit -m "Your concrete message"
GIT_SSH_COMMAND='ssh -o StrictHostKeyChecking=accept-new -p 443' \
  git push ssh://git@ssh.github.com/aoweichenn/CPU.git main:master
```
