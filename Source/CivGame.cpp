#include "CivGame.h"

#include "CivAdvances.h"
#include "CivBuildings.h"
#include "CivFactions.h"
#include "CivMapGenerator.h"
#include "CivTerritory.h"
#include "CivUnits.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <utility>
#include <vector>

CivPlayer* CivGame::CurrentPlayer()
{
	return setup.PlayerAt(currentPlayerIndex);
}

const CivPlayer* CivGame::CurrentPlayer() const
{
	return setup.PlayerAt(currentPlayerIndex);
}

bool CivGame::IsHumanTurn() const
{
	const CivPlayer* p = CurrentPlayer();
	return p && p->IsHuman();
}

int CivGame::TurnToYear(int turn)
{
	// Approximate classic Civ1 calendar (matches common remake tables):
	// 4000 BC at turn 0; 20 years/turn until 1000 BC, then 25, then 50, etc.
	// Simple linear for early game: year = -4000 + turn * 20.
	return -4000 + turn * 20;
}

std::string CivGame::YearString() const
{
	const int y = TurnToYear(gameTurn);
	char buf[32];
	if (y < 0)
		snprintf(buf, sizeof(buf), "%d BC", -y);
	else if (y == 0)
		snprintf(buf, sizeof(buf), "1 AD");
	else
		snprintf(buf, sizeof(buf), "%d AD", y);
	return buf;
}

void CivGame::StartNewGame(RNG& rng)
{
	(void)rng;
	gameTurn = 0;
	units.clear();
	nextUnitId = 1;
	started = true;

	// AI starting gold cushion by difficulty.
	for (CivPlayer& p : setup.players)
	{
		if (!p.active)
			continue;
		if (p.IsAI())
			p.gold += CivAIStartingGold(setup.difficulty);
		// Ensure research pick for everyone.
		if (p.currentResearch < 0)
		{
			std::vector<int> avail = CivAdvancesAvailable(p.advances);
			if (!avail.empty())
				p.currentResearch = avail[0];
		}
	}

	// Human goes first if present; else player 0.
	const int human = setup.HumanIndex();
	currentPlayerIndex = human >= 0 ? human : 0;

	// Each civ starts with one Settler (no free capital). ~35% chance of a
	// second Settler. Positions come from CivPlaceStartingCities (startX/Y).
	for (CivPlayer& p : setup.players)
	{
		if (!p.active)
			continue;
		if (p.startX < 0 || p.startY < 0)
			continue;
		SpawnUnit(static_cast<int>(CivUnitId::Settlers), p.id, p.startX, p.startY, /*homeCityId=*/-1);
		// Random second settler (same tile; AI/human can peel off later).
		if (rng.Random(100) < 35)
			SpawnUnit(static_cast<int>(CivUnitId::Settlers), p.id, p.startX, p.startY, /*homeCityId=*/-1);
	}

	RefreshMapState();
	// Unit LOS at spawn (no cities yet).
	for (const auto& u : units)
	{
		if (!u.Valid())
			continue;
		if (CivPlayer* pl = setup.PlayerAt(u.owner))
			CivAddVision(*pl, u.x, u.y, kCivUnitVisionRange, map.width, map.height);
	}
	// Start-of-turn economy for first player (no cities yet → gold cushion only).
	if (CivPlayer* p = CurrentPlayer())
		ProcessPlayerEconomy(*p, rng);
	lastLog = "Game start — " + YearString();
}

CivUnitInstance* CivGame::SpawnUnit(int typeId, int owner, int x, int y, int homeCityId)
{
	CivUnitInstance u;
	u.id = nextUnitId++;
	u.typeId = typeId;
	u.owner = owner;
	u.x = x;
	u.y = y;
	u.homeCityId = homeCityId;
	u.fortify = false;
	u.sentry = false;
	u.ResetMoves();
	units.push_back(u);
	return &units.back();
}

