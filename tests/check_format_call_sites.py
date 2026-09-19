#!/usr/bin/env python3
"""Keep the authoritative ioquake3 format-string call-site fixes in place."""

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
            f"{path}: expected {count} copy/copies of safe call site, found {actual}: "
            f"{snippet}"
        )


def reject(path: str, snippet: str) -> None:
    if re.sub(r"\s+", "", snippet) in compact(path):
        raise SystemExit(f"{path}: vulnerable format call site returned: {snippet}")


# ioquake3 59c231c6: render untrusted text only through literal formats.
require("code/botlib/be_aas_main.c", 'botimport.Print(PRT_FATAL, "%s", str);')
reject("code/botlib/be_aas_main.c", "botimport.Print(PRT_FATAL, str);")

require(
    "code/botlib/l_script.c",
    'Com_sprintf(basefolder, sizeof(basefolder), "%s", path);',
)
reject("code/botlib/l_script.c", "Com_sprintf(basefolder, sizeof(basefolder), path);")

require(
    "code/client/cl_cgame.c",
    'Com_Error(ERR_SERVERDISCONNECT, "Server disconnected - %s", Cmd_Argv(1));',
)
reject("code/client/cl_cgame.c", "Com_Error(ERR_SERVERDISCONNECT, va(")

require(
    "code/client/cl_main.c",
    'NET_OutOfBandPrint(NS_CLIENT, cls.authorizeServer, '
    '"getKeyAuthorize %i %s", fs->integer, nums);',
)
require(
    "code/client/cl_main.c",
    'NET_OutOfBandPrint(NS_SERVER, to, "%s", command);',
)
reject("code/client/cl_main.c", "NET_OutOfBandPrint(NS_CLIENT, cls.authorizeServer, va(")
reject("code/client/cl_main.c", "NET_OutOfBandPrint(NS_SERVER, to, command);")
reject("code/client/cl_main.c", "Com_Printf(buffer);")

require(
    "code/client/cl_parse.c",
    'Com_Error(ERR_DROP, "%s", MSG_ReadString(msg));',
)
reject("code/client/cl_parse.c", "Com_Error(ERR_DROP, MSG_ReadString(msg));")

require(
    "code/game/ai_dmnet.c",
    'BotAI_Print(PRT_MESSAGE, "%s", nodeswitch[i]);',
)
require(
    "code/game/ai_dmnet.c",
    'BotAI_Print(PRT_MESSAGE, "%s", nodeswitch[numnodeswitches]);',
)
reject("code/game/ai_dmnet.c", "BotAI_Print(PRT_MESSAGE, nodeswitch[")

require(
    "code/ui/ui_main.c",
    'Com_sprintf(scratch, sizeof(scratch), "%s", dirptr);',
)
require(
    "code/ui/ui_main.c",
    "Com_sprintf(uiInfo.q3HeadNames[uiInfo.q3HeadCount], "
    'sizeof(uiInfo.q3HeadNames[uiInfo.q3HeadCount]), "%s", scratch);',
)
reject("code/ui/ui_main.c", "Com_sprintf(scratch, sizeof(scratch), dirptr);")
reject(
    "code/ui/ui_main.c",
    "Com_sprintf(uiInfo.q3HeadNames[uiInfo.q3HeadCount], "
    "sizeof(uiInfo.q3HeadNames[uiInfo.q3HeadCount]), scratch);",
)

# ioquake3 8ca8d845: bound the common datagram sink and keep authorize text data.
require(
    "code/qcommon/net_chan.c",
    "Q_vsnprintf(string + 4, sizeof(string) - 4, format, argptr);",
)
require("code/qcommon/net_chan.c", "string[sizeof(string) - 1] = '\\0';")
reject("code/qcommon/net_chan.c", "vsprintf(string + 4, format, argptr);")

require(
    "code/server/sv_client.c",
    'NET_OutOfBandPrint(NS_SERVER, svs.challenges[i].adr, "print\\n%s\\n", r);',
    count=2,
)
reject("code/server/sv_client.c", "NET_OutOfBandPrint(NS_SERVER, svs.challenges[i].adr, ret);")

# The local audit adds the same capacity guarantee to the native fatal sink.
require(
    "code/qcommon/common.c",
    "Q_vsnprintf(com_errorMessage, sizeof(com_errorMessage), fmt, argptr);",
)
require(
    "code/qcommon/common.c",
    "com_errorMessage[sizeof(com_errorMessage) - 1] = '\\0';",
)
reject("code/qcommon/common.c", "vsprintf(com_errorMessage, fmt, argptr);")

print("Known native format-string call-site contract passes")
