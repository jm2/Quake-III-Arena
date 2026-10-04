/* Issue #49: Team Arena menu loading when the UI pools run out.
 * Menus are parsed into two fixed pools in ui_shared.c: UI_Alloc's memory pool
 * (items, their type data and the string hash nodes) and String_Alloc's string
 * pool. Each case serves a menu through a small trap_PC_ReadToken stand-in for
 * the engine's precompiler and runs it against every budget either pool can
 * have, so the allocation that fails is in turn every allocation the menu
 * makes. A menu that does not fit is not counted, a counted menu is complete,
 * and a failed allocation takes no pool space.
 * Built once as the ui module and once with -DCGAME (smaller pools). */
#include "../code/ui/ui_shared.c"

#include <stdio.h>
#include <stdlib.h>

static void Check( int ok, const char *what );
#include "ui_menu_source_fixture.h"

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI menu allocation regression failed: %s\n", what );
		exit( 1 );
	}
}

/* A menu with copies of six items: one of every kind that has type data
 * (list box, edit field, multi lists of strings and of floats, model; the
 * slider is an edit field), with
 * menu and item strings and scripts. Each @ becomes the menu's name, so menus
 * share no string and every string of a menu takes new pool space. */
static const char fixtureHead[] =
	"{ name \"@\" rect 0 0 640 480 visible 1 background \"ui/@\" onOpen { setcvar ui_@ 1 } ";
static const char fixtureItems[] =
	"itemDef { name \"@list\" type 6 rect 10 10 200 100 elementwidth 120 elementheight 20 "
		"elementtype 0 feeder 1 columns 2 2 40 20 50 40 30 doubleclick { play \"@click\" } visible 1 } "
	"itemDef { name \"@edit\" type 4 text \"@Name:\" cvar \"ui_@edit\" maxChars 16 maxPaintChars 12 visible 1 } "
	"itemDef { name \"@multi\" type 12 text \"@Mode:\" cvar \"ui_@multi\" "
		"cvarStrList { \"@Free\" \"@ffa\" \"@Team\" \"@tdm\" } visible 1 } "
	"itemDef { name \"@model\" type 7 asset_model \"models/@/head.md3\" model_fovx 40 visible 1 } "
	"itemDef { name \"@slider\" type 10 text \"@Volume:\" cvarFloat \"ui_@volume\" 0.5 0 1 visible 1 } "
	"itemDef { name \"@floats\" type 12 cvar \"ui_@floats\" cvarFloatList { \"@Low\" 0 \"@High\" 2 } visible 1 } ";
#define FIXTURE_ITEMS 6

static char source[65536];

/** Serve text as the precompiler source that Menu_New reads. */
static void ServeSource( const char *text ) {
	Check( strlen( text ) < sizeof( source ), "fixture source fits" );
	strcpy( source, text );
	MenuSource_Serve( 1, source );
}

/** Parse and allocation reports are not checked. */
void QDECL Com_Printf( const char *msg, ... ) { (void)msg; }
/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}
static void QDECL TestPrint( const char *msg, ... ) { (void)msg; }
static qhandle_t TestRegisterShaderNoMip( const char *name ) { return name && *name ? 1 : 0; }
static qhandle_t TestRegisterModel( const char *name ) { return name && *name ? 1 : 0; }
static sfxHandle_t TestRegisterSound( const char *name, qboolean compressed ) { (void)compressed; return name && *name ? 1 : 0; }

static displayContextDef_t testDC;

/** Start over as UI_Load does, with both pools empty. */
static void ResetPools( void ) {
	String_Init();
	Check( allocPoint == 0 && strPoolIndex == 0 && menuCount == 0, "pools reset" );
}

