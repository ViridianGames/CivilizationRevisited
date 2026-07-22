#include "CivAdvances.h"
#include <algorithm>

static const char* const kBlds_CodeOfLaws[] = { "Courthouse" };
static const char* const kBlds_Currency[] = { "MarketPlace" };
static const char* const kWnds_Astronomy[] = { "Copernicus' Observatory" };
static const char* const kUnits_MapMaking[] = { "Trireme" };
static const char* const kWnds_MapMaking[] = { "Lighthouse" };
static const char* const kUnits_Navigation[] = { "Sail" };
static const char* const kWnds_Navigation[] = { "Magellan's Expedition" };
static const char* const kUnits_Mathematics[] = { "Catapult" };
static const char* const kWnds_Medicine[] = { "Shakespeare's Theatre" };
static const char* const kBlds_University[] = { "University" };
static const char* const kUnits_Magnetism[] = { "Frigate" };
static const char* const kBlds_Electronics[] = { "Hydro Plant" };
static const char* const kWnds_Electronics[] = { "Hoover Dam" };
static const char* const kBlds_Masonry[] = { "City Walls", "Palace" };
static const char* const kWnds_Masonry[] = { "Great Wall", "Pyramids" };
static const char* const kUnits_BronzeWorking[] = { "Phalanx" };
static const char* const kWnds_BronzeWorking[] = { "Colossus" };
static const char* const kUnits_IronWorking[] = { "Legion" };
static const char* const kWnds_Computers[] = { "SETI Program" };
static const char* const kUnits_Writing[] = { "Diplomat" };
static const char* const kBlds_Writing[] = { "Library" };
static const char* const kUnits_SteamEngine[] = { "Ironclad" };
static const char* const kUnits_Trade[] = { "Caravan" };
static const char* const kBlds_CeremonialBurial[] = { "Temple" };
static const char* const kWnds_Mysticism[] = { "Oracle" };
static const char* const kWnds_NuclearFission[] = { "Manhattan Project" };
static const char* const kBlds_Religion[] = { "Cathedral" };
static const char* const kWnds_Religion[] = { "J.S.Bach's Cathedral", "Michelangelo's Chapel" };
static const char* const kWnds_Literacy[] = { "Great Library" };
static const char* const kUnits_HorsebackRiding[] = { "Cavalry" };
static const char* const kUnits_TheWheel[] = { "Chariot" };
static const char* const kUnits_Gunpowder[] = { "Militia", "Musketeers" };
static const char* const kUnits_Industrialization[] = { "Transport" };
static const char* const kBlds_Industrialization[] = { "Factory" };
static const char* const kWnds_Industrialization[] = { "Women's Suffrage" };
static const char* const kUnits_Combustion[] = { "Cruiser" };
static const char* const kUnits_Flight[] = { "Fighter" };
static const char* const kUnits_AdvancedFlight[] = { "Bomber", "Carrier" };
static const char* const kBlds_SpaceFlight[] = { "SS Structural" };
static const char* const kWnds_SpaceFlight[] = { "Apollo Program" };
static const char* const kUnits_MassProduction[] = { "Submarine" };
static const char* const kBlds_MassProduction[] = { "Mass Transit" };
static const char* const kBlds_Pottery[] = { "Granary" };
static const char* const kWnds_Pottery[] = { "Hanging Gardens" };
static const char* const kWnds_Communism[] = { "United Nations" };
static const char* const kBlds_Construction[] = { "Aqueduct", "Colosseum" };
static const char* const kUnits_Rocketry[] = { "Nuclear" };
static const char* const kUnits_Metallurgy[] = { "Cannon" };
static const char* const kWnds_RailRoad[] = { "Darwin's Voyage" };
static const char* const kBlds_NuclearPower[] = { "Nuclear Plant" };
static const char* const kWnds_TheoryOfGravity[] = { "Isaac Newton's College" };
static const char* const kUnits_Steel[] = { "Battleship" };
static const char* const kBlds_Banking[] = { "Bank" };
static const char* const kBlds_Refining[] = { "Power Plant" };
static const char* const kBlds_SuperConductor[] = { "SDI Defense" };
static const char* const kUnits_Automobile[] = { "Armor" };
static const char* const kWnds_GeneticEngineering[] = { "Cure for Cancer" };
static const char* const kBlds_Plastics[] = { "SS Component" };
static const char* const kBlds_Recycling[] = { "Recycling Cntr." };
static const char* const kUnits_Chivalry[] = { "Knights" };
static const char* const kUnits_Robotics[] = { "Artillery" };
static const char* const kBlds_Robotics[] = { "Mfg. Plant", "SS Module" };
static const char* const kUnits_Conscription[] = { "Riflemen" };
static const char* const kUnits_LaborUnion[] = { "Mech. Inf." };

