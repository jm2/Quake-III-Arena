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

// mac_event.c
void Sys_SendKeyEvents( void );
// mac_input.c
// mac_input.c
void Sys_Input( void );

// Retro68 Console Log Buffer
#define RETRO_LOG_SIZE (256 * 1024)
static char retroLogBuffer[RETRO_LOG_SIZE];
static int retroLogHead = 0;
static int retroLogTotal = 0;

// Sys_LogPrintf implementation
void Sys_LogPrintf( const char *fmt, ... ) {
    va_list argptr;
    char text[1024];
    int len;
    int i;

    va_start( argptr, fmt );
    len = vsprintf( text, fmt, argptr );
    va_end( argptr );

    // 1. Output to actual console
    printf("%s", text);

    // 2. Log to buffer
    if ( len < 0 ) return;
    if ( len > sizeof(text)-1 ) len = sizeof(text)-1;

    for ( i = 0; i < len; i++ ) {
        retroLogBuffer[retroLogHead] = text[i];
        retroLogHead = (retroLogHead + 1) % RETRO_LOG_SIZE;
        if (retroLogTotal < RETRO_LOG_SIZE) retroLogTotal++;
    }

    // 3. SYNCHRONOUS FILE APPEND (Performance hit accepted for debug)
    {
        FILE *fp = fopen("retro68_console.log", "a");
        if (fp) {
            fwrite(text, 1, len, fp);
            fclose(fp);
        }
    }
}

void Sys_DumpRetroLogs( const char *fileName ) {
    FILE *fp;
    int i, idx, count;

    fp = fopen( fileName, "wb" );
    if ( !fp ) {
        printf( "Sys_DumpRetroLogs: Failed to open %s\n", fileName );
        return;
    }

    if (retroLogTotal < RETRO_LOG_SIZE) {
        idx = 0;
        count = retroLogTotal;
    } else {
        idx = retroLogHead;
        count = RETRO_LOG_SIZE;
    }

    for ( i = 0; i < count; i++ ) {
        fwrite( &retroLogBuffer[idx], 1, 1, fp );
        idx = (idx + 1) % RETRO_LOG_SIZE;
    }

    fclose( fp );
    printf( "Sys_DumpRetroLogs: Dumped %d bytes to %s\n", count, fileName );
}

// Debug Breadcrumb
void Debug_Breadcrumb( int color ) {
    Rect r;
    // Draw a small square in the top-left corner
    SetRect(&r, 0, 0, 10, 10);
    ForeColor(color);
    PaintRect(&r);
    ForeColor(blackColor);
}

