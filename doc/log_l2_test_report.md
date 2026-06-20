# L2 日志测试报告

## 1. 测试说明

本报告基于清空 `runtime` 后重新执行的 fresh run 结果整理，测试目标是验证 `L2` 原始数据日志在“数据不丢失”这一核心目标下的行为。

本轮测试采用两类方式：

1. 使用临时验证程序对 `L2` 底层接口做可重复验证
2. 使用 `build_ros/log_agent/log_agent_demo` 补充后台处理验证

说明：

1. 本轮所有结果均来自重新创建 `runtime` 后的实测
2. 测试完成后临时验证源码已删除，仅保留测试产物目录
3. 对于 `TC27`，本轮采用受控长稳样本而非小时级持续压测

## 2. 总体结论

本轮 fresh run 得到以下结论：

1. `L2` 的基础写入、自然分段、正常退出封口、异常退出恢复、后治理协同均通过
2. 同秒多次自然分段场景下，唯一序号完整，未再出现丢数
3. 旧 `shutdown bundle` 遗留目录已在启动前被清理，`TC22` 通过
4. 本轮唯一未通过项为 `TC28`：异常路径场景下错误未显式暴露
5. 运行中自然分段后的压缩已切换为由 `agent` 周期扫描完成，`TC29` 通过
6. 时间段归档命中 active 分段时会先收口并只打包 `.gz` 段，`TC30` 通过

当前遗留问题：

1. `TC28` 需要补强异常路径/不可写路径下的错误暴露与可观测性

## 3. 测试结果汇总

| 编号 | 测试内容 | 测试步骤 | 测试结果 |
| --- | --- | --- | --- |
| TC01 | 基本录制 | 运行 `graceful /workspaces/log/runtime/tc_basic 1048576 20 2 6 600 1000 1 1 1760918400000000` | 通过。`unique_sequences=28`，与输入 `20+2+6=28` 一致 |
| TC02 | 多 topic 同时录制 | 复用 `tc_basic`，检查 `business_data/static_data/large_data` | 通过。三类目录均生成文件，互不干扰 |
| TC03 | 空闲 topic 不生成文件 | `tc_basic` 中注册 `idle` topic 但不写入数据 | 通过。未生成 `idle` 对应文件 |
| TC04 | 按大小自动分段 | 运行 `graceful /workspaces/log/runtime/tc_segment 4096 40 1 1 2000 1000 1 1 1760918400000000` | 通过。`gz_idx=42`、`unique_sequences=42`，自然分段后唯一序号完整 |
| TC05 | 分段阈值边界 | 分别运行 `tc_boundary3` 与 `tc_boundary5` 小阈值样本，核对记录完整性 | 通过。边界样本执行正常，唯一序号分别完整为 `3/3`、`5/5` |
| TC06 | 高频写入下分段 | 复用 `tc_segment` 的同秒高频写入场景 | 通过。高频自然分段后 `unique_sequences=42`，未出现丢数 |
| TC07 | 多 topic 并发分段 | 复用 `tc_segment`，同时写入业务、静态和大数据 topic | 通过。整体 `unique_sequences=42`，多类 topic 记录集合完整 |
| TC08 | 分段后文件命名连续性 | 检查 `tc_segment` 输出文件名 | 通过。业务段生成 `_part_<n>` 变体名，避免同秒多段覆盖 |
| TC09 | 正常退出封口 | 复用 `tc_basic`，检查退出后目录状态 | 通过。`raw_data=0`、`raw_idx=0`、`active_files=0` |
| TC10 | 正常退出后后治理协同 | 复用 `tc_basic`，检查 agent drain 结果 | 通过。`agent_drain=1 affected=6`，退出后文件均为 `.gz` |
| TC11 | 仅处理本进程文件 | 创建 `tc_scope_other/foreign.log`，再运行 `tc_scope` | 通过。无关目录文件内容保持 `keep` 不变 |
| TC12 | 异常退出恢复 | 运行 `abrupt /workspaces/log/runtime/tc_recover ...` 后再运行 `recover /workspaces/log/runtime/tc_recover 1048576` | 通过。恢复后 `active_files=0`、`unique_sequences=14` |
| TC13 | 恢复时不误改历史文件 | 对比 `tc_recover` 恢复前后文件集合 | 通过。仅未封口文件被恢复，未出现历史文件重复处理 |
| TC14 | 恢复后继续录制 | 运行 `tc_continue`：先 `abrupt`，再 `recover`，再 `graceful` 继续写入 | 通过。最终生成 `6` 个 `.data.gz` 和 `6` 个 `.idx.gz`，同根目录可继续录制 |
| TC15 | 多组遗留文件恢复 | `tc_recover` 中同时包含 `business/static/large` 三类遗留文件 | 通过。三类遗留文件均完成封口 |
| TC16 | 数据文件与索引文件一致恢复 | 复用 `tc_recover`，核对恢复后数据与索引数量 | 通过。恢复后 `raw_data=3/raw_idx=3`，且 `unique_sequences=14` |
| TC17 | 残缺文件异常处理 | 运行 `broken /workspaces/log/runtime/tc_broken` 后执行 `recover /workspaces/log/runtime/tc_broken 1048576` | 通过。恢复过程未崩溃，残缺 `.data/.idx` 文件被保留，可观测 |
| TC18 | 后治理结果校验 | 对 `tc_basic` 读取 `.idx.gz` 内容，确认可解析 | 通过。`gzip -cd ... | head` 可正常读出索引记录 |
| TC19 | 处理前后数据一致性 | 复用 `tc_basic`，核对输入与处理后唯一序号 | 通过。`unique_sequences=28` 与输入一致 |
| TC20 | 活动文件不提前处理 | 运行 `sleep /workspaces/log/runtime/tc_sleep 1048576 5 500 5 1000 1760918400000000`，睡眠期间执行 `inspect` | 通过。运行中 `gz_data=0/gz_idx=0`，未被提前压缩处理 |
| TC21 | 已处理文件不重复处理 | 对 `tc_basic` 再次运行 `build_ros/log_agent/log_agent_demo /workspaces/log/runtime/tc_basic` | 通过。输出 `compress ... affected=0`，未重复生成产物 |
| TC22 | 旧遗留目录清理 | 运行 `tc22 /workspaces/log/runtime/tc22_case2`，启动前人工创建 `l2_shutdown_bundle` | 通过。`bundle_dirs=0`，旧遗留目录已在启动前清理 |
| TC23 | 启动恢复接口有效性 | 复用 `tc_recover` 的 `recover` 场景 | 通过。启动恢复流程能够完成统一封口 |
| TC24 | 正常退出顺序正确 | 复用 `tc_basic`，检查最终状态 | 通过。退出后无 raw active 文件，且后治理只处理已封口文件 |
| TC25 | 空目录启动 | 运行 `graceful /workspaces/log/runtime/tc_empty 1048576 0 0 0 500 1000 1 1 1760918400000000` | 通过。无异常，无无意义文件生成 |
| TC26 | 单条大消息边界 | 运行 `graceful /workspaces/log/runtime/tc_large_msg 1024 1 0 0 5000 1000 1 1 1760918400000000` | 通过。单条超阈值消息可正常写入，`unique_sequences=1` |
| TC27 | 长时间运行稳定性 | 运行 `graceful /workspaces/log/runtime/tc_longrun 65536 1500 10 10 1024 1000 1 1 1760918400000000` | 通过。样本长稳场景下 `unique_sequences=1510`、`active_files=0`，未发现缺口 |
| TC28 | 目录权限或异常路径 | 运行 `invalid_root /sys/kernel/log` 与 `invalid_root /dev/null/log` | 失败。均返回 `invalid_root_result=unexpected_success`，错误未显式暴露 |
| TC29 | 运行中由 agent 压缩已收口分段 | 运行 `build_ros/log_l2/l2_runtime_validation /tmp/l2_runtime_validation_case`，观察 recorder 持续运行阶段的 agent 状态 | 通过。输出 `runtime_compressed_files=22`、`runtime_gzip_files=22`，说明自然分段后的 sealed 文件已由 agent 在运行中压缩 |
| TC30 | 时间段归档命中 active 文件 | 复用 `build_ros/log_l2/l2_runtime_validation /tmp/l2_runtime_validation_case`，检查归档输出 | 通过。输出 `package_only_gz=1`，归档前会先收口命中的 active 分段，并且归档中仅包含 `.gz` 段 |

