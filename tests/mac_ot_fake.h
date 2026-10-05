/* A fake Open Transport for host regressions of code/mac/mac_net.c, whose
 * functions the runner extracts verbatim (the real file needs the whole Mac
 * Toolbox).  The types and constants are those of Universal Interfaces 3.4
 * (Retro68 OpenTransport.h, OpenTransportProviders.h, MacErrors.h), cut to
 * what the extracted code uses, with the target's field sizes.
 *
 * The fake endpoint holds a queue of datagrams and reads them as XTI's
 * t_rcvudata does: a datagram larger than the caller's buffer comes back in
 * pieces with T_MORE set, and only the first piece carries the source
 * address.  fakeOTFailCall makes that OTRcvUData call (counting from 1)
 * fail with fakeOTFailError and read nothing.  The fake resolver returns
 * fakeOTResolvedHost.  OTInitDNSAddress copies the name with no bound, as
 * the target's does.  Every call is counted.
 *
 * For Sys_InitNetworking the fake also opens, binds and closes endpoints:
 * the first OTOpenEndpoint gives FAKE_OT_ENDPOINT and the second
 * FAKE_OT_RESOLVER.  Ports in fakeOTBusyPorts are taken: binding one fails
 * with kOTAddressBusyErr, or with fakeOTBindReassigns succeeds on another
 * port as XTI's t_bind may.  The fakeOTFail* settings make one stage fail
 * with FAKE_OT_INJECTED_ERR, and fakeOTResolveError makes resolving fail. */
#ifndef MAC_OT_FAKE_H
#define MAC_OT_FAKE_H

#include <stdint.h>
#include <string.h>

typedef uint8_t		UInt8;
typedef uint16_t	UInt16;
typedef uint32_t	UInt32;
typedef int32_t		OSStatus;
typedef UInt32		ByteCount;
typedef ByteCount	OTByteCount;
typedef UInt32		OTFlags;
typedef UInt32		OTQLen;
typedef UInt32		OTTimeout;
typedef UInt16		OTAddressType;
typedef UInt16		InetPort;
typedef UInt32		InetHost;
typedef void		*EndpointRef;
typedef char		InetDomainName[256];
typedef int32_t		OTResult;
typedef int32_t		OTDataSize;
typedef UInt32		OTServiceType;
typedef UInt32		OTOpenFlags;
typedef UInt32		OTXTILevel;
typedef UInt32		OTXTIName;
typedef void		*ProviderRef;
typedef struct OTConfiguration	*OTConfigurationRef;

#define kOTInvalidEndpointRef		((EndpointRef)0L)
#define kOTInvalidConfigurationPtr	((OTConfigurationRef)-1L)
#define kUDPName					"udp"

enum { false = 0, true = 1 };	/* MacTypes.h */
#define nil NULL
enum { T_MORE = 0x0001 };
enum { noErr = 0 };
enum { kOTLookErr = -3158, kOTNoDataErr = -3162 };
enum { kOTBadAddressErr = -3150, kOTOutStateErr = -3155, kOTAddressBusyErr = -3172,
	kOTOutOfMemoryErr = -3211, kOTBadConfigurationErr = -3282 };
enum { INET_IP = 0x00, IP_BROADCAST = 0x20, T_YES = 1 };
enum { kOTAnyInetAddress = 0 };
enum { AF_DNS = 42 };
#define AF_INET 2

typedef struct {
	ByteCount	maxlen;
	ByteCount	len;
	UInt8		*buf;
} TNetbuf;

typedef struct {
	TNetbuf		addr;
	OTQLen		qlen;
} TBind;

typedef struct {
	TNetbuf		addr;
	TNetbuf		opt;
	TNetbuf		udata;
} TUnitData;

typedef struct {
	OTDataSize		addr;
	OTDataSize		options;
	OTDataSize		tsdu;
	OTDataSize		etsdu;
	OTDataSize		connect;
	OTDataSize		discon;
	OTServiceType	servtype;
	UInt32			flags;
} TEndpointInfo;

typedef struct {
	OTAddressType	fAddressType;	/* always AF_INET */
	InetPort		fPort;			/* network byte order */
	InetHost		fHost;			/* network byte order */
	UInt8			fUnused[8];
} InetAddress;

typedef struct {
	OTAddressType	fAddressType;	/* always AF_DNS */
	InetDomainName	fName;
} DNSAddress;

