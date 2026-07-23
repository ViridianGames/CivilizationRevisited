#include "CivAI.h"

#include "CivAdvances.h"
#include "CivBuildings.h"
#include "CivDifficulty.h"
#include "CivTerritory.h"
#include "CivUnits.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

// =============================================================================
// Civilization AI — design notes
//
// CivOne's AI is intentionally dumb (fixed production list, random walks).
// Ours is goal-driven:
//   • Empire phase: Settle → Expand → Develop (by city count / settlers / food)
//   • Cities produce what they *need* (garrison, food, growth, infrastructure)
//   • Settlers path toward high-value city sites or high-ROI tile improvements
//   • Research follows a short priority spine then opportunistic unlocks
// =============================================================================

namespace
{
	// ---- counts ----------------------------------------------------------------

	int CountUnitsOfType(const std::vector<CivUnitInstance>& units, int owner, int typeId)
	{
		int n = 0;
		for (const auto& u : units)
			if (u.Valid() && u.owner == owner && u.typeId == typeId)
				++n;
		return n;
	}

	int CountTypeOnTile(const std::vector<CivUnitInstance>& units, int owner, int x, int y, int typeId)
	{
		int n = 0;
		for (const auto& u : units)
			if (u.Valid() && u.owner == owner && u.x == x && u.y == y && u.typeId == typeId)
				++n;
		return n;
	}

	int CountUnitsOnTile(const std::vector<CivUnitInstance>& units, int owner, int x, int y)
	{
		int n = 0;
		for (const auto& u : units)
			if (u.Valid() && u.owner == owner && u.x == x && u.y == y)
				++n;
		return n;
	}

	int CountSettlersInProduction(const CivPlayer& player)
	{
		int n = 0;
		for (const CivCity& c : player.cities)
			if (c.Valid() && c.production.kind == CivProductionKind::Unit
				&& c.production.id == static_cast<int>(CivUnitId::Settlers))
				++n;
		return n;
	}

	int HomeSettlers(const std::vector<CivUnitInstance>& units, int owner, int cityId)
	{
		int n = 0;
		for (const auto& u : units)
			if (u.Valid() && u.owner == owner && u.IsSettlers() && u.homeCityId == cityId)
				++n;
		return n;
	}

	// ---- unit / tech helpers ---------------------------------------------------

