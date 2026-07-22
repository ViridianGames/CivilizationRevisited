#include "CivTerritory.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Distance
// ---------------------------------------------------------------------------

int CivTileDistance(int x1, int y1, int x2, int y2, int mapWidth)
{
	int dx = std::abs(x2 - x1);
	if (mapWidth > 0)
	{
		if (dx > mapWidth - dx)
			dx = mapWidth - dx;
	}
	const int dy = std::abs(y2 - y1);
	return std::max(dx, dy);
}

// ---------------------------------------------------------------------------
// Territory
// ---------------------------------------------------------------------------

void CivTerritoryMap::Compute(const CivMapData& map, const std::vector<const CivCity*>& cities)
{
	Resize(map.width, map.height);
	if (map.tiles.empty() || cities.empty() || map.width <= 0 || map.height <= 0)
		return;

	// Each city claims land in a true Euclidean circle of radius kCivTerritoryRadius.
	// Overlaps: nearest city (by Euclidean dist²) wins; tie → lower city id.
	struct Claim
	{
		int distSq = std::numeric_limits<int>::max();
		int owner = -1;
		int cityId = -1;
	};
	std::vector<Claim> best(static_cast<size_t>(width * height));

	// Callers should run map.ComputeContinents() first so continentId is set.

	const int R = kCivTerritoryRadius;
	const int R2 = kCivTerritoryRadiusSq;
	for (const CivCity* c : cities)
	{
		if (!c || !c->Valid())
			continue;

		const uint8_t cityCont = map.TileAt(c->x, c->y).continentId;

		// Bounding box of the circle; still filter with dx²+dy².
		for (int dy = -R; dy <= R; ++dy)
		{
			for (int dx = -R; dx <= R; ++dx)
			{
				const int distSq = dx * dx + dy * dy;
				if (distSq > R2)
					continue; // outside Euclidean circle

				int tx = c->x + dx;
				const int ty = c->y + dy;
				if (ty < 0 || ty >= height)
					continue;
				while (tx < 0)
					tx += width;
				tx %= width;

				const CivTile& t = map.TileAt(tx, ty);
				if (t.IsOcean())
					continue;

				// Must be same land continent as the city (no ownership across sea).
				if (cityCont != 0 && t.continentId != cityCont)
					continue;
				if (cityCont == 0 && t.continentId != 0)
					continue; // city on ocean? skip claiming foreign land

				const size_t i = Index(tx, ty);
				Claim& cl = best[i];
				if (distSq < cl.distSq || (distSq == cl.distSq && (cl.cityId < 0 || c->id < cl.cityId)))
				{
					cl.distSq = distSq;
					cl.owner = c->owner;
					cl.cityId = c->id;
				}
			}
		}
	}

	for (size_t i = 0; i < best.size(); ++i)
	{
		if (best[i].owner >= 0)
		{
			owners[i] = static_cast<int8_t>(best[i].owner);
			cityIds[i] = static_cast<int16_t>(best[i].cityId);
		}
	}

	// --- Inland lakes ---
	// Flood-fill connected ocean components. The largest body is the world
	// ocean (never owned). Smaller bodies are lakes: if every land tile
	// touching the lake is owned by the same player, claim the lake for them.
	const size_t n = static_cast<size_t>(width * height);
	std::vector<int> bodyOf(n, -1);
	std::vector<std::vector<std::pair<int, int>>> bodies;
	bodies.reserve(64);

	auto wrapX = [&](int x) {
		while (x < 0)
			x += width;
		return x % width;
	};

	static const int kOx[4] = { 0, 1, 0, -1 };
	static const int kOy[4] = { -1, 0, 1, 0 };

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const size_t si = Index(x, y);
			if (bodyOf[si] >= 0)
				continue;
			if (!map.TileAt(x, y).IsOcean())
				continue;

			const int bid = static_cast<int>(bodies.size());
			bodies.emplace_back();
			std::queue<std::pair<int, int>> q;
			q.push({ x, y });
			bodyOf[si] = bid;
			bodies.back().push_back({ x, y });

			while (!q.empty())
			{
				const auto [cx, cy] = q.front();
				q.pop();
				for (int d = 0; d < 4; ++d)
				{
					const int nx = wrapX(cx + kOx[d]);
					const int ny = cy + kOy[d];
					if (ny < 0 || ny >= height)
						continue;
					const size_t ni = Index(nx, ny);
					if (bodyOf[ni] >= 0)
						continue;
					if (!map.TileAt(nx, ny).IsOcean())
						continue;
					bodyOf[ni] = bid;
					bodies.back().push_back({ nx, ny });
					q.push({ nx, ny });
				}
			}
		}
	}

	if (bodies.empty())
		return;

	// Largest ocean body = world ocean (skip claiming).
	size_t largest = 0;
	for (size_t b = 1; b < bodies.size(); ++b)
		if (bodies[b].size() > bodies[largest].size())
			largest = b;

	for (size_t b = 0; b < bodies.size(); ++b)
	{
		if (b == largest)
			continue;
		if (bodies[b].empty())
			continue;

		// Collect unique land owners on the shoreline (4-adjacent).
		int soleOwner = -2; // -2 = unset, -1 = unowned land seen, >=0 = player
		bool mixed = false;
		for (const auto& [wx, wy] : bodies[b])
		{
			for (int d = 0; d < 4; ++d)
			{
				const int lx = wrapX(wx + kOx[d]);
				const int ly = wy + kOy[d];
				if (ly < 0 || ly >= height)
					continue;
				if (map.TileAt(lx, ly).IsOcean())
					continue;
				const int8_t lo = owners[Index(lx, ly)];
				if (lo < 0)
				{
					// Unowned land on shore → lake stays unowned.
					mixed = true;
					break;
				}
				if (soleOwner == -2)
					soleOwner = lo;
				else if (soleOwner != lo)
				{
					mixed = true;
					break;
				}
			}
			if (mixed)
				break;
		}

		if (mixed || soleOwner < 0)
			continue;

		// Claim every tile of this lake for soleOwner.
		for (const auto& [wx, wy] : bodies[b])
		{
			const size_t i = Index(wx, wy);
			owners[i] = static_cast<int8_t>(soleOwner);
			// cityIds stays -1 (no single city "owns" the water body)
		}
	}
}

