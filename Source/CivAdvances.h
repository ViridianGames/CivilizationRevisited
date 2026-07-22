#ifndef _CIVADVANCES_H_
#define _CIVADVANCES_H_

#include <cstdint>
#include <vector>

// Classic Civilization (Civ1) advances — ids match CivOne / original ordering.
enum class CivAdvanceId : int8_t
{
	None = -1,
	Alphabet = 0,
	CodeOfLaws = 1,
	Currency = 2,
	AtomicTheory = 3,
	Democracy = 4,
	Monarchy = 5,
	Astronomy = 6,
	MapMaking = 7,
	Navigation = 8,
	Mathematics = 9,
	Medicine = 10,
	Physics = 11,
	Engineering = 12,
	University = 13,
	Magnetism = 14,
	Electronics = 15,
	Masonry = 16,
	BronzeWorking = 17,
	IronWorking = 18,
	BridgeBuilding = 19,
	Invention = 20,
	Computers = 21,
	Writing = 22,
	SteamEngine = 23,
	Trade = 24,
	CeremonialBurial = 25,
	Mysticism = 26,
	NuclearFission = 27,
	Philosophy = 28,
	Religion = 29,
	Literacy = 30,
	HorsebackRiding = 31,
	Feudalism = 32,
	TheWheel = 33,
	Gunpowder = 34,
	Industrialization = 35,
	Chemistry = 36,
	Combustion = 37,
	Flight = 38,
	AdvancedFlight = 39,
	SpaceFlight = 40,
	MassProduction = 41,
	Pottery = 42,
	Communism = 43,
	TheRepublic = 44,
	Construction = 45,
	Rocketry = 46,
	TheCorporation = 47,
	Metallurgy = 48,
	RailRoad = 49,
	NuclearPower = 50,
	TheoryOfGravity = 51,
	Steel = 52,
	Banking = 53,
	Electricity = 54,
	Refining = 55,
	Explosives = 56,
	SuperConductor = 57,
	Automobile = 58,
	GeneticEngineering = 59,
	Plastics = 60,
	Recycling = 61,
	Chivalry = 62,
	Robotics = 63,
	Conscription = 64,
	LaborUnion = 65,
	FusionPower = 66,
	// Always known at game start (settler infrastructure; not in original Civ1 tech tree).
	Roads = 67,
	Irrigation = 68,
	Mining = 69,
	Count = 70
};

struct CivAdvanceDef
{
	CivAdvanceId id;
	const char* name;
	// Up to two prerequisite advance ids (-1 = unused).
	int8_t requires0;
	int8_t requires1;
	// Civilopedia icon placement in ICONPGx sheets (CivOne).
	uint8_t iconPage;
	uint8_t iconColumn;
	uint8_t iconRow;
	// What this advance enables (string names until unit/building enums exist).
	const char* const* units;
	int unitCount;
	const char* const* buildings;
	int buildingCount;
	const char* const* wonders;
	int wonderCount;
};

const CivAdvanceDef& CivAdvance(int id);
const CivAdvanceDef& CivAdvance(CivAdvanceId id);
const char* CivAdvanceName(int id);
int CivAdvanceCount();

// True if player has all prerequisites for this advance.
bool CivAdvancePrereqsMet(int advanceId, const std::vector<int>& knownAdvances);

// Advances that become available given known set (prereqs met, not already known).
std::vector<int> CivAdvancesAvailable(const std::vector<int>& knownAdvances);

// True if this advance has no prerequisites.
bool CivAdvanceIsFree(int advanceId);

// Grant starting techs: always Roads, Irrigation, Mining; then a small chance
// for each other free (no-prereq) advance (Pottery, Alphabet, etc.).
// Returns how many techs were granted (including the three guaranteed).
int CivGrantStartingAdvances(std::vector<int>& knownAdvances, class RNG& rng);

// Research cost in beakers for the *next* advance (same for every tech).
// Original Civ1 formula (CivOne / reverse engineering) — NOT a per-advance table:
//   cost = (difficulty + 3) * 2 * (knownAdvances + 1) * (yearAD ? 2 : 1)
//   minimum 12
// difficulty: 0=Chieftain … 5=Deity
int CivScienceCost(int difficulty, int knownAdvanceCount, bool yearIsAD);

#endif
