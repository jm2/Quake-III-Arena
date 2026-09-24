/*
 * Issue #356: TossClientItems() takes the weapon number from the client's
 * own usercmd byte (pers.cmd.weapon, 0-255) while a weapon change is still
 * dropping, then computes "1 << weapon" and indexes ps.ammo[weapon] with it.
 *
 * A weapon value outside [WP_NONE, WP_NUM_WEAPONS) is:
 *   - undefined behaviour in the shift (1 << 66 is UB in C; the game module is
 *     native code here, so the compiler decides -- UBSan flags it), and
 *   - an out-of-bounds read of ps.ammo[MAX_WEAPONS] when the shift happens to
 *     alias an owned STAT_WEAPONS bit (on x86, 1 << 66 == 1 << (66 & 31) ==
 *     the machinegun bit, which every player owns), after which
 *     BG_FindItemForWeapon(66) raises Com_Error( ERR_DROP ) and drops the map.
 *
 * This test links the real TossClientItems (code/game/g_combat.c) and the real
 * BG_FindItemForWeapon (code/game/bg_misc.c), stubbing only the four engine
 * symbols those paths reach, and drives every weapon byte 0..255 through the
 * usercmd path. Without the fix it aborts under UBSan on "1 << 66"; with only
 * -fsanitize=address it instead reaches Com_Error( ERR_DROP ). With the fix the
 * clamp maps the value to WP_NONE before any shift or index and nothing fires.
 * Stock drops for legitimate weapons must be unchanged.
 */
#include "../code/game/g_local.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- engine symbols reached by the tested paths (after --gc-sections) --- */
vmCvar_t	g_gametype;
level_locals_t	level;

static jmp_buf	errorJump;
static int	errorGuard;
static int	errorCount;
static char	errorText[1024];

void QDECL Com_Error( int errLevel, const char *error, ... ) {
	va_list ap;
	(void)errLevel;
	va_start( ap, error );
	vsnprintf( errorText, sizeof( errorText ), error, ap );
	va_end( ap );
	errorCount++;
	if ( errorGuard ) {
		longjmp( errorJump, 1 );
	}
	fprintf( stderr, "Com_Error raised outside a guarded call: %s\n", errorText );
	exit( 1 );
}

static gentity_t	dropDummy;
static int		dropCount;
static int		lastDropTag;

gentity_t *Drop_Item( gentity_t *ent, gitem_t *item, float angle ) {
	(void)ent;
	(void)angle;
	dropCount++;
	lastDropTag = item ? item->giTag : -1;
	return &dropDummy;
}

/* --- test harness --- */
static gentity_t	self;
static gclient_t	client;

static void Fail( const char *message ) {
	fprintf( stderr, "TossClientItems weapon regression failed: %s\n", message );
	exit( 1 );
}

/* Run TossClientItems under a Com_Error guard; returns 1 if ERR_DROP fired. */
static int RunToss( void ) {
	dropCount = 0;
	lastDropTag = -1;
	if ( setjmp( errorJump ) ) {
		errorGuard = 0;
		return 1;
	}
	errorGuard = 1;
	TossClientItems( &self );
	errorGuard = 0;
	return 0;
}

/*
 * Reset to a mid-weapon-change state: the player is still holding the
 * machinegun (WEAPON_DROPPING), owns the gauntlet + machinegun, and the
 * memory past ps.ammo[] is non-zero so the out-of-bounds read would be
 * non-zero on master. Powerups are cleared so the powerup loop is inert.
 */
static void SetupDroppingMachinegun( int cmdWeapon ) {
	int i;

	memset( &self, 0xA5, sizeof( self ) );
	memset( &client, 0xA5, sizeof( client ) );
	self.client = &client;

	self.s.weapon = WP_MACHINEGUN;
	client.ps.weapon = WP_MACHINEGUN;
	client.ps.weaponstate = WEAPON_DROPPING;
	client.ps.stats[STAT_WEAPONS] = ( 1 << WP_GAUNTLET ) | ( 1 << WP_MACHINEGUN );
	client.pers.cmd.weapon = (byte)cmdWeapon;

	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		client.ps.powerups[i] = 0;
	}
	for ( i = 0 ; i < MAX_WEAPONS ; i++ ) {
		client.ps.ammo[i] = 0;
	}
	g_gametype.integer = GT_FFA;
	level.time = 1000;
}

/* The reported path: an out-of-range weapon byte drops in the DROPPING window. */
static void Test_UntrustedWeaponByte( void ) {
	int w;

	/* Headline value from the issue. On master this shifts 1 << 66 (UB;
	 * UBSan aborts here) and, with address-only sanitizing, aliases to the
	 * owned machinegun bit and reads ps.ammo[66] out of bounds. */
	SetupDroppingMachinegun( 66 );
	if ( RunToss() ) {
		Fail( "weapon 66 reached Com_Error( ERR_DROP )" );
	}
	if ( dropCount != 0 ) {
		Fail( "weapon 66 dropped an item the player does not own" );
	}

	/* Every possible usercmd weapon byte. */
	for ( w = 0 ; w <= 255 ; w++ ) {
		SetupDroppingMachinegun( w );
		if ( RunToss() ) {
			fprintf( stderr, "weapon byte %d raised ERR_DROP: %s\n", w, errorText );
			Fail( "usercmd weapon byte reached Com_Error( ERR_DROP )" );
		}
		/* The player owns only gauntlet + machinegun, neither of which is
		 * droppable here, so no legitimate value drops anything. */
		if ( dropCount != 0 ) {
			fprintf( stderr, "weapon byte %d produced %d drop(s), tag %d\n",
				w, dropCount, lastDropTag );
			Fail( "unowned/out-of-range weapon produced a drop" );
		}
	}
}

