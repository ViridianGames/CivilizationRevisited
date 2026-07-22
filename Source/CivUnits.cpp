#include "CivUnits.h"
#include <algorithm>

static const CivUnitDef kUnits[] = {
	{ CivUnitId::Settlers, "Settlers", CivUnitDomain::Land, 4, 0, 1, 1, 0, -1, -1 },
	{ CivUnitId::Militia, "Militia", CivUnitDomain::Land, 1, 1, 1, 1, 0, -1, static_cast<int8_t>(CivAdvanceId::Gunpowder) },
	{ CivUnitId::Phalanx, "Phalanx", CivUnitDomain::Land, 2, 1, 2, 1, 0, static_cast<int8_t>(CivAdvanceId::BronzeWorking), static_cast<int8_t>(CivAdvanceId::Gunpowder) },
	{ CivUnitId::Legion, "Legion", CivUnitDomain::Land, 2, 3, 1, 1, 0, static_cast<int8_t>(CivAdvanceId::IronWorking), static_cast<int8_t>(CivAdvanceId::Conscription) },
	{ CivUnitId::Musketeers, "Musketeers", CivUnitDomain::Land, 3, 2, 3, 1, 0, static_cast<int8_t>(CivAdvanceId::Gunpowder), static_cast<int8_t>(CivAdvanceId::Conscription) },
	{ CivUnitId::Riflemen, "Riflemen", CivUnitDomain::Land, 3, 3, 5, 1, 0, static_cast<int8_t>(CivAdvanceId::Conscription), -1 },
	{ CivUnitId::Cavalry, "Cavalry", CivUnitDomain::Land, 2, 2, 1, 2, 0, static_cast<int8_t>(CivAdvanceId::HorsebackRiding), static_cast<int8_t>(CivAdvanceId::Conscription) },
	{ CivUnitId::Knights, "Knights", CivUnitDomain::Land, 4, 4, 2, 2, 0, static_cast<int8_t>(CivAdvanceId::Chivalry), static_cast<int8_t>(CivAdvanceId::Automobile) },
	{ CivUnitId::Catapult, "Catapult", CivUnitDomain::Land, 4, 6, 1, 1, 0, static_cast<int8_t>(CivAdvanceId::Mathematics), static_cast<int8_t>(CivAdvanceId::Metallurgy) },
	{ CivUnitId::Cannon, "Cannon", CivUnitDomain::Land, 4, 8, 1, 1, 0, static_cast<int8_t>(CivAdvanceId::Metallurgy), static_cast<int8_t>(CivAdvanceId::Robotics) },
	{ CivUnitId::Chariot, "Chariot", CivUnitDomain::Land, 4, 4, 1, 2, 0, static_cast<int8_t>(CivAdvanceId::TheWheel), static_cast<int8_t>(CivAdvanceId::Chivalry) },
	{ CivUnitId::Armor, "Armor", CivUnitDomain::Land, 8, 10, 5, 3, 0, static_cast<int8_t>(CivAdvanceId::Automobile), -1 },
	{ CivUnitId::MechInf, "Mech. Inf.", CivUnitDomain::Land, 5, 6, 6, 3, 0, static_cast<int8_t>(CivAdvanceId::LaborUnion), -1 },
	{ CivUnitId::Artillery, "Artillery", CivUnitDomain::Land, 6, 12, 2, 2, 0, static_cast<int8_t>(CivAdvanceId::Robotics), -1 },
	{ CivUnitId::Fighter, "Fighter", CivUnitDomain::Air, 6, 4, 2, 10, 0, static_cast<int8_t>(CivAdvanceId::Flight), -1 },
	{ CivUnitId::Bomber, "Bomber", CivUnitDomain::Air, 12, 12, 1, 8, 0, static_cast<int8_t>(CivAdvanceId::AdvancedFlight), -1 },
	{ CivUnitId::Trireme, "Trireme", CivUnitDomain::Water, 4, 1, 0, 3, 0, static_cast<int8_t>(CivAdvanceId::MapMaking), static_cast<int8_t>(CivAdvanceId::Navigation) },
	{ CivUnitId::Sail, "Sail", CivUnitDomain::Water, 4, 1, 1, 3, 0, static_cast<int8_t>(CivAdvanceId::Navigation), static_cast<int8_t>(CivAdvanceId::Magnetism) },
	{ CivUnitId::Frigate, "Frigate", CivUnitDomain::Water, 4, 2, 2, 3, 0, static_cast<int8_t>(CivAdvanceId::Magnetism), -1 },
	{ CivUnitId::Ironclad, "Ironclad", CivUnitDomain::Water, 6, 4, 4, 4, 0, static_cast<int8_t>(CivAdvanceId::SteamEngine), static_cast<int8_t>(CivAdvanceId::Combustion) },
	{ CivUnitId::Cruiser, "Cruiser", CivUnitDomain::Water, 8, 6, 6, 6, 2, static_cast<int8_t>(CivAdvanceId::Combustion), -1 },
	{ CivUnitId::Battleship, "Battleship", CivUnitDomain::Water, 16, 18, 12, 4, 2, static_cast<int8_t>(CivAdvanceId::Steel), -1 },
	{ CivUnitId::Submarine, "Submarine", CivUnitDomain::Water, 5, 8, 2, 3, 2, static_cast<int8_t>(CivAdvanceId::MassProduction), -1 },
	{ CivUnitId::Carrier, "Carrier", CivUnitDomain::Water, 16, 1, 12, 5, 2, static_cast<int8_t>(CivAdvanceId::AdvancedFlight), -1 },
	{ CivUnitId::Transport, "Transport", CivUnitDomain::Water, 5, 0, 3, 4, 0, static_cast<int8_t>(CivAdvanceId::Industrialization), -1 },
	{ CivUnitId::Nuclear, "Nuclear", CivUnitDomain::Air, 16, 99, 0, 16, 0, static_cast<int8_t>(CivAdvanceId::Rocketry), -1 },
	{ CivUnitId::Diplomat, "Diplomat", CivUnitDomain::Land, 3, 0, 0, 2, 0, static_cast<int8_t>(CivAdvanceId::Writing), -1 },
	{ CivUnitId::Caravan, "Caravan", CivUnitDomain::Land, 5, 0, 1, 1, 0, static_cast<int8_t>(CivAdvanceId::Trade), -1 },
};

const CivUnitDef& CivUnit(int id)
{
	static const CivUnitDef kNone{ CivUnitId::None, "None", CivUnitDomain::Land, 0,0,0,0,0, -1, -1 };
	if (id < 0 || id >= 28) return kNone;
	return kUnits[id];
}
const CivUnitDef& CivUnit(CivUnitId id) { return CivUnit(static_cast<int>(id)); }
const char* CivUnitName(int id) { return CivUnit(id).name; }
int CivUnitCount() { return 28; }
int CivUnitShieldCost(int id) { return CivUnit(id).price * 10; }

bool CivUnitAvailable(int unitId, const std::vector<int>& knownAdvances)
{
	const CivUnitDef& u = CivUnit(unitId);
	if (u.id == CivUnitId::None) return false;
	auto has = [&](int tech) {
		if (tech < 0) return true;
		return std::find(knownAdvances.begin(), knownAdvances.end(), tech) != knownAdvances.end();
	};
	if (!has(u.requiredTech)) return false;
	if (u.obsoleteTech >= 0 && has(u.obsoleteTech)) return false;
	return true;
}
