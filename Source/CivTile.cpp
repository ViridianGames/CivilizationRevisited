#include "CivTile.h"

#include <queue>
#include <utility>

namespace
{
	int ModGrid(int x, int y) { return (x % 4) * 4 + (y % 4); }

	bool TileIsSpecial(int x, int y, int height, int masterWord)
	{
		if (y < 2 || y > (height - 3))
			return false;
		return ModGrid(x, y) == ((x / 4) * 13 + (y / 4) * 11 + masterWord) % 16;
	}

	bool TileHasHut(int x, int y, int height, int masterWord)
	{
		if (y < 2 || y > (height - 3))
			return false;
		return ModGrid(x, y) == ((x / 4) * 13 + (y / 4) * 11 + masterWord + 8) % 32;
	}

	// CivOne Grassland::CalculateTileType — Grassland2 when bit clear.
	bool IsGrassland2(int x, int y)
	{
		return (((x * 7) + (y * 11)) & 0x02) == 0;
	}
}

void CivMapData::ApplySpecialsAndHuts(int masterWordIn)
{
	masterWord = masterWordIn;
	if (tiles.empty() || width <= 0 || height <= 0)
		return;

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			CivTile& tile = tiles[Index(x, y)];
			tile.special = TileIsSpecial(x, y, height, masterWord);
			tile.grassland2 = (tile.terrain == CivTerrain::Grassland) && IsGrassland2(x, y);

			// Huts only on land (not ocean).
			if (tile.terrain != CivTerrain::Ocean)
				tile.hut = TileHasHut(x, y, height, masterWord);
			else
				tile.hut = false;
		}
	}
}

bool CivMapData::LoadTerrainBytes(const uint8_t* data, size_t len, int w, int h, int masterWordIn)
{
	if (!data || w <= 0 || h <= 0 || len < static_cast<size_t>(w * h))
		return false;

	Resize(w, h, CivTerrain::Ocean);
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			uint8_t t = data[static_cast<size_t>(y * w + x)];
			if (t >= CivTerrain::Count)
				t = CivTerrain::Ocean;
			tiles[Index(x, y)].terrain = t;
		}
	}
	ApplySpecialsAndHuts(masterWordIn);
	ComputeContinents();
	valid = true;
	return true;
}

int CivMapData::ComputeContinents()
{
	if (tiles.empty() || width <= 0 || height <= 0)
		return 0;

	for (CivTile& t : tiles)
		t.continentId = 0;

	int nextId = 0;
	static const int kOx[4] = { 0, 1, 0, -1 };
	static const int kOy[4] = { -1, 0, 1, 0 };

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			CivTile& start = tiles[Index(x, y)];
			if (start.IsOcean() || start.continentId != 0)
				continue;

			++nextId;
			if (nextId > 255)
				nextId = 255; // clamp; map is small enough this is fine

			std::queue<std::pair<int, int>> q;
			start.continentId = static_cast<uint8_t>(nextId);
			q.push({ x, y });

			while (!q.empty())
			{
				const auto [cx, cy] = q.front();
				q.pop();
				for (int d = 0; d < 4; ++d)
				{
					const int nx = WrapX(cx + kOx[d]);
					const int ny = cy + kOy[d];
					if (ny < 0 || ny >= height)
						continue;
					CivTile& nt = tiles[Index(nx, ny)];
					// Land only (rivers count as land). Ocean — including inland
					// lakes — does not connect continents; land routes around lakes.
					if (nt.IsOcean() || nt.continentId != 0)
						continue;
					nt.continentId = static_cast<uint8_t>(nextId);
					q.push({ nx, ny });
				}
			}
		}
	}
	return nextId;
}
