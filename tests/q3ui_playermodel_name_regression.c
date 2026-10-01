/* Found auditing issue #440: the base q3_ui Player Model menu lists every
 * models/players/<model>/icon_<skin>.tga and, to select one, copies its
 * "<model>/<skin>" into a 64-byte modelskin (a stack buffer in
 * PlayerModel_SetMenuItems) with Q_strncpyz sized by the model name and an
 * unbounded strcat of the skin. Model directory and skin names come from pk3
 * entries of up to 255 bytes, so a name of 64 bytes or more overflowed it when the
 * menu opened. The menu now lists only models whose "<model>/<skin>" fits, and
 * lists and selects the others as before. */
#include "../code/q3_ui/ui_playermodel.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "q3_ui player model name regression failed: %s\n", what );
		exit( 1 );
	}
}

typedef struct {
	char name[256];
	const char *icons[4];
} servedModel_t;

static servedModel_t servedModels[8];
static int servedModelCount;
static char modelCvar[MAX_CVAR_VALUE_STRING];
static char setModel[MAX_QPATH];
static int pushed;
static int cutNames;

/** FS_GetFileList: the model directories under models/players, each with a
 * trailing '/', or the tga files in one of them. */
int trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	const char *names[8];
	int i, count, len, used;

	count = 0;
	if ( !strcmp( path, "models/players" ) ) {
		Check( !strcmp( extension, "/" ), "listed directories" );
		for ( i = 0; i < servedModelCount; i++ ) {
			names[count++] = servedModels[i].name;
		}
	} else {
		Check( !strncmp( path, "models/players/", 15 ) && !strcmp( extension, "tga" ), "listed icons" );
		for ( i = 0; i < servedModelCount; i++ ) {
			if ( !strncmp( path + 15, servedModels[i].name, strlen( path + 15 ) ) ) {
				break;
			}
		}
		Check( i < servedModelCount, "listed model directory" );
		for ( count = 0; count < 4 && servedModels[i].icons[count]; count++ ) {
			names[count] = servedModels[i].icons[count];
		}
	}
	used = 0;
	for ( i = 0; i < count; i++ ) {
		len = strlen( names[i] ) + 1;
		Check( used + len < bufsize, "listed names fit" );
		memcpy( listbuf + used, names[i], len );
		used += len;
	}
	return count;
}
/** The player's name and model. */
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	if ( !strcmp( var_name, "name" ) ) {
		Q_strncpyz( buffer, "Player", bufsize );
		return;
	}
	Check( !strcmp( var_name, "model" ), "read cvar" );
	Q_strncpyz( buffer, modelCvar, bufsize );
}
/** No build script precaches sounds. */
float trap_Cvar_VariableValue( const char *var_name ) {
	Check( !strcmp( var_name, "com_buildscript" ), "read cvar value" );
	return 0;
}
/** Too little memory to show the chosen model, so a click only selects it. */
int trap_MemoryRemaining( void ) {
	return 0;
}
/** Every menu picture has a handle. */
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) {
	(void)name;
	return 1;
}
/** Remember the model the menu shows. */
void UI_PlayerInfo_SetModel( playerInfo_t *pi, const char *model ) {
	(void)pi;
	Check( strlen( model ) < sizeof( setModel ), "shown model name fits" );
	strcpy( setModel, model );
}
void UI_PlayerInfo_SetInfo( playerInfo_t *pi, int legsAnim, int torsoAnim, vec3_t viewAngles, vec3_t moveAngles, weapon_t weaponNum, qboolean chat ) {
	(void)pi; (void)legsAnim; (void)torsoAnim; (void)viewAngles; (void)moveAngles; (void)weaponNum; (void)chat;
}
/** The menu is built but not drawn. */
void Menu_AddItem( menuframework_s *menu, void *item ) {
	(void)menu; (void)item;
}
void UI_PushMenu( menuframework_s *menu ) {
	Check( menu == &s_playermodel.menu, "pushed menu" );
	pushed = 1;
}
void Menu_SetCursorToItem( menuframework_s *m, void *ptr ) {
	(void)m; (void)ptr;
}
sfxHandle_t menu_move_sound;
sfxHandle_t menu_buzz_sound;
vec4_t color_red = { 1.00f, 0.00f, 0.00f, 1.00f };
vec4_t color_white = { 1.00f, 1.00f, 1.00f, 1.00f };
vec4_t text_color_normal = { 1.00f, 0.43f, 0.00f, 1.00f };
uiStatic_t uis;
/** The menus' drawing, event and key handlers are kept but never run here. */
void UI_DrawProportionalString( int x, int y, const char *str, int style, vec4_t color ) {
	(void)x; (void)y; (void)str; (void)style; (void)color;
	Check( 0, "no drawing" );
}
void UI_DrawPlayer( float x, float y, float w, float h, playerInfo_t *pi, int time ) {
	(void)x; (void)y; (void)w; (void)h; (void)pi; (void)time;
	Check( 0, "no drawing" );
}
void UI_PopMenu( void ) {
	Check( 0, "no menu events" );
}
void trap_Cvar_Set( const char *var_name, const char *value ) {
	(void)var_name; (void)value;
	Check( 0, "no menu events" );
}
sfxHandle_t Menu_DefaultKey( menuframework_s *m, int key ) {
	(void)m; (void)key;
	Check( 0, "no menu events" );
	return 0;
}
void Menu_SetCursor( menuframework_s *m, int cursor ) {
	(void)m; (void)cursor;
	Check( 0, "no menu events" );
}
void *Menu_ItemAtCursor( menuframework_s *m ) {
	(void)m;
	Check( 0, "no menu events" );
	return NULL;
}
sfxHandle_t trap_S_RegisterSound( const char *sample, qboolean compressed ) {
	(void)sample; (void)compressed;
	Check( 0, "no sounds precached" );
	return 0;
}
/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}
/** Count Com_sprintf's reports of a model name cut to fit modelnames. */
void QDECL Com_Printf( const char *msg, ... ) {
	Check( !strncmp( msg, "Com_sprintf: overflow", 21 ), "only Com_sprintf prints" );
	cutNames++;
}