/* Defence in depth: even a bogus high STAT_WEAPONS bit must not let the raw
 * index run past the clamp. Every weapon from WP_NUM_WEAPONS up to 30 (a
 * defined shift, so this exercises the array-index / BG_FindItemForWeapon
 * guard independently of the UB shift) has its bit set and a non-zero ammo
 * word, so an upper bound off by one (> WP_NUM_WEAPONS, or >= MAX_WEAPONS)
 * reaches ERR_DROP. Below MAX_WEAPONS the ammo is set here; past ps.ammo[] the
 * index reaches the playerState_t and gclient_t words after it, which
 * SetupDroppingMachinegun leaves non-zero the way real memory would be. */
static void Test_HighStatBitIndex( void ) {
	int	w;
	int	word;

	for ( w = WP_NUM_WEAPONS ; w <= 30 ; w++ ) {
		SetupDroppingMachinegun( w );
		client.ps.stats[STAT_WEAPONS] |= ( 1 << w );
		if ( w < MAX_WEAPONS ) {
			client.ps.ammo[w] = 10;
		}
		memcpy( &word, (byte *)client.ps.ammo + w * sizeof( int ), sizeof( word ) );
		if ( !word ) {
			fprintf( stderr, "weapon %d\n", w );
			Fail( "the ammo word a high weapon index reaches is zero" );
		}
		if ( RunToss() ) {
			fprintf( stderr, "weapon %d with its STAT_WEAPONS bit raised ERR_DROP: %s\n", w, errorText );
			Fail( "a high weapon with its STAT_WEAPONS bit reached ERR_DROP" );
		}
		if ( dropCount != 0 ) {
			fprintf( stderr, "weapon %d produced %d drop(s), tag %d\n", w, dropCount, lastDropTag );
			Fail( "a high weapon with its STAT_WEAPONS bit dropped a nonexistent weapon item" );
		}
	}
}

/* Retail behaviour: a legitimate weapon must still be dropped on death. */
static void Test_StockDropsUnchanged( void ) {
	/* Direct case: holding the rocket launcher, not mid-change. */
	memset( &self, 0, sizeof( self ) );
	memset( &client, 0, sizeof( client ) );
	self.client = &client;
	self.s.weapon = WP_ROCKET_LAUNCHER;
	client.ps.weapon = WP_ROCKET_LAUNCHER;
	client.ps.weaponstate = WEAPON_READY;
	client.ps.stats[STAT_WEAPONS] = ( 1 << WP_MACHINEGUN ) | ( 1 << WP_ROCKET_LAUNCHER );
	client.ps.ammo[WP_ROCKET_LAUNCHER] = 10;
	g_gametype.integer = GT_FFA;
	level.time = 1000;
	if ( RunToss() ) {
		Fail( "stock rocket launcher drop raised ERR_DROP" );
	}
	if ( dropCount != 1 || lastDropTag != WP_ROCKET_LAUNCHER ) {
		Fail( "held rocket launcher was not dropped on death" );
	}

	/* Dropping-to-a-valid-weapon case: still holding the machinegun while a
	 * change to the rocket launcher is in flight. The clamp leaves the valid
	 * value untouched, so the rocket launcher must still drop. */
	memset( &self, 0, sizeof( self ) );
	memset( &client, 0, sizeof( client ) );
	self.client = &client;
	self.s.weapon = WP_MACHINEGUN;
	client.ps.weapon = WP_MACHINEGUN;
	client.ps.weaponstate = WEAPON_DROPPING;
	client.ps.stats[STAT_WEAPONS] = ( 1 << WP_MACHINEGUN ) | ( 1 << WP_ROCKET_LAUNCHER );
	client.pers.cmd.weapon = WP_ROCKET_LAUNCHER;
	client.ps.ammo[WP_ROCKET_LAUNCHER] = 10;
	g_gametype.integer = GT_FFA;
	level.time = 1000;
	if ( RunToss() ) {
		Fail( "valid weapon-change drop raised ERR_DROP" );
	}
	if ( dropCount != 1 || lastDropTag != WP_ROCKET_LAUNCHER ) {
		Fail( "pending rocket launcher change was not dropped on death" );
	}
}

int main( void ) {
	Test_StockDropsUnchanged();
	Test_UntrustedWeaponByte();
	Test_HighStatBitIndex();
	printf( "TossClientItems weapon regression passed (weapon bytes 0-255).\n" );
	return 0;
}
