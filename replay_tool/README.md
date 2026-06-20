# replay_tool

独立于 `log_module` 的 L2 日志回放工具目录。

目录结构：

- `backend/`
- `frontend/`

示例：

```bash
python3 replay_tool/backend/replay_viewer.py /path/to/l2
python3 replay_tool/backend/replay_viewer.py /path/to/l2.tar.xz
python3 replay_tool/backend/replay_server.py ./l2 --host 0.0.0.0 --port 8765
python3 replay_tool/backend/replay_server.py ./l2.tar.xz --host 0.0.0.0 --port 8765

# 服务脚本
bash replay_tool/service.sh start
bash replay_tool/service.sh restart
bash replay_tool/service.sh stop
```

说明：

- 回放工具仍依赖 `numpy`
- 支持直接传入 `l2.tar.xz`，服务端会先解压到 `/tmp/replay_tool_extract/`
- 也支持传入包含 `l2.tar.xz` 的目录，后端会优先自动选用目录下的 `l2.tar.xz`
- 新版 L2 目录结构为 `static_data`、`large_data/local_map`、`business_data`
- Web 回放会优先读取 `static_data` 下的最新静态地图映射到前端，再边播放边解析 `business_data`，并异步补载 `large_data/local_map`
- Web 页面资源位于 `frontend/index.html`
- `backend/replay_server.py` 会读取 `frontend/index.html`
- `service.sh` 会把 PID 和日志写入 `replay_tool/.runtime/`
- 这次迁移只拆分 Python 回放工具，不影响 `log_module` 库和 `log_module/tools` 下的 C++ 工具
