#include "CivPlayer.h"

#include "CivTerrain.h"

#include <algorithm>
#include <cmath>

namespace
{
	bool IsBuildable(uint8_t t)
	{
		using namespace CivTerrain;
		return t == Plains || t == Grassland || t == River || t == Hills
			|| t == Forest || t == Jungle || t == Swamp || t == Desert;
	}

	bool IsPreferred(uint8_t t)
	{
		using namespace CivTerrain;
		return t == Plains || t == Grassland || t == River;
	}

	int Dist(int x0, int y0, int x1, int y1, int mapW)
	{
		int dx = std::abs(x0 - x1);
		if (dx > mapW / 2)
			dx = mapW - dx;
		const int dy = std::abs(y0 - y1);
		return dx + dy;
	}

	int ScoreTile(const CivMapData& map, int x, int y)
	{
		const uint8_t t = map.TerrainAt(x, y);
		if (t == CivTerrain::Ocean || t == CivTerrain::Arctic)
			return -1000;
		if (map.TileAt(x, y).hut)
			return -100;

		int score = 0;
		if (IsPreferred(t))
			score += 8;
		else if (IsBuildable(t))
			score += 3;
		else
			score -= 5;

		static const int dx[] = { 0, 1, 0, -1, 1, 1, -1, -1 };
		static const int dy[] = { -1, 0, 1, 0, -1, 1, 1, -1 };
		for (int i = 0; i < 8; ++i)
		{
			const int ny = y + dy[i];
			if (ny < 0 || ny >= map.height)
				continue;
			const uint8_t n = map.TerrainAt(x + dx[i], ny);
			if (IsPreferred(n))
				score += 2;
			else if (IsBuildable(n))
				score += 1;
			else if (n == CivTerrain::Ocean)
				score += 1;
		}
		return score;
	}
}

int CivPlaceStartingCities(CivMapData& map, CivGameSetup& setup, RNG& rng)
{
	// Place spaced starting *positions* only. Civs begin with settlers (no
	// free capital) — cities are founded in play via CivGame / AI.
	if (!map.valid || map.tiles.empty() || setup.players.empty())
		return 0;

	for (CivPlayer& p : setup.players)
	{
		p.cities.clear();
		p.startX = -1;
		p.startY = -1;
	}

	const int w = map.width;
	const int h = map.height;
	const int minDistStart = 10;
	int placed = 0;

	struct Pos { int x, y; };
	std::vector<Pos> placedPos;

	for (CivPlayer& player : setup.players)
	{
		if (!player.active)
			continue;

		int bestX = -1, bestY = -1, bestScore = -99999;

		for (int attempt = 0; attempt < 2500; ++attempt)
		{
			const int x = static_cast<int>(rng.Random(static_cast<unsigned>(w)));
			const int y = 2 + static_cast<int>(rng.Random(static_cast<unsigned>(std::max(1, h - 4))));
			const uint8_t t = map.TerrainAt(x, y);
			if (!IsBuildable(t))
				continue;
			if (map.TileAt(x, y).hut)
				continue;

			const int minDist = std::max(4, minDistStart - attempt / 200);
			bool tooClose = false;
			for (const Pos& c : placedPos)
			{
				if (Dist(x, y, c.x, c.y, w) < minDist)
				{
					tooClose = true;
					break;
				}
			}
			if (tooClose)
				continue;

			int score = ScoreTile(map, x, y);
			score += static_cast<int>(rng.Random(3));
			if (score > bestScore)
			{
				bestScore = score;
				bestX = x;
				bestY = y;
			}

			if (bestScore >= 20 && attempt > 200)
				break;
		}

		if (bestX < 0)
		{
			for (int y = 2; y < h - 2 && bestX < 0; ++y)
				for (int x = 0; x < w; ++x)
				{
					if (!IsBuildable(map.TerrainAt(x, y)))
						continue;
					bool tooClose = false;
					for (const Pos& c : placedPos)
					{
						if (Dist(x, y, c.x, c.y, w) < 4)
						{
							tooClose = true;
							break;
						}
					}
					if (!tooClose)
					{
						bestX = x;
						bestY = y;
						break;
					}
				}
		}

		if (bestX < 0)
			continue;

		// No city yet — just a spawn point for starting settlers.
		player.startX = bestX;
		player.startY = bestY;
		// Clear hut under spawn so settlers aren't blocked.
		map.TileAt(bestX, bestY).hut = false;

		placedPos.push_back({ bestX, bestY });
		++placed;
	}

	return placed;
}