/** Leave exactly room bytes of the memory pool for what follows. */
static void LimitMemoryPool( int room ) {
	Check( room >= 0 && room <= MEM_POOL_SIZE - allocPoint && ( room & 15 ) == 0, "memory budget" );
	if ( MEM_POOL_SIZE - allocPoint > room ) {
		Check( UI_Alloc( MEM_POOL_SIZE - allocPoint - room ) != NULL, "memory pool filler" );
	}
	Check( MEM_POOL_SIZE - allocPoint == room, "memory pool left with the budget" );
}

/** Leave exactly room bytes of the string pool for what follows. The filler is
 * distinct strings of at most FILLER_LENGTH characters, as long as the longest
 * token or script a menu stores, which keeps hashForString within a 32-bit long. */
#define FILLER_LENGTH 1000
static void LimitStringPool( int room ) {
	char filler[FILLER_LENGTH + 1];
	char digits[16];
	int serial, length, need;

	/* a string fits only while one byte of the pool stays free */
	Check( room > 0 && room <= STRING_POOL_SIZE - strPoolIndex, "string budget" );
	for ( serial = 0; ( need = STRING_POOL_SIZE - strPoolIndex - room ) > 0; serial++ ) {
		/* each string takes its length and a terminator; never leave 1 byte,
		 * which no string can take */
		length = need - 1;
		if ( length > FILLER_LENGTH ) {
			length = need - 1 - FILLER_LENGTH == 1 ? FILLER_LENGTH - 1 : FILLER_LENGTH;
		}
		Check( length > 0, "filler length" );
		/* '#' starts no fixture string; the serial after it makes every
		 * filler new (only the last one can be too short to hold it all) */
		memset( filler, '#', length );
		filler[length] = 0;
		Com_sprintf( digits, sizeof( digits ), "%d", serial );
		memcpy( filler + 1, digits, (int)strlen( digits ) < length - 1 ? strlen( digits ) : (size_t)( length - 1 ) );
		Check( String_Alloc( filler ) != NULL, "string pool filler" );
		Check( STRING_POOL_SIZE - strPoolIndex == room + need - length - 1, "filler is new" );
	}
	Check( STRING_POOL_SIZE - strPoolIndex == room, "string pool left with the budget" );
}

/** Append fixture text to text at *length, with each @ replaced by name. */
static void AppendFixture( char *text, int *length, const char *fixture, const char *name ) {
	for ( ; *fixture; fixture++ ) {
		Check( *length + strlen( name ) + 2 < sizeof( source ), "fixture menu fits" );
		if ( *fixture == '@' ) {
			strcpy( text + *length, name );
			*length += strlen( name );
		} else {
			text[( *length )++] = *fixture;
		}
	}
	text[*length] = 0;
}

/** Parse the fixture menu named name with copies of its items; returns whether
 * it was counted. */
static qboolean LoadMenuCopies( const char *name, int copies ) {
	static char text[sizeof( source )];
	int before = menuCount;
	int length = 0;
	int i;

	AppendFixture( text, &length, fixtureHead, name );
	for ( i = 0; i < copies; i++ ) {
		AppendFixture( text, &length, fixtureItems, name );
	}
	AppendFixture( text, &length, "}", name );
	ServeSource( text );
	Menu_New( 1 );
	Check( menuCount == before || menuCount == before + 1, "a menu is counted at most once" );
	return menuCount > before;
}

/** Parse the fixture menu named name with one copy of its items. */
static qboolean LoadMenu( const char *name ) {
	return LoadMenuCopies( name, 1 );
}

/** A counted menu is complete: every item is there, belongs to the menu, and
 * has the type data and strings its type reads when it is drawn or used. */
