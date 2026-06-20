# LogL2 与 LogService 对齐设计

## 1. 目标

`log_l2` 作为 `L2` 原始数据日志的前台控制适配层。

本次重构的目标不是改写 `L2` 现有 recorder 的原始数据写盘逻辑，而是在保持以下内容不变的前提下，将前台控制职责对齐到 `LogService`：

1. 不改变 `.data` / `.idx` / `.meta` 的写入逻辑
2. 不改变文件后缀规则
3. 不改变 topic 注册与分类逻辑
4. 不改变原始记录格式和索引格式

对齐后的职责划分如下：

1. legacy `L2` recorder 继续负责原始数据、索引和会话元数据的实际写入
2. `log_l2` 负责初始化控制会话、强制切段、封口、查询、打包、导出和本地交付
3. `LogService` 负责查询、打包、导出和本地交付，以及提供统一的前台控制接口语义

## 2. 适配策略

### 2.1 初始化

`LogL2::InitRecorder()` 在调用 legacy `L2::InitRecorder()` 后，会为当前 L2 会话创建一个 `LogService` 控制会话：

1. legacy recorder 初始化实际写盘状态
2. `LogService::CreateActiveFile()` 创建控制层活跃文件
3. 后续切段和封口动作由 `log_l2` 统一触发

### 2.2 强制切段

`LogL2::SwitchSegment()` 的行为如下：

1. 调用 legacy `l2_log::ForceRotateActiveSegments()`，对当前 `.data` / `.idx` 活跃分段执行切段续写
2. 调用 `LogService::SwitchSegment()`，同步前台控制会话的分段状态

### 2.3 封口

`LogL2::SealSession()` 的行为如下：

1. 调用 legacy `l2_log::SealActiveSegments()`，封口当前所有活跃分段
2. 调用 `LogService::SealFile()`，同步前台控制会话的封口状态

### 2.4 查询与打包

`log_l2` 不再调用 legacy `L2::PackageRecords()` 作为默认前台打包入口，而是改为：

1. 使用 `LogService::QueryLogs()` 对 L2 根目录下的文件进行查询
2. 使用 `LogService::PackageLogs()` 进行前台打包
3. 使用 `LogService::ExportPackage()` / `UploadPackage()` 完成交付

其中：

1. L2 根目录仍然沿用 legacy recorder 的目录结构
2. `LogService` 通过统一时间命名规则识别 `.data` / `.idx` / `.meta` 文件
3. 为兼容 `.data.gz` / `.idx.gz`，`LogService` 已支持解析多重后缀文件名

## 3. 当前边界

当前 `log_l2` 是“控制层对齐”，不是“写盘内核替换”。

这意味着：

1. L2 的原始写入实现仍然是内部 recorder
2. `LogService` 当前不直接驱动 `.data` / `.idx` 的字节写入
3. `LogService` 负责统一前台控制语义与交付接口
