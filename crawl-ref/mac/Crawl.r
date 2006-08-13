/*
 *  File:       Crawl.r
 *  Summary:   Mac resources
 *  Written by: Jesse Jones
 *
 *  Change History (most recent first):
 *
 *    <2>   5/25/02   JDJ   Added some Carbon resources
 *    <1>   3/26/99   JDJ   Created
 */

#include <BalloonTypes.r>
#include <Types.r>


// ============================================================================
// Carbon Resources
// ============================================================================
data 'carb' (0) {
};

// $$$ is this only for bundled apps?
resource 'plst' (0) {
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<!DOCTYPE plist SYSTEM\n\"file://localhost/System/Library/DTDs/PropertyList.dtd\">\n"
	"<plist version=\"4.0\">\n"
		"<dict>\n"
			"<key>CFBundleInfoDictionaryVersion</key>\n"
				"<string>4.0</string>\n"
			"<key>CFBundleIdentifier</key>\n"
				"<string>Crawl4</string>\n"
			"<key>CFBundleVersion</key>\n"
				"<string>4.0</string>\n"
			"<key>CFBundleDevelopmentRegion</key>\n"
				"<string>English</string>\n"
			"<key>CFBundleName</key>\n"
				"<string>Crawl</string>\n"
			"<key>CFBundlePackageType</key>\n"
				"<string>APPL</string>\n"
			"<key>CFBundleSignature</key>\n"
				"<string>????</string>\n"
		"</dict>\n"
	"</plist>"
};


// ============================================================================
// Finder Icon Help Message
// ============================================================================
resource 'hfdr' (-5696, "Finder Help") {
	HelpMgrVersion,
	hmDefaultOptions,
	0, 0,

	{
	HMStringItem {
	"Crawl is a fun game in the grand tradition of games like Rogue, Hack and Moria."
	},
	}
};


// ============================================================================
// Get Info String
// ============================================================================
type 'Crwl' {pstring;};

data 'Crwl' (0, "Owner resource") {
	"Crawl 3.3 ©1997 -1999 by Linley Henzell (Mac Port by Jesse Jones)"
};


// ============================================================================
// Derezed Resources
// ============================================================================

resource 'BNDL' (128) {
	'Crwl',
	0,
	{ 
	/* [1] */
	'FREF',
	{ /* array IDArray: 2 elements */
	/* [1] */
	0, 128,
	/* [2] */
	1, 129
	},
	/* [2] */
	'ICN#',
	{ 
	/* [1] */
	0, 0,
	/* [2] */
	1, 0
	}
	}
};

resource 'FREF' (128) {
	'CrlF',
	0,
	""
};

resource 'FREF' (129) {
	'APPL',
	1,
	""
};

resource 'DITL' (129) {
	{ 
	/* [1] */
	{45, 353, 65, 411},
	Button {
	enabled,
	"OK"
	},
	/* [2] */
	{19, 68, 90, 339},
	StaticText {
	disabled,
	"^0"
	},
	/* [3] */
	{38, 21, 70, 53},
	Icon {
	disabled,
	128
	}
	}
};

resource 'DITL' (130) {
	{ 
	/* [1] */
	{81, 136, 101, 194},
	Button {
	enabled,
	"No"
	},
	/* [2] */
	{81, 37, 101, 95},
	Button {
	enabled,
	"Yes"
	},
	/* [3] */
	{24, 43, 56, 188},
	StaticText {
	disabled,
	"Do you really want to quit without savin"
	"g?"
	}
	}
};

resource 'DITL' (256, "About", purgeable) {
	{ 
	/* [1] */
	{70, 220, 90, 280},
	Button {
	enabled,
	"OK"
	},
	/* [2] */
	{10, 70, 64, 279},
	StaticText {
	disabled,
	"Crawl 3.3 \n© 1997-1999 by Linley Henzell"
	"\nMac Port by Jesse Jones"
	},
	/* [3] */
	{10, 20, 42, 52},
	Icon {
	disabled,
	1
	}
	}
};

resource 'DITL' (131, purgeable) {
	{ 
	/* [1] */
	{71, 288, 91, 348},
	Button {
	enabled,
	"Save"
	},
	/* [2] */
	{71, 215, 91, 275},
	Button {
	enabled,
	"Cancel"
	},
	/* [3] */
	{71, 75, 91, 159},
	Button {
	enabled,
	"Don't Save"
	},
	/* [4] */
	{10, 75, 58, 348},
	StaticText {
	disabled,
	"Do you want to save your game before qui"
	"tting?"
	}
	}
};

data 'ALRT' (129) {
$"0028 0028 0096 01D7 0081 5555"                 /*.(.(.ñ.×.ÅUU */
};

data 'ALRT' (130) {
	$"0090 009A 011B 0180 0082 5555"             /* .ê.ö...Ä.ÇUU */
};

resource 'ALRT' (256, "About", purgeable) {
	{88, 85, 184, 378},
	256,
	{ 
		/* [1] */
		OK, visible, silent,
		/* [2] */
		OK, visible, silent,
		/* [3] */
		OK, visible, silent,
		/* [4] */
		OK, visible, silent
	},
	alertPositionMainScreen
};

resource 'ALRT' (131, "Save Changes", purgeable) {
	{104, 130, 205, 488},
	131,
	{ 
		/* [1] */
		OK, visible, silent,
		/* [2] */
		OK, visible, silent,
		/* [3] */
		OK, visible, silent,
		/* [4] */
		OK, visible, silent
	},
	alertPositionParentWindowScreen
};

resource 'clut' (256) {
	{
	// [0] // DOS colors crayon colors
	0xFFFF, 0xFFFF, 0xFFFF, // white

	// [1]
	0x0000, 0x3333, 0xCCCC, // blue

	// [2]
	0x0000, 0x6666, 0x3333, // green pine

	// [3]
	0x6666, 0xCCCC, 0xCCCC, // cyan fog

	// [4]
	0xFFFF, 0x0000, 0x3333, // red

	// [5]
	0xCCCC, 0x6666, 0xCCCC, // magenta orchid

	// [6]
	0x9999, 0x6666, 0x3333, // brown dirt

	// [7]
	0x9999, 0x9999, 0x9999, // light grey granite

	// [8]
	0x3333, 0x3333, 0x3333, // drag grey gabbro

	// [9]
	0x6666, 0x9999, 0xFFFF, // light blue sky blue

	// [10]
	0x3333, 0x9999, 0x3333, // light green clover

	// [11]
	0x6666, 0xCCCC, 0x9999, // light cyan ocean green

	// [12]
	0xFFFF, 0x6666, 0x0000, // light red fire

	// [13]
	0xFFFF, 0x9999, 0x0000, // light magenta orange

	// [14]
	0xFFFF, 0xFFFF, 0x0000, // yellow lemon

	// [15]
	0x0000, 0x0000, 0x0000 // black
	}
};
