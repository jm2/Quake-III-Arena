#!/usr/bin/env python3
"""Keep the authoritative ioquake3 format-string call-site fixes in place."""

from dataclasses import dataclass
from pathlib import Path
import sys


ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]

STRING_LITERAL_MACROS = {
    "S_COLOR_BLACK",
    "S_COLOR_RED",
    "S_COLOR_GREEN",
    "S_COLOR_YELLOW",
    "S_COLOR_BLUE",
    "S_COLOR_CYAN",
    "S_COLOR_MAGENTA",
    "S_COLOR_WHITE",
}


@dataclass(frozen=True)
class Token:
    kind: str
    text: str
    line: int


@dataclass(frozen=True)
class Call:
    line: int
    args: tuple[tuple[Token, ...], ...]


def lex_c(source: str) -> tuple[Token, ...]:
    """Tokenize enough C to distinguish calls, literals and comments."""
    tokens: list[Token] = []
    i = 0
    line = 1
    length = len(source)

    while i < length:
        char = source[i]
        if char.isspace():
            line += char == "\n"
            i += 1
            continue

        if source.startswith("//", i):
            i += 2
            while i < length:
                if source[i] == "\\" and i + 1 < length and source[i + 1] == "\n":
                    line += 1
                    i += 2
                elif source[i] == "\n":
                    break
                else:
                    i += 1
            continue

        if source.startswith("/*", i):
            comment_line = line
            end = source.find("*/", i + 2)
            if end < 0:
                raise SystemExit(f"unterminated block comment starting on line {comment_line}")
            line += source.count("\n", i, end + 2)
            i = end + 2
            continue

        prefix_length = 0
        quote = ""
        for prefix in ('u8"', 'L"', 'u"', 'U"', "L'", "u'", "U'", '"', "'"):
            if source.startswith(prefix, i):
                prefix_length = len(prefix) - 1
                quote = prefix[-1]
                break
        if quote:
            start = i
            start_line = line
            i += prefix_length + 1
            while i < length:
                if source[i] == "\\":
                    if i + 1 >= length:
                        break
                    if source[i + 1] == "\n":
                        line += 1
                    i += 2
                    continue
                if source[i] == quote:
                    i += 1
                    tokens.append(
                        Token("string" if quote == '"' else "char", source[start:i], start_line)
                    )
                    break
                if source[i] == "\n":
                    line += 1
                i += 1
            else:
                raise SystemExit(f"unterminated literal starting on line {start_line}")
            if not tokens or tokens[-1].line != start_line or tokens[-1].text != source[start:i]:
                raise SystemExit(f"unterminated literal starting on line {start_line}")
            continue

        if char.isalpha() or char == "_":
            start = i
            i += 1
            while i < length and (source[i].isalnum() or source[i] == "_"):
                i += 1
            tokens.append(Token("identifier", source[start:i], line))
            continue

        # Punctuation is kept as individual tokens. Joining token text therefore
        # normalizes whitespace without losing the shape of an expression.
        tokens.append(Token("punctuation", char, line))
        i += 1

    return tuple(tokens)


def callee_tokens(callee: str) -> tuple[str, ...]:
    return tuple(token.text for token in lex_c(callee))


def parse_call(
    tokens: tuple[Token, ...], opening: int
) -> tuple[tuple[tuple[Token, ...], ...], int]:
    pairs = {")": "(", "]": "[", "}": "{"}
    stack: list[str] = []
    args: list[tuple[Token, ...]] = []
    current: list[Token] = []
    i = opening + 1

    if i < len(tokens) and tokens[i].text == ")":
        return (), i

    while i < len(tokens):
        token = tokens[i]
        if token.text in ("(", "[", "{"):
            stack.append(token.text)
            current.append(token)
        elif token.text in pairs:
            if token.text == ")" and not stack:
                args.append(tuple(current))
                return tuple(args), i
            if not stack or stack[-1] != pairs[token.text]:
                raise SystemExit(f"unbalanced delimiter near line {token.line}")
            stack.pop()
            current.append(token)
        elif token.text == "," and not stack:
            args.append(tuple(current))
            current = []
        else:
            current.append(token)
        i += 1

    raise SystemExit(f"unterminated call starting near line {tokens[opening].line}")


