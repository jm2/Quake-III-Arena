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
 * the target's does.  Every call is counted. */
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

enum { false = 0, true = 1 };	/* MacTypes.h */
enum { T_MORE = 0x0001 };
enum { kOTLookErr = -3158, kOTNoDataErr = -3162 };
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

#endif
