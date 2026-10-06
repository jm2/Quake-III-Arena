/*
 * Issue #395: compile-only check that every printf-style function of the
 * engine and the modules keeps its Q_PRINTF_FORMAT attribute.
 *
 * Each section includes the real headers of one part of the code (they cannot
 * all share one translation unit) and calls each printf-style function once,
 * with "%s" and Q3_GOOD. tests/run_format_attribute_tests.sh builds every
 * section with -Werror=format, which must pass. It then rebuilds the section
 * once per Q3_GOOD line with that one argument replaced by Q3_BAD (an int for
 * "%s"), which must fail with a format error on exactly that line. A function
 * that loses its attribute lets its planted call compile, and the runner
 * fails.
 */

#define Q3_GOOD "text"
#define Q3_BAD 395

#ifdef Q3_FORMAT_TU_ENGINE
#include "server/server.h"
#include "renderer/tr_public.h"
#include "game/botlib.h"

void Q3_FormatCalls( client_t *cl, netadr_t adr, fileHandle_t f, refimport_t *ri, botlib_import_t *bi ) {
	char buffer[64];

	Com_Printf( "%s\n", Q3_GOOD );
	Com_DPrintf( "%s\n", Q3_GOOD );
	Com_Error( ERR_DROP, "%s\n", Q3_GOOD );
	Com_sprintf( buffer, sizeof( buffer ), "%s", Q3_GOOD );
	(void)va( "%s", Q3_GOOD );
	Com_FlightRecord( "%s\n", Q3_GOOD );
	COM_ParseError( "%s", Q3_GOOD );
	COM_ParseWarning( "%s", Q3_GOOD );
	NET_OutOfBandPrint( NS_SERVER, adr, "%s", Q3_GOOD );
	FS_Printf( f, "%s\n", Q3_GOOD );
	Sys_Error( "%s\n", Q3_GOOD );
	SV_SendServerCommand( cl, "%s", Q3_GOOD );
	ri->Printf( PRINT_ALL, "%s\n", Q3_GOOD );
	ri->Error( ERR_DROP, "%s\n", Q3_GOOD );
	bi->Print( PRT_MESSAGE, "%s\n", Q3_GOOD );
}
#endif

#ifdef Q3_FORMAT_TU_BOTLIB
#include "game/q_shared.h"
#include "botlib/l_memory.h"
#include "botlib/l_libvar.h"
#include "botlib/l_utils.h"
#include "botlib/l_script.h"
#include "botlib/l_precomp.h"
#include "botlib/l_struct.h"
#include "botlib/l_log.h"
#include "botlib/aasfile.h"
#include "game/botlib.h"
#include "game/be_aas.h"
#include "botlib/be_aas_funcs.h"
#include "botlib/be_interface.h"
#include "botlib/be_aas_def.h"	// declares AAS_Error under AASINTERN

void Q3_FormatCalls( script_t *script, source_t *source ) {
	ScriptError( script, "%s", Q3_GOOD );
	ScriptWarning( script, "%s", Q3_GOOD );
	SourceError( source, "%s", Q3_GOOD );
	SourceWarning( source, "%s", Q3_GOOD );
	AAS_Error( "%s", Q3_GOOD );
	Log_Write( "%s", Q3_GOOD );
	Log_WriteTimeStamped( "%s", Q3_GOOD );
}
#endif

#ifdef Q3_FORMAT_TU_GAME
#include "game/g_local.h"
#include "game/botlib.h"
#include "game/be_aas.h"
#include "game/be_ea.h"
#include "game/be_ai_char.h"
#include "game/be_ai_chat.h"
#include "game/be_ai_gen.h"
#include "game/be_ai_goal.h"
#include "game/be_ai_move.h"
#include "game/be_ai_weap.h"
#include "game/ai_main.h"

void Q3_FormatCalls( gentity_t *ent ) {
	G_Printf( "%s\n", Q3_GOOD );
	G_Error( "%s\n", Q3_GOOD );
	G_LogPrintf( "%s\n", Q3_GOOD );
	PrintMsg( ent, "%s\n", Q3_GOOD );
	BotAI_Print( PRT_MESSAGE, "%s\n", Q3_GOOD );
}
#endif

#ifdef Q3_FORMAT_TU_CGAME
#include "cgame/cg_local.h"

void Q3_FormatCalls( void ) {
	CG_Printf( "%s\n", Q3_GOOD );
	CG_Error( "%s\n", Q3_GOOD );
}
#endif

#ifdef Q3_FORMAT_TU_UI
#include "ui/ui_local.h"

void Q3_FormatCalls( displayContextDef_t *dc, int handle ) {
	dc->Print( "%s\n", Q3_GOOD );
	dc->Error( ERR_DROP, "%s\n", Q3_GOOD );
	PC_SourceWarning( handle, "%s", Q3_GOOD );
	PC_SourceError( handle, "%s", Q3_GOOD );
}
#endif