void CivTerritoryMap::Compute(const CivMapData& map, const std::vector<CivCity*>& cities)
{
	std::vector<const CivCity*> ptrs;
	ptrs.reserve(cities.size());
	for (CivCity* c : cities)
		ptrs.push_back(c);
	Compute(map, ptrs);
}

void CivTerritoryMap::Compute(const CivMapData& map, CivGameSetup& setup)
{
	const std::vector<CivCity*> all = setup.AllCities();
	Compute(map, all);
}

bool CivTerritoryMap::IsBorderWith(int x, int y, int nx, int ny) const
{
	if (!Valid() || y < 0 || y >= height)
		return false;
	const int8_t o = OwnerAt(x, y);
	if (o < 0)
		return false;
	if (ny < 0 || ny >= height)
		return true; // edge of world counts as border for owned land
	const int8_t n = OwnerAt(nx, ny);
	return n != o;
}

void CivApplyTerritoryToMap(CivMapData& map, const CivTerritoryMap& territory)
{
	if (!territory.Valid() || map.tiles.empty())
		return;
	if (territory.width != map.width || territory.height != map.height)
		return;

	for (int y = 0; y < map.height; ++y)
	{
		for (int x = 0; x < map.width; ++x)
		{
			// Includes claimed inland lakes (ocean tiles with an owner).
			map.TileAt(x, y).owner = territory.OwnerAt(x, y);
		}
	}
}

// ---------------------------------------------------------------------------
// Fog of war
// ---------------------------------------------------------------------------

void CivEnsurePlayerFog(CivPlayer& player, int mapWidth, int mapHeight)
{
	if (mapWidth <= 0 || mapHeight <= 0)
		return;
	const size_t n = static_cast<size_t>(mapWidth * mapHeight);
	if (player.fogWidth != mapWidth || player.fogHeight != mapHeight
		|| player.explored.size() != n || player.visible.size() != n)
	{
		player.fogWidth = mapWidth;
		player.fogHeight = mapHeight;
		player.explored.assign(n, false);
		player.visible.assign(n, false);
	}
}

void CivClearCurrentVisibility(CivPlayer& player)
{
	std::fill(player.visible.begin(), player.visible.end(), false);
}

void CivAddVision(CivPlayer& player, int cx, int cy, int range,
	int mapWidth, int mapHeight)
{
	if (mapWidth <= 0 || mapHeight <= 0 || range < 0)
		return;
	CivEnsurePlayerFog(player, mapWidth, mapHeight);

	for (int dy = -range; dy <= range; ++dy)
	{
		const int y = cy + dy;
		if (y < 0 || y >= mapHeight)
			continue;
		for (int dx = -range; dx <= range; ++dx)
		{
			// Chebyshev square (CivOne Explore loop is full square, not diamond).
			int x = cx + dx;
			while (x < 0)
				x += mapWidth;
			x %= mapWidth;
			const size_t i = static_cast<size_t>(y * mapWidth + x);
			player.visible[i] = true;
			player.explored[i] = true;
		}
	}
}

void CivRebuildPlayerVisibility(CivPlayer& player, const CivMapData& map,
	int cityVisionRange)
{
	if (!player.IsAlive() || map.tiles.empty())
		return;

	CivEnsurePlayerFog(player, map.width, map.height);
	CivClearCurrentVisibility(player);

	for (const CivCity& c : player.cities)
	{
		if (!c.Valid())
			continue;
		CivAddVision(player, c.x, c.y, cityVisionRange, map.width, map.height);
	}

	// Unit vision will plug in here when unit instances exist:
	// for (each unit of player) CivAddVision(..., kCivUnitVisionRange, ...);
}

void CivRebuildAllVisibility(CivGameSetup& setup, const CivMapData& map,
	int cityVisionRange)
{
	for (CivPlayer& p : setup.players)
	{
		if (!p.active)
			continue;
		CivRebuildPlayerVisibility(p, map, cityVisionRange);
	}
}
