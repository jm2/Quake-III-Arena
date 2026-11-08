/* botlib's TryGetMemory and TryGetClearedMemory (code/botlib/l_memory.c) for
 * fixtures that supply their own GetMemory and GetClearedMemory instead of
 * linking l_memory.c: the parser's file-driven allocations then come from the
 * fixture's allocator, with its counts and injected failures unchanged. */
void *GetMemory( unsigned long size );
void *GetClearedMemory( unsigned long size );
void *TryGetMemory( unsigned long size );
void *TryGetClearedMemory( unsigned long size );

void *TryGetMemory( unsigned long size ) { return GetMemory( size ); }
void *TryGetClearedMemory( unsigned long size ) { return GetClearedMemory( size ); }
