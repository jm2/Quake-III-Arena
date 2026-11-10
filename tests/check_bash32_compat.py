#!/usr/bin/env python3
"""Reject bash 4+ constructs in the scripts macOS users run.

macOS ships /bin/bash 3.2, and `bash build_mac.sh` there runs it (as do
setup_retro68.sh and check_retro68.sh, which build_mac.sh calls). CI has no
bash 3.2, so `bash -n` there accepts what bash 3.2 rejects: a case pattern
without its leading "(" inside $( ) is a syntax error in bash 3.2 (it ends
the substitution), and build_mac.sh then stops with "could not check the
Retro68 toolchain (exit status 2)". This lint looks for that and for the
bash 4+ features listed in RULES. A line ending in "# bash32-ok" is skipped.

    python3 tests/check_bash32_compat.py [--self-test] [FILE...]
    python3 tests/check_bash32_compat.py --list

Without FILE it checks MACOS_SCRIPTS, which --list prints.
tests/check_shell_syntax.sh runs it; set Q3_BASH32=/path/to/bash-3.2 there
to parse those scripts with a real bash 3.2 as well.
"""

import os
import re
import sys

# The scripts a macOS user runs with /bin/bash, and what they run.
MACOS_SCRIPTS = [
    "build_mac.sh",
    "setup_retro68.sh",
    "check_retro68.sh",
    "tests/run_retro68_readiness_tests.sh",
]

NAME = r"[A-Za-z_][A-Za-z0-9_]*"
RULES = [
    (re.compile(r"\$\{[#!]?" + NAME + r"(\[[^]]*\])?(,,?|\^\^?)"),
     "${var,,} / ${var^^} case conversion (bash 4.0)"),
    (re.compile(r"\$\{" + NAME + r"@[QEPAaKkUuL]\}"),
     "${var@op} transformation (bash 4.4)"),
    (re.compile(r"\b(declare|local|typeset)\s+-[A-Za-z]*A"),
     "associative array (bash 4.0)"),
    (re.compile(r"\b(declare|local|typeset)\s+-[A-Za-z]*n\b"),
     "nameref (bash 4.3)"),
    (re.compile(r"\bdeclare\s+-[A-Za-z]*g"), "declare -g (bash 4.2)"),
    (re.compile(r"(^|[;&|(\s])(mapfile|readarray|coproc)\b"),
     "mapfile/readarray/coproc (bash 4.0)"),
    (re.compile(r"&>>"), "&>> redirection (bash 4.0)"),
    (re.compile(r"\|&"), "|& pipe (bash 4.0)"),
    (re.compile(r";;&|(?<![;&]);&(?!&)"), ";& / ;;& case terminator (bash 4.0)"),
    (re.compile(r"\$\{" + NAME + r"\[-[0-9]"), "negative array index (bash 4.3)"),
    (re.compile(r"\$\{" + NAME + r":[^}:]*:\s*-"),
     "negative substring length (bash 4.2)"),
    (re.compile(r"(\[\[|\btest|\[)\s+-v\s"), "-v variable test (bash 4.2)"),
    (re.compile(r"\bwait\s+-[A-Za-z]*n"), "wait -n (bash 4.3)"),
    (re.compile(r"\bshopt\s+-s\s+.*\b(globstar|lastpipe|autocd|direxpand)\b"),
     "bash 4+ shopt option"),
    (re.compile(r"(?<!\$)\{" + NAME + r"\}[<>]"), "{var}> automatic fd (bash 4.1)"),
    (re.compile(r"\$\{?(EPOCHSECONDS|EPOCHREALTIME|BASHPID|BASH_ARGV0|SRANDOM)\b"),
     "bash 4+/5 variable"),
    (re.compile(r"%\([^)]*\)T"), "printf %(...)T (bash 4.2)"),
    (re.compile(r"\{-?[0-9]+\.\.-?[0-9]+\.\.-?[0-9]+\}"),
     "brace expansion step (bash 4.0)"),
]
# "${name[@]}" with name empty fails under set -u before bash 4.4; the
# ${name[@]+"${name[@]}"} form expands to nothing instead.
ARRAY_ALL = re.compile(r'(?<!\+)"\$\{(' + NAME + r')\[[@*]\]\}"')
SET_U = re.compile(r"^\s*set\s+-[A-Za-z]*u|^\s*set\s+-o\s+nounset", re.M)
HEREDOC = re.compile(r"<<(-?)[ \t]*(['\"]?)\\?(" + NAME + r")\2")
CASE_IN = re.compile(r"case[ \t]+[^ \t\n;|&()<>]+[ \t]+in(?=[ \t\n;(])")


