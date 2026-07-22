#ifndef _CIVTERRITORY_H_
#define _CIVTERRITORY_H_

#include "CivCity.h"
#include "CivPlayer.h"
#include "CivTile.h"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Territory (Civ1 history / replay map style):
// Each living city claims land within Euclidean distance ≤ kCivTerritoryRadius
// (true circle of radius 5 on the grid — not a Chebyshev/king-move square),
// but ONLY tiles on the same land continent as the city (no claim across sea).
// More cities ⇒ more claimed land. Overlaps: nearest city wins (tie → lower id).
// Land beyond all city radii stays unowned.
// Inland lakes (smaller ocean bodies than the world ocean) are owned by a
// player if every land tile adjacent to that lake is owned by that player.
// ---------------------------------------------------------------------------

// Replay-map influence radius (user-confirmed from original history screen).
constexpr int kCivTerritoryRadius = 5;
// Compare squared distance to avoid float: dx*dx+dy*dy <= R*R
constexpr int kCivTerritoryRadiusSq = kCivTerritoryRadius * kCivTerritoryRadius;

struct CivTerritoryMap
{
	int width = 0;
	int height = 0;
	// Per-tile owner player id, or -1 unowned. Size width*height.
	std::vector<int8_t> owners;
	// Optional: city id that claimed the tile (-1 if none). Useful for debug.
	std::vector<int16_t> cityIds;

	void Clear()
	{
		width = height = 0;
		owners.clear();
		cityIds.clear();
	}

	void Resize(int w, int h)
	{
		width = w;
		height = h;
		const size_t n = static_cast<size_t>(w * h);
		owners.assign(n, static_cast<int8_t>(-1));
		cityIds.assign(n, static_cast<int16_t>(-1));
	}

	bool Valid() const { return width > 0 && height > 0 && !owners.empty(); }

	size_t Index(int x, int y) const
	{
		if (width <= 0)
			return 0;
		while (x < 0)
			x += width;
		x %= width;
		return static_cast<size_t>(y * width + x);
	}

	int8_t OwnerAt(int x, int y) const
	{
		if (!Valid() || y < 0 || y >= height)
			return -1;
		return owners[Index(x, y)];
	}

	// Claim land tiles within kCivTerritoryRadius of each city.
	void Compute(const CivMapData& map, const std::vector<const CivCity*>& cities);
	void Compute(const CivMapData& map, const std::vector<CivCity*>& cities);
	void Compute(const CivMapData& map, CivGameSetup& setup);

	// True if this tile is owned land and the neighbor (nx,ny) has a different owner
	// (or is ocean / unowned / off-map). Used to draw border segments.
	bool IsBorderWith(int x, int y, int nx, int ny) const;
};

// Classic map distance (CivOne Common.DistanceToTile): max(wrapX(|dx|), |dy|).
int CivTileDistance(int x1, int y1, int x2, int y2, int mapWidth);

// ---------------------------------------------------------------------------
// Fog of war: permanent explored + current visibility.
// Original-style: terrain memory stays; live view requires current LOS.
// ---------------------------------------------------------------------------

// Vision ranges (Chebyshev).
constexpr int kCivCityVisionRange = 2; // covers city BFC extent
constexpr int kCivUnitVisionRange = 1; // land unit default (when units exist)

// Mark a vision diamond/square around (cx,cy) as currently visible and explored.
void CivAddVision(CivPlayer& player, int cx, int cy, int range,
	int mapWidth, int mapHeight);

// Clear current visibility (keep explored). Call before rebuilding LOS each turn.
void CivClearCurrentVisibility(CivPlayer& player);

// Rebuild current visibility from the player's living cities (and later units).
// Always marks those tiles explored as well.
void CivRebuildPlayerVisibility(CivPlayer& player, const CivMapData& map,
	int cityVisionRange = kCivCityVisionRange);

// Rebuild for every active player.
void CivRebuildAllVisibility(CivGameSetup& setup, const CivMapData& map,
	int cityVisionRange = kCivCityVisionRange);

// Allocate fog bitmaps for a player to map size (safe to call repeatedly).
void CivEnsurePlayerFog(CivPlayer& player, int mapWidth, int mapHeight);

// Copy territory owners onto CivTile::owner for land tiles (ocean stays -1).
void CivApplyTerritoryToMap(CivMapData& map, const CivTerritoryMap& territory);

#endif