	int BestDefenderType(const CivPlayer& player)
	{
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::LaborUnion)))
			return static_cast<int>(CivUnitId::MechInf);
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::Conscription)))
			return static_cast<int>(CivUnitId::Riflemen);
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::Gunpowder)))
			return static_cast<int>(CivUnitId::Musketeers);
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::BronzeWorking)))
			return static_cast<int>(CivUnitId::Phalanx);
		return static_cast<int>(CivUnitId::Militia);
	}

	int BestAttackType(const CivPlayer& player)
	{
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::Automobile)))
			return static_cast<int>(CivUnitId::Armor);
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::Robotics)))
			return static_cast<int>(CivUnitId::Artillery);
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::Metallurgy)))
			return static_cast<int>(CivUnitId::Cannon);
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::Chivalry)))
			return static_cast<int>(CivUnitId::Knights);
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::TheWheel)))
			return static_cast<int>(CivUnitId::Chariot);
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::HorsebackRiding)))
			return static_cast<int>(CivUnitId::Cavalry);
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::Mathematics)))
			return static_cast<int>(CivUnitId::Catapult);
		if (player.HasAdvance(static_cast<int>(CivAdvanceId::IronWorking)))
			return static_cast<int>(CivUnitId::Legion);
		return -1;
	}

	bool Despotic(CivGovernment g)
	{
		return CivGovIsDespotic(g);
	}

	// ---- map geometry ----------------------------------------------------------

	void WrapX(int& x, int mapW)
	{
		while (x < 0)
			x += mapW;
		x %= mapW;
	}

	bool InMapY(const CivMapData& map, int y)
	{
		return y >= 0 && y < map.height;
	}

	void CollectAllCities(const CivPlayer& player, const std::vector<CivPlayer>* allPlayers,
		std::vector<const CivCity*>& out)
	{
		out.clear();
		if (allPlayers)
		{
			for (const CivPlayer& p : *allPlayers)
				for (const CivCity& c : p.cities)
					if (c.Valid())
						out.push_back(&c);
		}
		else
		{
			for (const CivCity& c : player.cities)
				if (c.Valid())
					out.push_back(&c);
		}
	}

	int NearestCityDist(const std::vector<const CivCity*>& cities, int x, int y, int mapW)
	{
		int best = 255;
		for (const CivCity* c : cities)
		{
			if (!c || !c->Valid())
				continue;
			best = std::min(best, CivTileDistance(x, y, c->x, c->y, mapW));
		}
		return best;
	}

	int NearestOwnCityDist(const CivPlayer& player, int x, int y, int mapW)
	{
		int best = 255;
		for (const CivCity& c : player.cities)
			if (c.Valid())
				best = std::min(best, CivTileDistance(x, y, c.x, c.y, mapW));
		return best;
	}

	const CivCity* NearestOwnCity(const CivPlayer& player, int x, int y, int mapW)
	{
		const CivCity* best = nullptr;
		int bestD = 255;
		for (const CivCity& c : player.cities)
		{
			if (!c.Valid())
				continue;
			const int d = CivTileDistance(x, y, c.x, c.y, mapW);
			if (d < bestD)
			{
				bestD = d;
				best = &c;
			}
		}
		return best;
	}

	bool TileHasOwnCity(const CivPlayer& player, int x, int y)
	{
		for (const CivCity& c : player.cities)
			if (c.Valid() && c.x == x && c.y == y)
				return true;
		return false;
	}

	// ---- tile improvement rules ------------------------------------------------

	bool HasWaterAccess(const CivMapData& map, int x, int y)
	{
		if (map.TileAt(x, y).IsRiver())
			return true;
		static const int kDx[4] = { 0, 1, 0, -1 };
		static const int kDy[4] = { -1, 0, 1, 0 };
		for (int i = 0; i < 4; ++i)
		{
			int nx = x + kDx[i];
			const int ny = y + kDy[i];
			if (!InMapY(map, ny))
				continue;
			WrapX(nx, map.width);
			const CivTile& n = map.TileAt(nx, ny);
			if (n.IsOcean() || n.IsRiver() || n.HasIrrigation())
				return true;
		}
		return false;
	}

	bool CanBuildRoad(const CivMapData& map, const CivPlayer& player, int x, int y)
	{
		if (!InMapY(map, y))
			return false;
		WrapX(x, map.width);
		const CivTile& t = map.TileAt(x, y);
		if (t.IsOcean() || TileHasOwnCity(player, x, y) || t.HasRail())
			return false;
		if (t.HasRoad())
			return player.HasAdvance(static_cast<int>(CivAdvanceId::RailRoad));
		if (t.IsRiver() && !player.HasAdvance(static_cast<int>(CivAdvanceId::BridgeBuilding)))
			return false;
		return true;
	}

	bool CanIrrigate(const CivMapData& map, const CivPlayer& player, int x, int y)
	{
		if (!InMapY(map, y))
			return false;
		WrapX(x, map.width);
		const CivTile& t = map.TileAt(x, y);
		if (t.IsOcean() || TileHasOwnCity(player, x, y) || t.HasIrrigation() || t.HasMine())
			return false;
		const bool soft =
			t.terrain == CivTerrain::Grassland
			|| t.terrain == CivTerrain::Plains
			|| t.terrain == CivTerrain::Desert
			|| t.terrain == CivTerrain::Hills
			|| t.terrain == CivTerrain::River;
		return soft && HasWaterAccess(map, x, y);
	}

	bool CanMine(const CivMapData& map, const CivPlayer& player, int x, int y)
	{
		if (!InMapY(map, y))
			return false;
		WrapX(x, map.width);
		const CivTile& t = map.TileAt(x, y);
		if (t.IsOcean() || TileHasOwnCity(player, x, y) || t.HasMine() || t.HasIrrigation())
			return false;
		return t.terrain == CivTerrain::Hills
			|| t.terrain == CivTerrain::Mountains
			|| t.terrain == CivTerrain::Desert;
	}

	// ---- city site scoring -----------------------------------------------------

	// Estimate long-term value of founding at (x,y): center + best BFC tiles.
	int ScoreCitySite(const CivMapData& map, int x, int y,
		const std::vector<const CivCity*>& allCities, CivGovernment gov)
	{
		if (!InMapY(map, y))
			return -100000;
		int xx = x;
		WrapX(xx, map.width);
		const CivTile& center = map.TileAt(xx, y);
		if (center.IsOcean() || center.terrain == CivTerrain::Arctic
			|| center.terrain == CivTerrain::Mountains)
			return -100000;

		// Spacing: hard reject if too close to any city.
		const int near = NearestCityDist(allCities, xx, y, map.width);
		if (near <= 3)
			return -100000;

		// Prefer classic founding terrains.
		int score = 0;
		if (center.terrain == CivTerrain::Grassland || center.terrain == CivTerrain::Plains
			|| center.terrain == CivTerrain::River)
			score += 40;
		else if (center.terrain == CivTerrain::Hills || center.terrain == CivTerrain::Forest)
			score += 15;
		else
			score += 5;

		score += static_cast<int>(center.landValue) * 2;
		if (center.special || center.grassland2)
			score += 12;

		// Sum best non-center BFC yields (food weighted highest for early growth).
		std::vector<int> tileScores;
		tileScores.reserve(20);
		for (int dy = -2; dy <= 2; ++dy)
		{
			for (int dx = -2; dx <= 2; ++dx)
			{
				if (dx == 0 && dy == 0)
					continue;
				if (!CivCity::IsCityRadiusOffset(dx, dy))
					continue;
				int tx = xx + dx;
				const int ty = y + dy;
				if (!InMapY(map, ty))
					continue;
				WrapX(tx, map.width);
				const CivYields yld = CivCity::TileYieldAt(map, tx, ty, gov, false);
				// Soft score: food is king early, then shields, then trade.
				const int ts = yld.food * 6 + yld.shields * 3 + yld.trade * 2;
				if (ts > 0)
					tileScores.push_back(ts);
			}
		}
		std::sort(tileScores.begin(), tileScores.end(), std::greater<int>());
		// Size-1 city works center + 1 extra; value the best few extras more.
		for (size_t i = 0; i < tileScores.size() && i < 6; ++i)
			score += tileScores[i] / (1 + static_cast<int>(i) / 2);

		// Prefer some spacing but not isolation forever.
		if (near >= 5 && near <= 10)
			score += 15;
		else if (near > 12)
			score -= 8;

		// Coast is nice for later ships / trade.
		static const int cdx[] = { 0, 1, 0, -1 };
		static const int cdy[] = { -1, 0, 1, 0 };
		for (int i = 0; i < 4; ++i)
		{
			int nx = xx + cdx[i];
			const int ny = y + cdy[i];
			if (!InMapY(map, ny))
				continue;
			WrapX(nx, map.width);
			if (map.TileAt(nx, ny).IsOcean())
			{
				score += 8;
				break;
			}
		}
		return score;
	}

	bool IsValidCitySite(const CivMapData& map, int x, int y,
		const std::vector<const CivCity*>& allCities)
	{
		return ScoreCitySite(map, x, y, allCities, CivGovernment::Despotism) > 0;
	}

	// Search a radius for the best founding target from a unit's perspective.
	bool FindBestCitySiteNear(const CivMapData& map, const CivPlayer& player,
		const std::vector<const CivCity*>& allCities, int fromX, int fromY,
		int radius, int& outX, int& outY)
	{
		int best = 0;
		bool found = false;
		for (int dy = -radius; dy <= radius; ++dy)
		{
			for (int dx = -radius; dx <= radius; ++dx)
			{
				if (std::abs(dx) + std::abs(dy) > radius * 3 / 2 && std::max(std::abs(dx), std::abs(dy)) > radius)
					continue;
				int x = fromX + dx;
				const int y = fromY + dy;
				if (!InMapY(map, y))
					continue;
				WrapX(x, map.width);
				// Prefer sites the player has at least heard of.
				if (player.HasFog() && !player.IsExplored(x, y))
					continue;
				int sc = ScoreCitySite(map, x, y, allCities, player.government);
				if (sc <= 0)
					continue;
				// Prefer closer sites slightly so settlers don't march forever.
				const int dist = CivTileDistance(fromX, fromY, x, y, map.width);
				sc -= dist * 2;
				if (!found || sc > best)
				{
					best = sc;
					outX = x;
					outY = y;
					found = true;
				}
			}
		}
		return found;
	}

	// ---- movement --------------------------------------------------------------

	bool EnemyOnTile(const std::vector<CivUnitInstance>& units, int x, int y, int myOwner)
	{
		for (const auto& u : units)
			if (u.Valid() && u.x == x && u.y == y && u.owner != myOwner)
				return true;
		return false;
	}

	bool StepToward(CivUnitInstance& unit, CivMapData& map, int tx, int ty, CivPlayer& player,
		const std::vector<CivUnitInstance>* units)
	{
		if (unit.movesLeft <= 0)
			return false;
		const int curD = CivTileDistance(unit.x, unit.y, tx, ty, map.width);
		int bestX = unit.x, bestY = unit.y, bestD = curD;
		bool moved = false;
		for (int dy = -1; dy <= 1; ++dy)
		{
			for (int dx = -1; dx <= 1; ++dx)
			{
				if (dx == 0 && dy == 0)
					continue;
				int nx = unit.x + dx;
				const int ny = unit.y + dy;
				if (!InMapY(map, ny))
					continue;
				WrapX(nx, map.width);
				if (map.TileAt(nx, ny).IsOcean())
					continue;
				if (units && EnemyOnTile(*units, nx, ny, unit.owner))
					continue;
				const int d = CivTileDistance(nx, ny, tx, ty, map.width);
				if (d < bestD)
				{
					bestD = d;
					bestX = nx;
					bestY = ny;
					moved = true;
				}
			}
		}
		if (!moved)
			return false;
		unit.x = bestX;
		unit.y = bestY;
		unit.movesLeft = std::max(0, unit.movesLeft - 1);
		CivAddVision(player, unit.x, unit.y, kCivUnitVisionRange, map.width, map.height);
		return true;
	}

	void StartBuild(CivUnitInstance& unit, CivUnitInstance::BuildJob job, int turns)
	{
		unit.buildJob = job;
		unit.buildTurnsLeft = static_cast<int8_t>(turns);
		unit.movesLeft = 0;
		unit.fortify = false;
		unit.ClearGoto();
	}

	bool ProgressBuildJob(CivUnitInstance& unit, CivMapData& map, CivPlayer& player)
	{
		if (!unit.IsBuilding())
			return false;
		--unit.buildTurnsLeft;
		if (unit.buildTurnsLeft > 0)
		{
			unit.movesLeft = 0;
			return true;
		}
		CivTile& t = map.TileAt(unit.x, unit.y);
		switch (unit.buildJob)
		{
		case CivUnitInstance::BuildRoad:
			if (t.HasRoad() && player.HasAdvance(static_cast<int>(CivAdvanceId::RailRoad)))
				t.SetImp(CivImp_RailRoad, true);
			else
				t.SetImp(CivImp_Road, true);
			break;
		case CivUnitInstance::BuildIrrigation:
			t.SetImp(CivImp_Irrigation, true);
			t.SetImp(CivImp_Mine, false);
			break;
		case CivUnitInstance::BuildMine:
			t.SetImp(CivImp_Mine, true);
			t.SetImp(CivImp_Irrigation, false);
			break;
		default:
			break;
		}
		unit.ClearBuild();
		unit.movesLeft = 0;
		return false;
	}

	// ---- city economic snapshot ------------------------------------------------

	struct CitySnap
	{
		int foodSurplus = 0;
		int shields = 0;
		int trade = 0;
		int foodBox = 0;
		int size = 1;
	};

	CitySnap SnapshotCity(const CivPlayer& player, const CivCity& city, const CivMapData& map,
		const std::vector<CivUnitInstance>& units)
	{
		CitySnap s;
		s.size = city.size;
		CivCity tmp = city;
		tmp.AutoAssignWorkedTiles(map, player.government, nullptr);
		const CivYields y = tmp.ComputeWorkedYields(map, player.government);
		const int settlers = HomeSettlers(units, player.id, city.id);
		s.foodSurplus = tmp.FoodIncome(y.food, settlers, Despotic(player.government));
		s.shields = y.shields;
		s.trade = y.trade;
		s.foodBox = city.food;
		return s;
	}

	// Improvement ROI on a specific tile for a nearby city.
	int ImprovementRoi(const CivMapData& map, const CivPlayer& player, int x, int y,
		bool prioritizeFood, bool prioritizeShields)
	{
		const CivTile& t = map.TileAt(x, y);
		if (t.IsOcean())
			return -1;
		int score = 0;
		if (CanIrrigate(map, player, x, y))
		{
			score += prioritizeFood ? 50 : 30;
			if (t.terrain == CivTerrain::Grassland || t.terrain == CivTerrain::Plains)
				score += 15;
		}
		if (CanMine(map, player, x, y))
		{
			score += prioritizeShields ? 45 : 25;
			if (t.terrain == CivTerrain::Hills || t.special)
				score += 12;
		}
		if (CanBuildRoad(map, player, x, y) && !t.HasRoad())
		{
			score += 18; // trade + movement
			if (t.terrain == CivTerrain::Plains || t.terrain == CivTerrain::Grassland)
				score += 6;
		}
		if (t.special || t.grassland2)
			score += 10;
		return score;
	}

	bool FindBestImproveTile(const CivMapData& map, const CivPlayer& player, const CivCity& city,
		bool prioritizeFood, bool prioritizeShields, int& outX, int& outY)
	{
		// Prefer tiles the city is already working, then rest of BFC.
		CivCity tmp = city;
		tmp.AutoAssignWorkedTiles(map, player.government, nullptr);

		int best = 0;
		bool found = false;
		auto consider = [&](int x, int y, int bonus) {
			const int sc = ImprovementRoi(map, player, x, y, prioritizeFood, prioritizeShields) + bonus;
			if (sc > best)
			{
				best = sc;
				outX = x;
				outY = y;
				found = true;
			}
		};

		for (const CivWorkedTile& w : tmp.workedTiles)
		{
			int tx = 0, ty = 0;
			tmp.WorkedTileAbs(map, w.dx, w.dy, tx, ty);
			consider(tx, ty, 25); // bonus: currently worked
		}
		// Center rarely needs improvement (free irrig/road for yields), skip.
		for (int dy = -2; dy <= 2; ++dy)
		{
			for (int dx = -2; dx <= 2; ++dx)
			{
				if (dx == 0 && dy == 0)
					continue;
				if (!CivCity::IsCityRadiusOffset(dx, dy))
					continue;
				int tx = 0, ty = 0;
				tmp.WorkedTileAbs(map, dx, dy, tx, ty);
				consider(tx, ty, 0);
			}
		}
		return found;
	}

	// ---- empire phase ----------------------------------------------------------

	enum class Phase
	{
		Settle,  // 0 cities or all settlers out — found ASAP
		Expand,  // grow city count
		Develop, // infrastructure + military depth
	};

	Phase DetectPhase(const CivPlayer& player, const std::vector<CivUnitInstance>& units,
		CivDifficulty difficulty)
	{
		const int cities = player.CityCount();
		const int settlers = CountUnitsOfType(units, player.id, static_cast<int>(CivUnitId::Settlers))
			+ CountSettlersInProduction(player);
		const int softMax = CivAIMaxCitiesSoft(difficulty);

		if (cities == 0)
			return Phase::Settle;
		if (cities < std::min(softMax, 4 + static_cast<int>(difficulty)) && settlers > 0)
			return Phase::Expand;
		if (cities < softMax / 2)
			return Phase::Expand;
		return Phase::Develop;
	}
}

