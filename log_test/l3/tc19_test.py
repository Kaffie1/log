#!/usr/bin/env python3

import gzip
from pathlib import Path

from robot_log import l3


def read_text(path: Path) -> str:
    if path.suffix == ".gz":
        with gzip.open(path, "rt", encoding="utf-8") as handle:
            return handle.read()
    return path.read_text(encoding="utf-8")


def main() -> int:
    root = Path("/tmp/l3_tc19_test")
    if root.exists():
        for item in root.iterdir():
            if item.is_file():
                item.unlink()
    else:
        root.mkdir(parents=True, exist_ok=True)

    l3.init("INFO", str(root), "robot-py")
    for i in range(800):
        l3.info("NAVIGATION", f"py_hf_{i}", 1781764689338040 + i)
    l3.shutdown()

    files = sorted(path for path in root.iterdir() if path.is_file())
    text = "".join(read_text(path) for path in files)
    count = sum(1 for i in range(800) if f'"payload":"py_hf_{i}"' in text)
    passed = count == 800
    print(
        f"TC19 result={'PASS' if passed else 'FAIL'} "
        f"summary=python high frequency writes total_files={len(files)} matched={count}"
    )
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