#define FAKE_OT_ENDPOINT	((EndpointRef)&fakeOTDatagrams)
#define FAKE_OT_RESOLVER	((EndpointRef)&fakeOTResolvedHost)
#define FAKE_OT_MAX_QUEUE	8

typedef struct {
	const UInt8	*data;
	int			length;
	InetAddress	from;
} fakeOTDatagram_t;

static fakeOTDatagram_t	fakeOTDatagrams[FAKE_OT_MAX_QUEUE];
static int		fakeOTQueued;		/* datagrams queued */
static int		fakeOTNext;			/* the datagram the next read returns from */
static int		fakeOTOffset;		/* bytes of it already read */
static int		fakeOTRcvCalls;
static int		fakeOTFailCall;		/* 0: no call fails */
static OSStatus	fakeOTFailError;
static OSStatus	fakeOTResolveError;	/* noErr: resolving succeeds */

static DNSAddress	*fakeOTDNSAddress;	/* the last OTInitDNSAddress call's */
static OTByteCount	fakeOTDNSLength;
static int		fakeOTInitDNSCalls;
static InetHost		fakeOTResolvedHost;
static char		fakeOTResolvedName[sizeof( InetDomainName )];
static int		fakeOTResolveCalls;

/* Empty the endpoint and clear its counts and injected failure. */
static void FakeOT_ResetEndpoint( void ) {
	fakeOTQueued = fakeOTNext = fakeOTOffset = fakeOTRcvCalls = fakeOTFailCall = 0;
}

/* Queue a datagram from ip:port (bytes in network order) on the endpoint. */
static void FakeOT_QueueDatagram( const UInt8 *data, int length, const UInt8 ip[4], const UInt8 port[2] ) {
	fakeOTDatagram_t *d = &fakeOTDatagrams[fakeOTQueued++];

	d->data = data;
	d->length = length;
	memset( &d->from, 0, sizeof( d->from ) );
	d->from.fAddressType = AF_INET;
	memcpy( &d->from.fHost, ip, 4 );
	memcpy( &d->from.fPort, port, 2 );
}

OSStatus OTRcvUData( EndpointRef ref, TUnitData *udata, OTFlags *flags ) {
	fakeOTDatagram_t	*d;
	int					n;

	fakeOTRcvCalls++;
	if ( fakeOTRcvCalls == fakeOTFailCall ) {
		return fakeOTFailError;
	}
	if ( ref != FAKE_OT_ENDPOINT || fakeOTNext == fakeOTQueued ) {
		return kOTNoDataErr;
	}
	d = &fakeOTDatagrams[fakeOTNext];

	udata->addr.len = 0;	/* continuation pieces carry no address */
	if ( fakeOTOffset == 0 && udata->addr.maxlen >= sizeof( d->from ) ) {
		memcpy( udata->addr.buf, &d->from, sizeof( d->from ) );
		udata->addr.len = sizeof( d->from );
	}
	udata->opt.len = 0;

	n = d->length - fakeOTOffset;
	if ( n > (int)udata->udata.maxlen ) {
		n = udata->udata.maxlen;
	}
	memcpy( udata->udata.buf, d->data + fakeOTOffset, n );
	udata->udata.len = n;
	fakeOTOffset += n;

	*flags = 0;
	if ( fakeOTOffset < d->length ) {
		*flags = T_MORE;
	} else {
		fakeOTNext++;
		fakeOTOffset = 0;
	}
	return 0;
}

OTByteCount OTInitDNSAddress( DNSAddress *addr, char *str ) {
	int		i;

	fakeOTInitDNSCalls++;
	addr->fAddressType = AF_DNS;
	for ( i = 0 ; ( addr->fName[i] = str[i] ) != 0 ; i++ ) {
	}
	fakeOTDNSAddress = addr;
	fakeOTDNSLength = sizeof( addr->fAddressType ) + i + 1;
	return fakeOTDNSLength;
}