namespace CivAI
{
	bool ChooseResearch(CivPlayer& player, RNG& rng)
	{
		if (player.currentResearch >= 0)
			return false;
		std::vector<int> avail = CivAdvancesAvailable(player.advances);
		if (avail.empty())
			return false;

		// Priority spine: early infrastructure & military, then economy.
		static const CivAdvanceId kSpine[] = {
			CivAdvanceId::Pottery,           // granary
			CivAdvanceId::BronzeWorking,     // phalanx
			CivAdvanceId::CeremonialBurial,  // temple
			CivAdvanceId::Masonry,           // walls / palace tech
			CivAdvanceId::Alphabet,
			CivAdvanceId::CodeOfLaws,
			CivAdvanceId::MapMaking,
			CivAdvanceId::Writing,
			CivAdvanceId::HorsebackRiding,
			CivAdvanceId::TheWheel,
			CivAdvanceId::IronWorking,
			CivAdvanceId::Mathematics,
			CivAdvanceId::Currency,
			CivAdvanceId::Trade,
			CivAdvanceId::Construction,
			CivAdvanceId::Monarchy,
			CivAdvanceId::Literacy,
			CivAdvanceId::Philosophy,
			CivAdvanceId::TheRepublic,
			CivAdvanceId::Banking,
			CivAdvanceId::University,
			CivAdvanceId::Chivalry,
			CivAdvanceId::Engineering,
			CivAdvanceId::Invention,
			CivAdvanceId::Gunpowder,
			CivAdvanceId::Navigation,
			CivAdvanceId::Physics,
			CivAdvanceId::SteamEngine,
			CivAdvanceId::RailRoad,
			CivAdvanceId::Industrialization,
			CivAdvanceId::Conscription,
			CivAdvanceId::Automobile,
		};
		for (CivAdvanceId pref : kSpine)
		{
			const int id = static_cast<int>(pref);
			for (int a : avail)
				if (a == id)
				{
					player.currentResearch = id;
					return true;
				}
		}
		// Otherwise random among remaining (keeps variety late game).
		player.currentResearch = avail[static_cast<size_t>(rng.Random(static_cast<unsigned>(avail.size())))];
		return true;
	}