def find_calls(source: str, callee: str) -> tuple[Call, ...]:
    tokens = lex_c(source)
    name = callee_tokens(callee)
    calls: list[Call] = []
    i = 0
    while i + len(name) < len(tokens):
        if tuple(token.text for token in tokens[i : i + len(name)]) != name:
            i += 1
            continue
        if i and tokens[i - 1].text in (".", "->"):
            i += 1
            continue
        opening = i + len(name)
        if tokens[opening].text != "(":
            i += 1
            continue
        args, closing = parse_call(tokens, opening)
        calls.append(Call(tokens[i].line, args))
        i = closing + 1
    return tuple(calls)


def normalized(tokens: tuple[Token, ...]) -> str:
    return "".join(token.text for token in tokens)


def normalized_expr(expression: str) -> str:
    return normalized(lex_c(expression))


def is_literal_format(tokens: tuple[Token, ...]) -> bool:
    return bool(tokens) and all(
        token.kind == "string"
        or (token.kind == "identifier" and token.text in STRING_LITERAL_MACROS)
        for token in tokens
    )


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def audit_literal_formats(path: str, callee: str, format_index: int) -> None:
    calls = find_calls(read(path), callee)
    if not calls:
        raise SystemExit(f"{path}: found no {callee} calls to audit")
    for call in calls:
        if len(call.args) <= format_index or not is_literal_format(call.args[format_index]):
            actual = (
                normalized(call.args[format_index])
                if len(call.args) > format_index
                else "<missing>"
            )
            raise SystemExit(
                f"{path}:{call.line}: {callee} format argument is not a string literal: {actual}"
            )


def require_call(path: str, callee: str, expected: tuple[str, ...], count: int = 1) -> None:
    wanted = tuple(normalized_expr(arg) for arg in expected)
    actual = sum(
        tuple(normalized(arg) for arg in call.args) == wanted
        for call in find_calls(read(path), callee)
    )
    if actual != count:
        rendered = ", ".join(expected)
        raise SystemExit(
            f"{path}: expected {count} copy/copies of {callee}({rendered}), found {actual}"
        )


def require_no_calls(path: str, callee: str) -> None:
    calls = find_calls(read(path), callee)
    if calls:
        lines = ", ".join(str(call.line) for call in calls)
        raise SystemExit(f"{path}: forbidden {callee} call(s) on line(s) {lines}")


def require_tokens(path: str, snippet: str, count: int = 1) -> None:
    haystack = tuple(token.text for token in lex_c(read(path)))
    needle = tuple(token.text for token in lex_c(snippet))
    actual = sum(
        haystack[i : i + len(needle)] == needle
        for i in range(len(haystack) - len(needle) + 1)
    )
    if actual != count:
        raise SystemExit(
            f"{path}: expected {count} token-level copy/copies, found {actual}: {snippet}"
        )


def parser_self_test() -> None:
    fixture = r'''
        // Com_Error(ERR_DROP, "%s", MSG_ReadString(msg));
        /* Com_Error(ERR_DROP, "%s", MSG_ReadString(msg)); */
        Com_Error ( ERR_DROP, format, MSG_ReadString ( msg ) );
        Com_Error(ERR_DROP, "safe " "%s", MSG_ReadString(msg));
    '''
    calls = find_calls(fixture, "Com_Error")
    if len(calls) != 2:
        raise SystemExit(f"parser self-test: comments produced calls ({len(calls)} found)")
    if is_literal_format(calls[0].args[1]):
        raise SystemExit("parser self-test: a nonliteral format was accepted")
    if not is_literal_format(calls[1].args[1]):
        raise SystemExit("parser self-test: adjacent string literals were rejected")
    if normalized(calls[0].args[2]) != "MSG_ReadString(msg)":
        raise SystemExit("parser self-test: spaced nested call was not parsed structurally")


