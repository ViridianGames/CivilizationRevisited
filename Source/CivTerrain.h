#ifndef _CIVTERRAIN_H_
#define _CIVTERRAIN_H_

#include <cstdint>

// Terrain enum ids used by map data and TER257 base tiles (column 0, row = id).
// Matches tools/slice_civ_terrain.py and Earth/MAP.PIC conversion.
namespace CivTerrain
{
	enum Id : uint8_t
	{
		Desert = 0,
		Plains = 1,
		Grassland = 2,
		Forest = 3,
		Hills = 4,
		Mountains = 5,
		Tundra = 6,
		Arctic = 7,
		Swamp = 8,
		Jungle = 9,
		Ocean = 10,
		River = 11,
		Count = 12
	};

	inline bool IsOcean(uint8_t t) { return t == Ocean; }
	inline bool IsLand(uint8_t t) { return t != Ocean; }
	inline bool IsRiverOrOcean(uint8_t t) { return t == Ocean || t == River; }
}

// Classic Civ world size (MAP.PIC / random generator default).
constexpr int kCivMapW = 80;
constexpr int kCivMapH = 50;

#endif
