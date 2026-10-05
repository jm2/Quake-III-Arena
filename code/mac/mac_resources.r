#include "Types.r"
#include "CodeFragments.r"
#include "quake3_icons.r"

type 'sizc' {
    longint; /* Minimum Size */
    longint; /* Preferred Size */
};

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

resource 'sizc' (0) {
	/* Minimum Size (heap margin). Engine fixed demand is ~73 MB
	   (56 MB hunk + 16 MB zone + smallzone) before code, CFM, and
	   malloc slack; the old 62.5 MB minimum let the Finder grant a
	   partition the game could never start in. */
	96000 * 1024,
	/* Preferred Size */
	128000 * 1024
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
	
	/* Memory Size (Same as sizc for consistency) */
	128000 * 1024,
	96000 * 1024
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
