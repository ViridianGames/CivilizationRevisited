#ifndef _CIVBUILDINGS_H_
#define _CIVBUILDINGS_H_

#include "CivAdvances.h"
#include <cstdint>
#include <vector>

enum class CivBuildingId : int8_t
{
	None = -1,
	Palace = 0,
	Barracks = 1,
	Granary = 2,
	Temple = 3,
	MarketPlace = 4,
	Library = 5,
	Courthouse = 6,
	CityWalls = 7,
	Aqueduct = 8,
	Bank = 9,
	Cathedral = 10,
	University = 11,
	MassTransit = 12,
	Colosseum = 13,
	Factory = 14,
	MfgPlant = 15,
	SdiDefense = 16,
	RecyclingCenter = 17,
	PowerPlant = 18,
	HydroPlant = 19,
	NuclearPlant = 20,
	SSStructural = 21,
	SSComponent = 22,
	SSModule = 23,
	Count = 24
};

struct CivBuildingDef
{
	CivBuildingId id;
	const char* name;
	uint8_t price;        // cost factor; shields = price * 10
	uint8_t maintenance;  // gold per turn
	int8_t requiredTech;  // CivAdvanceId or -1
};

const CivBuildingDef& CivBuilding(int id);
const CivBuildingDef& CivBuilding(CivBuildingId id);
const char* CivBuildingName(int id);
int CivBuildingCount();
int CivBuildingShieldCost(int id); // price * 10
int CivBuildingBuyCost(int id);    // price * 40
int CivBuildingSellValue(int id);  // price * 10

bool CivBuildingAvailable(int buildingId, const std::vector<int>& knownAdvances);

#endif
