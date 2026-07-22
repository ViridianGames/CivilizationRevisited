#ifndef _CIVAI_H_
#define _CIVAI_H_

#include "CivPlayer.h"
#include "CivTile.h"
#include "CivUnitInstance.h"
#include "Geist/RNG.h"

#include <string>
#include <vector>

// Shared AI for all computer players. Stronger than CivOne's rule stubs:
// need-based city production, scored city sites, targeted tile improvements,
// and research prioritization. Difficulty still mainly changes bonuses/caps.
namespace CivAI
{
	bool ChooseResearch(CivPlayer& player, RNG& rng);

	void ChooseCityProduction(CivPlayer& player, CivCity& city,
		const CivMapData& map, const std::vector<CivUnitInstance>& units,
		CivDifficulty difficulty, RNG& rng);

	void AdjustBudget(CivPlayer& player, CivDifficulty difficulty);

	// Full AI turn after economy. May found cities / improve tiles / move units.
	// allPlayers: full roster for spacing (may be nullptr).
	std::string PlayTurn(CivPlayer& player, CivMapData& map,
		std::vector<CivUnitInstance>& units, int& nextUnitId, int& nextCityId,
		CivDifficulty difficulty, RNG& rng,
		std::vector<CivPlayer>* allPlayers = nullptr);
}

#endif
