#!/usr/bin/env bash
# Usage:
#   bash replay_tool/service.sh start
#   bash replay_tool/service.sh stop
#   bash replay_tool/service.sh restart
#
# Optional environment overrides:
#   REPLAY_ROOT=./l2 REPLAY_PORT=8765 bash replay_tool/service.sh start
#   PYTHON_BIN=python3 REPLAY_HOST=0.0.0.0 bash replay_tool/service.sh restart

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="${SCRIPT_DIR}/.runtime"
PID_FILE="${RUNTIME_DIR}/replay_server.pid"
LOG_FILE="${RUNTIME_DIR}/replay_server.log"
PYTHON_BIN="${PYTHON_BIN:-python3}"
REPLAY_ROOT="${REPLAY_ROOT:-}"
REPLAY_HOST="${REPLAY_HOST:-0.0.0.0}"
REPLAY_PORT="${REPLAY_PORT:-8765}"

mkdir -p "${RUNTIME_DIR}"

STOP_WAIT_SECONDS="${STOP_WAIT_SECONDS:-10}"

is_running() {
    if [[ ! -f "${PID_FILE}" ]]; then
        return 1
    fi
    local pid
    pid="$(cat "${PID_FILE}")"
    [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null
}

wait_for_exit() {
    local pid="$1"
    local waited=0
    while kill -0 "${pid}" 2>/dev/null; do
        if (( waited >= STOP_WAIT_SECONDS * 10 )); then
            return 1
        fi
        sleep 0.1
        ((waited += 1))
    done
}

start() {
    if is_running; then
        echo "replay_server is already running, pid=$(cat "${PID_FILE}")"
        return 0
    fi

    rm -f "${PID_FILE}"
    local cmd=(
        "${PYTHON_BIN}"
        "${SCRIPT_DIR}/backend/replay_server.py"
        --host "${REPLAY_HOST}"
        --port "${REPLAY_PORT}"
    )
    if [[ -n "${REPLAY_ROOT}" ]]; then
        cmd=("${PYTHON_BIN}" "${SCRIPT_DIR}/backend/replay_server.py" \
            "${REPLAY_ROOT}" --host "${REPLAY_HOST}" --port "${REPLAY_PORT}")
    fi
    nohup "${cmd[@]}" >> "${LOG_FILE}" 2>&1 &
    local pid=$!
    echo "${pid}" > "${PID_FILE}"
    sleep 1

    if kill -0 "${pid}" 2>/dev/null; then
        echo "replay_server started, pid=${pid}, log=${LOG_FILE}"
        return 0
    fi

    echo "failed to start replay_server, check log=${LOG_FILE}" >&2
    rm -f "${PID_FILE}"
    return 1
}

stop() {
    if ! is_running; then
        echo "replay_server is not running"
        rm -f "${PID_FILE}"
        return 0
    fi

    local pid
    pid="$(cat "${PID_FILE}")"
    kill "${pid}"
    if ! wait_for_exit "${pid}"; then
        echo "replay_server did not stop within ${STOP_WAIT_SECONDS}s, pid=${pid}" >&2
        return 1
    fi
    rm -f "${PID_FILE}"
    echo "replay_server stopped, pid=${pid}"
}

restart() {
    stop
    start
}

usage() {
    cat <<EOF
Usage: $(basename "$0") {start|stop|restart}

Environment overrides:
  PYTHON_BIN   Python executable, default: python3
  REPLAY_ROOT  Replay root path, default: empty (wait for upload)
  REPLAY_HOST  Bind host, default: ${REPLAY_HOST}
  REPLAY_PORT  Bind port, default: ${REPLAY_PORT}
EOF
}

case "${1:-}" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        restart
        ;;
    *)
        usage
        exit 1
        ;;
esac
