#!/usr/bin/env python3
"""Keep shipping static/QVM formatting on capacity-aware sinks."""

from pathlib import Path
import re
import sys


ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]


def compact(path: str) -> str:
    return re.sub(r"\s+", "", (ROOT / path).read_text(encoding="utf-8"))


def require(path: str, snippet: str, count: int = 1) -> None:
    actual = compact(path).count(re.sub(r"\s+", "", snippet))
    if actual != count:
        raise SystemExit(
            f"{path}: expected {count} copy/copies, found {actual}: {snippet}"
        )


shipping_sources = (
    "code/game/q_shared.c",
    "code/game/g_main.c",
    "code/cgame/cg_main.c",
    "code/q3_ui/ui_atoms.c",
    "code/ui/ui_atoms.c",
    "code/ui/ui_shared.c",
)
for source in shipping_sources:
    if "vsprintf(" in compact(source):
        raise SystemExit(f"{source}: unbounded vsprintf remains in a shipping module")

require(
    "code/game/bg_lib.h",
    "int Q_vsnprintf(char *buffer, size_t length, const char *fmt, va_list argptr);",
)
require(
    "code/game/bg_lib.c",
    "int Q_vsnprintf(char *str, size_t length, const char *fmt, va_list args)",
)
require(
    "code/game/bg_lib.c",
    "if(maxlen > 0) buffer[currlen] = '\\0';",
)
require(
    "code/game/bg_lib.c",
    "if(*currlen + 1 < maxlen) buffer[(*currlen)++] = c;",
)

header = compact("code/game/q_shared.h")
if "#defineQ_vsnprintf(buffer,length,format,argptr)vsprintf" in header:
    raise SystemExit("code/game/q_shared.h: QVM formatter still aliases vsprintf")

require(
    "code/game/q_shared.c",
    "Q_vsnprintf(dest, size, fmt, argptr);",
)
require(
    "code/game/q_shared.c",
    "Q_vsnprintf(buf, sizeof(string[0]), format, argptr);",
)
require(
    "code/game/q_shared.c",
    "Q_vsnprintf(string, sizeof(string), format, argptr);",
    count=2,
)
require(
    "code/game/q_shared.c",
    "string[sizeof(string) - 1] = '\\0';",
    count=2,
)
require("code/game/q_shared.c", "dest[size - 1] = '\\0';")
require("code/game/q_shared.c", "buf[sizeof(string[0]) - 1] = '\\0';")

for source, calls, terminator in (
    ("code/game/g_main.c", 4, "text[sizeof(text) - 1] = '\\0';"),
    ("code/cgame/cg_main.c", 4, "text[sizeof(text) - 1] = '\\0';"),
    ("code/q3_ui/ui_atoms.c", 2, "text[sizeof(text) - 1] = '\\0';"),
    ("code/ui/ui_atoms.c", 2, "text[sizeof(text) - 1] = '\\0';"),
    ("code/ui/ui_shared.c", 2, "string[sizeof(string) - 1] = '\\0';"),
):
    text = compact(source)
    expected = "Q_vsnprintf(" if source == "code/ui/ui_shared.c" else "Q_vsnprintf(text,sizeof(text),"
    actual = text.count(expected)
    if actual < calls:
        raise SystemExit(f"{source}: expected at least {calls} bounded formatter calls, found {actual}")
    actual_terminators = text.count(re.sub(r"\s+", "", terminator))
    if actual_terminators < calls:
        raise SystemExit(
            f"{source}: expected at least {calls} explicit terminators, "
            f"found {actual_terminators}"
        )

print("Shipping static/QVM format-call contract passes")