	void AdjustBudget(CivPlayer& player, CivDifficulty difficulty)
	{
		(void)difficulty;
		player.ClampBudgetRates();
		const int cities = player.CityCount();

		if (player.gold < 0)
		{
			// Emergency taxes.
			player.luxuriesRate = 0;
			player.taxesRate = 8;
			player.scienceRate = 2;
		}
		else if (player.gold < 20 || cities == 0)
		{
			player.luxuriesRate = 0;
			player.taxesRate = 5;
			player.scienceRate = 5;
		}
		else if (cities < 3)
		{
			// Early: push science a bit for Pottery/Bronze.
			player.luxuriesRate = 0;
			player.taxesRate = 4;
			player.scienceRate = 6;
		}
		else if (player.gold > 200)
		{
			// Comfortable: max science.
			player.luxuriesRate = 1;
			player.taxesRate = 2;
			player.scienceRate = 7;
		}
		else
		{
			player.luxuriesRate = 0;
			player.taxesRate = 3;
			player.scienceRate = 7;
		}
		player.ClampBudgetRates();
	}

	void ChooseCityProduction(CivPlayer& player, CivCity& city,
		const CivMapData& map, const std::vector<CivUnitInstance>& units,
		CivDifficulty difficulty, RNG& rng)
	{
		if (!city.Valid())
			return;

		// Don't thrash mid-build unless production became illegal.
		if (city.production.kind == CivProductionKind::Unit)
		{
			if (CivUnitAvailable(city.production.id, player.advances))
				return;
		}
		else if (city.production.kind == CivProductionKind::Building)
		{
			if (CivBuildingAvailable(city.production.id, player.advances)
				&& !city.HasBuilding(CivBuildingIdToFlag(city.production.id)))
				return;
		}

		const Phase phase = DetectPhase(player, units, difficulty);
		const CitySnap snap = SnapshotCity(player, city, map, units);
		const int defType = BestDefenderType(player);
		const int defHere = CountTypeOnTile(units, player.id, city.x, city.y, defType);
		const int anyDef = CountUnitsOnTile(units, player.id, city.x, city.y); // rough garrison size
		(void)anyDef;

		auto tryBld = [&](CivBuildingId id) -> bool {
			const int bid = static_cast<int>(id);
			if (!CivBuildingAvailable(bid, player.advances))
				return false;
			if (city.HasBuilding(CivBuildingIdToFlag(bid)))
				return false;
			// Avoid maintenance spiral.
			if (CivBuilding(bid).maintenance > 0 && player.gold < CivBuilding(bid).maintenance * 3)
				return false;
			city.SetProductionBuilding(bid);
			return true;
		};

		// --- Need 1: garrison of 2 modern defenders ---
		if (defHere < 2 && CivUnitAvailable(defType, player.advances))
		{
			city.SetProductionUnit(defType);
			return;
		}

		// --- Need 2: growth tools when food is the bottleneck ---
		if (snap.foodSurplus <= 0 && city.size < 8)
		{
			if (tryBld(CivBuildingId::Granary))
				return;
			// No granary tech yet → settlers only if we can still feed them later;
			// otherwise barracks is useless for food; fall through.
		}
		if (snap.foodSurplus > 0 && city.size >= 3 && city.size < 10)
		{
			// Healthy growth: granary is high ROI.
			if (tryBld(CivBuildingId::Granary))
				return;
		}

		// --- Need 3: happiness / order ---
		if (city.size >= 3 && tryBld(CivBuildingId::Temple))
			return;
		if (city.size >= 6 && tryBld(CivBuildingId::Colosseum))
			return;
		if (city.size >= 8 && tryBld(CivBuildingId::Cathedral))
			return;

		// --- Need 4: aqueduct gate ---
		if (city.size >= 8 && tryBld(CivBuildingId::Aqueduct))
			return;

		// --- Need 5: expansion settlers ---
		const int minSize = std::max(2, CivAIMinSizeForSettlers(difficulty));
		const int softMax = CivAIMaxCitiesSoft(difficulty);
		const int settlersField = CountUnitsOfType(units, player.id, static_cast<int>(CivUnitId::Settlers));
		const int settlersProd = CountSettlersInProduction(player);
		const int settlersOnCity = CountTypeOnTile(units, player.id, city.x, city.y,
			static_cast<int>(CivUnitId::Settlers));
		const int cities = player.CityCount();

		int settlersWanted = 0;
		if (phase == Phase::Settle || phase == Phase::Expand)
			settlersWanted = std::min(3, 1 + (softMax - cities) / 3);
		else
			settlersWanted = (cities < softMax) ? 1 : 0;
		// Also want a worker-settler when tiles need work and food allows.
		if (phase == Phase::Develop && snap.foodSurplus >= 2 && city.size >= minSize + 1)
			settlersWanted = std::max(settlersWanted, 1);

		if (city.size >= minSize
			&& settlersOnCity == 0
			&& settlersField + settlersProd < settlersWanted
			&& cities < softMax
			&& snap.foodSurplus >= 0
			&& CivUnitAvailable(static_cast<int>(CivUnitId::Settlers), player.advances))
		{
			city.SetProductionUnit(static_cast<int>(CivUnitId::Settlers));
			return;
		}

		// --- Need 6: economy / science buildings ---
		if (tryBld(CivBuildingId::Barracks))
			return;
		if (tryBld(CivBuildingId::Library))
			return;
		if (tryBld(CivBuildingId::MarketPlace))
			return;
		if (tryBld(CivBuildingId::CityWalls))
			return;
		if (city.size >= 5 && tryBld(CivBuildingId::Courthouse))
			return;
		if (tryBld(CivBuildingId::University))
			return;
		if (tryBld(CivBuildingId::Bank))
			return;
		if (tryBld(CivBuildingId::Factory))
			return;

		// --- Need 7: military / civilian extras ---
		const int unitsHere = CountUnitsOnTile(units, player.id, city.x, city.y);
		if (unitsHere < 4)
		{
			const bool peacefulGov = (player.government == CivGovernment::Republic
				|| player.government == CivGovernment::Democracy);
			if (peacefulGov
				&& player.HasAdvance(static_cast<int>(CivAdvanceId::Writing))
				&& CivUnitAvailable(static_cast<int>(CivUnitId::Diplomat), player.advances))
			{
				city.SetProductionUnit(static_cast<int>(CivUnitId::Diplomat));
				return;
			}
			const int atk = BestAttackType(player);
			if (atk >= 0 && CivUnitAvailable(atk, player.advances))
			{
				city.SetProductionUnit(atk);
				return;
			}
		}
		else if (player.HasAdvance(static_cast<int>(CivAdvanceId::Trade))
			&& CivUnitAvailable(static_cast<int>(CivUnitId::Caravan), player.advances)
			&& CountUnitsOfType(units, player.id, static_cast<int>(CivUnitId::Caravan)) < cities)
		{
			city.SetProductionUnit(static_cast<int>(CivUnitId::Caravan));
			return;
		}

		// --- Fallback: random legal build so we never soft-lock ---
		std::vector<int> optsU, optsB;
		for (int i = 0; i < CivUnitCount(); ++i)
			if (CivUnitAvailable(i, player.advances))
				optsU.push_back(i);
		for (int i = 0; i < CivBuildingCount(); ++i)
		{
			if (!CivBuildingAvailable(i, player.advances))
				continue;
			if (CivBuildingIdToFlag(i) == CivBld_None)
				continue;
			if (city.HasBuilding(CivBuildingIdToFlag(i)))
				continue;
			optsB.push_back(i);
		}
		const int n = static_cast<int>(optsU.size() + optsB.size());
		if (n > 0)
		{
			const int pick = static_cast<int>(rng.Random(static_cast<unsigned>(n)));
			if (pick < static_cast<int>(optsU.size()))
				city.SetProductionUnit(optsU[static_cast<size_t>(pick)]);
			else
				city.SetProductionBuilding(optsB[static_cast<size_t>(pick - optsU.size())]);
			return;
		}
		city.SetProductionUnit(defType);
	}

