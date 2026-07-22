#ifndef _CIVTILE_H_
#define _CIVTILE_H_

#include "CivTerrain.h"

#include <cstdint>
#include <string>
#include <vector>

// Bonus resource drawn on a tile (SP257 specials / grassland shield).
// Not a separate terrain type — an attribute of a normal tile.
enum class CivResource : uint8_t
{
	None = 0,
	Oasis,       // Desert
	Horses,      // Plains
	Shield,      // Grassland (special or Grassland2 pattern)
	Game,        // Forest
	Coal,        // Hills
	Gold,        // Mountains
	TundraGame,  // Tundra
	Seals,       // Arctic
	Oil,         // Swamp
	Gems,        // Jungle
	Fish,        // Ocean
};

// Bit flags for tile improvements (roads, irrigation, etc.).
enum CivImprovement : uint8_t
{
	CivImp_None       = 0,
	CivImp_Road       = 1 << 0,
	CivImp_RailRoad   = 1 << 1,
	CivImp_Irrigation = 1 << 2,
	CivImp_Mine       = 1 << 3,
	CivImp_Fortress   = 1 << 4,
};

// Per-tile world state. Starts with terrain + specials; ownership and
// improvements grow here as the game gains systems.
struct CivTile
{
	uint8_t terrain = CivTerrain::Ocean;

	// True when the classic special-resource lattice hits this square
	// (coal, gems, fish, …). Grassland shields may also use grassland2.
	bool special = false;

	// Grassland production-shield pattern (CivOne Grassland2), independent
	// of the special lattice. Viewer treats special||grassland2 as shield.
	bool grassland2 = false;

	// Player ownership: -1 = unowned / neutral. 0..N = player index.
	int8_t owner = -1;

	// CivImprovement bitfield.
	uint8_t improvements = CivImp_None;

	// Goody hut present (barbarian village).
	bool hut = false;

	// Settler AI / city scoring value (0 if unused).
	uint8_t landValue = 0;

	// Land continent id (1..n). 0 = ocean / unset.
	// Continents = continuous land (incl. rivers), separated by open water.
	// Inland lakes do not split a continent (land connects around them).
	uint8_t continentId = 0;

	// Visibility fog (future): bit per player or simple explored flag.
	uint8_t visibility = 0;

	// --- helpers ---

	bool IsOcean() const { return terrain == CivTerrain::Ocean; }
	bool IsLand() const { return terrain != CivTerrain::Ocean; }
	bool IsRiver() const { return terrain == CivTerrain::River; }

	bool HasRoad() const { return (improvements & CivImp_Road) != 0; }
	bool HasRail() const { return (improvements & CivImp_RailRoad) != 0; }
	bool HasIrrigation() const { return (improvements & CivImp_Irrigation) != 0; }
	bool HasMine() const { return (improvements & CivImp_Mine) != 0; }
	bool HasFortress() const { return (improvements & CivImp_Fortress) != 0; }

	void SetImp(CivImprovement bit, bool on)
	{
		if (on)
			improvements = static_cast<uint8_t>(improvements | bit);
		else
			improvements = static_cast<uint8_t>(improvements & ~bit);
	}

	// Resource shown on this tile, if any (for rendering / game rules).
	CivResource Resource() const
	{
		// Rivers never carry a special resource graphic.
		if (terrain == CivTerrain::River)
			return CivResource::None;

		// Grassland shield: Grassland2 pattern or special lattice hit.
		if (terrain == CivTerrain::Grassland)
		{
			if (grassland2 || special)
				return CivResource::Shield;
			return CivResource::None;
		}

		if (!special)
			return CivResource::None;

		switch (terrain)
		{
		case CivTerrain::Desert:    return CivResource::Oasis;
		case CivTerrain::Plains:    return CivResource::Horses;
		case CivTerrain::Forest:    return CivResource::Game;
		case CivTerrain::Hills:     return CivResource::Coal;
		case CivTerrain::Mountains: return CivResource::Gold;
		case CivTerrain::Tundra:    return CivResource::TundraGame;
		case CivTerrain::Arctic:    return CivResource::Seals;
		case CivTerrain::Swamp:     return CivResource::Oil;
		case CivTerrain::Jungle:    return CivResource::Gems;
		case CivTerrain::Ocean:     return CivResource::Fish;
		default:                    return CivResource::None;
		}
	}