OSStatus OTResolveAddress( EndpointRef ref, TBind *reqAddr, TBind *retAddr, OTTimeout timeOut ) {
	InetAddress	result;

	(void)timeOut;
	fakeOTResolveCalls++;
	if ( fakeOTResolveError ) {
		return fakeOTResolveError;
	}
	if ( ref != FAKE_OT_RESOLVER || reqAddr->addr.buf != (UInt8 *)fakeOTDNSAddress
		|| reqAddr->addr.len != fakeOTDNSLength || retAddr->addr.maxlen < sizeof( result ) ) {
		return kOTNoDataErr;	/* not the request OTInitDNSAddress built */
	}
	memcpy( fakeOTResolvedName, fakeOTDNSAddress->fName, sizeof( fakeOTResolvedName ) );

	memset( &result, 0, sizeof( result ) );
	result.fAddressType = AF_INET;
	result.fHost = fakeOTResolvedHost;
	memcpy( retAddr->addr.buf, &result, sizeof( result ) );
	retAddr->addr.len = sizeof( result );
	return 0;
}

/* ---------- endpoint lifetime, for Sys_InitNetworking ---------- */

#define FAKE_OT_INJECTED_ERR	kOTOutOfMemoryErr
#define FAKE_OT_TSDU			65507
#define FAKE_OT_MAX_BUSY		16

typedef struct {
	EndpointRef	ref;
	int			opened, closed;		/* times */
	int			nonBlocking;
	int			bound;
	InetPort	port;				/* as bound */
	InetHost	host;
} fakeOTEndpoint_t;

static fakeOTEndpoint_t	fakeOTEndpoints[2];	/* FAKE_OT_ENDPOINT, FAKE_OT_RESOLVER */
static struct OTConfiguration { int unused; } fakeOTConfiguration;
static int		fakeOTInited;			/* InitOpenTransport calls not yet closed */
static int		fakeOTInitCalls, fakeOTCloseCalls, fakeOTConfigCalls, fakeOTOpenCalls, fakeOTBindCalls;
static int		fakeOTMisuse;			/* calls on endpoints that are not open, ... */
static InetPort	fakeOTBusyPorts[FAKE_OT_MAX_BUSY];
static int		fakeOTBusyCount;
static int		fakeOTBindReassigns;

static int		fakeOTFailInit;			/* InitOpenTransport fails */
static int		fakeOTFailConfig;		/* this OTCreateConfiguration call (from 1) returns NULL */
static int		fakeOTFailOpen;			/* this OTOpenEndpoint call (from 1) fails */
static int		fakeOTFailNonBlocking;	/* OTSetNonBlocking fails */
static EndpointRef	fakeOTFailBind;		/* OTBind on this endpoint fails */

static void FakeOT_ResetLifetime( void ) {
	memset( fakeOTEndpoints, 0, sizeof( fakeOTEndpoints ) );
	fakeOTEndpoints[0].ref = FAKE_OT_ENDPOINT;
	fakeOTEndpoints[1].ref = FAKE_OT_RESOLVER;
	fakeOTInited = fakeOTInitCalls = fakeOTCloseCalls = fakeOTConfigCalls = 0;
	fakeOTOpenCalls = fakeOTBindCalls = fakeOTMisuse = 0;
	fakeOTBusyCount = fakeOTBindReassigns = 0;
	fakeOTFailInit = fakeOTFailConfig = fakeOTFailOpen = fakeOTFailNonBlocking = 0;
	fakeOTFailBind = kOTInvalidEndpointRef;
	fakeOTResolveError = noErr;
}

static void FakeOT_BusyPort( int port ) {
	fakeOTBusyPorts[fakeOTBusyCount++] = (InetPort)port;
}

static int FakeOT_PortBusy( InetPort port ) {
	int i;

	for ( i = 0 ; i < fakeOTBusyCount ; i++ ) {
		if ( fakeOTBusyPorts[i] == port ) {
			return 1;
		}
	}
	return 0;
}

/* The open endpoint ref names, or NULL (counted as misuse). */
static fakeOTEndpoint_t *FakeOT_Endpoint( void *ref ) {
	int i;

	for ( i = 0 ; i < 2 ; i++ ) {
		if ( fakeOTEndpoints[i].ref == ref && fakeOTEndpoints[i].opened > fakeOTEndpoints[i].closed ) {
			return &fakeOTEndpoints[i];
		}
	}
	fakeOTMisuse++;
	return NULL;
}

/* How many endpoints are open. */
static int FakeOT_OpenEndpoints( void ) {
	return ( fakeOTEndpoints[0].opened - fakeOTEndpoints[0].closed )
		+ ( fakeOTEndpoints[1].opened - fakeOTEndpoints[1].closed );
}

OSStatus InitOpenTransport( void ) {
	fakeOTInitCalls++;
	if ( fakeOTFailInit ) {
		return FAKE_OT_INJECTED_ERR;
	}
	fakeOTInited++;
	return noErr;
}