/** Serve a model directory named with length bytes of c and its icons. */
static void ServeModel( int length, char c, const char *icon1, const char *icon2 ) {
	servedModel_t *model = &servedModels[servedModelCount++];

	memset( model, 0, sizeof( *model ) );
	memset( model->name, c, length );
	model->icons[0] = icon1;
	model->icons[1] = icon2;
}

/** Click the menu's picture at index and check what it selects. */
static void Click( int index, const char *modelskin, const char *model, const char *skin ) {
	menucommon_s *button = &s_playermodel.picbuttons[index].generic;

	button->callback( button, QM_ACTIVATED );
	Check( s_playermodel.selectedmodel == index, "clicked model is selected" );
	Check( !strcmp( s_playermodel.modelskin, modelskin ), "clicked model and skin" );
	Check( !strcmp( s_playermodel.modelname.string, model ), "clicked model name" );
	Check( !strcmp( s_playermodel.skinname.string, skin ), "clicked skin name" );
}

int main( void ) {
	/* "<50 f>/abcdefghijkl" is 63 bytes and fits modelskin; "<50 g>/abcdefghijklm"
	 * is 64 bytes and does not; a 120-byte model name does not fit either. */
	char fits[64], fitsModel[16];
	int i;

	servedModelCount = 0;
	ServeModel( 0, 0, "icon_default.tga", "icon_krusade.tga" );
	strcpy( servedModels[0].name, "sarge/" );
	ServeModel( 50, 'f', "band.tga", "icon_abcdefghijkl.tga" );
	ServeModel( 50, 'g', "icon_abcdefghijklm.tga", NULL );
	ServeModel( 120, 'h', "icon_default.tga", NULL );
	ServeModel( 0, 0, "icon_default.tga", NULL );
	strcpy( servedModels[4].name, "visor" );
	memset( fits, 'f', 50 );
	strcpy( fits + 50, "/abcdefghijkl" );
	Check( strlen( fits ) == 63, "fitting model and skin length" );
	memset( fitsModel, 'F', 15 );
	fitsModel[15] = '\0';

	/* The player uses visor, the last model, so opening the menu compares the
	 * player's model with every listed one. */
	strcpy( modelCvar, "visor/default" );
	UI_PlayerModelMenu();
	Check( pushed, "menu shown" );
	Check( s_playermodel.nummodels == 4, "listed models" );
	Check( !strcmp( s_playermodel.modelnames[0], "models/players/sarge/icon_default" ), "first listed model" );
	Check( !strcmp( s_playermodel.modelnames[1], "models/players/sarge/icon_krusade" ), "second listed model" );
	Check( !strncmp( s_playermodel.modelnames[2], "models/players/", 15 ) && !strcmp( s_playermodel.modelnames[2] + 15 + 51, "icon_abcdefghijkl" ), "fitting listed model" );
	Check( !strcmp( s_playermodel.modelnames[3], "models/players/visor/icon_default" ), "last listed model" );
	for ( i = 0; i < 4; i++ ) {
		Check( s_playermodel.pics[i].generic.name == s_playermodel.modelnames[i], "listed pictures" );
	}
	Check( s_playermodel.selectedmodel == 3, "the player's model is selected" );
	Check( !strcmp( s_playermodel.modelname.string, "VISOR" ), "the player's model name" );
	Check( !strcmp( s_playermodel.skinname.string, "DEFAULT" ), "the player's skin name" );
	Check( !strcmp( setModel, "visor/default" ), "the player's model is shown" );

	Click( 0, "sarge/default", "SARGE", "DEFAULT" );
	Click( 2, fits, fitsModel, "ABCDEFGHIJKL" );
	Click( 1, "sarge/krusade", "SARGE", "KRUSADE" );

	/* A player whose model and skin are the 63-byte ones. */
	strcpy( modelCvar, fits );
	UI_PlayerModelMenu();
	Check( s_playermodel.nummodels == 4 && s_playermodel.selectedmodel == 2, "the 63-byte model is selected" );
	Check( !strcmp( s_playermodel.modelname.string, fitsModel ), "the 63-byte model's name" );
	Check( !strcmp( s_playermodel.skinname.string, "ABCDEFGHIJKL" ), "the 63-byte model's skin name" );
	Check( !strcmp( setModel, fits ), "the 63-byte model is shown" );

	Check( cutNames == 0, "every listed model name fits modelnames" );
	puts( "q3_ui player model name regressions passed (issue #440 audit)" );
	return 0;
}