def scan(text):
    """Returns (code, lines): the text with comments, single-quoted text and
    here-document bodies blanked (newlines kept), and the line numbers of
    case patterns without a leading "(" inside $( ).

    A small shell scanner: it follows quotes, comments, here-documents,
    $( ), $(( )), ( ) and case ... esac, which is what bash 3.2 gets wrong.
    """
    out = list(text)
    found = []
    stack = []      # [kind, expecting a case pattern]; kind: sub arith par case dq
    heredocs = []   # (tag, strip tabs) waiting for the end of the line
    line = 1
    word_start = True
    j = 0
    n = len(text)

    def blank(start, end):
        for k in range(start, end):
            if out[k] != "\n":
                out[k] = " "

    while j < n:
        c = text[j]
        top = stack[-1][0] if stack else None
        if c == "\n":
            line += 1
            j += 1
            word_start = True
            while heredocs:
                tag, tabs = heredocs.pop(0)
                start = j
                while j < n:
                    end = text.find("\n", j)
                    end = n if end < 0 else end
                    body = text[j:end].rstrip("\r")
                    j = min(end + 1, n)
                    line += 1
                    if (body.lstrip("\t") if tabs else body) == tag:
                        break
                blank(start, j)
            continue
        if c == "\\":
            if j + 1 < n and text[j + 1] == "\n":
                line += 1
            j += 2
            word_start = False
            continue
        if text.startswith("$((", j):
            stack.append(["arith", False])
            j += 3
            continue
        if text.startswith("$(", j):
            stack.append(["sub", False])
            j += 2
            word_start = True
            continue
        if top == "dq":
            if c == '"':
                stack.pop()
            j += 1
            continue
        if c == "'" or text.startswith("$'", j):
            ansi = c == "$"
            k = j + (2 if ansi else 1)
            while k < n and text[k] != "'":
                if ansi and text[k] == "\\":
                    k += 1
                k += 1
            line += text.count("\n", j, k)
            blank(j + (2 if ansi else 1), k)
            j = k + 1
            word_start = False
            continue
        if c == '"':
            stack.append(["dq", False])
            j += 1
            word_start = False
            continue
        if c == "#" and word_start:
            end = text.find("\n", j)
            end = n if end < 0 else end
            blank(j, end)
            j = end
            continue
        if text.startswith("<<", j) and not text.startswith("<<<", j):
            m = HEREDOC.match(text, j)
            if m:
                heredocs.append((m.group(3), m.group(1) == "-"))
                j = m.end()
                continue
        if top == "case" and stack[-1][1]:
            if c in " \t;":
                j += 1
                continue
            if re.match(r"esac\b", text[j:j + 5]):
                stack.pop()
                j += 4
                word_start = False
                continue
            leading = c == "("
            end = text.find(")", j + 1 if leading else j)
            if not leading and any(entry[0] == "sub" for entry in stack):
                found.append(line)
            if end < 0:
                break
            line += text.count("\n", j, end)
            stack[-1][1] = False
            j = end + 1
            word_start = True
            continue
        if c == "(":
            stack.append(["par", False])
            j += 1
            word_start = True
            continue
        if c == ")":
            if top == "arith" and text.startswith("))", j):
                stack.pop()
                j += 2
            else:
                if top in ("sub", "par", "arith"):
                    stack.pop()
                j += 1
            word_start = False
            continue
        if top == "case" and text.startswith(";;", j):
            stack[-1][1] = True
            j += 3 if text.startswith(";;&", j) else 2
            continue
        if top == "case" and text.startswith(";&", j):
            stack[-1][1] = True
            j += 2
            continue
        if word_start:
            m = CASE_IN.match(text, j)
            if m:
                stack.append(["case", True])
                j = m.end()
                continue
            if top == "case" and re.match(r"esac\b", text[j:j + 5]):
                stack.pop()
                j += 4
                word_start = False
                continue
        word_start = c in " \t;|&"
        j += 1
    return "".join(out), found


