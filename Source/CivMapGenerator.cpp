#include "CivMapGenerator.h"

#include <algorithm>
#include <sstream>

using namespace CivTerrain;

namespace
{
	int NextRange(RNG& rng, int minInclusive, int maxExclusive)
	{
		if (maxExclusive <= minInclusive)
			return minInclusive;
		return static_cast<int>(rng.Random(static_cast<unsigned int>(maxExclusive - minInclusive))) + minInclusive;
	}

	int NextN(RNG& rng, int n)
	{
		if (n <= 0)
			return 0;
		return static_cast<int>(rng.Random(static_cast<unsigned int>(n)));
	}
}

bool CivMapGenerator::NearOcean(const CivMapData& map, int x, int y)
{
	static const int dx[] = { 0, 1, 0, -1 };
	static const int dy[] = { -1, 0, 1, 0 };
	for (int i = 0; i < 4; ++i)
	{
		const int ny = y + dy[i];
		if (ny < 0 || ny >= map.height)
			continue;
		if (map.TerrainAt(x + dx[i], ny) == Ocean)
			return true;
	}
	return false;
}

std::vector<bool> CivMapGenerator::GenerateLandChunk(int width, int height, RNG& rng)
{
	std::vector<bool> stencil(static_cast<size_t>(width * height), false);

	int x = NextRange(rng, 4, width - 4);
	int y = NextRange(rng, 8, height - 8);
	const int pathLength = NextRange(rng, 1, 64);

	for (int i = 0; i < pathLength; ++i)
	{
		if (x >= 0 && x < width - 1 && y >= 0 && y < height - 1)
		{
			stencil[static_cast<size_t>(Idx(width, x, y))] = true;
			stencil[static_cast<size_t>(Idx(width, x + 1, y))] = true;
			stencil[static_cast<size_t>(Idx(width, x, y + 1))] = true;
		}
		switch (NextN(rng, 4))
		{
		case 0: y--; break;
		case 1: x++; break;
		case 2: y++; break;
		default: x--; break;
		}
		if (x < 3 || y < 3 || x > (width - 4) || y > (height - 5))
			break;
	}
	return stencil;
}

std::vector<int> CivMapGenerator::GenerateLandMass(int width, int height, int landMass, RNG& rng)
{
	std::vector<int> elevation(static_cast<size_t>(width * height), 0);
	const int landMassSize = static_cast<int>((static_cast<float>(width * height) / 12.5f)) * (landMass + 2);

	auto landCount = [&]() {
		int n = 0;
		for (int e : elevation)
			if (e > 0)
				++n;
		return n;
	};

	while (landCount() < landMassSize)
	{
		const std::vector<bool> chunk = GenerateLandChunk(width, height, rng);
		for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x)
			{
				if (chunk[static_cast<size_t>(Idx(width, x, y))])
					elevation[static_cast<size_t>(Idx(width, x, y))]++;
			}
	}

	for (int y = 0; y < height - 1; ++y)
		for (int x = 0; x < width - 1; ++x)
		{
			const int a = elevation[static_cast<size_t>(Idx(width, x, y))];
			const int b = elevation[static_cast<size_t>(Idx(width, x + 1, y + 1))];
			const int c = elevation[static_cast<size_t>(Idx(width, x + 1, y))];
			const int d = elevation[static_cast<size_t>(Idx(width, x, y + 1))];
			if ((a > 0 && b > 0) && (c == 0 && d == 0))
			{
				elevation[static_cast<size_t>(Idx(width, x + 1, y))]++;
				elevation[static_cast<size_t>(Idx(width, x, y + 1))]++;
			}
			else if ((a == 0 && b == 0) && (c > 0 && d > 0))
			{
				elevation[static_cast<size_t>(Idx(width, x + 1, y + 1))]++;
			}
		}

	return elevation;
}

std::vector<int> CivMapGenerator::TemperatureAdjustments(int width, int height, int temperature, RNG& rng)
{
	std::vector<int> latitude(static_cast<size_t>(width * height), 0);
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			int l = static_cast<int>((static_cast<float>(y) / static_cast<float>(height)) * 50.0f) - 29;
			l += NextN(rng, 7);
			if (l < 0)
				l = -l;
			l += 1 - temperature;
			l = (l / 6) + 1;

			int band;
			switch (l)
			{
			case 0:
			case 1: band = 0; break;
			case 2:
			case 3: band = 1; break;
			case 4:
			case 5: band = 2; break;
			default: band = 3; break;
			}
			latitude[static_cast<size_t>(Idx(width, x, y))] = band;
		}
	return latitude;
}

