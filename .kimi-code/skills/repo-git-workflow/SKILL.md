---
name: repo-git-workflow
description: 本仓库的 Git 分支约定与远程状态——工作分支是 main，master 保留不用，gh CLI token 已失效
whenToUse: 每次涉及 git 提交、推送、分支、合并、rebase 或清理分支操作时必读
---

# 本仓库 Git 分支约定（2026-07-17 起）

## 以 main 为准

- 本仓库的工作分支是 **`main`**。所有新提交、推送、rebase 都在 `main` 上进行：`git push origin main`（本地 main 追踪 origin/main）。
- `main` 是 2026-07-17 由 `master` 重命名而来（`git branch -m master main`），两者当时指向同一提交 `8b0eb1d`。

## 远程 master 保留，不要删除

- 远程 `origin/master` **故意保留**。用户已明确决定不删除它。
- 它仍是 GitHub 上的默认分支（改默认分支需要有效的 gh token 或网页操作，当时未做）。
- 不要把 `origin/master` 当成工作分支，也不要往它上面推新提交；以 `origin/main` 为准。
- 如果未来用户完成了 GitHub 默认分支切换（Settings → Branches，或 `gh repo edit aoweichenn/CPU --default-branch main`），可以再执行 `git push origin --delete master` 清理。

## 分支历史背景（仅供查账，勿复活）

- `codex/integrate-lcqi-on-remote`：原集成分支，2026-07-17 已合并进 master 并在本地+远程删除。
- `codex/lcqi-q5-practice-backup`、`backup/b1a2123-regenerated-hardware-pdf`：备份分支，同日经用户确认删除。

## 远程与凭据现状

- `origin` = `github-aurek:aoweichenn/CPU.git`（SSH alias，推送正常）。
- `gh` CLI 已安装（/usr/sbin/gh）但 `/root/.config/gh/hosts.yml` 里的 token **已失效**；凡是需要 GitHub API 的操作（改默认分支、PR、issue）先做 `gh auth login -h github.com`。
