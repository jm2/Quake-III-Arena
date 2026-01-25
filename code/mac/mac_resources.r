#include "CodeFragmentTypes.r"

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
	18000 * 1024,
	/* Preferred Size */
	48000 * 1024
};
