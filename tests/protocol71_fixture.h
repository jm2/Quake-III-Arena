/* Issue #37: the server side of tests/protocol71_regression.c, in tests/protocol71_server.c (server.h and
 * client.h cannot share a translation unit). */
void Server_Init( void );
qboolean Server_Connected( int n );	/* CS_CONNECTED */
qboolean Server_Free( int n );		/* CS_FREE */
qboolean Server_ClientCompat( int n );
int Server_ClientChallenge( int n );
netadr_t Server_ClientAddress( int n );
int Server_ClientLastPacketTime( int n );
int Server_OutgoingSequence( int n );
/** The strings each side's XOR key uses: the client's last command and the server command it acknowledges. */
void Server_SetCommands( int n, const char *lastClientCommand, int acknowledged, const char *serverCommand );
/** SV_Netchan_Transmit, as SV_SendMessageToClient calls it, and SV_Netchan_TransmitNextFragment, as
 * SV_SendClientMessages does while fragments are left; returns whether one was. */
void Server_Transmit( int n, msg_t *msg );
qboolean Server_NextFragment( int n );
/** The message SV_PacketEvent last handed SV_ExecuteClientMessage: its client, read position and bytes. */
int Server_TakeExecuted( int *n, int *readcount, byte *data );
/** The challenge record for an address, if there is one, and how many records there are (MAX_CHALLENGES). */
qboolean Server_Challenge( netadr_t from, int *challenge, qboolean *refused );
int Server_Challenges( void );
void Server_Refuse( netadr_t from );
void Server_SetTime( int msec );
