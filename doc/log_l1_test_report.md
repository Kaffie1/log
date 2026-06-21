# L1 系统监控日志测试报告

## 1. 测试说明

本报告用于验证 `L1` 作为系统级监控日志层的首版实现是否具备稳定的基础写入、级别控制、生命周期收口、异常恢复和高频写入能力。

当前覆盖 `20` 项：

1. 基本监控日志写入
2. 单目录布局
3. 全局级别过滤
4. 组件级别覆盖
5. 正常退出收口并压缩
6. 启动前 lingering active 恢复
7. 关闭 shutdown recovery 后保留 active
8. 高频同步写入
9. 关闭源码位置
10. 异常路径显式失败
11. 高频异步写入
12. 关闭 `recover_on_init` 保留旧 active
13. 恢复后的 sealed 文件由 agent drain
14. 自定义指标与进程字段透传
15. 打开源码位置输出
16. CLI 时间段打包支持 `start + duration`
17. CLI 在 `.gz` 文件上按模块和级别查询
18. 磁盘、CPU、内存阈值字段场景
19. 打包前自动压缩 raw sealed 文件
20. CLI 支持 `yymmdd_HHMMSS + duration` 语法

## 2. 测试项清单

| 编号 | 测试内容 | 对应用例文件 |
| --- | --- | --- |
| TC01 | 基本监控日志写入 | `log_test/l1/tc01_test.cpp` |
| TC02 | 单目录布局 | `log_test/l1/tc02_test.cpp` |
| TC03 | 全局级别过滤 | `log_test/l1/tc03_test.cpp` |
| TC04 | 组件级别覆盖 | `log_test/l1/tc04_test.cpp` |
| TC05 | 正常退出收口并压缩 | `log_test/l1/tc05_test.cpp` |
| TC06 | 启动前 lingering active 恢复 | `log_test/l1/tc06_test.cpp` |
| TC07 | 关闭 shutdown recovery 后保留 active | `log_test/l1/tc07_test.cpp` |
| TC08 | 高频同步写入 | `log_test/l1/tc08_test.cpp` |
| TC09 | 关闭源码位置 | `log_test/l1/tc09_test.cpp` |
| TC10 | 异常路径显式失败 | `log_test/l1/tc10_test.cpp` |
| TC11 | 高频异步写入 | `log_test/l1/tc11_test.cpp` |
| TC12 | 关闭 `recover_on_init` 保留旧 active | `log_test/l1/tc12_test.cpp` |
| TC13 | 恢复后的 sealed 文件由 agent drain | `log_test/l1/tc13_test.cpp` |
| TC14 | 自定义指标与进程字段透传 | `log_test/l1/tc14_test.cpp` |
| TC15 | 打开源码位置输出 | `log_test/l1/tc15_test.cpp` |
| TC16 | CLI 时间段打包支持 `start + duration` | `log_test/l1/tc16_test.cpp` |
| TC17 | CLI 在 `.gz` 文件上按模块和级别查询 | `log_test/l1/tc17_test.cpp` |
| TC18 | 磁盘、CPU、内存阈值字段场景 | `log_test/l1/tc18_test.cpp` |
| TC19 | 打包前自动压缩 raw sealed 文件 | `log_test/l1/tc19_test.cpp` |
| TC20 | CLI 支持 `yymmdd_HHMMSS + duration` 语法 | `log_test/l1/tc20_test.cpp` |

## 3. 测试结果汇总

本轮测试在 Docker 容器 `log-ros1-dev` 中执行，步骤如下：

1. 配置构建目录  
   `docker exec log-ros1-dev bash -lc 'cd /workspaces/log && cmake -S . -B build_l1 -DCMAKE_DISABLE_FIND_PACKAGE_catkin=ON'`
