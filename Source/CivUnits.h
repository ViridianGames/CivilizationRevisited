#ifndef _CIVUNITS_H_
#define _CIVUNITS_H_

#include "CivAdvances.h"
#include <cstdint>
#include <vector>

enum class CivUnitDomain : uint8_t
{
	Land = 0,
	Water,
	Air
};

enum class CivUnitId : int8_t
{
	None = -1,
	Settlers = 0,
	Militia = 1,
	Phalanx = 2,
	Legion = 3,
	Musketeers = 4,
	Riflemen = 5,
	Cavalry = 6,
	Knights = 7,
	Catapult = 8,
	Cannon = 9,
	Chariot = 10,
	Armor = 11,
	MechInf = 12,
	Artillery = 13,
	Fighter = 14,
	Bomber = 15,
	Trireme = 16,
	Sail = 17,
	Frigate = 18,
	Ironclad = 19,
	Cruiser = 20,
	Battleship = 21,
	Submarine = 22,
	Carrier = 23,
	Transport = 24,
	Nuclear = 25,
	Diplomat = 26,
	Caravan = 27,
	Count = 28
};

struct CivUnitDef
{
	CivUnitId id;
	const char* name;
	CivUnitDomain domain;
	uint8_t price;      // production cost factor; shields = price * 10
	uint8_t attack;
	uint8_t defense;
	uint8_t move;
	uint8_t range;      // sea fuel/range extra (0 if N/A)
	int8_t requiredTech;  // CivAdvanceId or -1
	int8_t obsoleteTech;  // CivAdvanceId or -1
};

const CivUnitDef& CivUnit(int id);
const CivUnitDef& CivUnit(CivUnitId id);
const char* CivUnitName(int id);
int CivUnitCount();
int CivUnitShieldCost(int id); // price * 10

// Units available given known advances (required known, not obsolete).
bool CivUnitAvailable(int unitId, const std::vector<int>& knownAdvances);

#endif