parser_self_test()

# Every audited printf-style sink must use a literal format. Parsing calls makes
# this independent of whitespace and prevents comments from satisfying the audit.
for audit in (
    ("code/botlib/be_aas_main.c", "botimport.Print", 1),
    ("code/botlib/l_script.c", "Com_sprintf", 2),
    ("code/client/cl_cgame.c", "Com_Error", 1),
    ("code/client/cl_main.c", "NET_OutOfBandPrint", 2),
    ("code/client/cl_main.c", "Com_Printf", 0),
    ("code/client/cl_parse.c", "Com_Error", 1),
    ("code/game/ai_dmnet.c", "BotAI_Print", 1),
    ("code/ui/ui_main.c", "Com_sprintf", 2),
    ("code/server/sv_client.c", "NET_OutOfBandPrint", 2),
):
    audit_literal_formats(*audit)

# ioquake3 59c231c6: render untrusted text only through literal formats.
require_call("code/botlib/be_aas_main.c", "botimport.Print", ("PRT_FATAL", '"%s"', "str"))
require_call(
    "code/botlib/l_script.c",
    "Com_sprintf",
    ("basefolder", "sizeof(basefolder)", '"%s"', "path"),
)
require_call(
    "code/client/cl_cgame.c",
    "Com_Error",
    ("ERR_SERVERDISCONNECT", '"Server disconnected - %s"', "Cmd_Argv(1)"),
)
require_call(
    "code/client/cl_main.c",
    "NET_OutOfBandPrint",
    ("NS_CLIENT", "cls.authorizeServer", '"getKeyAuthorize %i %s"', "fs->integer", "nums"),
)
require_call(
    "code/client/cl_main.c",
    "NET_OutOfBandPrint",
    ("NS_SERVER", "to", '"%s"', "command"),
)
require_call(
    "code/client/cl_parse.c",
    "Com_Error",
    ("ERR_DROP", '"%s"', "MSG_ReadString(msg)"),
)
require_call(
    "code/game/ai_dmnet.c",
    "BotAI_Print",
    ("PRT_MESSAGE", '"%s"', "nodeswitch[i]"),
)
require_call(
    "code/game/ai_dmnet.c",
    "BotAI_Print",
    ("PRT_MESSAGE", '"%s"', "nodeswitch[numnodeswitches]"),
)
require_call(
    "code/ui/ui_main.c",
    "Com_sprintf",
    ("scratch", "sizeof(scratch)", '"%s"', "dirptr"),
)
require_call(
    "code/ui/ui_main.c",
    "Com_sprintf",
    (
        "uiInfo.q3HeadNames[uiInfo.q3HeadCount]",
        "sizeof(uiInfo.q3HeadNames[uiInfo.q3HeadCount])",
        '"%s"',
        "scratch",
    ),
)

# ioquake3 8ca8d845: bound the common datagram sink and keep authorize text data.
require_call(
    "code/qcommon/net_chan.c",
    "Q_vsnprintf",
    ("string + 4", "sizeof(string) - 4", "format", "argptr"),
)
require_tokens("code/qcommon/net_chan.c", "string[sizeof(string) - 1] = '\\0';")
require_no_calls("code/qcommon/net_chan.c", "vsprintf")

require_call(
    "code/server/sv_client.c",
    "NET_OutOfBandPrint",
    ("NS_SERVER", "svs.challenges[i].adr", '"print\\n%s\\n"', "r"),
    count=2,
)

# The local audit adds the same capacity guarantee to the native fatal sink.
require_call(
    "code/qcommon/common.c",
    "Q_vsnprintf",
    ("com_errorMessage", "sizeof(com_errorMessage)", "fmt", "argptr"),
)
require_tokens(
    "code/qcommon/common.c",
    "com_errorMessage[sizeof(com_errorMessage) - 1] = '\\0';",
)
require_no_calls("code/qcommon/common.c", "vsprintf")

print("Known native format-string call-site contract passes")
