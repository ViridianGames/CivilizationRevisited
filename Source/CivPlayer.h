#ifndef _CIVPLAYER_H_
#define _CIVPLAYER_H_

#include "CivAdvances.h"
#include "CivCity.h"
#include "CivFactions.h"
#include "CivMapGenerator.h"
#include "CivTerrainYields.h"
#include "Geist/RNG.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// A civilization in the game — human or AI. Owns cities and tracks empire state.
class CivPlayer
{
public:
	// --- identity ---
	int id = -1;                    // slot index 0..7
	bool active = false;
	bool human = false;
	bool destroyed = false;

	CivFactionId faction = CivFactionId::Romans;
	CivColor color = CivColor::White;

	// Display names (may be customized later).
	std::string leaderName;
	std::string tribeName;          // "Romans"
	std::string tribeNamePlural;    // "Roman" / tribe adjective — keep flexible

	// --- economy ---
	int gold = 0;
	int science = 0;                // beakers toward current research
	// Budget rates 0..10 that sum to 10 (classic): luxuries + taxes + science.
	int luxuriesRate = 0;
	int taxesRate = 5;
	int scienceRate = 5;

	// --- government / tech ---
	CivGovernment government = CivGovernment::Despotism;
	int anarchyTurns = 0;
	std::vector<int> advances;      // researched advance ids
	int currentResearch = -1;       // advance id, or -1

	// --- map presence ---
	std::vector<CivCity> cities;    // cities belonging to this civilization
	// Unit list comes later (vector of unit objects / ids).
	std::vector<int> unitIds;

	// Starting settler/city placement bookkeeping.
	int startX = -1;
	int startY = -1;
	int handicap = 0;               // AI bonus score from start position

	// Fog of war: explored = permanent terrain memory; visible = current LOS.
	// Sized fogWidth * fogHeight (row-major, y * fogWidth + x). Empty = no fog.
	int fogWidth = 0;
	int fogHeight = 0;
	std::vector<bool> explored;
	std::vector<bool> visible;

	bool hasContactWith[8] = {};

	bool HasFog() const
	{
		return fogWidth > 0 && fogHeight > 0
			&& explored.size() == static_cast<size_t>(fogWidth * fogHeight);
	}

	bool IsExplored(int x, int y) const
	{
		if (!HasFog() || y < 0 || y >= fogHeight)
			return false;
		while (x < 0)
			x += fogWidth;
		x %= fogWidth;
		return explored[static_cast<size_t>(y * fogWidth + x)];
	}

	bool IsVisible(int x, int y) const
	{
		if (!HasFog() || y < 0 || y >= fogHeight)
			return false;
		while (x < 0)
			x += fogWidth;
		x %= fogWidth;
		return visible[static_cast<size_t>(y * fogWidth + x)];
	}

	// --- constructors / factory ---

	CivPlayer() = default;

	static CivPlayer Create(int playerId, CivFactionId fac, CivColor col, bool isHuman)
	{
		CivPlayer p;
		p.id = playerId;
		p.active = true;
		p.human = isHuman;
		p.faction = fac;
		p.color = col;
		const CivFactionDef& def = CivFaction(fac);
		p.leaderName = def.leader;
		p.tribeName = def.name;
		p.tribeNamePlural = def.namePlural;
		p.government = CivGovernment::Despotism;
		p.gold = 0;
		p.luxuriesRate = 0;
		p.taxesRate = 5;
		p.scienceRate = 5;
		return p;
	}

	// --- queries ---

	bool IsHuman() const { return human; }
	bool IsAI() const { return active && !human && !destroyed; }
	bool IsAlive() const { return active && !destroyed; }

	const CivFactionDef& FactionDef() const { return CivFaction(faction); }
	Color ColorRgb() const { return CivColorRgb(color); }
	Color ColorDarkRgb() const { return CivColorDarkRgb(color); }

	int CityCount() const
	{
		int n = 0;
		for (const CivCity& c : cities)
			if (c.Valid())
				++n;
		return n;
	}

	// Approximate population (classic size→citizens is size-based; use size sum for now).
	int Population() const
	{
		int pop = 0;
		for (const CivCity& c : cities)
			if (c.Valid())
				pop += c.size;
		return pop;
	}

	CivCity* Capital()
	{
		for (CivCity& c : cities)
			if (c.Valid() && c.capital)
				return &c;
		for (CivCity& c : cities)
			if (c.Valid())
				return &c;
		return nullptr;
	}

	const CivCity* Capital() const
	{
		return const_cast<CivPlayer*>(this)->Capital();
	}

	CivCity* FindCityAt(int x, int y)
	{
		for (CivCity& c : cities)
			if (c.Valid() && c.x == x && c.y == y)
				return &c;
		return nullptr;
	}

	const CivCity* FindCityAt(int x, int y) const
	{
		return const_cast<CivPlayer*>(this)->FindCityAt(x, y);
	}