	bool HasResource() const { return Resource() != CivResource::None; }

	static const char* ResourceName(CivResource r)
	{
		switch (r)
		{
		case CivResource::Oasis:      return "Oasis";
		case CivResource::Horses:     return "Horses";
		case CivResource::Shield:     return "Shield";
		case CivResource::Game:       return "Game";
		case CivResource::Coal:       return "Coal";
		case CivResource::Gold:       return "Gold";
		case CivResource::TundraGame: return "Game";
		case CivResource::Seals:      return "Seals";
		case CivResource::Oil:        return "Oil";
		case CivResource::Gems:       return "Gems";
		case CivResource::Fish:       return "Fish";
		default:                      return "None";
		}
	}
};

// Classic Civ world options (0 / 1 / 2).
struct CivWorldOptions
{
	int landMass = 1;     // 0 Small, 1 Normal, 2 Large
	int temperature = 1;  // 0 Cool, 1 Temperate, 2 Warm
	int climate = 1;      // 0 Arid, 1 Normal, 2 Wet
	int age = 1;          // 0 3by, 1 4by, 2 5by
};

// Full map: dense array of CivTile plus metadata.
struct CivMapData
{
	int width = kCivMapW;
	int height = kCivMapH;
	std::vector<CivTile> tiles;
	CivWorldOptions options;
	std::string title;
	unsigned int seed = 0;
	// Seed word used for special / hut lattice (0..15 typical for specials).
	int masterWord = 0;
	bool valid = false;

	void Clear()
	{
		tiles.clear();
		title.clear();
		seed = 0;
		masterWord = 0;
		valid = false;
	}

	void Resize(int w, int h, uint8_t fillTerrain = CivTerrain::Ocean)
	{
		width = w;
		height = h;
		tiles.assign(static_cast<size_t>(w * h), CivTile{});
		for (CivTile& t : tiles)
			t.terrain = fillTerrain;
	}

	bool InBounds(int /*x*/, int y) const
	{
		// X wraps cylindrically; only Y is hard-bounded.
		return y >= 0 && y < height && width > 0;
	}

	// Horizontal wrap (cylindrical world); Y must be in range.
	int WrapX(int x) const
	{
		if (width <= 0)
			return 0;
		while (x < 0)
			x += width;
		return x % width;
	}

	size_t Index(int x, int y) const
	{
		return static_cast<size_t>(y * width + WrapX(x));
	}

	const CivTile& TileAt(int x, int y) const
	{
		static const CivTile kOceanSentinel = [] {
			CivTile t;
			t.terrain = CivTerrain::Ocean;
			return t;
		}();
		if (!InBounds(x, y) || tiles.empty())
			return kOceanSentinel;
		return tiles[Index(x, y)];
	}

	CivTile& TileAt(int x, int y)
	{
		// Non-const: caller must ensure in-bounds Y.
		return tiles[Index(x, y)];
	}

	uint8_t TerrainAt(int x, int y) const { return TileAt(x, y).terrain; }

	void SetTerrain(int x, int y, uint8_t t)
	{
		if (!InBounds(x, y) || tiles.empty())
			return;
		tiles[Index(x, y)].terrain = t;
	}

	// Apply special-resource + hut lattice and grassland2 flags after terrain exists.
	void ApplySpecialsAndHuts(int masterWordIn);

	// Assign continentId on land tiles (4-connected non-ocean). Call after terrain is final.
	// Returns number of land continents found.
	int ComputeContinents();

	// Load plain terrain bytes (Earth MAP export) then apply specials + continents.
	bool LoadTerrainBytes(const uint8_t* data, size_t len, int w, int h, int masterWordIn = 0);
};

#endif
