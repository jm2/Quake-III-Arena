#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "../client/client.h"
#include "mac_local.h"

// Game/UI API Glue
#include "../game/g_public.h"
#include "../cgame/cg_public.h"
#include "../ui/ui_public.h"

// Export structs
typedef struct {
    int    apiversion;
    int    (*vmMain)( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );
} game_export_t;

typedef struct {
    int    apiversion;
    int    (*vmMain)( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );
} ui_export_t;

extern int Game_vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );
extern int UI_vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );

game_export_t *GetGameAPI( gameImport_t *import ) {
    static game_export_t export;
    export.apiversion = GAME_API_VERSION;
    export.vmMain = Game_vmMain;
    return &export;
}

ui_export_t *GetUIAPI( uiImport_t *import ) {
    static ui_export_t export;
    export.apiversion = UI_API_VERSION;
    export.vmMain = UI_vmMain;
    return &export;
}

void Sys_UnloadBotLib( void ) {
}

void *Sys_GetGameAPI( void *parms ) {
    return GetGameAPI( (gameImport_t *)parms );
}

void *Sys_GetUIAPI( void ) {
    return GetUIAPI( NULL );
}

// String helpers
int PStringToCString( char *s ) {
    int len = (unsigned char)s[0];
    int i;
    for (i=0; i<len; i++) s[i] = s[i+1];
    s[len] = 0;
    return len;
}

int CStringToPString( char *s ) {
    int len = strlen(s);
    if (len > 255) len = 255;
    int i;
    for (i=len; i>0; i--) s[i] = s[i-1];
    s[0] = (char)len;
    return len;
}

// Event Queue
#define MAX_MAC_EVENTS 256
static sysEvent_t eventQue[MAX_MAC_EVENTS];
static int eventHead = 0;
static int eventTail = 0;

void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) {
    sysEvent_t *ev;
    int next = (eventHead + 1) % MAX_MAC_EVENTS;
    
    if (next == eventTail) {
        return; // Overflow
    }
    
    ev = &eventQue[eventHead];
    ev->evTime = time;
    ev->evType = type;
    ev->evValue = value;
    ev->evValue2 = value2;
    ev->evPtrLength = ptrLength;
    ev->evPtr = ptr;
    
    eventHead = next;
}

sysEvent_t Sys_GetEvent( void ) {
    sysEvent_t ev;
    
    if (eventHead == eventTail) {
        memset( &ev, 0, sizeof(ev) );
        ev.evType = SE_NONE;
        return ev;
    }
    
    ev = eventQue[eventTail];
    eventTail = (eventTail + 1) % MAX_MAC_EVENTS;
    return ev;
}


// ==========================================
// Main Entry Point and System Routines
// ==========================================

int		sys_ticBase;
int		sys_msecBase;
int		sys_lastEventTic;

void Sys_Init( void ) {
    Sys_InitConsole();
    Sys_InitNetworking();
    Sys_InitInput();
}

void Sys_Quit( void ) {
    Sys_ShutdownInput();
    Sys_ShutdownNetworking();
    exit( 0 );
}

void Sys_Error( const char *error, ... ) {
    va_list argptr;
    char    text[1024];

    va_start( argptr, error );
    vsprintf( text, error, argptr );
    va_end( argptr );

    fprintf( stderr, "Sys_Error: %s\n", text );
    Sys_Quit();
}

// Time
int Sys_Milliseconds( void ) {
    UnsignedWide micros;
    Microseconds(&micros);
    return (int)((micros.lo / 1000) & 0x7FFFFFFF); // Simple implementation
    // Or use TickCount() * (1000/60)
}

void Sys_PumpEvents( void ) {
    // Basic event loop pump if needed here
    // Usually handled in Sys_GetEvent or main loop
}

// Return path to executable or useful dir
char *Sys_GetCwd( void ) {
    static char cwd[1024];
    // In Retro68, maybe we can get application path?
    // returning "." is usually safe enough for relative lookups
    return ".";
}

char *Sys_DefaultCDPath( void ) {
    return "";
}

char *Sys_DefaultBasePath( void ) {
    return Sys_GetCwd();
}

// Stubs for missing symbols
void Sys_BeginStreamedFile( int handle, int readAhead ) {}
void Sys_EndStreamedFile( int handle ) {}
int Sys_StreamedRead( void *buffer, int size, int count, int handle ) { return 0; }
void Sys_ShowIP( void ) {}
char *Sys_GetClipboardData( void ) { return NULL; }
qboolean Sys_LowPhysicalMemory( void ) { return qfalse; }

cvar_t *sys_waitNextEvent;

#define MAX_FOUND_FILES 0x1000

