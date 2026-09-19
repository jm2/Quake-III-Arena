/* Private download-name validation shared with the isolated host regression. */
#ifndef SV_DOWNLOAD_H
#define SV_DOWNLOAD_H

/*
==================
SV_ReferencedDownload

Accept only the canonical spelling emitted by FS_ReferencedPakNames.  The
filesystem normally treats slash, backslash and colon as equivalent, but the
latter two are native path separators on supported hosts and must not be
accepted from an untrusted download command.
==================
*/
static qboolean SV_ReferencedDownload( const char *downloadName,
	qboolean *idPack, qboolean *missionPack ) {
	char pakName[MAX_QPATH];
	const char *referencedPaks;
	const char *token;
	int downloadLen;
	int pakNameLen;
	int tokenLen;

	*idPack = qfalse;
	*missionPack = qfalse;

	if ( !downloadName || !*downloadName ) {
		return qfalse;
	}

	downloadLen = strlen( downloadName );
	if ( downloadLen < 5 || downloadLen >= sizeof( pakName )
		|| Q_stricmp( downloadName + downloadLen - 4, ".pk3" )
		|| downloadName[0] == '/' || downloadName[0] == '\\'
		|| strstr( downloadName, ".." ) || strstr( downloadName, "::" )
		|| strchr( downloadName, ':' ) || strchr( downloadName, '\\' )
		|| strchr( downloadName, '@' ) || strchr( downloadName, '\n' )
		|| strchr( downloadName, '\r' ) || strchr( downloadName, ';' ) ) {
		return qfalse;
	}

	pakNameLen = downloadLen - 4;
	Com_Memcpy( pakName, downloadName, pakNameLen );
	pakName[pakNameLen] = 0;

	referencedPaks = FS_ReferencedPakNames();
	while ( *referencedPaks ) {
		while ( *referencedPaks && *referencedPaks <= ' ' ) {
			referencedPaks++;
		}
		token = referencedPaks;
		while ( *referencedPaks > ' ' ) {
			referencedPaks++;
		}
		tokenLen = referencedPaks - token;

		if ( tokenLen == pakNameLen
			&& !Q_stricmpn( token, pakName, pakNameLen ) ) {
			*missionPack = FS_idPak( pakName, "missionpack" );
			*idPack = *missionPack || FS_idPak( pakName, BASEGAME );
			return qtrue;
		}
	}

	return qfalse;
}

#endif
