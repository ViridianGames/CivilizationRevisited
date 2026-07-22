#ifndef _CIVFACTIONS_H_
#define _CIVFACTIONS_H_

#include "raylib.h"

#include <cstdint>

// Eight free team colors (original seven + red). Red is playable — not reserved
// for barbarians. Any faction may be assigned any color.
enum class CivColor : uint8_t
{
	White = 0,
	Green,
	Blue,
	Yellow,
	Cyan,
	Pink,
	Grey,
	Red,
	Count
};

inline const char* CivColorName(CivColor c)
{
	switch (c)
	{
	case CivColor::White:  return "White";
	case CivColor::Green:  return "Green";
	case CivColor::Blue:   return "Blue";
	case CivColor::Yellow: return "Yellow";
	case CivColor::Cyan:   return "Cyan";
	case CivColor::Pink:   return "Pink";
	case CivColor::Grey:   return "Grey";
	case CivColor::Red:    return "Red";
	default:               return "?";
	}
}

// Classic Civ1 player color pairs (CivOne Common.ColourLight / ColourDark).
// RGB is taken from the SP257.PIC VGA palette (indices below), matching the
// original game art rather than Raylib’s named colors.
//
//   Color    light idx  dark idx
//   White    15         7
//   Green    10         2
//   Blue      9         1
//   Yellow   14        10
//   Cyan     11         3
//   Pink     13         4
//   Grey      7         8
//   Red      12         4   (CivOne player 0 / barbarian slot)
//
// ColourLight = primary fill (cities, units, minimap, UI swatches).
// ColourDark  = outline / city icon / rim.

// SP257 palette indices 0–15 (decoded from SP257.PIC M0 chunk).
inline Color CivVgaPaletteColor(int index)
{
	// Matches extract_civ_pics / SP257.PIC (6-bit VGA ×4).
	static const Color kPal[16] = {
		{   0,   0,   0, 255 }, // 0  black / transparent key
		{  48,  76, 176, 255 }, // 1  dark blue
		{  44, 120,   0, 255 }, // 2  dark green
		{   0, 168, 168, 255 }, // 3  dark cyan
		{ 128,  32,  20, 255 }, // 4  dark red / brown
		{   0,   0,   0, 255 }, // 5  black
		{ 140,  88,  40, 255 }, // 6  brown
		{ 136, 136, 140, 255 }, // 7  medium grey (white-player dark)
		{  76,  76,  76, 255 }, // 8  dark grey
		{ 120, 140, 252, 255 }, // 9  blue
		{  96, 224, 100, 255 }, // 10 light green
		{  12, 224, 232, 255 }, // 11 cyan
		{ 244,  84,  84, 255 }, // 12 light red / pink-red
		{ 252,  84, 252, 255 }, // 13 magenta
		{ 252, 252, 148, 255 }, // 14 yellow
		{ 232, 232, 232, 255 }, // 15 near-white
	};
	if (index < 0 || index > 15)
		return Color{ 255, 0, 255, 255 };
	return kPal[index];
}

// Palette index for ColourLight (primary).
inline int CivColorLightIndex(CivColor c)
{
	switch (c)
	{
	case CivColor::White:  return 15;
	case CivColor::Green:  return 10;
	case CivColor::Blue:   return 9;
	case CivColor::Yellow: return 14;
	case CivColor::Cyan:   return 11;
	case CivColor::Pink:   return 13;
	case CivColor::Grey:   return 7;
	case CivColor::Red:    return 12;
	default:               return 15;
	}
}

// Palette index for ColourDark (rim / icon).
inline int CivColorDarkIndex(CivColor c)
{
	switch (c)
	{
	case CivColor::White:  return 7;
	case CivColor::Green:  return 2;
	case CivColor::Blue:   return 1;
	case CivColor::Yellow: return 10;
	case CivColor::Cyan:   return 3;
	case CivColor::Pink:   return 4;
	case CivColor::Grey:   return 8;
	case CivColor::Red:    return 4;
	default:               return 7;
	}
}

