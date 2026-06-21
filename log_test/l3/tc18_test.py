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
    root = Path("/tmp/l3_tc18_test")
    if root.exists():
        for item in root.iterdir():
            if item.is_file():
                item.unlink()
    else:
        root.mkdir(parents=True, exist_ok=True)

    l3.init("ERROR", str(root), "robot-py")
    l3.set_module_level("NAVIGATION", "INFO")
    l3.info("NAVIGATION", "python nav info visible", 1781764689338040)
    l3.info("SCHEDULER", "python scheduler info hidden", 1781764689407759)
    l3.shutdown()

    files = sorted(path for path in root.iterdir() if path.is_file())
    text = "".join(read_text(path) for path in files)
    passed = (
        '"payload":"python nav info visible"' in text
        and '"payload":"python scheduler info hidden"' not in text
    )
    print(
        f"TC18 result={'PASS' if passed else 'FAIL'} "
        f"summary=python module level override total_files={len(files)}"
    )
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