void CivMapGenerator::MergeElevationAndLatitude(CivMapData& map, const std::vector<int>& elevation,
	const std::vector<int>& latitude)
{
	const int w = map.width;
	const int h = map.height;
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x)
		{
			const int i = Idx(w, x, y);
			uint8_t t = Ocean;
			switch (elevation[static_cast<size_t>(i)])
			{
			case 0:
				t = Ocean;
				break;
			case 1:
				switch (latitude[static_cast<size_t>(i)])
				{
				case 0: t = Desert; break;
				case 1: t = Plains; break;
				case 2: t = Tundra; break;
				default: t = Arctic; break;
				}
				break;
			case 2:
				t = Hills;
				break;
			default:
				t = Mountains;
				break;
			}
			map.tiles[static_cast<size_t>(i)].terrain = t;
		}
}

void CivMapGenerator::ClimateAdjustments(CivMapData& map, int climate, RNG& rng)
{
	const int w = map.width;
	const int h = map.height;

	for (int y = 0; y < h; ++y)
	{
		const int yy = static_cast<int>((static_cast<float>(y) / static_cast<float>(h)) * 50.0f);
		int wetness = 0;
		int latitude = std::abs(25 - yy);

		for (int x = 0; x < w; ++x)
		{
			const uint8_t t = map.TerrainAt(x, y);
			if (t == Ocean)
			{
				int wy = latitude - 12;
				if (wy < 0)
					wy = -wy;
				wy += (climate * 4);
				if (wy > wetness)
					wetness++;
			}
			else if (wetness > 0)
			{
				const int rainfall = NextN(rng, 7 - (climate * 2));
				wetness -= rainfall;
				switch (t)
				{
				case Plains: map.SetTerrain(x, y, Grassland); break;
				case Tundra: map.SetTerrain(x, y, Arctic); break;
				case Hills: map.SetTerrain(x, y, Forest); break;
				case Desert: map.SetTerrain(x, y, Plains); break;
				case Mountains: wetness -= 3; break;
				default: break;
				}
			}
		}

		wetness = 0;
		latitude = std::abs(25 - yy);
		for (int x = w - 1; x >= 0; --x)
		{
			const uint8_t t = map.TerrainAt(x, y);
			if (t == Ocean)
			{
				const int wy = (latitude / 2) + climate;
				if (wy > wetness)
					wetness++;
			}
			else if (wetness > 0)
			{
				const int rainfall = NextN(rng, 7 - (climate * 2));
				wetness -= rainfall;
				switch (t)
				{
				case Swamp: map.SetTerrain(x, y, Forest); break;
				case Plains: map.SetTerrain(x, y, Grassland); break;
				case Grassland: map.SetTerrain(x, y, Jungle); break;
				case Hills: map.SetTerrain(x, y, Forest); break;
				case Mountains:
					map.SetTerrain(x, y, Forest);
					wetness -= 3;
					break;
				case Desert: map.SetTerrain(x, y, Plains); break;
				default: break;
				}
			}
		}
	}
}

void CivMapGenerator::AgeAdjustments(CivMapData& map, int age, RNG& rng)
{
	const int w = map.width;
	const int h = map.height;
	const int ageRepeat = static_cast<int>(
		(800.0f * static_cast<float>(1 + age) / static_cast<float>(80 * 50)) *
		static_cast<float>(w * h));

	int x = 0;
	int y = 0;
	for (int i = 0; i < ageRepeat; ++i)
	{
		if (i % 2 == 0)
		{
			x = NextN(rng, w);
			y = NextN(rng, h);
		}
		else
		{
			switch (NextN(rng, 8))
			{
			case 0: x--; y--; break;
			case 1: y--; break;
			case 2: x++; y--; break;
			case 3: x--; break;
			case 4: x++; break;
			case 5: x--; y++; break;
			case 6: y++; break;
			default: x++; y++; break;
			}
			if (x < 0) x = 1;
			if (y < 0) y = 1;
			if (x >= w) x = w - 2;
			if (y >= h) y = h - 2;
		}

		switch (map.TerrainAt(x, y))
		{
		case Forest: map.SetTerrain(x, y, Jungle); break;
		case Swamp: map.SetTerrain(x, y, Grassland); break;
		case Plains: map.SetTerrain(x, y, Hills); break;
		case Tundra: map.SetTerrain(x, y, Hills); break;
		case River: map.SetTerrain(x, y, Forest); break;
		case Grassland: map.SetTerrain(x, y, Forest); break;
		case Jungle: map.SetTerrain(x, y, Swamp); break;
		case Hills: map.SetTerrain(x, y, Mountains); break;
		case Mountains:
		{
			const auto diag = [&](int dx, int dy) -> uint8_t {
				const int ny = y + dy;
				if (ny < 0 || ny >= h)
					return Ocean;
				return map.TerrainAt(x + dx, ny);
			};
			if ((x == 0 || diag(-1, -1) != Ocean) &&
			    (y == 0 || diag(1, -1) != Ocean) &&
			    (x == (w - 1) || diag(1, 1) != Ocean) &&
			    (y == (h - 1) || diag(-1, 1) != Ocean))
			{
				map.SetTerrain(x, y, Ocean);
			}
			break;
		}
		case Desert: map.SetTerrain(x, y, Plains); break;
		case Arctic: map.SetTerrain(x, y, Mountains); break;
		default: break;
		}
	}
}