// Helper: Convert C path to FSSpec
static OSErr PathToFSSpec(const char *path, FSSpec *spec) {
    Str255 ppath;
    int len = strlen(path);
    if (len > 255) len = 255;
    ppath[0] = len;
    memcpy(&ppath[1], path, len);
    return FSMakeFSSpec(0, 0, ppath, spec);
}

char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs ) {
    FSSpec dirSpec;
    CInfoPBRec pb;
    Str255 name;
    OSErr err;
    short vRefNum;
    long dirID;
    int nfiles = 0;
    char *list[MAX_FOUND_FILES];
    char **listCopy;
    int i;
    int extLen;
    qboolean dironly = wantsubs;
    
    *numfiles = 0;
    
    // Convert path to FSSpec
    err = PathToFSSpec(directory, &dirSpec);
    if (err != noErr && err != fnfErr) {
        return NULL;
    }
    
    // Get directory ID
    memset(&pb, 0, sizeof(pb));
    pb.dirInfo.ioNamePtr = dirSpec.name;
    pb.dirInfo.ioVRefNum = dirSpec.vRefNum;
    pb.dirInfo.ioDrDirID = dirSpec.parID;
    pb.dirInfo.ioFDirIndex = 0;
    
    err = PBGetCatInfoSync(&pb);
    if (err != noErr) {
        return NULL;
    }
    
    if (!(pb.dirInfo.ioFlAttrib & 0x10)) {
        // Not a directory
        return NULL;
    }
    
    vRefNum = dirSpec.vRefNum;
    dirID = pb.dirInfo.ioDrDirID;
    
    if (!extension)
        extension = "";
    
    if (extension[0] == '/' && extension[1] == 0) {
        extension = "";
        dironly = qtrue;
    }
    
    extLen = strlen(extension);
    
    // Iterate through directory
    for (i = 1; ; i++) {
        memset(&pb, 0, sizeof(pb));
        pb.hFileInfo.ioNamePtr = name;
        pb.hFileInfo.ioVRefNum = vRefNum;
        pb.hFileInfo.ioDirID = dirID;
        pb.hFileInfo.ioFDirIndex = i;
        
        err = PBGetCatInfoSync(&pb);
        if (err != noErr) {
            break;  // No more files
        }
        
        // Convert Pascal string to C string
        char cname[256];
        int nameLen = name[0];
        memcpy(cname, &name[1], nameLen);
        cname[nameLen] = '\0';
        
        // Check if directory
        qboolean isDir = (pb.hFileInfo.ioFlAttrib & 0x10) != 0;
        
        if ((dironly && !isDir) || (!dironly && isDir)) {
            continue;
        }
        
        // Check extension
        if (*extension) {
            if (nameLen < extLen ||
                Q_stricmp(cname + nameLen - extLen, extension) != 0) {
                continue;
            }
        }
        
        if (nfiles >= MAX_FOUND_FILES - 1) {
            break;
        }
        
        list[nfiles] = CopyString(cname);
        nfiles++;
    }
    
    list[nfiles] = NULL;
    *numfiles = nfiles;
    
    if (!nfiles) {
        return NULL;
    }
    
    // Copy list to Z_Malloc'd memory
    listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
    for (i = 0; i < nfiles; i++) {
        listCopy[i] = list[i];
    }
    listCopy[i] = NULL;
    
    return listCopy;
}

void Sys_FreeFileList( char **list ) {
    int i;
    
    if (!list) {
        return;
    }
    
    for (i = 0; list[i]; i++) {
        Z_Free(list[i]);
    }
    
    Z_Free(list);
}

void *Sys_LoadDll( const char *name, char *fqpath, int (QDECL **entryPoint)(int, ...), int (*systemcalls)(int, ...) ) { return NULL; }
void Sys_UnloadDll( void *dllHandle ) {}

void Sys_SnapVector( float *v ) {}
void Sys_BeginProfiling( void ) {}
qboolean Sys_CheckCD( void ) { return qfalse; }

void Sys_Mkdir( const char *path ) {}
char *Sys_DefaultInstallPath( void ) { return Sys_GetCwd(); }
char *Sys_DefaultHomePath( void ) { return Sys_GetCwd(); }
void Sys_StreamSeek( int handle, int offset, int origin ) {}

// VM Stubs
void VM_Compile( void *vm, void *header ) {}
int VM_CallCompiled( void *vm, int *args ) { return 0; }

int main( int argc, char **argv ) {
    int i;
    char commandLine[1024];

    commandLine[0] = 0;
    for (i = 1; i < argc; i++) {
        strcat(commandLine, argv[i]);
        if (i < argc - 1) strcat(commandLine, " ");
    }

    Sys_Init();
    Com_Init( commandLine );

    while ( 1 ) {
        Com_Frame();
    }

    return 0;
}
