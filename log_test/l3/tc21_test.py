#!/usr/bin/env python3

import ctypes
import gzip
import os
from pathlib import Path

from robot_log import l3


def read_text(path: Path) -> str:
    if path.suffix == ".gz":
        with gzip.open(path, "rt", encoding="utf-8") as handle:
            return handle.read()
    return path.read_text(encoding="utf-8")


def load_c_api():
    lib_path = os.environ["LOG_L3_PYTHON_LIB"]
    lib = ctypes.CDLL(lib_path)
    lib.naviai_log_l3_init.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
    lib.naviai_log_l3_init.restype = ctypes.c_int
    lib.naviai_log_l3_write.argtypes = [
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.c_longlong,
    ]
    lib.naviai_log_l3_write.restype = ctypes.c_int
    lib.naviai_log_l3_shutdown.argtypes = []
    return lib


def main() -> int:
    root = Path("/tmp/l3_tc21_test")
    if root.exists():
        for item in root.iterdir():
            if item.is_file():
                item.unlink()
    else:
        root.mkdir(parents=True, exist_ok=True)

    lib = load_c_api()
    rc = lib.naviai_log_l3_init(b"INFO", str(root).encode("utf-8"), b"robot-mixed")
    if rc != 0:
        print("TC21 result=FAIL summary=mixed python/cpp write c_api_init_failed")
        return 1
    rc = lib.naviai_log_l3_write(
        b"NAVIGATION", b"INFO", b"cpp side message", 1781764689338040
    )
    if rc != 0:
        print("TC21 result=FAIL summary=mixed python/cpp write c_api_write_failed")
        return 1
    lib.naviai_log_l3_shutdown()

    l3.init("INFO", str(root), "robot-mixed")
    l3.info("SCHEDULER", "python side message", 1781764689407759)
    l3.shutdown()

    files = sorted(path for path in root.iterdir() if path.is_file())
    text = "".join(read_text(path) for path in files)
    passed = (
        '"payload":"cpp side message"' in text
        and '"payload":"python side message"' in text
        and text.count('"robot_sn":"robot-mixed"') >= 2
    )
    print(
        f"TC21 result={'PASS' if passed else 'FAIL'} "
        f"summary=python cpp mixed directory consistency total_files={len(files)}"
    )
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