void CivGame::ProcessResearch(CivPlayer& player)
{
	if (player.currentResearch < 0)
		return;
	const int cost = CivScienceCost(static_cast<int>(setup.difficulty),
		static_cast<int>(player.advances.size()), TurnToYear(gameTurn) >= 0);
	if (player.science < cost)
		return;

	player.science -= cost;
	player.AddAdvance(player.currentResearch);
	const int done = player.currentResearch;
	player.currentResearch = -1;
	// Immediately pick next (AI will re-pick; human can change later).
	std::vector<int> avail = CivAdvancesAvailable(player.advances);
	if (!avail.empty())
		player.currentResearch = avail[0];

	char buf[96];
	snprintf(buf, sizeof(buf), "%s discovers %s!", player.tribeName.c_str(), CivAdvanceName(done));
	lastLog = buf;
}

void CivGame::ProcessCityProduction(CivPlayer& player, CivCity& city, RNG& rng)
{
	(void)rng;
	if (!city.Valid())
		return;

	if (city.production.kind == CivProductionKind::None)
	{
		// Human/AI should set something; AI sets during PlayTurn.
		return;
	}

	// Shield income from worked tiles.
	city.AutoAssignWorkedTiles(map, player.government, nullptr);
	CivYields y = city.ComputeWorkedYields(map, player.government);
	int shields = city.InDisorder() ? 0 : y.shields;

	// AI production bonus by difficulty.
	if (player.IsAI())
	{
		const int tenths = CivAIShieldBonusTenths(setup.difficulty);
		shields += (shields * tenths) / 10;
	}

	city.shields += shields;

	int need = 0;
	if (city.production.kind == CivProductionKind::Unit)
		need = CivUnitShieldCost(city.production.id);
	else if (city.production.kind == CivProductionKind::Building)
		need = CivBuildingShieldCost(city.production.id);

	if (need <= 0 || city.shields < need)
		return;

	// Complete production.
	if (city.production.kind == CivProductionKind::Building)
	{
		city.CompleteBuilding(city.production.id);
		return;
	}

	if (city.production.kind == CivProductionKind::Unit)
	{
		const int typeId = city.production.id;
		// Settlers: city loses 1 population (classic).
		if (typeId == static_cast<int>(CivUnitId::Settlers))
		{
			const bool sole = player.CityCount() <= 1;
			const bool chieftain = setup.difficulty == CivDifficulty::Chieftain;
			CivCitySettlerBuildResult r = city.CompleteSettlersBuild(sole, chieftain);
			if (!r.built)
			{
				// Leave production; hold shields just under cost so we retry next turn.
				city.shields = need - 1;
				return;
			}
			if (r.cityDestroyed)
			{
				city.ClearProduction();
				return;
			}
		}

		SpawnUnit(typeId, player.id, city.x, city.y, city.id);
		city.ClearProduction();
	}
}

void CivGame::ProcessPlayerEconomy(CivPlayer& player, RNG& rng)
{
	if (!player.IsAlive())
		return;

	// Food growth.
	player.ProcessAllCitiesFoodTurn(map, nullptr);

	// Trade → gold / science / luxuries.
	player.ProcessAllCitiesTradeTurn(map, true);

	// Production.
	for (CivCity& c : player.cities)
	{
		if (!c.Valid())
			continue;
		// Ensure AI has a build order before shields apply.
		if (player.IsAI() && c.production.kind == CivProductionKind::None)
			CivAI::ChooseCityProduction(player, c, map, units, setup.difficulty, rng);
		ProcessCityProduction(player, c, rng);
	}

	ProcessResearch(player);
	CivRebuildPlayerVisibility(player, map, kCivCityVisionRange);
}

void CivGame::RunAiPlayer(CivPlayer& player, RNG& rng)
{
	if (!player.IsAI() || !player.IsAlive())
		return;

	// Economy already run at start of turn.
	lastLog = CivAI::PlayTurn(player, map, units, nextUnitId, setup.nextCityId,
		setup.difficulty, rng, &setup.players);

	// After AI founds cities, recompute territory.
	RefreshMapState();
}

void CivGame::RefreshMapState()
{
	map.ComputeContinents();
	CivTerritoryMap terr;
	terr.Compute(map, setup);
	CivApplyTerritoryToMap(map, terr);
	CivRebuildAllVisibility(setup, map, kCivCityVisionRange);
}