static void CheckMenuCopies( menuDef_t *menu, const char *name, int copies ) {
	itemDef_t **items;
	multiDef_t *multi;
	listBoxDef_t *list;
	editFieldDef_t *edit;
	int i;

	Check( menu->window.name && !strcmp( menu->window.name, name ), "menu name" );
	Check( menu->onOpen && strstr( menu->onOpen, va( "ui_%s", name ) ), "menu script" );
	Check( menu->itemCount == copies * FIXTURE_ITEMS, "every item counted" );
	for ( i = 0; i < menu->itemCount; i++ ) {
		Check( menu->items[i] && menu->items[i]->parent == menu, "item belongs to its menu" );
		Check( menu->items[i]->window.name != NULL, "item name" );
		Check( menu->items[i]->typeData != NULL, "item type data" );
	}
	for ( i = 0; i < copies; i++ ) {
		items = menu->items + i * FIXTURE_ITEMS;
		list = (listBoxDef_t *)items[0]->typeData;
		Check( list->elementWidth == 120 && list->numColumns == 2 && list->doubleClick
			&& strstr( list->doubleClick, va( "%sclick", name ) ), "list box" );
		edit = (editFieldDef_t *)items[1]->typeData;
		Check( edit->maxChars == 16 && edit->maxPaintChars == 12
			&& !strcmp( items[1]->cvar, va( "ui_%sedit", name ) ), "edit field" );
		multi = (multiDef_t *)items[2]->typeData;
		Check( multi->count == 2 && multi->strDef && !strcmp( multi->cvarList[1], va( "%sTeam", name ) )
			&& !strcmp( multi->cvarStr[1], va( "%stdm", name ) ), "multi list" );
		Check( items[3]->asset == 1 && ( (modelDef_t *)items[3]->typeData )->fov_x == 40, "model" );
		edit = (editFieldDef_t *)items[4]->typeData;
		Check( edit->maxVal == 1 && !strcmp( items[4]->cvar, va( "ui_%svolume", name ) ), "slider" );
		multi = (multiDef_t *)items[5]->typeData;
		Check( multi->count == 2 && !multi->strDef && !strcmp( multi->cvarList[1], va( "%sHigh", name ) )
			&& multi->cvarValue[1] == 2, "multi list of floats" );
	}
}

/** The same for a menu with one copy of the items. */
static void CheckMenu( menuDef_t *menu, const char *name ) {
	CheckMenuCopies( menu, name, 1 );
}

/** The memory and string pool use of one fixture menu. Menus named with the
 * same number of characters use the same amount of each pool. */
static void MeasureMenu( int *memory, int *strings ) {
	ResetPools();
	Check( LoadMenu( "menuA" ), "menu loads with room" );
	CheckMenu( &Menus[0], "menuA" );
	*memory = allocPoint;
	*strings = strPoolIndex;
}

/** Load the menu with every memory pool budget below what it needs, so each of
 * its allocations fails in turn: the menu is not counted and the menus loaded
 * before it are untouched. With the whole budget it loads in full. */
static void TestMemoryBudgets( void ) {
	int memory, strings, room, loaded;

	MeasureMenu( &memory, &strings );
	for ( room = 0; room <= memory; room += 16 ) {
		ResetPools();
		Check( LoadMenu( "menuA" ), "first menu loads" );
		loaded = allocPoint;
		LimitMemoryPool( room );
		if ( LoadMenu( "menuB" ) ) {
			Check( room == memory, "menu counted only with all of its memory" );
			CheckMenu( &Menus[1], "menuB" );
		} else {
			Check( room < memory, "menu with all of its memory loads" );
			Check( menuCount == 1, "failed menu not counted" );
		}
		CheckMenu( &Menus[0], "menuA" );
		Check( loaded <= allocPoint, "memory pool only grows" );
	}
}

/** The same with every string pool budget below what the menu needs. */
static void TestStringBudgets( void ) {
	int memory, strings, room;

	MeasureMenu( &memory, &strings );
	/* a string fits only while one byte of the pool stays free */
	for ( room = 1; room <= strings + 2; room++ ) {
		ResetPools();
		Check( LoadMenu( "menuA" ), "first menu loads" );
		LimitStringPool( room );
		if ( LoadMenu( "menuB" ) ) {
			Check( room > strings, "menu counted only with all of its strings" );
			CheckMenu( &Menus[1], "menuB" );
		} else {
			Check( room <= strings, "menu with all of its strings loads" );
			Check( menuCount == 1, "failed menu not counted" );
		}
		CheckMenu( &Menus[0], "menuA" );
	}
}

