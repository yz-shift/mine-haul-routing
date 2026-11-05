# PROGRESS — mine-haul-routing

> 进度日志。新的写在最上面；只追加，不删改旧记录。

## 2026-11-05 22:03 · T-009 · 卡住
- 做了：写了大图生成器、测速程序、制表脚本和出图脚本；`make bench` 生成 1 万、4 万、9 万节点的网格图，每种规模和载重跑 200 对随机起终点，共 5 轮取中位数，全程核对两种算法的代价一致
- 改了：bench/gen_road.c、bench/bench_routing.c、bench/results.json、viz/bench_table.py、viz/plot_route.py、viz/requirements.txt、Makefile、.gitignore、TASKS.md、PROGRESS.md
- 验证：9 万节点上，空载每次查询 Dijkstra 11.66 ms、A* 6.41 ms（快 45%），展开节点数少 40%；满载 11.87 ms、7.24 ms（快 39%），展开节点数少 28%。`make test`、`make valgrind` 退出码 0；测速程序在小图上跑 valgrind 退出码 0；生成器同样的种子输出相同。验证了 `make valgrind` 能发现命令行工具的泄漏。**未验证：出图。** 本机没有 matplotlib，脚本只检查了语法、数据处理和缺依赖时的报错，还没有真正画出图，也没有看过布局
- 问题：1 万节点的结果波动明显：同样的展开量，空载 1.03 ms、满载 0.61 ms，这个规模的差异不要过度解读。数据来自一台开发笔记本的单次运行，不是严格基准。配色用的是 dataviz 文档中已校验的前两个颜色，校验脚本在这里运行时没有输出，所以没有独立复核
- 下一步：等我同意安装 matplotlib 后出图并检查；先做 T-010、T-011

## 2026-11-05 21:53 · T-008 · 完成
- 做了：实现封路、按名字找节点、JSON 输出和命令行工具 `mine-route`；手工设计了 24 个节点的示例矿区；AGENTS.md 按之前确认的内容填上入口文件和运行命令
- 改了：include/graph.h、src/graph.c、include/json_out.h、src/json_out.c、src/main.c、data/sample_mine.road、tests/cli_test.sh、tests/test_json_out.c、tests/test_graph.c、tests/test_routing.c、Makefile、AGENTS.md、TASKS.md、PROGRESS.md
- 验证：示例矿区上，满载走西侧缓坡（668 s，2263 m），空载走陡坡（270 s，1546 m）；关闭 RAMP_MID_W 到 RAMP_TOP_W 后，从 RAMP_MID_W 重规划改走东侧（769 s，3098 m）。`make test` 退出码 0（C 单元测试 88 个，命令行场景 30 个）；`make valgrind` 退出码 0，包括命令行场景。输出用 Python `json.load` 解析通过。一次性检查：ASan + UBSan（含命令行）通过，`-O2` 无警告，写满磁盘（/dev/full）时退出码 1，故意改坏代码 14 处都能发现
- 问题：之前"待定"的两件事已决定：节点名重复时按名字查找直接报错；封路会关闭两点之间所有平行路段的两个方向。第一次跑命令行测试有 2 处失败，是我把路长写成 411，实际向上取整是 412，改正后全部重跑
- 下一步：T-009 Python 可视化 + 测速对比

## 2026-11-05 21:43 · T-007 · 完成
- 做了：实现 A*，和 Dijkstra 共用一个搜索循环，启发函数是直线距离除以全图最高速度；启发值关闭时就是 Dijkstra
- 改了：include/routing.h、src/routing.c、tests/test_astar.c、TASKS.md、PROGRESS.md
- 验证：900 个节点的随机图上，满载、空载各跑 1000 对随机起终点，两者总代价用 `==` 比较全部相同，路线也都验证过确实可走。展开节点数：空载 Dijkstra 444656、A* 277255（少 38%）；满载 441996、332694（少 25%）。`make test`、`make valgrind` 退出码都是 0，7 个程序都是 0 错误。一次性检查：ASan + UBSan 通过，`-O2` 无警告，故意改坏代码 7 处都能发现（含启发值高估）
- 问题：这批随机对没有出现不可达的情况，不可达由专门的测试覆盖。A* 省多少和地图有关，这里只是这张随机小图的结果；真实耗时由 T-009 测量
- 下一步：T-008 封路后重新规划 + JSON 输出 + 命令行工具