2. 编译 `L1`、`log_cli` 与测试矩阵  
   `docker exec log-ros1-dev bash -lc 'cd /workspaces/log && cmake --build build_l1 --target log_l1 log_cli log_l1_test_support l1_tc01_test l1_tc02_test l1_tc03_test l1_tc04_test l1_tc05_test l1_tc06_test l1_tc07_test l1_tc08_test l1_tc09_test l1_tc10_test l1_tc11_test l1_tc12_test l1_tc13_test l1_tc14_test l1_tc15_test l1_tc16_test l1_tc17_test l1_tc18_test l1_tc19_test l1_tc20_test'`
3. 执行 `TC01-TC20`  
   `docker exec log-ros1-dev bash -lc 'cd /workspaces/log && for t in l1_tc01_test l1_tc02_test l1_tc03_test l1_tc04_test l1_tc05_test l1_tc06_test l1_tc07_test l1_tc08_test l1_tc09_test l1_tc10_test l1_tc11_test l1_tc12_test l1_tc13_test l1_tc14_test l1_tc15_test l1_tc16_test l1_tc17_test l1_tc18_test l1_tc19_test l1_tc20_test; do ./build_l1/log_test/$t; done'`

### 3.1 总体结论

本轮 `L1` 测试共 `20` 项，当前全部通过。

结论如下：

1. `L1` 已具备系统监控日志的基础结构化写入能力
2. 当前实现保持单目录模型，不按组件分目录
3. 全局级别过滤与组件级别覆盖通过
4. 正常退出收口、启动前恢复、保留 active 开关语义通过
5. 同步与异步高频写入场景通过
6. 异常路径会显式失败，不会静默成功
7. 自定义监控字段、进程字段和源码位置字段均按预期输出
8. `log_cli` 已支持对 `L1` 执行时间段查询与 `start + duration` 打包
9. 压缩后的 `.gz` 文件可继续按模块和级别过滤

### 3.2 测试结果明细