	// ---- founding --------------------------------------------------------------

	static bool TryFoundCity(CivPlayer& player, CivUnitInstance& unit, CivMapData& map,
		int& nextCityId, const std::vector<CivPlayer>* allPlayers)
	{
		if (!unit.IsSettlers() || unit.movesLeft <= 0 || unit.IsBuilding())
			return false;

		std::vector<const CivCity*> allCities;
		CollectAllCities(player, allPlayers, allCities);
		if (!IsValidCitySite(map, unit.x, unit.y, allCities))
			return false;

		// Only found if this is a *good* site, not merely legal — unless we have no cities.
		const int sc = ScoreCitySite(map, unit.x, unit.y, allCities, player.government);
		if (player.CityCount() > 0 && sc < 50)
			return false;

		const int cityId = nextCityId++;
		const bool isCapital = (player.CityCount() == 0);
		char nameBuf[64];
		if (isCapital)
			snprintf(nameBuf, sizeof(nameBuf), "%s", player.FactionDef().capital);
		else
			snprintf(nameBuf, sizeof(nameBuf), "%s %d", player.FactionDef().name, player.CityCount() + 1);

		CivCity city = CivCity::Found(cityId, unit.x, unit.y, player.id, nameBuf, isCapital);

		static const int cdx[] = { 0, 1, 0, -1 };
		static const int cdy[] = { -1, 0, 1, 0 };
		for (int i = 0; i < 4; ++i)
		{
			int nx = unit.x + cdx[i];
			const int ny = unit.y + cdy[i];
			if (!InMapY(map, ny))
				continue;
			WrapX(nx, map.width);
			if (map.TileAt(nx, ny).IsOcean())
			{
				city.coastal = true;
				break;
			}
		}

		CivTile& tile = map.TileAt(unit.x, unit.y);
		tile.owner = static_cast<int8_t>(player.id);
		tile.hut = false;
		if (tile.terrain == CivTerrain::Grassland || tile.terrain == CivTerrain::Plains
			|| tile.terrain == CivTerrain::River || tile.terrain == CivTerrain::Desert
			|| tile.terrain == CivTerrain::Hills)
			tile.SetImp(CivImp_Irrigation, true);
		tile.SetImp(CivImp_Road, true);

		city.AutoAssignWorkedTiles(map, player.government, nullptr);
		player.AddCity(std::move(city));

		unit.id = -1;
		unit.owner = -1;
		unit.ClearBuild();
		return true;
	}