void CloseOpenTransport( void ) {
	fakeOTCloseCalls++;
	if ( fakeOTInited <= 0 || FakeOT_OpenEndpoints() ) {
		fakeOTMisuse++;		/* not open, or endpoints left behind */
	}
	fakeOTInited--;
}

OTConfigurationRef OTCreateConfiguration( const char *path ) {
	fakeOTConfigCalls++;
	if ( !fakeOTInited || strcmp( path, "udp" ) ) {
		fakeOTMisuse++;
	}
	if ( fakeOTConfigCalls == fakeOTFailConfig ) {
		return NULL;
	}
	return &fakeOTConfiguration;
}

EndpointRef OTOpenEndpoint( OTConfigurationRef config, OTOpenFlags oflag, TEndpointInfo *info, OSStatus *err ) {
	fakeOTEndpoint_t *ep;

	(void)oflag;
	fakeOTOpenCalls++;
	if ( !fakeOTInited || fakeOTOpenCalls > 2 ) {
		fakeOTMisuse++;
		*err = kOTOutStateErr;
		return kOTInvalidEndpointRef;
	}
	if ( config == NULL || config == kOTInvalidConfigurationPtr ) {
		*err = kOTBadConfigurationErr;
		return kOTInvalidEndpointRef;
	}
	if ( fakeOTOpenCalls == fakeOTFailOpen ) {
		*err = FAKE_OT_INJECTED_ERR;
		return kOTInvalidEndpointRef;
	}
	ep = &fakeOTEndpoints[fakeOTOpenCalls - 1];
	ep->opened++;
	ep->nonBlocking = ep->bound = 0;
	if ( info ) {
		memset( info, 0, sizeof( *info ) );
		info->addr = sizeof( InetAddress );
		info->tsdu = FAKE_OT_TSDU;
	}
	*err = noErr;
	return ep->ref;
}

OSStatus OTSetNonBlocking( ProviderRef ref ) {
	fakeOTEndpoint_t *ep = FakeOT_Endpoint( ref );

	if ( !ep ) {
		return kOTOutStateErr;
	}
	if ( fakeOTFailNonBlocking ) {
		return FAKE_OT_INJECTED_ERR;
	}
	ep->nonBlocking = 1;
	return noErr;
}

OSStatus OTBind( EndpointRef ref, TBind *reqAddr, TBind *retAddr ) {
	fakeOTEndpoint_t	*ep = FakeOT_Endpoint( ref );
	InetAddress			in, out;

	fakeOTBindCalls++;
	if ( !ep || ep->bound || !reqAddr || reqAddr->addr.len != sizeof( in ) ) {
		fakeOTMisuse++;
		return kOTOutStateErr;
	}
	if ( ref == fakeOTFailBind ) {
		return FAKE_OT_INJECTED_ERR;
	}
	memcpy( &in, reqAddr->addr.buf, sizeof( in ) );
	out = in;
	if ( in.fAddressType != AF_INET ) {
		return kOTBadAddressErr;
	}
	if ( in.fPort == 0 ) {
		out.fPort = 49152 + ( ep - fakeOTEndpoints );	/* any free port */
	} else if ( FakeOT_PortBusy( in.fPort ) ) {
		if ( !fakeOTBindReassigns ) {
			return kOTAddressBusyErr;
		}
		out.fPort = 50000;
	}
	ep->bound = 1;
	ep->port = out.fPort;
	ep->host = out.fHost;
	if ( retAddr && retAddr->addr.maxlen >= sizeof( out ) ) {
		memcpy( retAddr->addr.buf, &out, sizeof( out ) );
		retAddr->addr.len = sizeof( out );
	}
	return noErr;
}

OSStatus OTUnbind( EndpointRef ref ) {
	fakeOTEndpoint_t *ep = FakeOT_Endpoint( ref );

	if ( !ep || !ep->bound ) {
		return kOTOutStateErr;	/* unbinding an unbound endpoint just fails */
	}
	ep->bound = 0;
	return noErr;
}

OSStatus OTCloseProvider( ProviderRef ref ) {
	fakeOTEndpoint_t *ep = FakeOT_Endpoint( ref );

	if ( !ep ) {
		return kOTOutStateErr;
	}
	ep->closed++;
	ep->bound = 0;
	return noErr;
}

#endif