void CivMapGenerator::CreateRivers(CivMapData& map, int climate, int landMass, RNG& rng)
{
	const int w = map.width;
	const int h = map.height;
	const int targetRivers = ((climate + landMass) * 2) + 6;
	int rivers = 0;

	for (int attempt = 0; attempt < 256 && rivers < targetRivers; ++attempt)
	{
		const std::vector<CivTile> backup = map.tiles;

		int riverLength = 0;
		int varA = NextN(rng, 4) * 2;
		bool nearOcean = false;

		int sx = -1, sy = -1;
		for (int trySeed = 0; trySeed < 500; ++trySeed)
		{
			const int rx = NextN(rng, w);
			const int ry = NextN(rng, h);
			if (map.TerrainAt(rx, ry) == Hills)
			{
				sx = rx;
				sy = ry;
				break;
			}
		}
		if (sx < 0)
			continue;

		int tx = sx;
		int ty = sy;
		bool okPath = true;
		do
		{
			map.SetTerrain(tx, ty, River);
			const int varC = NextN(rng, 2);
			varA = (((varC - riverLength % 2) * 2 + varA) & 0x07);
			riverLength++;

			nearOcean = NearOcean(map, tx, ty);
			int nx = tx;
			int ny = ty;
			switch (varA)
			{
			case 0:
			case 1: ny = ty - 1; break;
			case 2:
			case 3: nx = tx + 1; break;
			case 4:
			case 5: ny = ty + 1; break;
			case 6:
			case 7: nx = tx - 1; break;
			}
			nx = map.WrapX(nx);
			if (ny < 0 || ny >= h)
			{
				okPath = false;
				break;
			}
			tx = nx;
			ty = ny;
			const uint8_t nt = map.TerrainAt(tx, ty);
			if (nt == Ocean || nt == River || nt == Mountains)
				break;
		} while (!nearOcean);

		const uint8_t endT = map.TerrainAt(tx, ty);
		if (okPath && (nearOcean || endT == River) && riverLength > 5)
		{
			rivers++;
			for (int oy = -3; oy <= 3; ++oy)
				for (int ox = -3; ox <= 3; ++ox)
				{
					const int yy = ty + oy;
					if (yy < 0 || yy >= h)
						continue;
					const int xx = map.WrapX(tx + ox);
					if (map.TerrainAt(xx, yy) == Forest)
						map.SetTerrain(xx, yy, Jungle);
				}
		}
		else
		{
			map.tiles = backup;
		}
	}
}

void CivMapGenerator::CreatePoles(CivMapData& map, RNG& rng)
{
	const int w = map.width;
	const int h = map.height;
	for (int x = 0; x < w; ++x)
	{
		map.SetTerrain(x, 0, Arctic);
		map.SetTerrain(x, h - 1, Arctic);
	}
	for (int i = 0; i < (w / 4); ++i)
	{
		for (const int y : { 0, 1, h - 2, h - 1 })
		{
			const int x = NextN(rng, w);
			map.SetTerrain(x, y, Tundra);
		}
	}
}

bool CivMapGenerator::Generate(CivMapData& out, const CivWorldOptions& options, RNG& rng)
{
	const int w = kCivMapW;
	const int h = kCivMapH;

	CivWorldOptions opt = options;
	opt.landMass = std::clamp(opt.landMass, 0, 2);
	opt.temperature = std::clamp(opt.temperature, 0, 2);
	opt.climate = std::clamp(opt.climate, 0, 2);
	opt.age = std::clamp(opt.age, 0, 2);

	out.Clear();
	out.Resize(w, h, Ocean);
	out.options = opt;
	out.seed = rng.GetOriginalSeed();
	out.valid = false;

	const int masterWord = NextN(rng, 16);

	const std::vector<int> elevation = GenerateLandMass(w, h, opt.landMass, rng);
	const std::vector<int> latitude = TemperatureAdjustments(w, h, opt.temperature, rng);
	MergeElevationAndLatitude(out, elevation, latitude);
	ClimateAdjustments(out, opt.climate, rng);
	AgeAdjustments(out, opt.age, rng);
	CreateRivers(out, opt.climate, opt.landMass, rng);
	CreatePoles(out, rng);

	// Special resources, huts, grassland2 pattern.
	out.ApplySpecialsAndHuts(masterWord);
	out.ComputeContinents();

	static const char* kLand[] = { "Small", "Normal", "Large" };
	static const char* kTemp[] = { "Cool", "Temperate", "Warm" };
	static const char* kClim[] = { "Arid", "Normal", "Wet" };
	static const char* kAge[] = { "3by", "4by", "5by" };
	std::ostringstream title;
	title << "Random Map  " << kLand[opt.landMass] << "/" << kTemp[opt.temperature]
	      << "/" << kClim[opt.climate] << "/" << kAge[opt.age];
	out.title = title.str();
	out.valid = true;
	return true;
}
