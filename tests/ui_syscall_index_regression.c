/* Issue #238: drive the real UI dispatcher with key numbers and ping slots. */
#include "../code/client/cl_ui.c"
#include "client_syscall_stubs.h"

#define COUNT(array) ((int)(sizeof(array) / sizeof((array)[0])))

/** Reject out-of-table indices on every key and ping trap and keep valid results unchanged. */
int main( void ) {
	const int badKeys[] = {-2, MAX_KEYS, INT_MIN, INT_MAX};
	const int goodKeys[] = {-1, 0, 'a', MAX_KEYS - 1};
	const int badSlots[] = {-2, -1, MAX_PINGREQUESTS, INT_MIN, INT_MAX};
	const int goodSlots[] = {0, MAX_PINGREQUESTS - 1};
	int args[5], i, key, slot;
	char *text;
	SetupVM();
	text = (char *)vm.dataBase;
	strcpy( text + 16, "+attack" );
	for ( i = 0; i < COUNT(badKeys); i++ ) {
		args[0] = UI_KEY_SETBINDING; args[1] = badKeys[i]; args[2] = 16;
		Reject( CL_UISystemCalls, args );
		args[0] = UI_KEY_GETBINDINGBUF; args[2] = 64; args[3] = 32;
		Reject( CL_UISystemCalls, args );
		args[0] = UI_KEY_ISDOWN;
		Reject( CL_UISystemCalls, args );
	}
	args[0] = UI_KEY_GETBINDINGBUF; args[1] = 'b'; args[2] = 64; args[3] = 32;
	memset( text + 64, 'x', 32 );
	Accept( CL_UISystemCalls, args, 'b' );
	Check( text[64] == 0, "unbound key reads empty" );
	for ( i = 0; i < COUNT(goodKeys); i++ ) {
		key = goodKeys[i];
		args[0] = UI_KEY_SETBINDING; args[1] = key; args[2] = 16;
		Accept( CL_UISystemCalls, args, key );
		Check( key == -1 || !strcmp( bindings[key], "+attack" ), "valid binding stored" );
		args[0] = UI_KEY_GETBINDINGBUF; args[2] = 64; args[3] = 32;
		memset( text + 64, 'x', 32 );
		Accept( CL_UISystemCalls, args, key );
		Check( !strcmp( text + 64, key == -1 ? "" : "+attack" ), "valid binding read" );
		if ( key != -1 ) keyDown[key] = qtrue;
		args[0] = UI_KEY_ISDOWN;
		Check( Accept( CL_UISystemCalls, args, key ) == (key != -1), "valid key state" );
	}
	Q_strncpyz( pings[0].info, "\\hostname\\first", sizeof(pings[0].info) );
	Q_strncpyz( pings[MAX_PINGREQUESTS - 1].info, "\\hostname\\last", sizeof(pings[0].info) );
	pings[0].time = 42; pings[MAX_PINGREQUESTS - 1].time = 99;
	for ( i = 0; i < COUNT(badSlots); i++ ) {
		args[0] = UI_LAN_GETPING; args[1] = badSlots[i]; args[2] = 64; args[3] = 32; args[4] = 128;
		Reject( CL_UISystemCalls, args );
		args[0] = UI_LAN_GETPINGINFO;
		Reject( CL_UISystemCalls, args );
	}
	for ( i = 0; i < COUNT(goodSlots); i++ ) {
		slot = goodSlots[i];
		args[0] = UI_LAN_GETPING; args[1] = slot; args[2] = 64; args[3] = 32; args[4] = 128;
		memset( text + 64, 'x', 32 ); *(int *)(text + 128) = -1;
		Accept( CL_UISystemCalls, args, slot );
		Check( !strcmp( text + 64, pings[slot].info ) && *(int *)(text + 128) == pings[slot].time,
		       "valid ping read" );
		args[0] = UI_LAN_GETPINGINFO;
		memset( text + 64, 'x', 32 );
		Accept( CL_UISystemCalls, args, slot );
		Check( !strcmp( text + 64, pings[slot].info ), "valid ping info read" );
	}
	free( vm.dataBase );
	puts( "UI syscall index regressions passed (issue #238)" );
	return 0;
}
