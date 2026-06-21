# L3 日志测试报告

## 1. 测试说明

本报告参考 `L2` 的单用例单文件测试体系，对 `L3` 当前已经实现的能力做可重复验证。

本轮覆盖当前 `L3` 已具备的能力：

1. 基本结构化写入
2. `Init(dir)` 单目录语义
3. 全局级别过滤
4. 模块级别覆盖
5. 源码位置信息输出
6. 正常退出收口与压缩
7. 初始化前 active 恢复
8. agent 对已封口文件的 drain / 压缩
9. 高频同步写入
10. 高频异步写入
11. 关闭源码位置信息
12. 自定义业务时间戳透传
13. 关闭运行期 agent 后保留 raw sealed 文件
14. 关闭 `recover_on_init` 时保留旧 active 文件
15. 打开 `recover_on_init` 时恢复旧 active 文件
16. 异常路径显式失败
17. Python 接口基本写入
18. Python 接口模块级别控制
19. Python 高频写入
20. Python 异常路径显式失败
21. Python 与 C++ 同目录混合写入一致性

## 2. 测试项清单

| 编号 | 测试内容 | 对应用例文件 |
| --- | --- | --- |
| TC01 | 基本结构化写入 | `log_test/l3/tc01_test.cpp` |
| TC02 | `Init(dir)` 单目录语义 | `log_test/l3/tc02_test.cpp` |
| TC03 | 全局日志级别过滤 | `log_test/l3/tc03_test.cpp` |
| TC04 | 模块级别覆盖 | `log_test/l3/tc04_test.cpp` |
| TC05 | 源码位置信息输出 | `log_test/l3/tc05_test.cpp` |
| TC06 | 正常退出收口并压缩 | `log_test/l3/tc06_test.cpp` |
| TC07 | 初始化前恢复 lingering active | `log_test/l3/tc07_test.cpp` |
| TC08 | agent drain 已封口文件 | `log_test/l3/tc08_test.cpp` |
| TC09 | 高频同步写入 | `log_test/l3/tc09_test.cpp` |
| TC10 | 高频异步写入 | `log_test/l3/tc10_test.cpp` |
| TC11 | 关闭源码位置信息 | `log_test/l3/tc11_test.cpp` |
| TC12 | 自定义业务时间戳透传 | `log_test/l3/tc12_test.cpp` |
| TC13 | 关闭运行期 agent 后保留 raw sealed 文件 | `log_test/l3/tc13_test.cpp` |
| TC14 | 关闭 `recover_on_init` 时保留旧 active 文件 | `log_test/l3/tc14_test.cpp` |
| TC15 | 打开 `recover_on_init` 时恢复旧 active 文件 | `log_test/l3/tc15_test.cpp` |
| TC16 | 异常路径显式失败 | `log_test/l3/tc16_test.cpp` |
| TC17 | Python 接口基本写入 | `log_test/l3/tc17_test.py` |
| TC18 | Python 接口模块级别控制 | `log_test/l3/tc18_test.py` |
| TC19 | Python 高频写入 | `log_test/l3/tc19_test.py` |
| TC20 | Python 异常路径显式失败 | `log_test/l3/tc20_test.py` |
| TC21 | Python 与 C++ 同目录混合写入一致性 | `log_test/l3/tc21_test.py` |

## 3. 测试结果汇总

本轮测试在 Docker 容器 `log-ros1-dev` 中执行，构建与执行步骤如下：

1. 重新配置隔离构建目录  
   `docker exec log-ros1-dev bash -lc 'cd /workspaces/log && cmake -S . -B build_l3 -DCMAKE_DISABLE_FIND_PACKAGE_catkin=ON'`
2. 编译 `L3` 测试矩阵  
   `docker exec log-ros1-dev bash -lc 'cd /workspaces/log && cmake --build build_l3 --target log_l3_test_support l3_tc01_test l3_tc02_test l3_tc03_test l3_tc04_test l3_tc05_test l3_tc06_test l3_tc07_test l3_tc08_test l3_tc09_test l3_tc10_test l3_tc11_test l3_tc12_test l3_tc13_test l3_tc14_test l3_tc15_test l3_tc16_test log_l3_c_api'`
3. 执行 `C++` 用例 `TC01-TC16`  
   `docker exec log-ros1-dev bash -lc 'cd /workspaces/log && for t in l3_tc01_test l3_tc02_test l3_tc03_test l3_tc04_test l3_tc05_test l3_tc06_test l3_tc07_test l3_tc08_test l3_tc09_test l3_tc10_test l3_tc11_test l3_tc12_test l3_tc13_test l3_tc14_test l3_tc15_test l3_tc16_test; do ./build_l3/log_test/$t; done'`