	static bool TryImproveHere(CivPlayer& player, CivUnitInstance& unit, CivMapData& map,
		bool prioritizeFood, bool prioritizeShields)
	{
		if (!unit.IsSettlers() || unit.movesLeft <= 0 || unit.IsBuilding())
			return false;
		if (TileHasOwnCity(player, unit.x, unit.y))
			return false;
		if (NearestOwnCityDist(player, unit.x, unit.y, map.width) > 3)
			return false;

		// Pick the best job available on this tile by ROI.
		struct Opt { CivUnitInstance::BuildJob job; int turns; int score; };
		Opt best{ CivUnitInstance::BuildNone, 0, 0 };
		if (CanIrrigate(map, player, unit.x, unit.y))
		{
			const int sc = prioritizeFood ? 50 : 30;
			if (sc > best.score)
				best = { CivUnitInstance::BuildIrrigation, 3, sc };
		}
		if (CanMine(map, player, unit.x, unit.y))
		{
			const int sc = prioritizeShields ? 45 : 25;
			if (sc > best.score)
				best = { CivUnitInstance::BuildMine, 4, sc };
		}
		if (CanBuildRoad(map, player, unit.x, unit.y) && !map.TileAt(unit.x, unit.y).HasRoad())
		{
			if (18 > best.score)
				best = { CivUnitInstance::BuildRoad, 2, 18 };
		}
		else if (CanBuildRoad(map, player, unit.x, unit.y) && map.TileAt(unit.x, unit.y).HasRoad())
		{
			// RR upgrade
			if (12 > best.score)
				best = { CivUnitInstance::BuildRoad, 3, 12 };
		}
		if (best.job == CivUnitInstance::BuildNone)
			return false;
		StartBuild(unit, best.job, best.turns);
		return true;
	}