## 2026-11-05 21:39 · T-006 · 完成
- 做了：实现 Dijkstra 和路径还原，终点出队就停止；不可达、节点编号无效、内存不足都有明确的返回状态；路程按实际走过的弧累计，平行路段也算得对
- 改了：include/routing.h、src/routing.c、tests/test_routing.c、TASKS.md、PROGRESS.md
- 验证：`make test` 退出码 0，71 个测试全过（新增 15 个，含手算图和满载、空载选不同路线）；`make valgrind` 退出码 0，6 个程序都是 0 错误。一次性检查：ASan + UBSan 通过，`-O2` 无警告，7 个分配点逐个模拟失败都干净返回，故意改坏代码 8 处都能发现
- 问题：第一轮改坏代码检查发现两处：去掉 decrease-key 后原测试发现不了，已加专门的测试；封闭弧的单独判断是多余代码（无穷大代价本来就不会更优），已删除
- 下一步：T-007 A* + 和 Dijkstra 交叉验证

## 2026-11-05 21:32 · T-005 · 完成
- 做了：实现行驶时间代价模型，速度取道路限速、卡车最高速、发动机功率、牵引力和下坡制动几项限制的最小值；提供全图最高速度，供 T-007 的 A* 启发函数使用。我要求后面的任务直接做完，不再逐个停下
- 改了：include/cost.h、src/cost.c、tests/test_cost.c、TASKS.md、PROGRESS.md
- 验证：`make test` 退出码 0，56 个测试全过（新增 15 个，包含满载上坡慢于空载、封闭边不可走、单行没有回程）；`make valgrind` 退出码 0，5 个程序都是 0 错误、堆内存全部释放。一次性检查：ASan + UBSan 通过，`-O2` 无警告，故意改坏代码 9 处都能发现。第一次跑有 1 个测试失败，是我手算的期望值错了（应为 6.52 km/h，不是 6.4），改正后全部重跑
- 问题：车辆参数（质量、功率、牵引力等）是取整的假设值，不是厂商规格，README 要写明。两边的 Python 都没有 matplotlib，T-009 出图前要问我是否安装
- 下一步：T-006 Dijkstra + 路径还原

## 2026-11-05 21:24 · T-004 · 完成
- 做了：实现带位置表的二叉最小堆，支持 insert、pop_min、decrease_key、contains，内存在创建时一次分配好
- 改了：include/heap.h、src/heap.c、tests/test_heap.c、TASKS.md、PROGRESS.md
- 验证：`make test` 退出码 0，41 个测试全过（新增堆测试 14 个，含最多 20000 个随机键的排序对照和 20000 步随机操作对照）；`make valgrind` 退出码 0，4 个程序都是 0 错误、堆内存全部释放。一次性检查（脚本不进仓库）：ASan + UBSan 通过；`-O2` 无警告；3 个内存分配点逐个模拟失败，都返回 NULL，valgrind 干净；故意改坏代码 11 处，测试都能发现。这几项跑完后只改过一条注释，随后重跑了 `make test` 和 `make valgrind`
- 问题：没有实测 decrease-key 和“重复入堆、取出时跳过”两种做法谁更快，目前只能讲理论差别
- 下一步：T-005 代价模型（坡度、载重、限速、单行、封闭）

