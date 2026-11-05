# PROGRESS — mine-haul-routing

> 进度日志。新的写在最上面；只追加，不删改旧记录。

## 2026-11-05 19:44 · T-002 · 卡住
- 做了：按开工流程读了 AGENTS.md、TASKS.md、PROGRESS.md、PLAN.md，并检查本机环境；把 T-002 标成卡住。没有写任何代码，也没有安装任何东西
- 改了：TASKS.md、PROGRESS.md
- 验证：`wsl --status` 退出码 50，提示未安装；WSL 和虚拟机平台两个 Windows 功能都未启用；gcc、make、valgrind、clang 都找不到。Python 3.12 已有。系统已检测到正在运行的虚拟机监控程序（hypervisor），说明 BIOS 虚拟化已开启，不需要改 BIOS
- 问题：WSL2 要管理员权限和重启，需要我自己安装
- 下一步：我在管理员终端运行 `wsl --install` 并重启，设置好 Ubuntu 用户后说"继续"，再接着做 T-002

## 2026-11-02 · T-001 · 完成
- 做了：我确认方案全部按推荐执行：开发环境用 WSL2 + Ubuntu（我自己装）；测试框架自己写极简断言头文件；手工设计小矿区，另生成大图测速。排在项目 3 之后。已拆成具体任务
- 改了：TASKS.md
- 验证：无代码
- 问题：无
- 下一步：T-002

## 2026-10-31 · T-001 · 进行中
- 做了：写好 PLAN.md（架构、技术选型、目录结构、分步计划、待确认问题）
- 改了：PLAN.md（新增）、TASKS.md、AGENTS.md（填了项目说明和特有规则）
- 验证：无代码。本机检查：没有 docker、gcc、make、WSL
- 问题：见 PLAN.md 第 6 节
- 下一步：等我回复 PLAN.md 第 6 节的 3 个问题

## 2026-10-31 00:43 · 初始化 · 完成
- 做了：用模板创建项目，生成 AGENTS.md / TASKS.md / PROGRESS.md
- 改了：无代码
- 验证：无
- 问题：无
- 下一步：填 AGENTS.md 第 1、2 节，然后做 T-001