## 4. 关键证据

### 4.1 基础录制场景

测试目录：`runtime/tc_basic`

结果摘要：

1. `gz_data=3`
2. `gz_idx=3`
3. `active_files=0`
4. `unique_sequences=28`

结论：

1. 基础录制、封口、压缩链路正常
2. 数据完整性正常

### 4.2 自然分段场景

测试目录：`runtime/tc_segment`

结果摘要：

1. `gz_data=42`
2. `gz_idx=42`
3. `active_files=0`
4. `unique_sequences=42`

结论：

1. 同秒多次自然分段场景下未再出现覆盖和丢数
2. 分段后文件命名具备避让能力

### 4.3 异常退出恢复场景

测试目录：`runtime/tc_recover`

结果摘要：

1. 恢复后 `raw_data=3/raw_idx=3`
2. `active_files=0`
3. `unique_sequences=14`

结论：

1. 启动前封口恢复逻辑有效
2. 在不触发压缩的恢复场景下，数据保持完整

### 4.4 长稳样本场景

测试目录：`runtime/tc_longrun`

结果摘要：

1. `gz_data=73`
2. `gz_idx=73`
3. `active_files=0`
4. `unique_sequences=1510`

结论：

1. 受控长稳样本下未发现记录缺口
2. 目录增长与分段行为稳定

### 4.5 异常路径场景

测试命令：

1. `invalid_root /sys/kernel/log`
2. `invalid_root /dev/null/log`

结果摘要：

1. 两次均返回 `invalid_root_result=unexpected_success`

结论：

1. 当前异常路径场景下错误没有显式暴露
2. `TC28` 未通过

## 5. 结论与建议

当前版本的 `L2` 可以认为“主要功能通过，异常路径可观测性待补强”：

1. 基础写入、分段、封口、恢复、后治理协同均通过
2. `TC22` 已通过，旧 `shutdown bundle` 遗留目录会在启动前清理
3. `TC27` 本轮受控长稳样本通过
4. `TC28` 未通过，需要补强错误暴露

建议优先级如下：

1. 优先补强 `TC28`，让异常路径或不可写路径场景明确失败并输出可观测错误
2. 保留当前分段回归用例，防止后续改动再次引入同秒分段覆盖问题
3. 若需要更高置信度，再补一轮更长时间窗口的长稳压测
