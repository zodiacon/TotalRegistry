#pragma once

enum class FindOptions {
	None = 0,
	SearchKeys = 1,
	SearchValues = 2,
	SearchData = 4,
	SearchRealRegistry = 8,
	SearchStdRegistry = 16,
	MatchCase = 0x20,
	MatchWholeWords = 0x40,
	SearchSelected = 0x80,
};
DEFINE_ENUM_FLAG_OPERATORS(FindOptions);