static const CivAdvanceDef kAdvances[static_cast<int>(CivAdvanceId::Count)] = {
	{ CivAdvanceId::Alphabet, "Alphabet", -1, -1, 3, 2, 1, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::CodeOfLaws, "Code of Laws", static_cast<int8_t>(CivAdvanceId::Alphabet), -1, 2, 0, 1, nullptr, 0, kBlds_CodeOfLaws, 1, nullptr, 0 },
	{ CivAdvanceId::Currency, "Currency", static_cast<int8_t>(CivAdvanceId::BronzeWorking), -1, 5, 0, 0, nullptr, 0, kBlds_Currency, 1, nullptr, 0 },
	{ CivAdvanceId::AtomicTheory, "Atomic Theory", static_cast<int8_t>(CivAdvanceId::TheoryOfGravity), static_cast<int8_t>(CivAdvanceId::Physics), 5, 2, 1, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Democracy, "Democracy", static_cast<int8_t>(CivAdvanceId::Philosophy), static_cast<int8_t>(CivAdvanceId::Literacy), 2, 2, 1, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Monarchy, "Monarchy", static_cast<int8_t>(CivAdvanceId::CeremonialBurial), static_cast<int8_t>(CivAdvanceId::CodeOfLaws), 2, 2, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Astronomy, "Astronomy", static_cast<int8_t>(CivAdvanceId::Mysticism), static_cast<int8_t>(CivAdvanceId::Mathematics), 6, 0, 0, nullptr, 0, nullptr, 0, kWnds_Astronomy, 1 },
	{ CivAdvanceId::MapMaking, "MapMaking", static_cast<int8_t>(CivAdvanceId::Alphabet), -1, 7, 1, 2, kUnits_MapMaking, 1, nullptr, 0, kWnds_MapMaking, 1 },
	{ CivAdvanceId::Navigation, "Navigation", static_cast<int8_t>(CivAdvanceId::MapMaking), static_cast<int8_t>(CivAdvanceId::Astronomy), 6, 2, 2, kUnits_Navigation, 1, nullptr, 0, kWnds_Navigation, 1 },
	{ CivAdvanceId::Mathematics, "Mathematics", static_cast<int8_t>(CivAdvanceId::Alphabet), static_cast<int8_t>(CivAdvanceId::Masonry), 7, 1, 1, kUnits_Mathematics, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Medicine, "Medicine", static_cast<int8_t>(CivAdvanceId::Philosophy), static_cast<int8_t>(CivAdvanceId::Trade), 3, 1, 2, nullptr, 0, nullptr, 0, kWnds_Medicine, 1 },
	{ CivAdvanceId::Physics, "Physics", static_cast<int8_t>(CivAdvanceId::Mathematics), static_cast<int8_t>(CivAdvanceId::Navigation), 1, 2, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Engineering, "Engineering", static_cast<int8_t>(CivAdvanceId::TheWheel), static_cast<int8_t>(CivAdvanceId::Construction), 4, 1, 1, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::University, "University", static_cast<int8_t>(CivAdvanceId::Mathematics), static_cast<int8_t>(CivAdvanceId::Philosophy), 1, 0, 2, nullptr, 0, kBlds_University, 1, nullptr, 0 },
	{ CivAdvanceId::Magnetism, "Magnetism", static_cast<int8_t>(CivAdvanceId::Navigation), static_cast<int8_t>(CivAdvanceId::Physics), 6, 0, 1, kUnits_Magnetism, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Electronics, "Electronics", static_cast<int8_t>(CivAdvanceId::Electricity), -1, 4, 2, 1, nullptr, 0, kBlds_Electronics, 1, kWnds_Electronics, 1 },
	{ CivAdvanceId::Masonry, "Masonry", -1, -1, 2, 1, 2, nullptr, 0, kBlds_Masonry, 2, kWnds_Masonry, 2 },
	{ CivAdvanceId::BronzeWorking, "Bronze Working", -1, -1, 5, 2, 0, kUnits_BronzeWorking, 1, nullptr, 0, kWnds_BronzeWorking, 1 },
	{ CivAdvanceId::IronWorking, "Iron Working", static_cast<int8_t>(CivAdvanceId::BronzeWorking), -1, 5, 1, 1, kUnits_IronWorking, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::BridgeBuilding, "Bridge Building", static_cast<int8_t>(CivAdvanceId::IronWorking), static_cast<int8_t>(CivAdvanceId::Construction), 4, 1, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Invention, "Invention", static_cast<int8_t>(CivAdvanceId::Engineering), static_cast<int8_t>(CivAdvanceId::Literacy), 6, 2, 1, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Computers, "Computers", static_cast<int8_t>(CivAdvanceId::Mathematics), static_cast<int8_t>(CivAdvanceId::Electronics), 3, 0, 2, nullptr, 0, nullptr, 0, kWnds_Computers, 1 },
	{ CivAdvanceId::Writing, "Writing", static_cast<int8_t>(CivAdvanceId::Alphabet), -1, 3, 1, 1, kUnits_Writing, 1, kBlds_Writing, 1, nullptr, 0 },
	{ CivAdvanceId::SteamEngine, "Steam Engine", static_cast<int8_t>(CivAdvanceId::Physics), static_cast<int8_t>(CivAdvanceId::Invention), 6, 1, 2, kUnits_SteamEngine, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Trade, "Trade", static_cast<int8_t>(CivAdvanceId::Currency), static_cast<int8_t>(CivAdvanceId::CodeOfLaws), 1, 0, 0, kUnits_Trade, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::CeremonialBurial, "Ceremonial Burial", -1, -1, 8, 2, 0, nullptr, 0, kBlds_CeremonialBurial, 1, nullptr, 0 },
	{ CivAdvanceId::Mysticism, "Mysticism", static_cast<int8_t>(CivAdvanceId::CeremonialBurial), -1, 1, 1, 1, nullptr, 0, nullptr, 0, kWnds_Mysticism, 1 },
	{ CivAdvanceId::NuclearFission, "Nuclear Fission", static_cast<int8_t>(CivAdvanceId::MassProduction), static_cast<int8_t>(CivAdvanceId::AtomicTheory), 5, 0, 1, nullptr, 0, nullptr, 0, kWnds_NuclearFission, 1 },
	{ CivAdvanceId::Philosophy, "Philosophy", static_cast<int8_t>(CivAdvanceId::Mysticism), static_cast<int8_t>(CivAdvanceId::Literacy), 8, 1, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Religion, "Religion", static_cast<int8_t>(CivAdvanceId::Philosophy), static_cast<int8_t>(CivAdvanceId::Writing), 3, 2, 0, nullptr, 0, kBlds_Religion, 1, kWnds_Religion, 2 },
	{ CivAdvanceId::Literacy, "Literacy", static_cast<int8_t>(CivAdvanceId::Writing), static_cast<int8_t>(CivAdvanceId::CodeOfLaws), 4, 1, 2, nullptr, 0, nullptr, 0, kWnds_Literacy, 1 },
	{ CivAdvanceId::HorsebackRiding, "Horseback Riding", -1, -1, 7, 0, 1, kUnits_HorsebackRiding, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Feudalism, "Feudalism", static_cast<int8_t>(CivAdvanceId::Masonry), static_cast<int8_t>(CivAdvanceId::Monarchy), 1, 1, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::TheWheel, "The Wheel", -1, -1, 3, 2, 2, kUnits_TheWheel, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Gunpowder, "Gunpowder", static_cast<int8_t>(CivAdvanceId::Invention), static_cast<int8_t>(CivAdvanceId::IronWorking), 6, 1, 0, kUnits_Gunpowder, 2, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Industrialization, "Industrialization", static_cast<int8_t>(CivAdvanceId::RailRoad), static_cast<int8_t>(CivAdvanceId::Banking), 2, 0, 2, kUnits_Industrialization, 1, kBlds_Industrialization, 1, kWnds_Industrialization, 1 },
	{ CivAdvanceId::Chemistry, "Chemistry", static_cast<int8_t>(CivAdvanceId::University), static_cast<int8_t>(CivAdvanceId::Medicine), 1, 2, 1, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Combustion, "Combustion", static_cast<int8_t>(CivAdvanceId::Refining), static_cast<int8_t>(CivAdvanceId::Explosives), 4, 2, 0, kUnits_Combustion, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Flight, "Flight", static_cast<int8_t>(CivAdvanceId::Combustion), static_cast<int8_t>(CivAdvanceId::Physics), 3, 1, 0, kUnits_Flight, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::AdvancedFlight, "Advanced Flight", static_cast<int8_t>(CivAdvanceId::Flight), static_cast<int8_t>(CivAdvanceId::Electricity), 2, 1, 0, kUnits_AdvancedFlight, 2, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::SpaceFlight, "Space Flight", static_cast<int8_t>(CivAdvanceId::Computers), static_cast<int8_t>(CivAdvanceId::Rocketry), 4, 2, 2, nullptr, 0, kBlds_SpaceFlight, 1, kWnds_SpaceFlight, 1 },
	{ CivAdvanceId::MassProduction, "Mass Production", static_cast<int8_t>(CivAdvanceId::Automobile), static_cast<int8_t>(CivAdvanceId::TheCorporation), 5, 1, 0, kUnits_MassProduction, 1, kBlds_MassProduction, 1, nullptr, 0 },
	{ CivAdvanceId::Pottery, "Pottery", -1, -1, 7, 0, 2, nullptr, 0, kBlds_Pottery, 1, kWnds_Pottery, 1 },
	{ CivAdvanceId::Communism, "Communism", static_cast<int8_t>(CivAdvanceId::Philosophy), static_cast<int8_t>(CivAdvanceId::Industrialization), 4, 0, 2, nullptr, 0, nullptr, 0, kWnds_Communism, 1 },
	{ CivAdvanceId::TheRepublic, "The Republic", static_cast<int8_t>(CivAdvanceId::CodeOfLaws), static_cast<int8_t>(CivAdvanceId::Literacy), 2, 0, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Construction, "Construction", static_cast<int8_t>(CivAdvanceId::Masonry), static_cast<int8_t>(CivAdvanceId::Currency), 4, 0, 1, nullptr, 0, kBlds_Construction, 2, nullptr, 0 },
	{ CivAdvanceId::Rocketry, "Rocketry", static_cast<int8_t>(CivAdvanceId::AdvancedFlight), static_cast<int8_t>(CivAdvanceId::Electronics), 8, 0, 1, kUnits_Rocketry, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::TheCorporation, "The Corporation", static_cast<int8_t>(CivAdvanceId::Banking), static_cast<int8_t>(CivAdvanceId::Industrialization), 7, 2, 2, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Metallurgy, "Metallurgy", static_cast<int8_t>(CivAdvanceId::Gunpowder), static_cast<int8_t>(CivAdvanceId::University), 7, 2, 1, kUnits_Metallurgy, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::RailRoad, "RailRoad", static_cast<int8_t>(CivAdvanceId::SteamEngine), static_cast<int8_t>(CivAdvanceId::BridgeBuilding), 3, 0, 1, nullptr, 0, nullptr, 0, kWnds_RailRoad, 1 },
	{ CivAdvanceId::NuclearPower, "Nuclear Power", static_cast<int8_t>(CivAdvanceId::NuclearFission), static_cast<int8_t>(CivAdvanceId::Electronics), 2, 2, 2, nullptr, 0, kBlds_NuclearPower, 1, nullptr, 0 },
	{ CivAdvanceId::TheoryOfGravity, "Theory of Gravity", static_cast<int8_t>(CivAdvanceId::Astronomy), static_cast<int8_t>(CivAdvanceId::University), 2, 1, 1, nullptr, 0, nullptr, 0, kWnds_TheoryOfGravity, 1 },
	{ CivAdvanceId::Steel, "Steel", static_cast<int8_t>(CivAdvanceId::Metallurgy), static_cast<int8_t>(CivAdvanceId::Industrialization), 3, 0, 0, kUnits_Steel, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Banking, "Banking", static_cast<int8_t>(CivAdvanceId::Trade), static_cast<int8_t>(CivAdvanceId::TheRepublic), 1, 2, 2, nullptr, 0, kBlds_Banking, 1, nullptr, 0 },
	{ CivAdvanceId::Electricity, "Electricity", static_cast<int8_t>(CivAdvanceId::Magnetism), static_cast<int8_t>(CivAdvanceId::Metallurgy), 8, 0, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Refining, "Refining", static_cast<int8_t>(CivAdvanceId::Chemistry), static_cast<int8_t>(CivAdvanceId::TheCorporation), 6, 2, 0, nullptr, 0, kBlds_Refining, 1, nullptr, 0 },
	{ CivAdvanceId::Explosives, "Explosives", static_cast<int8_t>(CivAdvanceId::Gunpowder), static_cast<int8_t>(CivAdvanceId::Chemistry), 5, 1, 2, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::SuperConductor, "SuperConductor", static_cast<int8_t>(CivAdvanceId::Plastics), static_cast<int8_t>(CivAdvanceId::MassProduction), 5, 2, 2, nullptr, 0, kBlds_SuperConductor, 1, nullptr, 0 },
	{ CivAdvanceId::Automobile, "Automobile", static_cast<int8_t>(CivAdvanceId::Combustion), static_cast<int8_t>(CivAdvanceId::Steel), 6, 0, 2, kUnits_Automobile, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::GeneticEngineering, "Genetic Engineering", static_cast<int8_t>(CivAdvanceId::Medicine), static_cast<int8_t>(CivAdvanceId::TheCorporation), 1, 1, 2, nullptr, 0, nullptr, 0, kWnds_GeneticEngineering, 1 },
	{ CivAdvanceId::Plastics, "Plastics", static_cast<int8_t>(CivAdvanceId::Refining), static_cast<int8_t>(CivAdvanceId::SpaceFlight), 4, 0, 0, nullptr, 0, kBlds_Plastics, 1, nullptr, 0 },
	{ CivAdvanceId::Recycling, "Recycling", static_cast<int8_t>(CivAdvanceId::MassProduction), static_cast<int8_t>(CivAdvanceId::Democracy), 5, 0, 2, nullptr, 0, kBlds_Recycling, 1, nullptr, 0 },
	{ CivAdvanceId::Chivalry, "Chivalry", static_cast<int8_t>(CivAdvanceId::Feudalism), static_cast<int8_t>(CivAdvanceId::HorsebackRiding), 6, 1, 1, kUnits_Chivalry, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Robotics, "Robotics", static_cast<int8_t>(CivAdvanceId::Plastics), static_cast<int8_t>(CivAdvanceId::Computers), 7, 2, 0, kUnits_Robotics, 1, kBlds_Robotics, 2, nullptr, 0 },
	{ CivAdvanceId::Conscription, "Conscription", static_cast<int8_t>(CivAdvanceId::TheRepublic), static_cast<int8_t>(CivAdvanceId::Explosives), 7, 0, 0, kUnits_Conscription, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::LaborUnion, "Labor Union", static_cast<int8_t>(CivAdvanceId::MassProduction), static_cast<int8_t>(CivAdvanceId::Communism), 1, 0, 1, kUnits_LaborUnion, 1, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::FusionPower, "Fusion Power", static_cast<int8_t>(CivAdvanceId::NuclearPower), static_cast<int8_t>(CivAdvanceId::SuperConductor), 7, 1, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
	// Starting infrastructure (always known).
	{ CivAdvanceId::Roads, "Roads", -1, -1, 0, 0, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Irrigation, "Irrigation", -1, -1, 0, 0, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
	{ CivAdvanceId::Mining, "Mining", -1, -1, 0, 0, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
};

#include "Geist/RNG.h"

const CivAdvanceDef& CivAdvance(int id)
{
	static const CivAdvanceDef kNone{
		CivAdvanceId::None, "None", -1, -1, 0, 0, 0,
		nullptr, 0, nullptr, 0, nullptr, 0
	};
	if (id < 0 || id >= static_cast<int>(CivAdvanceId::Count))
		return kNone;
	return kAdvances[id];
}

const CivAdvanceDef& CivAdvance(CivAdvanceId id)
{
	return CivAdvance(static_cast<int>(id));
}

const char* CivAdvanceName(int id)
{
	return CivAdvance(id).name;
}

int CivAdvanceCount()
{
	return static_cast<int>(CivAdvanceId::Count);
}

bool CivAdvancePrereqsMet(int advanceId, const std::vector<int>& knownAdvances)
{
	const CivAdvanceDef& a = CivAdvance(advanceId);
	if (a.id == CivAdvanceId::None)
		return false;
	auto has = [&](int need) {
		if (need < 0) return true;
		return std::find(knownAdvances.begin(), knownAdvances.end(), need) != knownAdvances.end();
	};
	return has(a.requires0) && has(a.requires1);
}

std::vector<int> CivAdvancesAvailable(const std::vector<int>& knownAdvances)
{
	std::vector<int> out;
	const int n = CivAdvanceCount();
	for (int i = 0; i < n; ++i)
	{
		if (std::find(knownAdvances.begin(), knownAdvances.end(), i) != knownAdvances.end())
			continue;
		if (CivAdvancePrereqsMet(i, knownAdvances))
			out.push_back(i);
	}
	return out;
}

bool CivAdvanceIsFree(int advanceId)
{
	const CivAdvanceDef& a = CivAdvance(advanceId);
	return a.id != CivAdvanceId::None && a.requires0 < 0 && a.requires1 < 0;
}

namespace
{
	void GrantIfMissing(std::vector<int>& known, int id)
	{
		if (std::find(known.begin(), known.end(), id) == known.end())
			known.push_back(id);
	}
}

int CivGrantStartingAdvances(std::vector<int>& knownAdvances, RNG& rng)
{
	// Always: settler infrastructure.
	GrantIfMissing(knownAdvances, static_cast<int>(CivAdvanceId::Roads));
	GrantIfMissing(knownAdvances, static_cast<int>(CivAdvanceId::Irrigation));
	GrantIfMissing(knownAdvances, static_cast<int>(CivAdvanceId::Mining));

	// Optional free techs (no prereqs), excluding the three guaranteed above.
	// Small independent chance per tech so a civ might get 0, 1, or rarely more.
	static const int kOptionalFree[] = {
		static_cast<int>(CivAdvanceId::Alphabet),
		static_cast<int>(CivAdvanceId::Pottery),
		static_cast<int>(CivAdvanceId::CeremonialBurial),
		static_cast<int>(CivAdvanceId::Masonry),
		static_cast<int>(CivAdvanceId::BronzeWorking),
		static_cast<int>(CivAdvanceId::HorsebackRiding),
		static_cast<int>(CivAdvanceId::TheWheel),
	};
	// ~18% each → expected ~1.25 extras across 7 options; often 0–2.
	constexpr unsigned kBonusChancePercent = 18;

	for (int id : kOptionalFree)
	{
		if (rng.Random(100) < kBonusChancePercent)
			GrantIfMissing(knownAdvances, id);
	}

	std::sort(knownAdvances.begin(), knownAdvances.end());
	return static_cast<int>(knownAdvances.size());
}

int CivScienceCost(int difficulty, int knownAdvanceCount, bool yearIsAD)
{
	// CivOne Player.ScienceCost:
	//   (Difficulty + 3) * 2 * (Advances.Count + 1) * (year > 0 AD ? 2 : 1), min 12
	if (difficulty < 0)
		difficulty = 0;
	if (knownAdvanceCount < 0)
		knownAdvanceCount = 0;
	int cost = (difficulty + 3) * 2 * (knownAdvanceCount + 1);
	if (yearIsAD)
		cost *= 2;
	if (cost < 12)
		cost = 12;
	return cost;
}