	static void MoveMilitary(CivPlayer& player, CivUnitInstance& unit, CivMapData& map,
		const std::vector<CivUnitInstance>& units, RNG& rng)
	{
		if (unit.movesLeft <= 0)
			return;
		unit.fortify = false;

		// Explore fog frontier if any.
		if (player.HasFog())
		{
			int bestX = unit.x, bestY = unit.y, bestSc = -1;
			for (int dy = -6; dy <= 6; ++dy)
			{
				for (int dx = -6; dx <= 6; ++dx)
				{
					int x = unit.x + dx;
					const int y = unit.y + dy;
					if (!InMapY(map, y))
						continue;
					WrapX(x, map.width);
					if (map.TileAt(x, y).IsOcean())
						continue;
					if (player.IsExplored(x, y))
						continue;
					// Prefer nearer unexplored.
					const int dist = CivTileDistance(unit.x, unit.y, x, y, map.width);
					const int sc = 100 - dist;
					if (sc > bestSc)
					{
						bestSc = sc;
						bestX = x;
						bestY = y;
					}
				}
			}
			if (bestSc >= 0)
			{
				unit.gotoX = bestX;
				unit.gotoY = bestY;
			}
		}

		if (!unit.HasGoto())
		{
			for (int a = 0; a < 16; ++a)
			{
				const int gx = unit.x + static_cast<int>(rng.Random(13)) - 6;
				const int gy = unit.y + static_cast<int>(rng.Random(13)) - 6;
				if (!InMapY(map, gy))
					continue;
				int nx = gx;
				WrapX(nx, map.width);
				if (map.TileAt(nx, gy).IsOcean())
					continue;
				unit.gotoX = nx;
				unit.gotoY = gy;
				break;
			}
		}

		if (unit.HasGoto())
		{
			if (!StepToward(unit, map, unit.gotoX, unit.gotoY, player, &units))
			{
				unit.ClearGoto();
				unit.movesLeft = 0;
			}
			else if (unit.x == unit.gotoX && unit.y == unit.gotoY)
				unit.ClearGoto();
		}
		else
			unit.movesLeft = 0;
	}