	CivCity* FindCityById(int cityId)
	{
		for (CivCity& c : cities)
			if (c.id == cityId)
				return &c;
		return nullptr;
	}

	const CivCity* FindCityById(int cityId) const
	{
		return const_cast<CivPlayer*>(this)->FindCityById(cityId);
	}

	bool HasAdvance(int advanceId) const
	{
		for (int a : advances)
			if (a == advanceId)
				return true;
		return false;
	}

	void AddAdvance(int advanceId)
	{
		if (!HasAdvance(advanceId))
			advances.push_back(advanceId);
	}

	// --- city ownership ---

	CivCity& AddCity(CivCity city)
	{
		city.owner = id;
		cities.push_back(std::move(city));
		return cities.back();
	}

	// Recompute owner field on all cities (after load / id change).
	void SyncCityOwners()
	{
		for (CivCity& c : cities)
			c.owner = id;
	}

	// Normalize tax rates so luxuries + taxes + science = 10.
	void ClampBudgetRates()
	{
		luxuriesRate = std::max(0, std::min(10, luxuriesRate));
		taxesRate = std::max(0, std::min(10, taxesRate));
		scienceRate = 10 - luxuriesRate - taxesRate;
		if (scienceRate < 0)
		{
			taxesRate = std::max(0, taxesRate + scienceRate);
			scienceRate = 10 - luxuriesRate - taxesRate;
		}
	}

	// Current budget rates as a struct (rates should sum to 10).
	CivBudgetRates BudgetRates() const
	{
		CivBudgetRates b;
		b.luxuriesRate = luxuriesRate;
		b.taxesRate = taxesRate;
		b.scienceRate = scienceRate;
		return b;
	}

	// True if any city owns this wonder id.
	bool HasWonder(int wonderId) const
	{
		for (const CivCity& c : cities)
			if (c.Valid() && c.HasWonder(wonderId))
				return true;
		return false;
	}

	// Run classic food growth for every living city (auto tile assign + surplus).
	// homeSettlersPerCity: optional parallel array (same order as cities) of settler
	// counts home to each city; null means 0 settlers everywhere.
	// Returns number of cities that grew this turn.
	int ProcessAllCitiesFoodTurn(const CivMapData& map,
		const std::vector<int>* homeSettlersPerCity = nullptr)
	{
		int grew = 0;
		for (size_t i = 0; i < cities.size(); ++i)
		{
			CivCity& c = cities[i];
			if (!c.Valid())
				continue;
			const int settlers = (homeSettlersPerCity && i < homeSettlersPerCity->size())
				? (*homeSettlersPerCity)[i]
				: 0;
			const CivCityFoodTurnResult r = c.ProcessFoodTurnFromMap(
				map, government, settlers, c.InDisorder(), nullptr);
			if (r.Has(CivFoodEvt_Grew))
				++grew;
			// Keep disorder flag history for advisor messages later.
			c.wasInDisorder = c.InDisorder();
		}
		// Drop destroyed cities (size 0).
		cities.erase(
			std::remove_if(cities.begin(), cities.end(),
				[](const CivCity& c) { return c.destroyed || c.size <= 0; }),
			cities.end());
		return grew;
	}

	// Convert each city's trade into gold / science / luxuries using budget rates.
	// Applies: corruption, tax/lux/science split, market/bank/library/university,
	// maintenance drain on gold. Disorder: no tax income (science still counts).
	// Returns empire totals for this turn (after maintenance).
	struct TradeTurnTotals
	{
		int goldIncome = 0;       // sum of taxes before maintenance
		int maintenance = 0;
		int netGold = 0;          // goldIncome - maintenance (added to player.gold)
		int science = 0;          // beakers added to player.science
		int luxuries = 0;         // luxury arrows (for happiness; not stored yet)
		int corruption = 0;
		int rawTrade = 0;
	};

	TradeTurnTotals ProcessAllCitiesTradeTurn(const CivMapData& map,
		bool autoAssignTiles = true)
	{
		TradeTurnTotals tot;
		ClampBudgetRates();
		const CivBudgetRates budget = BudgetRates();

		const CivCity* capital = Capital();
		const bool hasCapital = capital != nullptr;
		const bool empireSeti = HasWonder(CivWonder_SETIProgram);

		for (CivCity& c : cities)
		{
			if (!c.Valid())
				continue;

			int dist = 0;
			if (hasCapital)
				dist = CivMapDistance(c.x, c.y, capital->x, capital->y, map.width);
			else
				dist = 32;

			CivCityTradeBreakdown b;
			if (autoAssignTiles)
			{
				b = c.ProcessTradeTurnFromMap(map, budget, government, dist, hasCapital,
					c.InDisorder(), empireSeti, nullptr);
			}
			else
			{
				b = c.ComputeTradeBreakdownFromMap(map, budget, government, dist, hasCapital,
					c.InDisorder(), empireSeti);
			}

			tot.rawTrade += b.rawTrade;
			tot.corruption += b.corruption;
			tot.goldIncome += b.taxes;
			tot.maintenance += b.maintenance;
			tot.science += b.science;
			tot.luxuries += b.luxuries;
		}

		// CivOne NewTurn per city: Gold += taxes (0 if disorder), Gold -= maintenance,
		// Science += science. Applied empire-wide here.
		tot.netGold = tot.goldIncome - tot.maintenance;
		gold += tot.netGold;
		science += tot.science;
		return tot;
	}
};

