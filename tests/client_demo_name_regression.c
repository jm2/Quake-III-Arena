/* Issue #384: CL_PlayDemo_f formed arg + strlen( arg ) - 6 before checking
 * that the demo name was longer than six characters, so "demo ab" made a
 * pointer before the name.  Nothing read through it, but forming it is
 * undefined behaviour.
 *
 * UBSan's pointer-overflow check only reports arithmetic that wraps the
 * address space, and no real string lies within six bytes of address 0.
 * So the real CL_PlayDemo_f gets each short name at the fake address 1, and
 * the only calls that read the name (strstr, strchr, strlen and the
 * Com_sprintf in CL_WalkDemoExt) are sent to its real bytes.  Pointing
 * before a short name then wraps below 0 and is reported, while checking
 * the length first never does arithmetic on the fake address.  Longer names
 * use real addresses and check that the .dm_NN detection and the protocol
 * walk are unchanged. */
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAKE_NAME ( (char *)(uintptr_t)1 )
static const char *NameBytes( const char *s );
#define strlen( s ) strlen( NameBytes( s ) )
#define strchr( s, c ) strchr( NameBytes( s ), c )
#define strstr( s, find ) strstr( NameBytes( s ), find )
#define Com_sprintf DemoSprintf
#include "../code/client/cl_main.c"
#undef strlen
#undef strchr
#undef strstr
#undef Com_sprintf

/* common.c's list, which CL_WalkDemoExt walks in order. */
int demo_protocols[] = { 66, 67, 68, 0 };
cvar_t *com_cl_running;		/* NULL: CL_Disconnect has nothing to shut down */
vm_t *uivm;

static const char *fakeBytes;
static char *argument;
static char probed[8][MAX_OSPATH];
static int numProbed, killServer;
static char printed[1024], dropped[MAX_STRING_CHARS];
static jmp_buf dropJump;

static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Client demo name regression failed: %s\n", message ); exit( 1 ); }
}
static const char *NameBytes( const char *s ) {
	return s == FAKE_NAME ? fakeBytes : s;
}
void QDECL DemoSprintf( char *dest, int size, const char *fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	if ( !strcmp( fmt, "demos/%s.dm_%d" ) ) {
		const char *name = va_arg( args, const char * );
		int protocol = va_arg( args, int );
		snprintf( dest, size, fmt, NameBytes( name ), protocol );
	} else {
		vsnprintf( dest, size, fmt, args );
	}
	va_end( args );
}

/* Engine imports. */
void QDECL Com_Error( int level, const char *format, ... ) {
	va_list args;
	Check( level == ERR_DROP, "only ERR_DROP" );
	va_start( args, format );
	vsnprintf( dropped, sizeof( dropped ), format, args );
	va_end( args );
	longjmp( dropJump, 1 );
}
void QDECL Com_Printf( const char *format, ... ) {
	va_list args;
	size_t used = strlen( printed );
	va_start( args, format );
	vsnprintf( printed + used, sizeof( printed ) - used, format, args );
	va_end( args );
}
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *out, const void *in, size_t size ) { memcpy( out, in, size ); }
void Com_Memset( void *out, int value, size_t size ) { memset( out, value, size ); }
int Cmd_Argc( void ) { return 2; }
char *Cmd_Argv( int arg ) { return arg == 1 ? argument : "demo"; }
void Cvar_Set( const char *name, const char *value ) {
	Check( !strcmp( name, "sv_killserver" ) && !strcmp( value, "1" ), "only the local server is killed" );
	killServer++;
}
/* No demo exists: record each name CL_PlayDemo_f looks for. */
int FS_FOpenFileRead( const char *qpath, fileHandle_t *file, qboolean uniqueFILE ) {
	Check( uniqueFILE, "demos are opened unique" );
	Check( numProbed < (int)( sizeof( probed ) / sizeof( probed[0] ) ), "bounded probes" );
	Q_strncpyz( probed[numProbed++], qpath, sizeof( probed[0] ) );
	*file = 0;
	return -1;
}
/* Kept by the linker for an opened demo or a live client; no test has one. */
int cl_connectedToPureServer;
int FS_Read( void *buffer, int len, fileHandle_t f ) { (void)buffer; (void)len; (void)f; Check( 0, "FS_Read" ); return 0; }
int FS_Write( const void *buffer, int len, fileHandle_t f ) { (void)buffer; (void)f; Check( 0, "FS_Write" ); return len; }
void FS_FCloseFile( fileHandle_t f ) { (void)f; Check( 0, "FS_FCloseFile" ); }
void Con_Close( void ) { Check( 0, "Con_Close" ); }
void CL_ParseServerMessage( msg_t *msg ) { (void)msg; Check( 0, "CL_ParseServerMessage" ); }
void MSG_Init( msg_t *buf, byte *data, int length ) { (void)buf; (void)data; (void)length; Check( 0, "MSG_Init" ); }
int Sys_Milliseconds( void ) { Check( 0, "Sys_Milliseconds" ); return 0; }
char *Cvar_VariableString( const char *name ) { (void)name; Check( 0, "Cvar_VariableString" ); return ""; }
void Cbuf_AddText( const char *text ) { (void)text; Check( 0, "Cbuf_AddText" ); }
void Cbuf_Execute( void ) { Check( 0, "Cbuf_Execute" ); }
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) {
	(void)vm; (void)callNum; (void)args; (void)argCount; Check( 0, "VM_CallArgs" ); return 0;
}
void SCR_StopCinematic( void ) { Check( 0, "SCR_StopCinematic" ); }
void S_ClearSoundBuffer( void ) { Check( 0, "S_ClearSoundBuffer" ); }
void CL_WritePacket( void ) { Check( 0, "CL_WritePacket" ); }

