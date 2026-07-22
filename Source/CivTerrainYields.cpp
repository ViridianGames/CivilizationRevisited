#include "CivTerrainYields.h"

#include <algorithm>
#include <cmath>

namespace
{
	// Base table from CivOne Tiles/*.cs (yields without improvements; specials separate).
	// Irrigation/mine/road deltas match the same classes' property formulas.
	const CivTerrainYieldDef kYields[] = {
		// terrain, name, move, def, F,S,T, spF,spS,spT, irrF, mineS, roadT, irrCost, mineCost
		{ CivTerrain::Desert,    "Desert",    1, 2, 0, 1, 0,  2, 0, 0,  1, 1, 1,  5,  5 },
		{ CivTerrain::Plains,    "Plains",    1, 2, 1, 1, 0,  0, 1, 0,  1, 0, 1,  5, 15 },
		{ CivTerrain::Grassland, "Grassland", 1, 2, 2, 0, 0,  0, 1, 0,  0, 0, 1,  5, 10 },
		{ CivTerrain::Forest,    "Forest",    2, 3, 1, 2, 0,  1, 0, 0,  0, 0, 0,  5,  0 },
		{ CivTerrain::Hills,     "Hills",     2, 4, 1, 0, 0,  0, 2, 0,  1, 2, 0, 10, 10 },
		{ CivTerrain::Mountains, "Mountains", 3, 6, 0, 1, 0,  0, 0, 5,  0, 1, 0,  0, 10 },
		{ CivTerrain::Tundra,    "Tundra",    1, 2, 1, 0, 0,  1, 0, 0,  0, 0, 0,  0,  0 },
		{ CivTerrain::Arctic,    "Arctic",    2, 2, 0, 0, 0,  2, 0, 0,  0, 0, 0,  0,  0 },
		{ CivTerrain::Swamp,     "Swamp",     2, 3, 1, 0, 0,  0, 4, 0,  0, 0, 0, 15, 15 },
		{ CivTerrain::Jungle,    "Jungle",    2, 3, 1, 0, 0,  0, 0, 3,  0, 0, 0, 15, 15 },
		{ CivTerrain::Ocean,     "Ocean",     1, 2, 1, 0, 2,  1, 0, 0,  0, 0, 1,  0,  0 },
		{ CivTerrain::River,     "River",     1, 3, 2, 0, 1,  0, 1, 0,  0, 0, 0,  5,  0 },
	};
}

const CivTerrainYieldDef& CivTerrainYieldInfo(int terrainId)
{
	static const CivTerrainYieldDef kNone{
		255, "None", 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	if (terrainId < 0 || terrainId >= CivTerrain::Count)
		return kNone;
	return kYields[terrainId];
}

CivGovYieldGroup CivGovYieldGroupFrom(CivGovernment gov)
{
	switch (gov)
	{
	case CivGovernment::Anarchy:
	case CivGovernment::Despotism:
		return CivGovYieldGroup::Despotic;
	case CivGovernment::Monarchy:
	case CivGovernment::Communism:
		return CivGovYieldGroup::Monarchy;
	case CivGovernment::Republic:
	case CivGovernment::Democracy:
		return CivGovYieldGroup::Republic;
	default:
		return CivGovYieldGroup::Despotic;
	}
}

namespace
{
	// Tile property yields (improvements + special), matching CivOne Tiles/*.cs.
	CivYields TileProperties(int terrainId, bool special, const CivTileImprovements& imp)
	{
		const CivTerrainYieldDef& d = CivTerrainYieldInfo(terrainId);
		CivYields y;
		y.food = d.baseFood;
		y.shields = d.baseShields;
		y.trade = d.baseTrade;

		if (special)
		{
			y.food += d.specialFood;
			y.shields += d.specialShields;
			y.trade += d.specialTrade;
		}

		if (imp.irrigation)
			y.food += d.irrigationFood;
		if (imp.mine)
			y.shields += d.mineShields;
		// Road or railroad counts as road for the +1 trade on desert/plains/grass/ocean.
		if (imp.road || imp.railRoad)
			y.trade += d.roadTrade;

		return y;
	}

	// City.FoodValue / ShieldValue / TradeValue government extras (CivOne City.cs).
	void ApplyGovernment(CivYields& y, int terrainId, bool special,
		const CivTileImprovements& imp, CivGovYieldGroup gov)
	{
		const bool organized = (gov != CivGovYieldGroup::Despotic); // not anarchy/despotism
		const bool monarchy = (gov == CivGovYieldGroup::Monarchy);
		const bool republic = (gov == CivGovYieldGroup::Republic);

		// Food extras
		if (organized)
		{
			switch (terrainId)
			{
			case CivTerrain::Desert:
			case CivTerrain::Forest:
			case CivTerrain::Grassland:
			case CivTerrain::River:
				if (imp.irrigation)
					y.food += 1;
				break;
			case CivTerrain::Ocean:
			case CivTerrain::Tundra:
				if (special)
					y.food += 1;
				break;
			default:
				break;
			}
		}

		// Shield extras
		if (organized && terrainId == CivTerrain::Hills && imp.mine)
			y.shields += 1;

		// Trade extras
		switch (terrainId)
		{
		case CivTerrain::Desert:
		case CivTerrain::Grassland:
		case CivTerrain::Plains:
			if ((imp.road || imp.railRoad) && republic)
				y.trade += 1;
			break;
		case CivTerrain::Ocean:
		case CivTerrain::River:
			if (republic)
				y.trade += 1;
			break;
		case CivTerrain::Jungle:
			if (special)
			{
				if (monarchy)
					y.trade += 1;
				if (republic)
					y.trade += 2;
			}
			break;
		case CivTerrain::Mountains:
			if (special)
			{
				if (monarchy)
					y.trade += 1;
				if (republic)
					y.trade += 2;
			}
			break;
		default:
			break;
		}
	}

	void ApplyRailroad(CivYields& y, bool rail)
	{
		if (!rail)
			return;
		// CivOne: floor(output * 1.5) on each of food, shields, trade.
		y.food = static_cast<int>(std::floor(static_cast<double>(y.food) * 1.5));
		y.shields = static_cast<int>(std::floor(static_cast<double>(y.shields) * 1.5));
		y.trade = static_cast<int>(std::floor(static_cast<double>(y.trade) * 1.5));
	}
}

CivYields CivComputeTileYields(
	int terrainId,
	bool special,
	const CivTileImprovements& imp,
	CivGovYieldGroup govGroup)
{
	CivYields y = TileProperties(terrainId, special, imp);
	ApplyGovernment(y, terrainId, special, imp, govGroup);
	ApplyRailroad(y, imp.railRoad);
	// Clamp negatives (should not happen).
	y.food = std::max(0, y.food);
	y.shields = std::max(0, y.shields);
	y.trade = std::max(0, y.trade);
	return y;
}

CivYields CivComputeTileYields(
	int terrainId,
	bool special,
	const CivTileImprovements& imp,
	CivGovernment government)
{
	return CivComputeTileYields(terrainId, special, imp, CivGovYieldGroupFrom(government));
}