// Lightweight picks while configuring a new game (before full CivPlayer objects exist).
struct CivPlayerSlot
{
	CivFactionId faction = CivFactionId::Romans;
	CivColor color = CivColor::White;
	bool human = false;
	bool active = false;
};

// Session / game state shared across screens.
struct CivGameSetup
{
	bool newGameFlow = false;

	CivDifficulty difficulty = CivDifficulty::Prince;
	// Including human. Original: 3–7; we allow 3–8 (red playable).
	int numCivilizations = 7;

	CivWorldOptions world;

	// Full player objects once the game has started (and after faction select).
	std::vector<CivPlayer> players;
	int nextCityId = 1;

	void Reset()
	{
		newGameFlow = false;
		difficulty = CivDifficulty::Prince;
		numCivilizations = 7;
		world = CivWorldOptions{};
		players.clear();
		nextCityId = 1;
	}

	int PlayerCount() const { return static_cast<int>(players.size()); }

	CivPlayer* PlayerAt(int i)
	{
		if (i < 0 || i >= static_cast<int>(players.size()))
			return nullptr;
		return &players[static_cast<size_t>(i)];
	}

	const CivPlayer* PlayerAt(int i) const
	{
		if (i < 0 || i >= static_cast<int>(players.size()))
			return nullptr;
		return &players[static_cast<size_t>(i)];
	}

	int HumanIndex() const
	{
		for (int i = 0; i < static_cast<int>(players.size()); ++i)
			if (players[static_cast<size_t>(i)].human)
				return i;
		return -1;
	}

	CivPlayer* Human()
	{
		const int i = HumanIndex();
		return i >= 0 ? PlayerAt(i) : nullptr;
	}

	const CivPlayer* Human() const
	{
		const int i = HumanIndex();
		return i >= 0 ? PlayerAt(i) : nullptr;
	}

	// Flatten all living cities (for map draw / global search).
	std::vector<CivCity*> AllCities()
	{
		std::vector<CivCity*> out;
		for (CivPlayer& p : players)
			for (CivCity& c : p.cities)
				if (c.Valid())
					out.push_back(&c);
		return out;
	}

	std::vector<const CivCity*> AllCities() const
	{
		std::vector<const CivCity*> out;
		for (const CivPlayer& p : players)
			for (const CivCity& c : p.cities)
				if (c.Valid())
					out.push_back(&c);
		return out;
	}

	CivCity* FindCityAt(int x, int y)
	{
		for (CivPlayer& p : players)
			if (CivCity* c = p.FindCityAt(x, y))
				return c;
		return nullptr;
	}

	const CivCity* FindCityAt(int x, int y) const
	{
		for (const CivPlayer& p : players)
			if (const CivCity* c = p.FindCityAt(x, y))
				return c;
		return nullptr;
	}

	CivCity* FindCityById(int cityId)
	{
		for (CivPlayer& p : players)
			if (CivCity* c = p.FindCityById(cityId))
				return c;
		return nullptr;
	}

	// Build CivPlayer list from faction-select slots (no starting techs yet).
	void BuildPlayersFromSlots(const CivPlayerSlot* slots, int count)
	{
		players.clear();
		players.reserve(static_cast<size_t>(count));
		for (int i = 0; i < count; ++i)
		{
			if (!slots[i].active)
				continue;
			CivPlayer p = CivPlayer::Create(static_cast<int>(players.size()),
				slots[i].faction, slots[i].color, slots[i].human);
			p.id = i;
			players.push_back(std::move(p));
		}
		for (int i = 0; i < static_cast<int>(players.size()); ++i)
			players[static_cast<size_t>(i)].id = i;
	}

	// Give every civilization starting advances (Roads/Irrigation/Mining + optional free techs).
	void GrantAllStartingAdvances(RNG& rng)
	{
		for (CivPlayer& p : players)
		{
			if (!p.active)
				continue;
			CivGrantStartingAdvances(p.advances, rng);
		}
	}
};

// Pick spaced starting map positions (startX/Y). Does not found cities —
// each civ begins with settlers at those coordinates.
int CivPlaceStartingCities(CivMapData& map, CivGameSetup& setup, RNG& rng);

#endif