// Primary faction color (ColourLight) — city fill, minimap, UI swatches.
inline Color CivColorRgb(CivColor c)
{
	return CivVgaPaletteColor(CivColorLightIndex(c));
}

// Outline / rim / city-building tint (ColourDark).
inline Color CivColorDarkRgb(CivColor c)
{
	return CivVgaPaletteColor(CivColorDarkIndex(c));
}

// L-edge / chrome white used around cities (palette index 15).
inline Color CivCityEdgeWhite()
{
	return CivVgaPaletteColor(15);
}

// Playable tribes (original Civ1 roster; no forced color pairing).
enum class CivFactionId : uint8_t
{
	Romans = 0,
	Babylonians,
	Germans,
	Egyptians,
	Americans,
	Greeks,
	Indians,
	Russians,
	Zulus,
	French,
	Aztecs,
	Chinese,
	English,
	Mongols,
	Count
};

struct CivFactionDef
{
	CivFactionId id;
	const char* name;         // "Romans"
	const char* namePlural;   // "Roman"
	const char* leader;       // "Caesar"
	const char* capital;      // "Rome"
};

inline const CivFactionDef* CivFactionList(int* outCount = nullptr)
{
	static const CivFactionDef kList[] = {
		{ CivFactionId::Romans,      "Romans",      "Roman",      "Caesar",      "Rome" },
		{ CivFactionId::Babylonians, "Babylonians", "Babylonian", "Hammurabi",  "Babylon" },
		{ CivFactionId::Germans,     "Germans",     "German",     "Frederick",  "Berlin" },
		{ CivFactionId::Egyptians,   "Egyptians",   "Egyptian",   "Ramesses",   "Thebes" },
		{ CivFactionId::Americans,   "Americans",   "American",   "Lincoln",    "Washington" },
		{ CivFactionId::Greeks,      "Greeks",      "Greek",      "Alexander",  "Athens" },
		{ CivFactionId::Indians,     "Indians",     "Indian",     "Gandhi",     "Delhi" },
		{ CivFactionId::Russians,    "Russians",    "Russian",    "Stalin",     "Moscow" },
		{ CivFactionId::Zulus,       "Zulus",       "Zulu",       "Shaka",      "Zimbabwe" },
		{ CivFactionId::French,      "French",      "French",     "Napoleon",   "Paris" },
		{ CivFactionId::Aztecs,      "Aztecs",      "Aztec",      "Montezuma",  "Tenochtitlan" },
		{ CivFactionId::Chinese,     "Chinese",     "Chinese",    "Mao",        "Beijing" },
		{ CivFactionId::English,     "English",     "English",    "Elizabeth",  "London" },
		{ CivFactionId::Mongols,     "Mongols",     "Mongol",     "Genghis",    "Samarkand" },
	};
	if (outCount)
		*outCount = static_cast<int>(sizeof(kList) / sizeof(kList[0]));
	return kList;
}

inline const CivFactionDef& CivFaction(CivFactionId id)
{
	int n = 0;
	const CivFactionDef* list = CivFactionList(&n);
	const int i = static_cast<int>(id);
	if (i < 0 || i >= n)
		return list[0];
	return list[i];
}

// Classic difficulty levels (Civ1 names).
enum class CivDifficulty : uint8_t
{
	Chieftain = 0,
	Warlord,
	Prince,
	King,
	Emperor,
	Deity,
	Count
};

inline const char* CivDifficultyName(CivDifficulty d)
{
	switch (d)
	{
	case CivDifficulty::Chieftain: return "Chieftain (easiest)";
	case CivDifficulty::Warlord:   return "Warlord";
	case CivDifficulty::Prince:    return "Prince";
	case CivDifficulty::King:      return "King";
	case CivDifficulty::Emperor:   return "Emperor";
	case CivDifficulty::Deity:     return "Deity (toughest)";
	default:                       return "?";
	}
}

#endif
