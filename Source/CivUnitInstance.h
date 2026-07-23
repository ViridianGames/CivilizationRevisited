#ifndef _CIVUNITINSTANCE_H_
#define _CIVUNITINSTANCE_H_

#include "CivUnits.h"

#include <cstdint>

// Live unit on the map (minimal Civ1 unit state for AI + future human control).
struct CivUnitInstance
{
	int id = -1;
	int typeId = static_cast<int>(CivUnitId::Militia);
	int owner = -1;       // CivPlayer::id
	int x = 0;
	int y = 0;
	int homeCityId = -1;  // -1 = no home
	int movesLeft = 0;
	bool fortify = false;
	bool sentry = false;
	// Simple goto target (AI). Empty when gotoX < 0.
	int gotoX = -1;
	int gotoY = -1;

	// Settler construction job (Civ1 multi-turn tile work).
	enum BuildJob : int8_t
	{
		BuildNone = 0,
		BuildRoad = 1,
		BuildIrrigation = 2,
		BuildMine = 3,
	};
	int8_t buildJob = BuildNone;
	int8_t buildTurnsLeft = 0;

	bool Valid() const { return id >= 0 && owner >= 0; }
	bool Busy() const { return fortify || sentry || buildJob != BuildNone; }
	bool HasGoto() const { return gotoX >= 0 && gotoY >= 0; }
	bool IsBuilding() const { return buildJob != BuildNone && buildTurnsLeft > 0; }

	void ClearGoto()
	{
		gotoX = gotoY = -1;
	}

	void ClearBuild()
	{
		buildJob = BuildNone;
		buildTurnsLeft = 0;
	}

	void ResetMoves()
	{
		const CivUnitDef& d = CivUnit(typeId);
		movesLeft = d.move;
		// Sea range units get extra later; keep simple for now.
	}

	const CivUnitDef& Def() const { return CivUnit(typeId); }
	bool IsSettlers() const { return typeId == static_cast<int>(CivUnitId::Settlers); }
	bool IsDiplomat() const { return typeId == static_cast<int>(CivUnitId::Diplomat); }
	bool IsCaravan() const { return typeId == static_cast<int>(CivUnitId::Caravan); }

	// CivOne ShieldCosts: every unit except Diplomat/Caravan costs shields (settlers included).
	bool RequiresShieldSupport() const
	{
		return Valid() && !IsDiplomat() && !IsCaravan();
	}

	// Martial law: military units with attack power (not settlers / diplomats / caravans).
	bool CountsForMartialLaw() const
	{
		return RequiresShieldSupport() && !IsSettlers() && Def().attack >= 1;
	}

	// Republic/Democracy: non-exempt units away from their home city cause unhappiness.
	bool CanCauseWarUnhappiness() const
	{
		return RequiresShieldSupport() && !IsSettlers();
	}

	bool IsDefender() const
	{
		switch (static_cast<CivUnitId>(typeId))
		{
		case CivUnitId::Militia:
		case CivUnitId::Phalanx:
		case CivUnitId::Musketeers:
		case CivUnitId::Riflemen:
		case CivUnitId::MechInf:
			return true;
		default:
			return false;
		}
	}
};

#endif