	std::string PlayTurn(CivPlayer& player, CivMapData& map,
		std::vector<CivUnitInstance>& units, int& nextUnitId, int& nextCityId,
		CivDifficulty difficulty, RNG& rng,
		std::vector<CivPlayer>* allPlayers)
	{
		(void)nextUnitId;
		if (!player.IsAI())
			return {};

		AdjustBudget(player, difficulty);
		ChooseResearch(player, rng);

		const Phase phase = DetectPhase(player, units, difficulty);
		int founded = 0;
		int improved = 0;

		for (CivCity& city : player.cities)
		{
			if (!city.Valid())
				continue;
			ChooseCityProduction(player, city, map, units, difficulty, rng);
		}

		// Precompute city food stress for settler improvement priorities.
		bool empireHungry = false;
		for (const CivCity& c : player.cities)
		{
			if (!c.Valid())
				continue;
			const CitySnap s = SnapshotCity(player, c, map, units);
			if (s.foodSurplus <= 0)
				empireHungry = true;
		}

		for (CivUnitInstance& u : units)
		{
			if (!u.Valid() || u.owner != player.id)
				continue;

			if (u.IsSettlers() && u.IsBuilding())
			{
				if (ProgressBuildJob(u, map, player))
				{
					++improved;
					continue;
				}
			}

			u.ResetMoves();

			// Defenders: fortify in cities, march home if stranded, cull excess militia.
			if (u.IsDefender())
			{
				bool onCity = TileHasOwnCity(player, u.x, u.y);
				if (onCity)
				{
					u.fortify = true;
					u.movesLeft = 0;
					const int defType = BestDefenderType(player);
					const int modern = CountTypeOnTile(units, player.id, u.x, u.y, defType);
					if (modern >= 2 && u.typeId == static_cast<int>(CivUnitId::Militia)
						&& defType != static_cast<int>(CivUnitId::Militia))
					{
						u.id = -1;
						u.owner = -1;
					}
				}
				else if (const CivCity* home = NearestOwnCity(player, u.x, u.y, map.width))
					StepToward(u, map, home->x, home->y, player, &units);
				else
					u.movesLeft = 0;
				continue;
			}

			if (u.IsSettlers())
			{
				u.fortify = false;
				std::vector<const CivCity*> allCities;
				CollectAllCities(player, allPlayers, allCities);

				// Found if standing on a good site.
				const int sx = u.x, sy = u.y;
				if (TryFoundCity(player, u, map, nextCityId, allPlayers))
				{
					++founded;
					if (!player.cities.empty())
					{
						const int newId = player.cities.back().id;
						for (CivUnitInstance& o : units)
							if (o.Valid() && o.owner == player.id && o.x == sx && o.y == sy && o.homeCityId < 0)
								o.homeCityId = newId;
					}
					continue;
				}

				// Improve current tile if high ROI near a city.
				const bool pFood = empireHungry;
				const bool pShields = !empireHungry;
				if (TryImproveHere(player, u, map, pFood, pShields))
				{
					++improved;
					continue;
				}

				// Path to best city site if we still need cities.
				const int cities = player.CityCount();
				const int softMax = CivAIMaxCitiesSoft(difficulty);
				if (cities < softMax && (phase == Phase::Settle || phase == Phase::Expand || cities < softMax / 2))
				{
					int tx = u.x, ty = u.y;
					if (FindBestCitySiteNear(map, player, allCities, u.x, u.y, 12, tx, ty))
					{
						if (tx == u.x && ty == u.y)
						{
							// Site is here but score gate failed — force-found if no cities.
							if (cities == 0 && IsValidCitySite(map, u.x, u.y, allCities))
							{
								// Temporarily lower score gate by founding via size-0 path:
								// already handled by TryFoundCity for cities==0 with sc check.
							}
						}
						else
						{
							StepToward(u, map, tx, ty, player, &units);
							if (u.Valid() && TryFoundCity(player, u, map, nextCityId, allPlayers))
							{
								++founded;
								continue;
							}
							if (u.Valid() && TryImproveHere(player, u, map, pFood, pShields))
								++improved;
							continue;
						}
					}
				}

				// Worker mode: go improve a needy BFC tile of nearest city.
				if (const CivCity* home = NearestOwnCity(player, u.x, u.y, map.width))
				{
					int tx = u.x, ty = u.y;
					if (FindBestImproveTile(map, player, *home, pFood, pShields, tx, ty))
					{
						if (tx == u.x && ty == u.y)
						{
							if (TryImproveHere(player, u, map, pFood, pShields))
								++improved;
							else
								u.movesLeft = 0;
						}
						else
						{
							StepToward(u, map, tx, ty, player, &units);
							if (u.Valid() && u.x == tx && u.y == ty
								&& TryImproveHere(player, u, map, pFood, pShields))
								++improved;
						}
						continue;
					}
				}

				// Explore if nothing better.
				MoveMilitary(player, u, map, units, rng);
				if (u.Valid() && TryFoundCity(player, u, map, nextCityId, allPlayers))
					++founded;
				continue;
			}

			// Attack / civilian units: explore & patrol.
			MoveMilitary(player, u, map, units, rng);
		}

		units.erase(std::remove_if(units.begin(), units.end(),
			[](const CivUnitInstance& u) { return !u.Valid(); }),
			units.end());

		CivRebuildPlayerVisibility(player, map, kCivCityVisionRange);
		for (const auto& u : units)
			if (u.Valid() && u.owner == player.id)
				CivAddVision(player, u.x, u.y, kCivUnitVisionRange, map.width, map.height);

		const char* phaseName = phase == Phase::Settle ? "settle"
			: phase == Phase::Expand ? "expand" : "develop";
		char buf[192];
		snprintf(buf, sizeof(buf), "%s [%s]: res=%s cities=%d found=%d imp=%d",
			player.tribeName.c_str(), phaseName,
			player.currentResearch >= 0 ? CivAdvanceName(player.currentResearch) : "—",
			player.CityCount(), founded, improved);
		return std::string(buf);
	}
}
