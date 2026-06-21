import ctypes
import os
from pathlib import Path


def _candidate_library_paths():
    env_path = os.environ.get("LOG_L3_PYTHON_LIB")
    if env_path:
        yield Path(env_path)

    base = Path(__file__).resolve()
    repo_root = base.parents[3]
    yield repo_root / "build_l3" / "log_l3" / "liblog_l3_c_api.so"
    yield repo_root / "build" / "log_l3" / "liblog_l3_c_api.so"


def _load_library():
    for path in _candidate_library_paths():
        if path.exists():
            lib = ctypes.CDLL(str(path))
            lib.naviai_log_l3_init.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
            lib.naviai_log_l3_init.restype = ctypes.c_int
            lib.naviai_log_l3_write.argtypes = [
                ctypes.c_char_p,
                ctypes.c_char_p,
                ctypes.c_char_p,
                ctypes.c_longlong,
            ]
            lib.naviai_log_l3_write.restype = ctypes.c_int
            lib.naviai_log_l3_set_level.argtypes = [ctypes.c_char_p]
            lib.naviai_log_l3_set_level.restype = ctypes.c_int
            lib.naviai_log_l3_set_module_level.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
            lib.naviai_log_l3_set_module_level.restype = ctypes.c_int
            lib.naviai_log_l3_flush.argtypes = []
            lib.naviai_log_l3_shutdown.argtypes = []
            lib.naviai_log_l3_last_error.argtypes = []
            lib.naviai_log_l3_last_error.restype = ctypes.c_char_p
            return lib
    raise RuntimeError("log_l3_c_api shared library was not found")


_LIB = _load_library()


def _check(result):
    if result == 0:
        return
    error = _LIB.naviai_log_l3_last_error()
    raise RuntimeError(error.decode("utf-8") if error else "unknown log_l3 python error")


def init(level: str, root_dir: str, robot_sn: str = ""):
    _check(_LIB.naviai_log_l3_init(level.encode("utf-8"), root_dir.encode("utf-8"), robot_sn.encode("utf-8")))


def set_level(level: str):
    _check(_LIB.naviai_log_l3_set_level(level.encode("utf-8")))


def set_module_level(module: str, level: str):
    _check(_LIB.naviai_log_l3_set_module_level(module.encode("utf-8"), level.encode("utf-8")))


def write(module: str, level: str, payload: str, timestamp_us: int = 0):
    _check(
        _LIB.naviai_log_l3_write(
            module.encode("utf-8"),
            level.encode("utf-8"),
            str(payload).encode("utf-8"),
            int(timestamp_us),
        )
    )


def trace(module: str, payload: str, timestamp_us: int = 0):
    write(module, "TRACE", payload, timestamp_us)


def debug(module: str, payload: str, timestamp_us: int = 0):
    write(module, "DEBUG", payload, timestamp_us)


def info(module: str, payload: str, timestamp_us: int = 0):
    write(module, "INFO", payload, timestamp_us)


def warn(module: str, payload: str, timestamp_us: int = 0):
    write(module, "WARN", payload, timestamp_us)


def error(module: str, payload: str, timestamp_us: int = 0):
    write(module, "ERROR", payload, timestamp_us)


def critical(module: str, payload: str, timestamp_us: int = 0):
    write(module, "CRITICAL", payload, timestamp_us)


def flush():
    _LIB.naviai_log_l3_flush()


def shutdown():
    _LIB.naviai_log_l3_shutdown()
