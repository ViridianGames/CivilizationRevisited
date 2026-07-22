#ifndef _CIVTERRAINYIELDS_H_
#define _CIVTERRAINYIELDS_H_

#include "CivTerrain.h"

#include <cstdint>

// Government types (classic Civ1 set). Lives here so yields + cities can use it
// without a circular include through CivPlayer.
enum class CivGovernment : uint8_t
{
	Anarchy = 0,
	Despotism,
	Monarchy,
	Communism,
	Republic,
	Democracy,
	Count
};

inline const char* CivGovernmentName(CivGovernment g)
{
	switch (g)
	{
	case CivGovernment::Anarchy:    return "Anarchy";
	case CivGovernment::Despotism:  return "Despotism";
	case CivGovernment::Monarchy:   return "Monarchy";
	case CivGovernment::Communism:  return "Communism";
	case CivGovernment::Republic:   return "Republic";
	case CivGovernment::Democracy:  return "Democracy";
	default:                        return "?";
	}
}

// Food / production (shields) / trade from a worked map tile.
// Data reverse-engineered from CivOne tile classes + City.FoodValue/ShieldValue/TradeValue.
struct CivYields
{
	int food = 0;
	int shields = 0; // production
	int trade = 0;
};

// Static terrain facts (base yields without improvements; special resource bonuses).
struct CivTerrainYieldDef
{
	uint8_t terrain; // CivTerrain::Id
	const char* name;
	uint8_t movement;
	uint8_t defense; // combat defense multiplier base (Civ1 table)
	// Base yields (no improvements, not special).
	int8_t baseFood;
	int8_t baseShields;
	int8_t baseTrade;
	// Added when tile has special resource (oasis, game, gold, …).
	int8_t specialFood;
	int8_t specialShields;
	int8_t specialTrade;
	// Improvement deltas (added when present).
	int8_t irrigationFood; // e.g. desert/plains/hills
	int8_t mineShields;
	int8_t roadTrade;
	// Settler build costs (0 = cannot / N/A). Turns-ish costs from CivOne.
	uint8_t irrigationCost;
	uint8_t miningCost;
};

const CivTerrainYieldDef& CivTerrainYieldInfo(int terrainId);

// Tile improvement flags for yield calculation.
struct CivTileImprovements
{
	bool irrigation = false;
	bool mine = false;
	bool road = false;
	bool railRoad = false;
};

// Government group for yield penalties/bonuses (Civ1).
enum class CivGovYieldGroup : uint8_t
{
	// Anarchy / Despotism — no “organized” irrigation/mine extras; no rep trade bonus
	Despotic = 0,
	// Monarchy / Communism
	Monarchy,
	// Republic / Democracy
	Republic,
};

CivGovYieldGroup CivGovYieldGroupFrom(CivGovernment gov);

// Compute yields for one tile as a city would count it (worked square).
// Matches CivOne: tile base properties (improvements + special baked in) then
// government adjustments, then railroad ×1.5 (floored) on each resource.
CivYields CivComputeTileYields(
	int terrainId,
	bool special,
	const CivTileImprovements& imp,
	CivGovernment government);

// Convenience: government from enum group.
CivYields CivComputeTileYields(
	int terrainId,
	bool special,
	const CivTileImprovements& imp,
	CivGovYieldGroup govGroup);

#endif