4. 执行 `Python` 用例 `TC17-TC21`  
   `docker exec log-ros1-dev bash -lc 'cd /workspaces/log && export PYTHONPATH=/workspaces/log/log_l3/python && export LOG_L3_PYTHON_LIB=/workspaces/log/build_l3/log_l3/liblog_l3_c_api.so && for t in log_test/l3/tc17_test.py log_test/l3/tc18_test.py log_test/l3/tc19_test.py log_test/l3/tc20_test.py log_test/l3/tc21_test.py; do python3 $t; done'`

### 3.1 总体结论

本轮 `L3` 用例共 `21` 项，当前全部通过。

结论如下：

1. `L3` 的基础结构化写入通过
2. `Init(dir)` 单目录语义通过，未出现按模块分目录行为
3. 全局级别过滤和模块级别覆盖均通过
4. 源码位置字段输出通过
5. 正常退出收口、压缩、初始化前恢复和 agent drain 行为均通过
6. 高频同步与异步写入场景均通过
7. 恢复开关和异常路径行为符合当前实现
8. Python 接口已接到同一套 C++ 核心，当前基础行为通过
9. Python 高频写入、异常路径和与 C++ 混合写入一致性均通过

### 3.2 测试结果明细

| 编号 | 测试内容 | 测试步骤 | 测试结果 | 失败原因 |
| --- | --- | --- | --- | --- |
| TC01 | 基本结构化写入 | 运行 `./build_l3/log_test/l3_tc01_test` | 通过。输出 `TC01 result=PASS`，生成 `1` 个压缩封口文件，文本中包含 `layer/module/payload` | 无 |
| TC02 | `Init(dir)` 单目录语义 | 运行 `./build_l3/log_test/l3_tc02_test` | 通过。输出 `TC02 result=PASS`，根目录下仅有 `1` 个封口文件，无额外子目录 | 无 |
| TC03 | 全局日志级别过滤 | 运行 `./build_l3/log_test/l3_tc03_test` | 通过。输出 `TC03 result=PASS`，`INFO` 内容被过滤，`ERROR` 内容保留 | 无 |
| TC04 | 模块级别覆盖 | 运行 `./build_l3/log_test/l3_tc04_test` | 通过。输出 `TC04 result=PASS`，仅模块级放开的 `NAVIGATION INFO` 被写入 | 无 |
| TC05 | 源码位置信息输出 | 运行 `./build_l3/log_test/l3_tc05_test` | 通过。输出 `TC05 result=PASS`，日志文本中存在 `file/line/func` 字段 | 无 |
| TC06 | 正常退出收口并压缩 | 运行 `./build_l3/log_test/l3_tc06_test` | 通过。输出 `TC06 result=PASS`，退出后 `active_files=0`、`gz_files=1` | 无 |
| TC07 | 初始化前恢复 lingering active | 运行 `./build_l3/log_test/l3_tc07_test` | 通过。输出 `TC07 result=PASS`，重启后旧 active 被恢复，目录中得到 `2` 个封口文件 | 无 |
| TC08 | agent drain 已封口文件 | 运行 `./build_l3/log_test/l3_tc08_test` | 通过。输出 `TC08 result=PASS`，已有 sealed raw 文件被 agent drain 成 `.gz` | 无 |
| TC09 | 高频同步写入 | 运行 `./build_l3/log_test/l3_tc09_test` | 通过。输出 `TC09 result=PASS`，`500` 条同步高频写入全部保留 | 无 |
| TC10 | 高频异步写入 | 运行 `./build_l3/log_test/l3_tc10_test` | 通过。输出 `TC10 result=PASS`，`1000` 条异步高频写入全部保留 | 无 |
| TC11 | 关闭源码位置信息 | 运行 `./build_l3/log_test/l3_tc11_test` | 通过。输出 `TC11 result=PASS`，日志中不存在 `file/line/func` 字段 | 无 |
| TC12 | 自定义业务时间戳透传 | 运行 `./build_l3/log_test/l3_tc12_test` | 通过。输出 `TC12 result=PASS`，日志中保留自定义 `timestamp_us=1234567890123456` | 无 |
| TC13 | 关闭运行期 agent 后保留 raw sealed 文件 | 运行 `./build_l3/log_test/l3_tc13_test` | 通过。输出 `TC13 result=PASS`，退出后 `sealed_files=1` 且 `gz_files=0` | 无 |
| TC14 | 关闭 `recover_on_init` 时保留旧 active 文件 | 运行 `./build_l3/log_test/l3_tc14_test` | 通过。输出 `TC14 result=PASS`，旧 active 文件未被启动恢复，目录中保留 `1` 个 active 和 `1` 个 sealed | 无 |
| TC15 | 打开 `recover_on_init` 时恢复旧 active 文件 | 运行 `./build_l3/log_test/l3_tc15_test` | 通过。输出 `TC15 result=PASS`，旧 active 文件被恢复收口，目录中为 `2` 个 sealed 文件 | 无 |
| TC16 | 异常路径显式失败 | 运行 `./build_l3/log_test/l3_tc16_test` | 通过。输出 `TC16 result=PASS`，`Init("/dev/null/log")` 显式抛错，未出现静默成功 | 无 |
| TC17 | Python 接口基本写入 | 运行 `PYTHONPATH=/workspaces/log/log_l3/python LOG_L3_PYTHON_LIB=/workspaces/log/build_l3/log_l3/liblog_l3_c_api.so python3 log_test/l3/tc17_test.py` | 通过。输出 `TC17 result=PASS`，Python 写入产物为 `1` 个文件，字段内容与 C++ 侧一致 | 无 |
| TC18 | Python 接口模块级别控制 | 运行 `PYTHONPATH=/workspaces/log/log_l3/python LOG_L3_PYTHON_LIB=/workspaces/log/build_l3/log_l3/liblog_l3_c_api.so python3 log_test/l3/tc18_test.py` | 通过。输出 `TC18 result=PASS`，模块级别放开的 `NAVIGATION INFO` 可见，未放开的 `SCHEDULER INFO` 被过滤 | 无 |
| TC19 | Python 高频写入 | 运行 `PYTHONPATH=/workspaces/log/log_l3/python LOG_L3_PYTHON_LIB=/workspaces/log/build_l3/log_l3/liblog_l3_c_api.so python3 log_test/l3/tc19_test.py` | 通过。输出 `TC19 result=PASS matched=800`，`800` 条 Python 高频写入全部保留 | 无 |
| TC20 | Python 异常路径显式失败 | 运行 `PYTHONPATH=/workspaces/log/log_l3/python LOG_L3_PYTHON_LIB=/workspaces/log/build_l3/log_l3/liblog_l3_c_api.so python3 log_test/l3/tc20_test.py` | 通过。输出 `TC20 result=PASS`，`init("/dev/null/log")` 显式失败并打印底层错误 | 无 |
| TC21 | Python 与 C++ 同目录混合写入一致性 | 运行 `PYTHONPATH=/workspaces/log/log_l3/python LOG_L3_PYTHON_LIB=/workspaces/log/build_l3/log_l3/liblog_l3_c_api.so python3 log_test/l3/tc21_test.py` | 通过。输出 `TC21 result=PASS`，同目录下 C++ 桥接写入与 Python 写入内容一致可读 | 无 |

