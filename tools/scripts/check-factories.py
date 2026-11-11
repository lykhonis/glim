#!/usr/bin/env python3
"""Gate: props-form factory rule.

Scope: element factories (packages/glim-toolkit/include/.../elements/),
smarthome widgets, and themed(). For each Block/auto-returning factory:

Rule A (props-form): a factory (template or not) with 2+ fixed params
must take the element struct instead. Allowlist: tap (child is the
identity), list (count+generator, container-like).

Rule B (forwarding): any factory taking As&&... must forward every
trailing arg (std::forward<As>(as)...) and wrap via finish().

Usage: python3 tools/scripts/check-factories.py (exit 1 on violation).
"""
import pathlib
import re
import sys

ALLOW_MULTIFIXED = {"tap", "list"}

ROOT = pathlib.Path(__file__).resolve().parents[2]
DIRS = [
    ROOT / "packages/glim-toolkit/include/glim/toolkit/elements",
    ROOT / "examples/smarthome/src/widgets",
    ROOT / "examples/infotainment/src/widgets",
]
FILES = [ROOT / "packages/glim-toolkit/include/glim/toolkit/theme/Themed.h"]

SIG = re.compile(
    r"^(?:template\s*<[^;{}]*>\s*)?(?:inline\s+)?(?:Block|auto)\s+"
    r"([A-Za-z_]\w*)\s*\(",
    re.M,
)


def balanced(s, i, open_c, close_c):
    depth = 0
    j = i
    while j < len(s):
        c = s[j]
        if c == '"' or c == "'":
            j += 1
            while j < len(s) and s[j] != c:
                j += 2 if s[j] == "\\" else 1
            j += 1
            continue
        if c == "/" and j + 1 < len(s) and s[j + 1] == "/":
            while j < len(s) and s[j] != "\n":
                j += 1
            continue
        if c == open_c:
            depth += 1
        elif c == close_c:
            depth -= 1
            if depth == 0:
                return j
        j += 1
    raise ValueError("unbalanced")


def split_top(params):
    parts, depth, cur = [], 0, ""
    for ch in params:
        if ch in "(<[{":
            depth += 1
        elif ch in ")>]}":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    parts.append(cur)
    return [p.strip() for p in parts if p.strip()]


def check_file(path):
    errors = []
    s = path.read_text()
    for m in SIG.finditer(s):
        name = m.group(1)
        line = s[: m.start()].count("\n") + 1
        pend = balanced(s, m.end() - 1, "(", ")")
        params = split_top(s[m.end() : pend])
        fixed = [p for p in params if "..." not in p]
        fwd = any("..." in p for p in params)
        is_template = bool(re.match(r"\s*template\s*<", s[m.start() : m.end()]))
        if len(fixed) >= 2 and name not in ALLOW_MULTIFIXED:
            errors.append(
                f"{path}:{line}: {name} takes {len(fixed)} fixed params "
                f"({', '.join(fixed)}) - pass the element struct instead"
            )
        if fwd:
            bend = s.find("{", pend)
            body = s[bend : balanced(s, bend, "{", "}") + 1]
            if "std::forward<As>(as)..." not in body:
                errors.append(
                    f"{path}:{line}: {name} takes As&&... without forwarding"
                )
            if "finish(" not in body:
                errors.append(
                    f"{path}:{line}: {name} takes As&&... without finish()"
                )
    return errors


def main():
    targets = [f for d in DIRS for f in sorted(d.glob("*.h"))] + FILES
    errors = []
    count = 0
    for f in targets:
        # count factories for the report
        count += len(SIG.findall(f.read_text()))
        errors += check_file(f)
    if errors:
        print("\n".join(errors))
        return 1
    print(f"factories OK: {count} checked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
