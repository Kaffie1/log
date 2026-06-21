#!/usr/bin/env python3

from robot_log import l3


def main() -> int:
    threw = False
    try:
        l3.init("INFO", "/dev/null/log", "robot-py")
        l3.shutdown()
    except Exception:
        threw = True

    print(
        f"TC20 result={'PASS' if threw else 'FAIL'} "
        "summary=python invalid root path explicit failure"
    )
    return 0 if threw else 1


if __name__ == "__main__":
    raise SystemExit(main())