def check_text(text):
    code, case_lines = scan(text)
    raw_lines = text.split("\n")
    waived = set(number for number, raw in enumerate(raw_lines, 1)
                 if raw.rstrip().endswith("# bash32-ok"))
    problems = []
    for number in case_lines:
        problems.append((number, 'case pattern without a leading "(" inside $( ) (bash 3.2 cannot parse it; write "(pattern)")'))
    set_u = SET_U.search(code) is not None
    for number, line in enumerate(code.split("\n"), 1):
        for rule, message in RULES:
            if rule.search(line):
                problems.append((number, message))
        if set_u:
            for match in ARRAY_ALL.finditer(line):
                name = match.group(1)
                if name == "BASH_SOURCE":
                    continue
                problems.append((number, '"${%s[@]}" under set -u fails on an empty array before bash 4.4; write ${%s[@]+"${%s[@]}"}' % (name, name, name)))
    return sorted(set(p for p in problems if p[0] not in waived))


SELF_TEST_BAD = [
    'x=$(case "$a" in m68k-*) echo y ;; esac)',
    'x="$(case "$a" in\n    a) echo y ;;\n    b) echo z ;;\nesac)"',
    'y=$(for f in *; do case "$f" in (a) ;; b) continue ;; esac; done)',
    'echo "${name,,}"',
    'echo "${name^^}"',
    'declare -A map',
    'local -A map',
    'mapfile -t lines < f',
    'readarray lines < f',
    'cmd &>> log',
    'cmd |& tee log',
    'case $a in a) echo ;;& b) echo ;; esac',
    'echo "${arr[-1]}"',
    'echo "${s:0:-1}"',
    '[[ -v name ]]',
    'set -u\na=()\nfor x in "${a[@]}"; do :; done',
    'exec {fd}> file',
    'wait -n',
]
SELF_TEST_GOOD = [
    'x=$(case "$a" in (m68k-*) echo y ;; esac)',
    'case "$a" in m68k-*) echo y ;; esac',
    'x=$(echo "$(basename "$0")")',
    'x=$((1 + (2 * 3)))',
    "echo 'x=$(case a in a) ;; esac)'",
    '# x=$(case a in a) ;; esac)',
    'echo "${file##*/}" "${#arr[@]}" "${x:-1}" "${x: -1}"',
    'set -u\na=()\necho ${a[@]+"${a[@]}"} "$@" "${BASH_SOURCE[0]}"',
    'a=(); for x in "${a[@]}"; do :; done',
    'cmd > /dev/null 2>&1 && other',
    'cat <<EOF\nx=$(case a in a) ;; esac)\nEOF',
    'x=$(case a in a) ;; esac)  # bash32-ok',
]


def self_test():
    failed = 0
    for text in SELF_TEST_BAD:
        if not check_text(text):
            print("self-test: not flagged: %r" % text)
            failed = 1
    for text in SELF_TEST_GOOD:
        problems = check_text(text)
        if problems:
            print("self-test: wrongly flagged: %r: %s" % (text, problems))
            failed = 1
    return failed


def main(argv):
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    args = argv[1:]
    status = 0
    if args == ["--list"]:
        print("\n".join(MACOS_SCRIPTS))
        return 0
    if args and args[0] == "--self-test":
        args = args[1:]
        status = self_test()
        if status == 0:
            print("bash 3.2 lint self-test passed (%d bad, %d good snippets)."
                  % (len(SELF_TEST_BAD), len(SELF_TEST_GOOD)))
    files = args or [os.path.join(root, name) for name in MACOS_SCRIPTS]
    for path in files:
        try:
            with open(path, encoding="utf-8") as handle:
                text = handle.read()
        except OSError as error:
            print("%s: %s" % (path, error))
            status = 1
            continue
        for number, message in check_text(text):
            print("%s:%d: %s" % (os.path.relpath(path, root), number, message))
            status = 1
    if status == 0:
        print("No bash 4+ constructs in %d macOS-facing scripts." % len(files))
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv))