bool CivGame::EndHumanTurn(RNG& rng)
{
	if (!started)
		return false;

	const int n = setup.PlayerCount();
	if (n <= 0)
		return false;

	const int human = setup.HumanIndex();
	bool advancedYear = false;

	// Observe mode / pure AI: run every living player once, then advance year.
	if (observeMode || human < 0)
	{
		for (int i = 0; i < n; ++i)
		{
			currentPlayerIndex = i;
			CivPlayer* p = CurrentPlayer();
			if (!p || !p->IsAlive())
				continue;
			ProcessPlayerEconomy(*p, rng);
			if (p->IsAI())
				RunAiPlayer(*p, rng);
		}
		currentPlayerIndex = 0;
		++gameTurn;
		advancedYear = true;
		RefreshMapState();
		// Re-apply unit vision after AI moves (RefreshMapState only rebuilds city LOS).
		for (const auto& u : units)
		{
			if (!u.Valid())
				continue;
			CivPlayer* pl = setup.PlayerAt(u.owner);
			if (pl)
				CivAddVision(*pl, u.x, u.y, kCivUnitVisionRange, map.width, map.height);
		}
		lastLog = "Year advanced — " + YearString();
		return advancedYear;
	}

	// Leave current (human) player → cycle through everyone until human again.
	int guard = 0;
	do
	{
		currentPlayerIndex = (currentPlayerIndex + 1) % n;
		++guard;
		if (currentPlayerIndex == 0)
		{
			++gameTurn;
			advancedYear = true;
		}

		CivPlayer* p = CurrentPlayer();
		if (!p || !p->IsAlive())
			continue;

		ProcessPlayerEconomy(*p, rng);

		if (p->IsAI())
			RunAiPlayer(*p, rng);

	} while (guard < n + 2 && currentPlayerIndex != human);

	// Human start-of-turn already processed in the loop when we landed on them.
	RefreshMapState();
	return advancedYear;
}

bool CivGame::StartObserveGame(RNG& rng)
{
	Reset();
	observeMode = true;
	setup.difficulty = CivDifficulty::Prince;
	setup.numCivilizations = 8;
	setup.world = CivWorldOptions{}; // default Normal world
	setup.newGameFlow = false;

	// Shuffle factions and colors, assign 8 AI players.
	int nFac = 0;
	const CivFactionDef* facList = CivFactionList(&nFac);
	std::vector<int> facOrder;
	facOrder.reserve(static_cast<size_t>(nFac));
	for (int i = 0; i < nFac; ++i)
		facOrder.push_back(static_cast<int>(facList[i].id));
	// Fisher–Yates
	for (int i = static_cast<int>(facOrder.size()) - 1; i > 0; --i)
	{
		const int j = rng.Random(i + 1);
		std::swap(facOrder[static_cast<size_t>(i)], facOrder[static_cast<size_t>(j)]);
	}

	std::vector<int> colOrder;
	for (int c = 0; c < static_cast<int>(CivColor::Count); ++c)
		colOrder.push_back(c);
	for (int i = static_cast<int>(colOrder.size()) - 1; i > 0; --i)
	{
		const int j = rng.Random(i + 1);
		std::swap(colOrder[static_cast<size_t>(i)], colOrder[static_cast<size_t>(j)]);
	}

	CivPlayerSlot slots[8]{};
	const int n = 8;
	for (int i = 0; i < n; ++i)
	{
		slots[i].active = true;
		slots[i].human = false;
		slots[i].faction = static_cast<CivFactionId>(facOrder[static_cast<size_t>(i % facOrder.size())]);
		slots[i].color = static_cast<CivColor>(colOrder[static_cast<size_t>(i % colOrder.size())]);
	}
	// Ensure unique factions when enough exist.
	for (int i = 0; i < n && i < static_cast<int>(facOrder.size()); ++i)
		slots[i].faction = static_cast<CivFactionId>(facOrder[static_cast<size_t>(i)]);

	setup.BuildPlayersFromSlots(slots, n);
	setup.GrantAllStartingAdvances(rng);

	if (!CivMapGenerator::Generate(map, setup.world, rng))
	{
		Reset();
		return false;
	}

	CivPlaceStartingCities(map, setup, rng);
	StartNewGame(rng);
	observeMode = true; // StartNewGame path doesn't clear via Reset mid-way, but be sure
	map.title = "Observe — " + setup.players.front().tribeName;
	lastLog = "Observing " + setup.players.front().tribeName + " — " + YearString();
	return true;
}