## 2026-11-05 20:55 · T-003 · 完成
- 做了：实现邻接表图结构（每个节点一个动态数组）和道路网文件解析，错误信息带文件名和行号；经我同意填了 AGENTS.md 第 2 节（单独提交）
- 改了：include/graph.h、src/graph.c、include/road_file.h、src/road_file.c、tests/test_graph.c、tests/test_road_file.c、tests/data/small_mine.road、tests/test_framework.h、Makefile、AGENTS.md、TASKS.md、PROGRESS.md
- 验证：`make test` 退出码 0，27 个测试全过（图 10、解析 15、冒烟 2），覆盖正常文件、注释、格式错误；`make valgrind` 退出码 0，3 个程序都是 0 错误、堆内存全部释放。一次性检查（脚本不进仓库）：ASan + UBSan 通过；`-O2` 无警告；把 30 个内存分配点逐个模拟失败，都返回 NULL 和 out of memory，valgrind 干净；故意改坏代码 10 处，测试都能发现
- 问题：自查发现 `graph_add_road` 收到双向自环（from 等于 to）时，只预留一个位置却写入两条弧，可能堆溢出。解析器本来就拒绝自环，所以读文件碰不到这条路径，但 API 不安全。已修复并加测试。节点名重复和平行路段的处理留到 T-008，已写进"待定"
- 下一步：T-004 二叉堆（支持 decrease-key）

## 2026-11-05 20:43 · T-002 · 完成
- 做了：在 WSL2 Ubuntu 里跑通工具链（Ubuntu 26.04、gcc 15.2.0、GNU Make 4.4.1、valgrind 3.26.0，由我安装）；写了 Makefile（all、test、valgrind、clean）、极简断言头文件和冒烟测试；用 `.gitattributes` 固定 LF 换行
- 改了：Makefile、tests/test_framework.h、tests/test_smoke.c、.gitattributes、.gitignore、TASKS.md、PROGRESS.md
- 验证：在 Ubuntu 里 `make test` 退出码 0（2 run, 0 failed，含一个空测试）；`make valgrind` 退出码 0（0 errors，All heap blocks were freed）。改头文件会触发重新编译。用仓库外的临时副本做了 4 个反向检查，都按预期失败：断言失败时 `make test` 报错，且后面的测试继续跑；definitely lost 泄漏和 still reachable 内存都让 `make valgrind` 报错；未使用变量的警告让编译失败
- 问题：AGENTS.md 第 2 节的常用命令还没填，改这个文件要先问我。make 只能在 WSL 里跑，Windows 这边没有 gcc 和 make
- 下一步：等我确认 T-002 和是否填 AGENTS.md，说"继续"后做 T-003

## 2026-11-05 20:35 · T-002 · 卡住
- 做了：我说能看到 Ubuntu，复查后发现 WSL 本体已装好，但这个 Windows 账户下没有发行版，所以没有领任务，也没有写代码
- 改了：TASKS.md、PROGRESS.md
- 验证：`wsl --version` 显示 WSL 3.0.3.0，电脑已于 20:34 重启。`wsl --list --all --verbose` 提示没有已安装的分发，`wsl -d Ubuntu` 报 `WSL_E_DISTRO_NOT_FOUND`；注册表（HKCU、HKU）里没有发行版记录，也没有 Ubuntu 应用包。查询账户是 `hoey\hoeyz`，电脑上只查到这一个账户有 WSL 记录
- 问题：不清楚 Ubuntu 没装上的原因，也和我看到的结果对不上，需要我贴出 `wsl -l -v` 的实际输出
- 下一步：我运行 `wsl --install -d Ubuntu` 并设置用户，然后贴出 `wsl -l -v` 的输出

## 2026-11-05 20:27 · T-002 · 卡住
- 做了：我说 WSL 装好了，复查后发现这台电脑上还用不了 WSL，所以没有领任务，也没有写代码
- 改了：TASKS.md、PROGRESS.md
- 验证：`wsl --status` 退出码仍是 50，提示未安装；`wsl -l -v` 和进入发行版都失败。WSL 和虚拟机平台两个功能都未启用，没有 WSL 或 Ubuntu 应用包，也没有 `C:\Program Files\WSL\wsl.exe`。上次开机是 16:45，早于 19:44 的第一次检查，也就是之后没有重启过；系统显示有待重启的更改，但 Setup 日志里只看到 16:51 的 Windows 更新（KB5127694、KB5121794），没有看到 WSL 相关记录
- 问题：还不能确定 `wsl --install` 是已执行、只等重启，还是没有成功执行
- 下一步：我重启电脑；重启后运行 `wsl -l -v` 看有没有 Ubuntu，没有就在管理员终端重新运行 `wsl --install`，设置好 Ubuntu 用户后说"继续"

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