| 编号 | 测试内容 | 测试步骤 | 测试结果 | 失败原因 |
| --- | --- | --- | --- | --- |
| TC01 | 基本监控日志写入 | 运行 `./build_l1/log_test/l1_tc01_test` | 通过。输出 `TC01 result=PASS`，日志包含 `layer/module/message/event` | 无 |
| TC02 | 单目录布局 | 运行 `./build_l1/log_test/l1_tc02_test` | 通过。输出 `TC02 result=PASS`，根目录仅生成单个封口文件 | 无 |
| TC03 | 全局级别过滤 | 运行 `./build_l1/log_test/l1_tc03_test` | 通过。输出 `TC03 result=PASS`，`INFO` 被过滤，`ERROR` 保留 | 无 |
| TC04 | 组件级别覆盖 | 运行 `./build_l1/log_test/l1_tc04_test` | 通过。输出 `TC04 result=PASS`，仅放开的 `RESOURCE WARN` 被写入 | 无 |
| TC05 | 正常退出收口并压缩 | 运行 `./build_l1/log_test/l1_tc05_test` | 通过。输出 `TC05 result=PASS`，退出后 `active_files=0`、`gz_files=1` | 无 |
| TC06 | 启动前 lingering active 恢复 | 运行 `./build_l1/log_test/l1_tc06_test` | 通过。输出 `TC06 result=PASS`，旧 active 被恢复，目录中得到 `2` 个 sealed 文件 | 无 |
| TC07 | 关闭 shutdown recovery 后保留 active | 运行 `./build_l1/log_test/l1_tc07_test` | 通过。输出 `TC07 result=PASS`，退出后保留 `1` 个 active 文件 | 无 |
| TC08 | 高频同步写入 | 运行 `./build_l1/log_test/l1_tc08_test` | 通过。输出 `TC08 result=PASS`，`500` 条同步心跳日志全部保留 | 无 |
| TC09 | 关闭源码位置 | 运行 `./build_l1/log_test/l1_tc09_test` | 通过。输出 `TC09 result=PASS`，日志中不存在 `file/line/func` 字段 | 无 |
| TC10 | 异常路径显式失败 | 运行 `./build_l1/log_test/l1_tc10_test` | 通过。输出 `TC10 result=PASS`，`Init("/dev/null/log")` 显式失败并打印底层错误 | 无 |
| TC11 | 高频异步写入 | 运行 `./build_l1/log_test/l1_tc11_test` | 通过。输出 `TC11 result=PASS`，`1000` 条异步心跳日志全部保留 | 无 |
| TC12 | 关闭 `recover_on_init` 保留旧 active | 运行 `./build_l1/log_test/l1_tc12_test` | 通过。输出 `TC12 result=PASS`，旧 active 未被启动恢复，目录中保留 `1` 个 active 和 `1` 个 sealed | 无 |
| TC13 | 恢复后的 sealed 文件由 agent drain | 运行 `./build_l1/log_test/l1_tc13_test` | 通过。输出 `TC13 result=PASS`，恢复后的 sealed 文件可继续被 agent 压缩处理 | 无 |
| TC14 | 自定义指标与进程字段透传 | 运行 `./build_l1/log_test/l1_tc14_test` | 通过。输出 `TC14 result=PASS`，日志中保留 `process/metric_name/metric_value/timestamp_us` | 无 |
| TC15 | 打开源码位置输出 | 运行 `./build_l1/log_test/l1_tc15_test` | 通过。输出 `TC15 result=PASS`，日志中存在 `file/line/func` 字段 | 无 |
| TC16 | CLI 时间段打包支持 `start + duration` | 运行 `./build_l1/log_test/l1_tc16_test` | 通过。输出 `TC16 result=PASS`，`log_cli package` 可基于 `--start + --duration` 完成打包 | 无 |
| TC17 | CLI 在 `.gz` 文件上按模块和级别查询 | 运行 `./build_l1/log_test/l1_tc17_test` | 通过。输出 `TC17 result=PASS`，压缩后的 `.gz` 文件仍可按 `RESOURCE + WARN` 查询 | 无 |
| TC18 | 磁盘、CPU、内存阈值字段场景 | 运行 `./build_l1/log_test/l1_tc18_test` | 通过。输出 `TC18 result=PASS`，三类资源告警字段均完整落盘 | 无 |
| TC19 | 打包前自动压缩 raw sealed 文件 | 运行 `./build_l1/log_test/l1_tc19_test` | 通过。输出 `TC19 result=PASS`，raw sealed 文件在打包前会被先压缩再归档 | 无 |
| TC20 | CLI 支持 `yymmdd_HHMMSS + duration` 语法 | 运行 `./build_l1/log_test/l1_tc20_test` | 通过。输出 `TC20 result=PASS`，CLI 能接受 `260101_000000 + duration` 形式并进入查询流程 | 无 |

## 4. 关键结果

### 4.1 生命周期结果

`TC05/TC06/TC07/TC12/TC13/TC19` 共同说明：

1. `L1` 正常退出后可完成收口与压缩
2. 启动前可恢复上一轮遗留 active 文件
3. 关闭 `recover_on_shutdown` 或 `recover_on_init` 时行为符合开关预期
4. 恢复出的 sealed 文件可继续被 agent drain
5. 命中 raw sealed 文件时，打包前会先完成压缩

### 4.2 语义结果

`TC01/TC02/TC03/TC04/TC08/TC09/TC10/TC11/TC14/TC15/TC16/TC17/TC18/TC20` 共同说明：

1. `L1` 当前保持系统监控日志的单目录模型
2. `module` 仅作为组件字段和级别控制键，不参与分目录
3. 高频同步和异步写入下未发现记录缺口
4. 自定义监控字段和源码位置字段符合当前实现
5. 异常路径会显式失败，不会静默成功
6. `L1` 已可通过 `log_cli` 做查询和时间段打包
7. 压缩后的 `.gz` 文件仍支持模块和级别过滤

## 5. 后续建议

当前 `L1` 已完成首版基础测试，后续建议继续补充以下方向：

1. 更长时间窗口的高频心跳与资源采样稳定性场景
2. 运行期更细粒度的 agent 压缩时机观测场景
3. CLI 导出、上传和交付场景
4. 后续若补 Python 绑定，再做跨语言一致性场景
