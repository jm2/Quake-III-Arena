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
		0, 0,
		kIsApp, kOnDiskFlat, kZeroOffset, kWholeFork,
		"Quake3"
	}
};

resource 'sizc' (0) {
	/* Minimum Size (heap margin) */
	64000 * 1024,
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
	64000 * 1024
};