sysEvent_t Sys_GetEvent( void ) {
    sysEvent_t ev;
    KeyMap keys;
    unsigned char *k;
    
    // Panic Key Dump: Command (55) + D (2)
    GetKeys(keys);
    k = (unsigned char *)keys;
    // Command is key 55. 55/8=6, 55%8=7. Bit check: (1<<(55%8)) = 0x80
    // D is key 2. 2/8=0, 2%8=2. Bit check: (1<<2) = 0x04
    if ( (k[6] & 0x80) && (k[0] & 0x04) ) {
        static int lastDump = 0;
        int now = Sys_Milliseconds();
        if (now - lastDump > 2000) {
            Com_Printf("PANIC KEY DETECTED - DUMPING FLIGHT RECORD\n");
            Com_DumpFlightRecord("flight_record.txt");
            Sys_DumpRetroLogs("retro68_console.txt");
            SysBeep(30); 
            lastDump = now;
        }
    }
    
    /*
    // Auto Dump loop disabled - using synchronous append logging instead
    {
        static int lastAutoDump = 0;
        int now = Sys_Milliseconds();
        if (now - lastAutoDump > 100) {
           Com_DumpFlightRecord("flight_record_auto.txt"); 
           Sys_DumpRetroLogs("retro68_console_auto.txt");
           lastAutoDump = now;
        }
    }
    */

    Sys_LogPrintf("Sys_GetEvent: SendKeyEvents\n");
    // Pump Mac OS events (keyboard via WaitNextEvent)
    Sys_SendKeyEvents();
    Sys_LogPrintf("Sys_GetEvent: SendKeyEvents done\n");
    
    Sys_LogPrintf("Sys_GetEvent: Sys_Input\n");
    // Pump InputSprocket events (mouse)
    Sys_Input();
    Sys_LogPrintf("Sys_GetEvent: Sys_Input done\n");

    if (eventHead == eventTail) {
        memset( &ev, 0, sizeof(ev) );
        ev.evType = SE_NONE;
        ev.evTime = Sys_Milliseconds();
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
    Com_FlightRecord("Sys_Init: Is this on? Flight Recorder Start.\n");
    Debug_Breadcrumb(205); // redColor
    Sys_InitConsole();
    Com_FlightRecord("Sys_Init: Console initialized.\n");
    Sys_InitNetworking();
    Sys_InitInput();
}

void Sys_Quit( void ) {
    Sys_ShutdownInput();
    Sys_ShutdownNetworking();
    
    // DEBUG: Hook to catch startup errors
    // Use implicit declaration for SysBeep as we are lazy with includes for this debug hack
    // SysBeep(30); 
    
    Sys_LogPrintf("\nSys_Quit called. Waiting for input (press Return) to exit...\n");
    getchar();
    
    exit( 0 );
}

void Sys_Error( const char *error, ... ) {
    va_list argptr;
    char    text[1024];

    va_start( argptr, error );
    vsprintf( text, error, argptr );
    va_end( argptr );

    Com_FlightRecord("Sys_Error: %s\n", text);
    Com_DumpFlightRecord("crashdump.txt");

    fprintf( stderr, "Sys_Error: %s\n", text );
    Sys_LogPrintf("Sys_Error: %s\n", text);
    Sys_DumpRetroLogs("retro68_console_crash.txt");
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

// Yield to system to prevent timing race conditions
// This must provide timing equivalent to printf("...") + fflush(stdout)
// Key: We must actually WRITE to stdout to trigger I/O, not just flush empty buffer
void Sys_Yield( void ) {
    SystemTask();
    // Actually write to stdout like printf does - use space+backspace to be invisible
    printf(" \b");
    fflush(stdout);
    SystemTask();
}

// Return path to executable or useful dir
char *Sys_GetCwd( void ) {
    short vRefNum;
    Str255 volName;
    OSErr err;

    // Probe Default Volume
    err = GetVol(volName, &vRefNum);
    if (err == noErr) {
        // Convert Pascal String to C String
        char cVolName[256];
        int len = volName[0];
        memcpy(cVolName, &volName[1], len);
        cVolName[len] = 0;
        //printf("DEBUG: GetVol success! vRefNum=%d, VolName='%s'\n", vRefNum, cVolName);
        
        // === DIAGNOSTIC: Direct File Access Test ===
        // Try to open a file using an absolute HFS path
        // This tests if file I/O works AT ALL, independent of directory listing.
        
        FSSpec testSpec;
        short testRefNum;
        
        // Test 1: Try FSMakeFSSpec with full absolute path
        {
            const char *testPath = "Macintosh HD:Desktop Folder:Quake 3 Arena:baseq3:pak0.pk3";
            Str255 pPath;
            int pathLen = strlen(testPath);
            if (pathLen > 255) pathLen = 255;
            pPath[0] = pathLen;
            memcpy(&pPath[1], testPath, pathLen);
            
            err = FSMakeFSSpec(0, 0, pPath, &testSpec);
            //printf("DEBUG: DIAG Test1: FSMakeFSSpec('%s') -> err=%d\n", testPath, err);
            if (err == noErr) {
                //printf("DEBUG: DIAG Test1: SUCCESS! vRefNum=%d, parID=%ld, name='%.*s'\n", 
                //       testSpec.vRefNum, testSpec.parID, testSpec.name[0], &testSpec.name[1]);
                
                // Try to open the file
                err = FSpOpenDF(&testSpec, fsRdPerm, &testRefNum);
                //printf("DEBUG: DIAG Test1: FSpOpenDF -> err=%d\n", err);
                if (err == noErr) {
                    //printf("DEBUG: DIAG Test1: FILE OPENED SUCCESSFULLY! refNum=%d\n", testRefNum);
                    FSClose(testRefNum);
                }
            }
        }
        
        // Test 2: Try with vRefNum from GetVol (the WDRefNum) + relative path
        {
            const char *relPath = ":baseq3:pak0.pk3";
            Str255 pPath;
            int pathLen = strlen(relPath);
            if (pathLen > 255) pathLen = 255;
            pPath[0] = pathLen;
            memcpy(&pPath[1], relPath, pathLen);
            
            err = FSMakeFSSpec(vRefNum, 0, pPath, &testSpec);
            //printf("DEBUG: DIAG Test2: FSMakeFSSpec(vRef=%d, '%s') -> err=%d\n", vRefNum, relPath, err);
            if (err == noErr || err == fnfErr) {
                //printf("DEBUG: DIAG Test2: vRefNum=%d, parID=%ld\n", testSpec.vRefNum, testSpec.parID);
            }
        }
        
        // Test 3: Try with vRefNum=-1 (boot volume) + absolute partial path
        {
            const char *partPath = ":Desktop Folder:Quake 3 Arena:baseq3:pak0.pk3";
            Str255 pPath;
            int pathLen = strlen(partPath);
            if (pathLen > 255) pathLen = 255;
            pPath[0] = pathLen;
            memcpy(&pPath[1], partPath, pathLen);
            
            err = FSMakeFSSpec(-1, 0, pPath, &testSpec);
            //printf("DEBUG: DIAG Test3: FSMakeFSSpec(vRef=-1, '%s') -> err=%d\n", partPath, err);
            if (err == noErr) {
                 //printf("DEBUG: DIAG Test3: SUCCESS! vRefNum=%d, parID=%ld\n", testSpec.vRefNum, testSpec.parID);
            }
        }
        
        // Test 4: Try POSIX-style open (if Retro68 supports it)
        {
            FILE *fp = fopen(":baseq3:pak0.pk3", "rb");
            //printf("DEBUG: DIAG Test4: fopen(':baseq3:pak0.pk3') -> %s\n", fp ? "SUCCESS" : "FAILED");
            if (fp) fclose(fp);
        }
        
        //printf("DEBUG: === END DIAGNOSTIC ===\n");
        
    } else {
        //printf("DEBUG: GetVol failed, err=%d\n", err);
    }
    
    //printf("DEBUG: Sys_GetCwd returning '' (Empty for Relative Path)\n");
    return "";
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
    OSErr err;
    int len = strlen(path);
    if (len > 255) len = 255;
    ppath[0] = len;
    memcpy(&ppath[1], path, len);
    err = FSMakeFSSpec(0, 0, ppath, spec);
    if (err != noErr && err != fnfErr) { // fnfErr is okay for checking existence
        //printf("DEBUG: PathToFSSpec failed for '%s', err=%d\n", path, err);
    } else {
        //printf("DEBUG: PathToFSSpec success for '%s', vRefNum=%d, parID=%ld, err=%d\n", path, spec->vRefNum, spec->parID, err);
    }
    return err;
}

// NEW IMPLEMENTATION: Hardcoded file discovery since:
// 1. PBGetCatInfo directory iteration fails with -35
// 2. <dirent.h> is not supported by Retro68
// BUT fopen() works! So we probe for known files directly.

char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs ) {
    int nfiles = 0;
    static char *list[MAX_FOUND_FILES];
    char **listCopy;
    int i;
    int extLen;
    qboolean dironly = wantsubs;
    char testPath[512];
    FILE *fp;
    
    //printf("DEBUG: Sys_ListFiles (hardcoded probe) dir='%s' ext='%s'\n", directory, extension);
    
    *numfiles = 0;
    
    if (!extension)
        extension = "";
    
    if (extension[0] == '/' && extension[1] == 0) {
        extension = "";
        dironly = qtrue;
    }
    
    extLen = strlen(extension);
    
    // If looking for directories, we can't enumerate - return NULL
    if (dironly) {
        //printf("DEBUG: Sys_ListFiles: Directory enumeration not supported, returning NULL\n");
        return NULL;
    }
    
    // For pk3 files, probe known filenames
    if (Q_stricmp(extension, ".pk3") == 0) {
        // Standard Quake 3 pk3 files
        static const char *knownPk3[] = {
            "pak0.pk3", "pak1.pk3", "pak2.pk3", "pak3.pk3", "pak4.pk3",
            "pak5.pk3", "pak6.pk3", "pak7.pk3", "pak8.pk3",
            NULL
        };
        
        for (i = 0; knownPk3[i] && nfiles < MAX_FOUND_FILES - 1; i++) {
            // Build path - handle HFS path format
            if (directory[0] == ':') {
                snprintf(testPath, sizeof(testPath), "%s:%s", directory, knownPk3[i]);
            } else {
                snprintf(testPath, sizeof(testPath), "%s%s%s", 
                         directory, 
                         (directory[0] && directory[strlen(directory)-1] != ':') ? ":" : "",
                         knownPk3[i]);
            }
            
            fp = fopen(testPath, "rb");
            if (fp) {
                //printf("DEBUG: Sys_ListFiles: FOUND '%s'\n", knownPk3[i]);
                fclose(fp);
                list[nfiles] = CopyString(knownPk3[i]);
                nfiles++;
            }
        }
    }
    // For cfg files
    else if (Q_stricmp(extension, ".cfg") == 0) {
        static const char *knownCfg[] = {
            "default.cfg", "q3config.cfg", "autoexec.cfg",
            NULL
        };
        
        for (i = 0; knownCfg[i] && nfiles < MAX_FOUND_FILES - 1; i++) {
            if (directory[0] == ':') {
                snprintf(testPath, sizeof(testPath), "%s:%s", directory, knownCfg[i]);
            } else {
                snprintf(testPath, sizeof(testPath), "%s%s%s",
                         directory,
                         (directory[0] && directory[strlen(directory)-1] != ':') ? ":" : "",
                         knownCfg[i]);
            }
            
            fp = fopen(testPath, "rb");
            if (fp) {
                //printf("DEBUG: Sys_ListFiles: FOUND '%s'\n", knownCfg[i]);
                fclose(fp);
                list[nfiles] = CopyString(knownCfg[i]);
                nfiles++;
            }
        }
    }
    else {
        //printf("DEBUG: Sys_ListFiles: Unknown extension '%s', cannot probe\n", extension);
    }
    
    list[nfiles] = NULL;
    *numfiles = nfiles;
    
    //printf("DEBUG: Sys_ListFiles: Found %d files\n", nfiles);
    
    if (!nfiles) return NULL;
    
    listCopy = Z_Malloc((nfiles + 1) * sizeof(*listCopy));
    for (i = 0; i < nfiles; i++) {
        listCopy[i] = list[i];
    }
    listCopy[i] = NULL;
    
    return listCopy;
}

// OLD IMPLEMENTATION BELOW - COMMENTED OUT
#if 0
char **Sys_ListFiles_OLD( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs ) {
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
    
    // //printf("DEBUG: Sys_ListFiles dir='%s' ext='%s'\n", directory, extension);
    
    *numfiles = 0;
    
    // Convert path to FSSpec
    err = PathToFSSpec(directory, &dirSpec);
    if (err != noErr && err != fnfErr) {
        //printf("DEBUG: Sys_ListFiles: PathToFSSpec failed for '%s'\n", directory);
        return NULL;
    }
    
    // FIX: FSMakeFSSpec returning -1.
    // Try 0 (Default Volume) first.
    if (dirSpec.vRefNum == -1) {
        //printf("DEBUG: Sys_ListFiles: FSMakeFSSpec returned vRefNum=-1. Forcing to 0.\n");
        dirSpec.vRefNum = 0;
    }
    
    // Get directory ID
    memset(&pb, 0, sizeof(pb));
    pb.dirInfo.ioNamePtr = dirSpec.name;
    pb.dirInfo.ioVRefNum = dirSpec.vRefNum;
    pb.dirInfo.ioDrDirID = dirSpec.parID;
    pb.dirInfo.ioFDirIndex = 0;
    
    err = PBGetCatInfoSync(&pb);
    if (err != noErr) {
        //printf("DEBUG: Sys_ListFiles: PBGetCatInfoSync (DirInfo) failed for '%s' (vRef=%d, parID=%ld), err=%d\n", directory, dirSpec.vRefNum, dirSpec.parID, err);
        
        // Fallback: PROBE the current directory.
        short probeVRefNum = 0;
        long probeDirID = 0;
        Str255 currentVolName;
        
        // 1. Get the current Volume Name and WDRefNum
        if (GetVol(currentVolName, &probeVRefNum) == noErr) {
             //printf("DEBUG: Sys_ListFiles: GetVol -> vRef=%d, Name Len=%d\n", probeVRefNum, currentVolName[0]);
             
             // 2. Get the Directory ID (and repeat vRef)
             short hVRef;
             if (HGetVol(NULL, &hVRef, &probeDirID) != noErr) {
                 //printf("DEBUG: Sys_ListFiles: HGetVol Failed! Cannot proceed.\n");
                 return NULL;
             }
             //printf("DEBUG: Sys_ListFiles: HGetVol -> dirID=%ld\n", probeDirID);
             
             // 3. Find the REAL VRefNum for current volume using PBHGetVInfoSync
             HParamBlockRec volPB;
             Str255 volNameBuffer;
             short realVRefNum = 0;
             int volIndex = 1;
             
             //printf("DEBUG: Sys_ListFiles: List of Mounted Volumes (PBHGetVInfoSync):\n");
             while (1) {
                 memset(&volPB, 0, sizeof(volPB));
                 volPB.volumeParam.ioNamePtr = volNameBuffer;
                 volPB.volumeParam.ioVolIndex = volIndex;
                 volPB.volumeParam.ioVRefNum = 0; // Essential for indexing mode
                 
                 err = PBHGetVInfoSync(&volPB);
                 if (err != noErr) {
                     if (volIndex == 1) //printf("DEBUG: Sys_ListFiles: PBHGetVInfoSync failed on FIRST index! err=%d\n", err);
                     break; // End of volumes
                 }
                 
                 // Convert Pascal to C for print
                 char vName[256];
                 int vLen = volNameBuffer[0];
                 memcpy(vName, &volNameBuffer[1], vLen);
                 vName[vLen] = 0;
                 
                 //printf("DEBUG: Sys_ListFiles: Vol %d: '%s' (vRef=%d)\n", volIndex, vName, volPB.volumeParam.ioVRefNum);
                 
                 if (volPB.volumeParam.ioNamePtr[0] == currentVolName[0] &&
                     memcmp(&volPB.volumeParam.ioNamePtr[1], &currentVolName[1], currentVolName[0]) == 0) {
                     realVRefNum = volPB.volumeParam.ioVRefNum;
                 }
                 volIndex++;
             }
             
             // LOGIC:
             // 1. If we found a Real VRefNum via name matching, use it.
             // 2. If 'probeVRefNum' is a WDRefNum (-32xxx) and we found NO Real VRefNum:
             //    ---> USE THE WDRefNUm, but FORCE DirID = 0.
             
             if (realVRefNum != 0) {
                 //printf("DEBUG: Sys_ListFiles: Using Real VRefNum %d and HGetVol DirID %ld\n", realVRefNum, probeDirID);
                 probeVRefNum = realVRefNum;
             } else if (probeVRefNum < -32000) {
                 //printf("DEBUG: Sys_ListFiles: WDRefNum %d detected. Volume List failed or unneeded. Forcing DirID to 0 to use WD Context.\n", probeVRefNum);
                 // Do NOT blindly force to -1. Trust the WDRefNum from GetVol.
                 // probeVRefNum remains -32xxx
                 probeDirID = 0; 
             } else {
                  //printf("DEBUG: Sys_ListFiles: No better VRefNum found. Using original.\n");
             }
             


             // 4. Perform Probe with Clean VRef and DirID
             
             //printf("DEBUG: Sys_ListFiles: PROBE START using vRef=%d, dirID=%ld\n", probeVRefNum, probeDirID);

             int probeIndex = 1;
             int safety = 0;
             int safetyProbeAttempted = 0;
             Str255 probeName;
             
restart_probe:
             while (safety < 50) { 
                 memset(&pb, 0, sizeof(pb));
                 pb.dirInfo.ioNamePtr = probeName;
                 pb.dirInfo.ioVRefNum = probeVRefNum;
                 pb.dirInfo.ioDrDirID = probeDirID;
                 pb.dirInfo.ioFDirIndex = probeIndex;
                 
                 err = PBGetCatInfoSync(&pb);
                 if (err != noErr) {
                     if (err == fnfErr) //printf("DEBUG: Sys_ListFiles: PROBE END (End of Dir).\n");
                     else //printf("DEBUG: Sys_ListFiles: PROBE ERROR index=%d, err=%d\n", probeIndex, err);
                     
                     // SAFETY PROBE: If the first item failed with error...
                     if (probeIndex == 1 && err != fnfErr && !safetyProbeAttempted) {
                         // Fallback Levels:
                         // 1. Try Root Directory of the SAME Volume (DirID = 2)
                         // 2. Try Default Context (vRef=0, dirID=0)
                         
                         if (probeDirID != 0 && probeDirID != 2) {
                             //printf("DEBUG: Sys_ListFiles: Probe failed. Retrying with ROOT Directory (DirID=2) of vRef=%d.\n", probeVRefNum);
                             probeDirID = 2; // Root ID
                             probeIndex = 1;
                             safety = 0;
                             goto restart_probe; // This counts as part of the primary attempt, sort of...
                             // Actually, this might loop if ROOT fails. 
                             // Let's rely on safetyProbeAttempted to eventually kill it, but we need meaningful states.
                         }
                         
                         //printf("DEBUG: Sys_ListFiles: Standard Probe failed immediately. Attempting SAFETY PROBE (vRef=0, dirID=0).\n");
                         probeVRefNum = 0;
                         probeDirID = 0;
                         probeIndex = 1; 
                         safety = 0;     
                         safetyProbeAttempted = 1; 
                         goto restart_probe; 
                     }
                     break;
                 }
                 
                 char cName[256];
                 int pLen = probeName[0];
                 memcpy(cName, &probeName[1], pLen);
                 cName[pLen] = 0;
                 
                 //printf("DEBUG: Sys_ListFiles: PROBE Item %d: '%s' (dirID=%ld) [Attrib: %x]\n", probeIndex, cName, pb.dirInfo.ioDrDirID, pb.dirInfo.ioFlAttrib);
                 
                 const char *cleanTarget = directory;
                 if (cleanTarget[0] == ':') cleanTarget++;
                 
                 if (strcasecmp(cName, cleanTarget) == 0) {
                     //printf("DEBUG: Sys_ListFiles: PROBE FOUND TARGET! Found '%s'.\n", cName);
                     
                     if (pb.dirInfo.ioFlAttrib & 0x10) { 
                         // Success! We found the directory manually.
                         goto list_files_valid;
                     } else {
                         //printf("DEBUG: Sys_ListFiles: Target found but it is NOT a directory!\n");
                     }
                 }
                 probeIndex++;
                 safety++;
             }
        } else {
             //printf("DEBUG: Sys_ListFiles: PROBE FAILED. GetVol failed.\n");
        }
        return NULL;
    }

list_files_valid:
    //printf("DEBUG: Sys_ListFiles: Proceeding with valid DirID=%ld\n", pb.dirInfo.ioDrDirID);
    
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
#endif // Old Sys_ListFiles implementation

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

// Statically linked VM entry points
// Game_vmMain and UI_vmMain are declared at the top of the file, but CGame_vmMain is missing
extern int CGame_vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );

extern void UI_dllEntry( int (QDECL *syscallptr)( int arg,... ) );
extern void CGame_dllEntry( int (QDECL *syscallptr)( int arg,... ) );
extern void Game_dllEntry( int (QDECL *syscallptr)( int arg,... ) );

void *Sys_LoadDll( const char *name, char *fqpath, int (QDECL **entryPoint)(int, ...), int (*systemcalls)(int, ...) ) { 
	if ( !Q_stricmp( name, "ui" ) ) {
		*entryPoint = (int (QDECL *)(int, ...))UI_vmMain;
		UI_dllEntry( (int (QDECL *)( int, ...))systemcalls );
		return (void *)UI_vmMain;
	}
	if ( !Q_stricmp( name, "cgame" ) ) {
		*entryPoint = (int (QDECL *)(int, ...))CGame_vmMain;
		CGame_dllEntry( (int (QDECL *)( int, ...))systemcalls );
		return (void *)CGame_vmMain;
	}
	if ( !Q_stricmp( name, "qagame" ) ) {
		*entryPoint = (int (QDECL *)(int, ...))Game_vmMain;
		Game_dllEntry( (int (QDECL *)( int, ...))systemcalls );
		return (void *)Game_vmMain;
	}

	return NULL; 
}
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

    Sys_LogPrintf("main: START\n");

    commandLine[0] = 0;
    for (i = 1; i < argc; i++) {
        strcat(commandLine, argv[i]);
        if (i < argc - 1) strcat(commandLine, " ");
    }

    Sys_LogPrintf("main: Sys_Init\n");
    Sys_Init();
    Sys_LogPrintf("main: Sys_Init done\n");
    
    Sys_LogPrintf("main: Com_Init\n");
    Com_Init( commandLine );
    Sys_LogPrintf("main: Com_Init done\n");
    Debug_Breadcrumb(341); // greenColor

    Sys_LogPrintf("main: entering main loop\n");
    Com_FlightRecord("main: Entering Com_Frame loop.\n");
    while ( 1 ) {
        Com_Frame();
    }

    return 0;
}
