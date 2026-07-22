#ifndef _CIVMAPGENERATOR_H_
#define _CIVMAPGENERATOR_H_

#include "CivTile.h"
#include "Geist/RNG.h"

#include <vector>

// Port of CivOne Map.Generate (classic Civilization random map algorithm).
// Fills CivMapData.tiles with terrain; specials/huts applied at the end.
class CivMapGenerator
{
public:
	static bool Generate(CivMapData& out, const CivWorldOptions& options, RNG& rng);

private:
	static std::vector<bool> GenerateLandChunk(int width, int height, RNG& rng);
	static std::vector<int> GenerateLandMass(int width, int height, int landMass, RNG& rng);
	static std::vector<int> TemperatureAdjustments(int width, int height, int temperature, RNG& rng);
	static void MergeElevationAndLatitude(CivMapData& map, const std::vector<int>& elevation,
		const std::vector<int>& latitude);
	static void ClimateAdjustments(CivMapData& map, int climate, RNG& rng);
	static void AgeAdjustments(CivMapData& map, int age, RNG& rng);
	static void CreateRivers(CivMapData& map, int climate, int landMass, RNG& rng);
	static void CreatePoles(CivMapData& map, RNG& rng);

	static bool NearOcean(const CivMapData& map, int x, int y);
	static int Idx(int width, int x, int y) { return y * width + x; }
};

#endif
