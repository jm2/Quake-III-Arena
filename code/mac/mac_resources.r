#include "Types.r"
#include "CodeFragments.r"
#include "quake3_icons.r"

resource 'cfrg' (0) {
	{	/* array memberArray: 1 elements */
		/* [1] */
		kPowerPC,
		kFullLib,
		kNoVersionNum, kNoVersionNum,
		/* appStackSize: 1 MB. 0 meant "system default" (~64 KB), which
		   Q3's botlib recursion and large stack frames overflow. */
		1024 * 1024, 0,
		kIsApp, kOnDiskFlat, kZeroOffset, kWholeFork,
		"Quake3"
	}
};

resource 'SIZE' (-1) {
	reserved,
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
	doesActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreAppDiedEvents,
	is32BitCompatible,
	isHighLevelEventAware,
	localAndRemoteHLEvents,
	isStationeryAware,
	useTextEditServices,
	reserved,
	reserved,
	reserved,
	
	/* Preferred and minimum partition (issue #230). At the minimum the
	   engine's fixed demand is about 103.5 MB for Quake3 and 105.5 MB for
	   Quake3_TeamArena: the 56 MB hunk, the 16 MB zone, the 0.5 MB small
	   zone, the sound pool (12 MB at the Mac's com_soundMegs 4), the 1 MB
	   cfrg stack and the image (text, data and bss: 17.9 and 19.9 MB),
	   which CFM loads into the partition when virtual memory is off. That
	   leaves 11.7 MB for AGL/OpenGL, the Sound Manager and fragmentation in
	   Team Arena. cmake/mac_partition.py checks this against the sources
	   and the link. The preferred size also fits a com_soundMegs 8 carried
	   over from a PC q3config.cfg. */
	144000 * 1024,
	120000 * 1024
};

/* Sys_Error's Stop alert (mac_main.c), as retail's ALRT 128: ParamText
   puts "Quake 3 Error:" in ^0 and the message in ^1. */
resource 'ALRT' (128, purgeable) {
	{0, 0, 180, 420},
	128,
	beepStages,
	alertPositionMainScreen
};

resource 'DITL' (128, purgeable) {
	{
		{145, 330, 165, 400},
		Button { enabled, "Quit" };
		{13, 78, 31, 400},
		StaticText { disabled, "^0" };
		{35, 78, 135, 400},
		StaticText { disabled, "^1" }
	}
};
