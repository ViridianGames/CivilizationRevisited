#ifndef _CIVDIFFICULTY_H_
#define _CIVDIFFICULTY_H_

#include "CivFactions.h"

#include <cstdint>

// Classic Civ1: one AI brain; difficulty changes bonuses / limits, not "smarter" AI.
// Values reverse-engineered from CivOne (City happiness, settlers, science cost).

// Free content citizens before unhappiness (human).
// CivOne: unhappyCount = Size - (6 - Difficulty) - happyCount
// → free content slots = 6 - difficulty (Chieftain 6 … Deity 1).
inline int CivFreeContentCitizens(CivDifficulty d, bool isHuman)
{
	const int diff = static_cast<int>(d);
	// AI is treated more leniently on higher difficulties (fewer unhappy).
	// Approximate classic: AI gets +1 free content per difficulty step vs human.
	int free = 6 - diff;
	if (!isHuman)
		free += diff; // Deity AI ≈ fully content baseline; Chieftain AI same as human
	if (free < 1)
		free = 1;
	return free;
}

// Extra starting gold for AI (0 on Chieftain; scales up).
inline int CivAIStartingGold(CivDifficulty d)
{
	// Mild classic-style cushion so AI can maintain buildings early.
	return static_cast<int>(d) * 25;
}

// AI production shield bonus fraction (0..n tenths). Deity cities build faster.
// 0 = none, 1 = +10%, … 5 = +50% on Deity.
inline int CivAIShieldBonusTenths(CivDifficulty d)
{
	return static_cast<int>(d); // 0..5
}

// Max AI cities soft target (classic expansion caps were leader-based;
// difficulty loosens the soft cap slightly).
inline int CivAIMaxCitiesSoft(CivDifficulty d)
{
	return 8 + static_cast<int>(d) * 2; // 8..18
}

// Min city size before AI builds settlers.
inline int CivAIMinSizeForSettlers(CivDifficulty d)
{
	// Easier levels: AI expands slower (larger min size).
	return 4 - static_cast<int>(d) / 2; // 4,4,3,3,2,2
}

#endif