/* Run "demo <name>"; want is the expected probes, each followed by '\n'. */
static void Demo( const char *name, int fake, const char *want, const char *message ) {
	char got[sizeof( probed )];
	char *copy = NULL;
	int i;

	numProbed = killServer = 0;
	printed[0] = dropped[0] = 0;
	if ( fake ) {
		fakeBytes = name;
		argument = FAKE_NAME;
	} else {
		copy = malloc( strlen( name ) + 1 );
		Check( copy != NULL, "name allocation" );
		strcpy( copy, name );
		argument = copy;
	}
	if ( !setjmp( dropJump ) ) {
		CL_PlayDemo_f();
		Check( 0, "a missing demo is dropped" );
	}
	free( copy );
	Check( killServer == 1, "local server killed" );
	got[0] = 0;
	for ( i = 0; i < numProbed; i++ ) {
		Q_strcat( got, sizeof( got ), probed[i] );
		Q_strcat( got, sizeof( got ), "\n" );
	}
	if ( strcmp( got, want ) || strncmp( dropped, "couldn't open ", 14 ) ||
		strcmp( dropped + 14, probed[numProbed - 1] ) ) {
		fprintf( stderr, "demo \"%s\"\nwant:\n%sgot:\n%sdropped: %s\n", name, want, got, dropped );
		Check( 0, message );
	}
}

int main( void ) {
	static const char *const shortNames[] = { "", "a", "ab", "abc", "four", "q3dm1", NULL };
	char want[256];
	int i;

	/* Shorter than six characters: no extension check, every protocol tried. */
	for ( i = 0; shortNames[i]; i++ ) {
		snprintf( want, sizeof( want ), "demos/%s.dm_66\ndemos/%s.dm_67\ndemos/%s.dm_68\n",
			shortNames[i], shortNames[i], shortNames[i] );
		Demo( shortNames[i], 1, want, "short demo name walks every protocol" );
		Demo( shortNames[i], 0, want, "short demo name at a real address walks every protocol" );
		Check( !strstr( printed, "Protocol" ), "short names have no protocol" );
	}

	/* Exactly six characters is still too short for ".dm_NN", as in 1.32c. */
	Demo( ".dm_68", 0, "demos/.dm_68.dm_66\ndemos/.dm_68.dm_67\ndemos/.dm_68.dm_68\n",
		"six-character name walks every protocol" );
	Demo( "q3dm17", 0, "demos/q3dm17.dm_66\ndemos/q3dm17.dm_67\ndemos/q3dm17.dm_68\n",
		"six-character map name walks every protocol" );

	/* A supported .dm_NN (either case of "dm") opens that one file. */
	Demo( "x.dm_68", 0, "demos/x.dm_68\n", "seven-character demo opens directly" );
	Demo( "four.dm_68", 0, "demos/four.dm_68\n", "retail demo name opens directly" );
	Demo( "four.DM_66", 0, "demos/four.DM_66\n", "upper-case extension opens directly" );
	Demo( "four.dM_67", 0, "demos/four.dM_67\n", "mixed-case extension opens directly" );

	/* An unsupported protocol strips the extension and walks the list. */
	Demo( "four.dm_65", 0, "demos/four.dm_66\ndemos/four.dm_67\ndemos/four.dm_68\n",
		"unsupported protocol walks the stripped name" );
	Check( strstr( printed, "Protocol 65 not supported for demos\n" ) != NULL, "unsupported protocol reported" );

	/* ".dm_" must be exact; anything else walks the full name. */
	Demo( "four.dx_68", 0, "demos/four.dx_68.dm_66\ndemos/four.dx_68.dm_67\ndemos/four.dx_68.dm_68\n",
		"other extension walks every protocol" );

	puts( "Short demo names never point before the name; demo lookup unchanged (issue #384)" );
	return 0;
}