## 4. 关键结果

### 4.1 生命周期结果

`TC06/TC07/TC08/TC13/TC14/TC15` 共同说明：

1. `L3` 退出后不会遗留 active 文件
2. 退出后的 sealed 文件可被压缩
3. 启动前可恢复上一轮遗留 active 文件
4. 启动时和退出时的 agent drain 行为生效
5. 相关开关关闭时行为也符合预期，不会偷偷恢复或压缩

### 4.2 语义结果

`TC01/TC02/TC03/TC04/TC05/TC09/TC10/TC11/TC12/TC16/TC17/TC18/TC19/TC20/TC21` 共同说明：

1. `L3` 当前保持 `Init(dir)` 的单目录模型
2. `module` 只作为日志字段和级别控制键，不参与分目录
3. 结构化字段、级别过滤和源码位置信息行为符合当前实现
4. 高频同步/异步写入下未发现记录缺口
5. 异常路径会显式失败，不会静默成功
6. Python 接口和 C++ 接口当前在字段语义、级别控制和同目录产物上保持一致

## 5. 后续建议

当前 `L3` 已完成基础能力、生命周期、高频写入和 `Python` 绑定一致性验证，后续可继续扩展以下方向：

1. 更高强度的异步队列积压、丢弃策略和背压观测场景
2. 运行中长期压缩、清理和磁盘水位联动场景
3. `Python/C++` 并发混合写入与异常恢复组合场景
4. 查询、清理、统计和工具链交互场景
