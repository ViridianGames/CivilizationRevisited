#include "CivBuildings.h"
#include <algorithm>

static const CivBuildingDef kBuildings[] = {
	// Civilopedia lists Palace maintenance as 5, but the game charges $0
	// (CivOne Palace.SetFree()). Capitals must not drain gold at founding.
	{ CivBuildingId::Palace, "Palace", 20, 0, static_cast<int8_t>(CivAdvanceId::Masonry) },
	{ CivBuildingId::Barracks, "Barracks", 4, 0, -1 },
	{ CivBuildingId::Granary, "Granary", 6, 1, static_cast<int8_t>(CivAdvanceId::Pottery) },
	{ CivBuildingId::Temple, "Temple", 4, 1, static_cast<int8_t>(CivAdvanceId::CeremonialBurial) },
	{ CivBuildingId::MarketPlace, "MarketPlace", 8, 1, static_cast<int8_t>(CivAdvanceId::Currency) },
	{ CivBuildingId::Library, "Library", 8, 1, static_cast<int8_t>(CivAdvanceId::Writing) },
	{ CivBuildingId::Courthouse, "Courthouse", 8, 1, static_cast<int8_t>(CivAdvanceId::CodeOfLaws) },
	{ CivBuildingId::CityWalls, "City Walls", 12, 2, static_cast<int8_t>(CivAdvanceId::Masonry) },
	{ CivBuildingId::Aqueduct, "Aqueduct", 12, 2, static_cast<int8_t>(CivAdvanceId::Construction) },
	{ CivBuildingId::Bank, "Bank", 12, 3, static_cast<int8_t>(CivAdvanceId::Banking) },
	{ CivBuildingId::Cathedral, "Cathedral", 16, 3, static_cast<int8_t>(CivAdvanceId::Religion) },
	{ CivBuildingId::University, "University", 16, 3, static_cast<int8_t>(CivAdvanceId::University) },
	{ CivBuildingId::MassTransit, "Mass Transit", 16, 4, static_cast<int8_t>(CivAdvanceId::MassProduction) },
	{ CivBuildingId::Colosseum, "Colosseum", 10, 4, static_cast<int8_t>(CivAdvanceId::Construction) },
	{ CivBuildingId::Factory, "Factory", 20, 4, static_cast<int8_t>(CivAdvanceId::Industrialization) },
	{ CivBuildingId::MfgPlant, "Mfg. Plant", 32, 6, static_cast<int8_t>(CivAdvanceId::Robotics) },
	{ CivBuildingId::SdiDefense, "SDI Defense", 20, 4, static_cast<int8_t>(CivAdvanceId::SuperConductor) },
	{ CivBuildingId::RecyclingCenter, "Recycling Cntr.", 20, 2, static_cast<int8_t>(CivAdvanceId::Recycling) },
	{ CivBuildingId::PowerPlant, "Power Plant", 16, 4, static_cast<int8_t>(CivAdvanceId::Refining) },
	{ CivBuildingId::HydroPlant, "Hydro Plant", 24, 4, static_cast<int8_t>(CivAdvanceId::Electronics) },
	{ CivBuildingId::NuclearPlant, "Nuclear Plant", 16, 2, static_cast<int8_t>(CivAdvanceId::NuclearPower) },
	{ CivBuildingId::SSStructural, "SS Structural", 8, 0, static_cast<int8_t>(CivAdvanceId::SpaceFlight) },
	{ CivBuildingId::SSComponent, "SS Component", 16, 0, static_cast<int8_t>(CivAdvanceId::Plastics) },
	{ CivBuildingId::SSModule, "SS Module", 32, 0, static_cast<int8_t>(CivAdvanceId::Robotics) },
};

const CivBuildingDef& CivBuilding(int id)
{
	static const CivBuildingDef kNone{ CivBuildingId::None, "None", 0, 0, -1 };
	if (id < 0 || id >= 24) return kNone;
	return kBuildings[id];
}
const CivBuildingDef& CivBuilding(CivBuildingId id) { return CivBuilding(static_cast<int>(id)); }
const char* CivBuildingName(int id) { return CivBuilding(id).name; }
int CivBuildingCount() { return 24; }
int CivBuildingShieldCost(int id) { return CivBuilding(id).price * 10; }
int CivBuildingBuyCost(int id) { return CivBuilding(id).price * 40; }
int CivBuildingSellValue(int id) { return CivBuilding(id).price * 10; }

bool CivBuildingAvailable(int buildingId, const std::vector<int>& knownAdvances)
{
	const CivBuildingDef& b = CivBuilding(buildingId);
	if (b.id == CivBuildingId::None) return false;
	if (b.requiredTech < 0) return true;
	return std::find(knownAdvances.begin(), knownAdvances.end(), b.requiredTech) != knownAdvances.end();
}