/** String_Alloc whose hash node does not fit stores nothing: the string pool
 * keeps its room for a string whose node does fit later. */
static void TestStringNode( void ) {
	int used;

	ResetPools();
	Check( String_Alloc( "pooled" ) != NULL, "string with room" );
	LimitMemoryPool( 0 );
	used = strPoolIndex;
	Check( String_Alloc( "orphan" ) == NULL, "string without a node fails" );
	Check( strPoolIndex == used, "a string without a node takes no string pool space" );
	Check( !strcmp( String_Alloc( "pooled" ), "pooled" ), "pooled string still found" );

	/* with room in neither pool the string pool is not touched either */
	ResetPools();
	LimitStringPool( 4 );
	LimitMemoryPool( 0 );
	used = strPoolIndex;
	Check( String_Alloc( "toolong" ) == NULL && strPoolIndex == used, "string without room" );
}

/** Load menus named prefix0, prefix1, ... with copies of the items until one
 * does not fit; a pool must run out before the menu slots do. Every counted
 * menu is complete, and no later menu loads. */
static void LoadMenuSet( const char *prefix, int copies ) {
	static char name[MAX_TOKENLENGTH];
	int i, count;

	ResetPools();
	for ( i = 0; i < MAX_MENUS; i++ ) {
		Com_sprintf( name, sizeof( name ), "%s%d", prefix, i );
		if ( !LoadMenuCopies( name, copies ) ) {
			break;
		}
	}
	count = menuCount;
	Check( count > 0 && count < MAX_MENUS, "a pool ran out before the menu slots" );
	for ( i = 0; i < count; i++ ) {
		Com_sprintf( name, sizeof( name ), "%s%d", prefix, i );
		CheckMenuCopies( &Menus[i], name, copies );
	}
	Com_sprintf( name, sizeof( name ), "%s%d", prefix, MAX_MENUS );
	Check( !LoadMenuCopies( name, copies ) && menuCount == count, "no menu loads into a full pool" );
}

/** Menu sets larger than each pool of the module: menus with many items run
 * out of memory, menus with long strings run out of string space. Afterwards
 * the pools are reset and a menu loads in full, as on a menu reload. */
static void TestMenuSet( void ) {
	static char prefix[700];

	/* MAX_MENUITEMS items in each menu */
	LoadMenuSet( "menu", MAX_MENUITEMS / FIXTURE_ITEMS );
	Check( UI_OutOfMemory(), "memory pool ran out" );

	memset( prefix, 'x', sizeof( prefix ) - 1 );
	LoadMenuSet( prefix, 1 );
	Check( !UI_OutOfMemory() && STRING_POOL_SIZE - strPoolIndex < (int)sizeof( prefix ) + 32,
		"string pool ran out" );

	ResetPools();
	Check( LoadMenu( "reloaded" ), "menu loads after a reset" );
	CheckMenu( &Menus[0], "reloaded" );
}

/** One case per process, named by the argument, so each fails on its own. */
int main( int argc, char **argv ) {
	const char *test = argc > 1 ? argv[1] : "";

	testDC.Print = TestPrint;
	testDC.registerShaderNoMip = TestRegisterShaderNoMip;
	testDC.registerModel = TestRegisterModel;
	testDC.registerSound = TestRegisterSound;
	Init_Display( &testDC );
	if ( !strcmp( test, "node" ) ) {
		TestStringNode();
	} else if ( !strcmp( test, "memory" ) ) {
		TestMemoryBudgets();
	} else if ( !strcmp( test, "strings" ) ) {
		TestStringBudgets();
	} else {
		Check( !strcmp( test, "set" ), "known case" );
		TestMenuSet();
	}
	return 0;
}
