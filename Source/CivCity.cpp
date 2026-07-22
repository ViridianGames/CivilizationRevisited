#include "CivCity.h"

#include "CivBuildings.h"

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Food costs / income
// ---------------------------------------------------------------------------

int CivCity::FoodCosts(int homeSettlers, bool despoticGovernment) const
{
	int costs = size * 2;
	if (homeSettlers < 0)
		homeSettlers = 0;
	// Anarchy/Despotism: settlers cost 1 food; Monarchy+ : 2 food each.
	costs += despoticGovernment ? homeSettlers : (homeSettlers * 2);
	return costs;
}

int CivCity::FoodIncome(int foodFromTiles, int homeSettlers, bool despoticGovernment) const
{
	return foodFromTiles - FoodCosts(homeSettlers, despoticGovernment);
}

void CivCity::ClampFoodToRequired()
{
	const int req = FoodRequired();
	if (food > req)
		food = req;
	if (food < 0)
		food = 0;
}

bool CivCity::SetSize(int newSize)
{
	if (newSize <= 0)
	{
		size = 0;
		food = 0;
		workedTiles.clear();
		destroyed = true;
		return false;
	}
	size = newSize;
	ClampFoodToRequired();
	// Cap worked non-center tiles at size (CivOne allows up to Size extras).
	if (static_cast<int>(workedTiles.size()) > size)
		workedTiles.resize(static_cast<size_t>(size));
	return true;
}

// ---------------------------------------------------------------------------
// Growth / famine / granary / aqueduct / We Love the President
// Matches CivOne City.NewTurn food section.
// ---------------------------------------------------------------------------

CivCityFoodTurnResult CivCity::ProcessFoodTurn(const CivCityFoodTurnParams& params)
{
	CivCityFoodTurnResult r;
	r.sizeBefore = size;
	r.foodBefore = food;
	r.foodRequiredBefore = FoodRequired();

	if (!Valid())
	{
		r.sizeAfter = size;
		r.foodAfter = food;
		r.foodRequiredAfter = FoodRequired();
		return r;
	}

	// --- We Love the President Day (Republic / Democracy only) ---
	// CivOne: if Unhappy==0 && Happy>=Content && Size>=3 && Food>0 → Size++.
	// Runs before food income is applied. No aqueduct check (can pass size 10).
	if (params.allowWeLovePresident && QualifiesWeLovePresident())
	{
		SetSize(size + 1);
		r.events |= CivFoodEvt_WeLovePresident;
		r.events |= CivFoodEvt_Grew;
	}

	// --- Accumulate surplus ---
	const int income = FoodIncome(params.foodFromTiles, params.homeSettlers,
		params.despoticGovernment);
	r.foodIncome = income;

	const bool disorder = params.inDisorder || InDisorder();
	const int applied = disorder ? 0 : income;
	r.foodApplied = applied;
	food += applied;

	// --- Famine ---
	if (food < 0)
	{
		food = 0;
		r.events |= CivFoodEvt_Starved;
		if (!SetSize(size - 1))
		{
			r.events |= CivFoodEvt_Destroyed;
			r.sizeAfter = 0;
			r.foodAfter = 0;
			r.foodRequiredAfter = 0;
			return r;
		}
	}
	// --- Growth when storage exceeds the food box ---
	// CivOne uses strict `Food > FoodRequired` (not >=).
	else if (food > FoodRequired())
	{
		// Deduct a full box at the *current* size requirement.
		food -= FoodRequired();

		if (NeedsAqueductToGrow())
		{
			// Food already spent; size stays at 10. Surplus wasted each overflow.
			r.events |= CivFoodEvt_AqueductBlocked;
		}
		else
		{
			SetSize(size + 1);
			r.events |= CivFoodEvt_Grew;
		}

		// Granary: after growth (or blocked attempt), ensure food is at least
		// half of the *current* FoodRequired (new box if size changed).
		// CivOne: if (has Granary && Food < FoodRequired/2) Food = FoodRequired/2.
		if (HasBuilding(CivBld_Granary))
		{
			const int half = FoodRequired() / 2;
			if (food < half)
			{
				food = half;
				r.events |= CivFoodEvt_GranaryApplied;
			}
		}
	}

	if (food < 0)
		food = 0;

	r.sizeAfter = size;
	r.foodAfter = food;
	r.foodRequiredAfter = FoodRequired();
	return r;
}

// ---------------------------------------------------------------------------
// Settlers completion
// ---------------------------------------------------------------------------

CivCitySettlerBuildResult CivCity::CompleteSettlersBuild(bool soleCity, bool chieftainDifficulty)
{
	CivCitySettlerBuildResult r;
	r.sizeBefore = size;

	if (!Valid())
	{
		r.sizeAfter = size;
		return r;
	}

	// CivOne: Chieftain cannot produce Settlers in a size-1 city.
	if (chieftainDifficulty && size == 1)
	{
		r.built = false;
		r.sizeAfter = size;
		return r;
	}

	// Sole city at size 1: bump so the -1 does not destroy the last city.
	if (size == 1 && soleCity)
		SetSize(size + 1);

	if (size == 1)
	{
		// Would destroy city — still allowed if not sole city; city dies.
		// (CivOne sets unit home null when size becomes 1 before decrement.)
	}

	r.built = true;
	if (!SetSize(size - 1))
	{
		r.cityDestroyed = true;
		r.sizeAfter = 0;
		return r;
	}

	r.sizeAfter = size;
	// Shields/production cleared by caller after successful build.
	return r;
}

bool CivCity::CompleteBuilding(int buildingId)
{
	if (buildingId < 0 || buildingId >= static_cast<int>(CivBuildingId::Count))
		return false;
	const CivCityBuilding flag = CivBuildingIdToFlag(buildingId);
	if (flag != CivBld_None)
		SetBuilding(flag, true);
	// Buildings without a dedicated city bit (Mass Transit, SDI, etc.) still
	// complete production; full building list storage can expand later.
	ClearProduction();
	return true;
}

// ---------------------------------------------------------------------------
// City radius / yields
// ---------------------------------------------------------------------------

bool CivCity::IsCityRadiusOffset(int dx, int dy)
{
	if (dx < -2 || dx > 2 || dy < -2 || dy > 2)
		return false;
	// Corners of the 5×5 are outside the classic fat cross.
	if ((dx == -2 || dx == 2) && (dy == -2 || dy == 2))
		return false;
	return true;
}

void CivCity::WorkedTileAbs(const CivMapData& map, int dx, int dy, int& outX, int& outY) const
{
	outX = map.WrapX(x + dx);
	outY = y + dy;
}

CivYields CivCity::TileYieldAt(const CivMapData& map, int tileX, int tileY,
	CivGovernment government, bool cityCenter)
{
	if (!map.InBounds(tileX, tileY))
		return {};

	const CivTile& t = map.TileAt(tileX, tileY);
	CivTileImprovements imp;
	imp.irrigation = t.HasIrrigation();
	imp.mine = t.HasMine();
	imp.road = t.HasRoad();
	imp.railRoad = t.HasRail();

	// City square: treated as irrigated and roaded for yield purposes.
	if (cityCenter)
	{
		imp.irrigation = true;
		imp.road = true;
	}

	// Grassland shield / special lattice both count as "special" for yields.
	const bool special = t.special || (t.terrain == CivTerrain::Grassland && t.grassland2);

	return CivComputeTileYields(static_cast<int>(t.terrain), special, imp, government);
}

CivYields CivCity::ComputeWorkedYields(const CivMapData& map, CivGovernment government) const
{
	CivYields total = TileYieldAt(map, x, y, government, true);

	for (const CivWorkedTile& w : workedTiles)
	{
		if (w.dx == 0 && w.dy == 0)
			continue;
		if (!IsCityRadiusOffset(w.dx, w.dy))
			continue;
		int tx = 0, ty = 0;
		WorkedTileAbs(map, w.dx, w.dy, tx, ty);
		if (!map.InBounds(tx, ty))
			continue;
		const CivYields y = TileYieldAt(map, tx, ty, government, false);
		total.food += y.food;
		total.shields += y.shields;
		total.trade += y.trade;
	}
	return total;
}

void CivCity::AutoAssignWorkedTiles(const CivMapData& map, CivGovernment government,
	const std::vector<std::pair<int, int>>* occupiedAbs)
{
	if (!Valid())
	{
		workedTiles.clear();
		return;
	}

	auto isOccupied = [&](int tx, int ty) -> bool
	{
		if (!occupiedAbs)
			return false;
		for (const auto& p : *occupiedAbs)
			if (p.first == tx && p.second == ty)
				return true;
		return false;
	};

	// Drop invalid / out-of-radius / occupied assignments.
	{
		std::vector<CivWorkedTile> keep;
		keep.reserve(workedTiles.size());
		for (const CivWorkedTile& w : workedTiles)
		{
			if (w.dx == 0 && w.dy == 0)
				continue;
			if (!IsCityRadiusOffset(w.dx, w.dy))
				continue;
			int tx = 0, ty = 0;
			WorkedTileAbs(map, w.dx, w.dy, tx, ty);
			if (!map.InBounds(tx, ty))
				continue;
			if (isOccupied(tx, ty))
				continue;
			keep.push_back(w);
		}
		workedTiles.swap(keep);
	}

	// Cap at size: one additional BFC tile per population point (center free).
	if (static_cast<int>(workedTiles.size()) > size)
		workedTiles.resize(static_cast<size_t>(size));

	// Fill vacancies with best remaining BFC tiles (food > shields > trade).
	while (static_cast<int>(workedTiles.size()) < size)
	{
		int bestDx = 0, bestDy = 0;
		CivYields bestY{ -1, -1, -1 };
		bool found = false;

		for (int dy = -2; dy <= 2; ++dy)
		{
			for (int dx = -2; dx <= 2; ++dx)
			{
				if (dx == 0 && dy == 0)
					continue;
				if (!IsCityRadiusOffset(dx, dy))
					continue;

				// Already worked?
				bool already = false;
				for (const CivWorkedTile& w : workedTiles)
					if (w.dx == dx && w.dy == dy)
					{
						already = true;
						break;
					}
				if (already)
					continue;

				int tx = 0, ty = 0;
				WorkedTileAbs(map, dx, dy, tx, ty);
				if (!map.InBounds(tx, ty))
					continue;
				if (isOccupied(tx, ty))
					continue;

				const CivYields y = TileYieldAt(map, tx, ty, government, false);
				if (!found
					|| y.food > bestY.food
					|| (y.food == bestY.food && y.shields > bestY.shields)
					|| (y.food == bestY.food && y.shields == bestY.shields && y.trade > bestY.trade))
				{
					found = true;
					bestY = y;
					bestDx = dx;
					bestDy = dy;
				}
			}
		}

		if (!found)
			break;

		CivWorkedTile w;
		w.dx = static_cast<int8_t>(bestDx);
		w.dy = static_cast<int8_t>(bestDy);
		workedTiles.push_back(w);
	}
}

CivCityFoodTurnResult CivCity::ProcessFoodTurnFromMap(
	const CivMapData& map,
	CivGovernment government,
	int homeSettlers,
	bool inDisorder,
	const std::vector<std::pair<int, int>>* occupiedAbs)
{
	AutoAssignWorkedTiles(map, government, occupiedAbs);

	const CivYields y = ComputeWorkedYields(map, government);

	const bool despotic =
		government == CivGovernment::Anarchy || government == CivGovernment::Despotism;
	const bool weLove =
		government == CivGovernment::Republic || government == CivGovernment::Democracy;

	CivCityFoodTurnParams p;
	p.foodFromTiles = y.food;
	p.homeSettlers = homeSettlers;
	p.despoticGovernment = despotic;
	p.allowWeLovePresident = weLove;
	p.inDisorder = inDisorder;
	return ProcessFoodTurn(p);
}

// ---------------------------------------------------------------------------
// Trade → taxes / luxuries / science (CivOne City.cs)
// ---------------------------------------------------------------------------

namespace
{
	// .NET MidpointRounding.AwayFromZero for non-negative values used here.
	int RoundAwayFromZero(double x)
	{
		if (x >= 0.0)
			return static_cast<int>(std::floor(x + 0.5));
		return static_cast<int>(std::ceil(x - 0.5));
	}

	// Sequential +50% floors: market then bank (or library then university).
	int ApplyHalfBonus(int base, bool first, bool second)
	{
		int v = base;
		if (first)
			v += static_cast<int>(std::floor(static_cast<double>(v) * 0.5));
		if (second)
			v += static_cast<int>(std::floor(static_cast<double>(v) * 0.5));
		return v;
	}

	// Buildings we store as city bitflags → maintenance gold.
	struct BldMaint
	{
		CivCityBuilding flag;
		CivBuildingId id;
	};

	const BldMaint kMaintTable[] = {
		{ CivBld_Palace,      CivBuildingId::Palace },
		{ CivBld_Barracks,    CivBuildingId::Barracks },
		{ CivBld_Granary,     CivBuildingId::Granary },
		{ CivBld_Temple,      CivBuildingId::Temple },
		{ CivBld_MarketPlace, CivBuildingId::MarketPlace },
		{ CivBld_Library,     CivBuildingId::Library },
		{ CivBld_Courthouse,  CivBuildingId::Courthouse },
		{ CivBld_CityWalls,   CivBuildingId::CityWalls },
		{ CivBld_Aqueduct,    CivBuildingId::Aqueduct },
		{ CivBld_Bank,        CivBuildingId::Bank },
		{ CivBld_Cathedral,   CivBuildingId::Cathedral },
		{ CivBld_University,  CivBuildingId::University },
		{ CivBld_Colosseum,   CivBuildingId::Colosseum },
		{ CivBld_Factory,     CivBuildingId::Factory },
		{ CivBld_PowerPlant,  CivBuildingId::PowerPlant },
		{ CivBld_HydroPlant,  CivBuildingId::HydroPlant },
		{ CivBld_NuclearPlant,CivBuildingId::NuclearPlant },
		{ CivBld_SSStructural,CivBuildingId::SSStructural },
		{ CivBld_SSComponent, CivBuildingId::SSComponent },
		{ CivBld_SSModule,    CivBuildingId::SSModule },
	};
}

int CivCorruptionMultiplier(CivGovernment government)
{
	// CivOne Governments/*.cs CorruptionMultiplier values.
	switch (government)
	{
	case CivGovernment::Anarchy:    return 12;
	case CivGovernment::Despotism:  return 8;
	case CivGovernment::Monarchy:   return 16;
	case CivGovernment::Communism:  return 20;
	case CivGovernment::Republic:   return 24;
	case CivGovernment::Democracy:  return 0; // no corruption
	default:                        return 8;
	}
}

int CivMapDistance(int x1, int y1, int x2, int y2, int mapWidth)
{
	int dx = std::abs(x2 - x1);
	if (mapWidth > 0)
	{
		// Cylindrical wrap: shortest horizontal distance.
		if (dx > mapWidth - dx)
			dx = mapWidth - dx;
	}
	const int dy = std::abs(y2 - y1);
	return std::max(dx, dy);
}

int CivCity::TotalMaintenance() const
{
	int total = 0;
	for (const BldMaint& e : kMaintTable)
	{
		if (HasBuilding(e.flag))
			total += CivBuilding(e.id).maintenance;
	}
	return total;
}

int CivCity::ComputeCorruption(int rawTrade, CivGovernment government,
	int distanceToCapital, bool hasCapitalInEmpire) const
{
	const int mult = CivCorruptionMultiplier(government);
	if (mult == 0 || rawTrade <= 0)
		return 0;

	int distance = distanceToCapital;
	if (government == CivGovernment::Communism)
	{
		// Fixed distance for all cities under Communism.
		distance = 10;
	}
	else
	{
		// Palace city has no corruption (non-communism).
		if (HasBuilding(CivBld_Palace))
			return 0;
		if (!hasCapitalInEmpire)
			distance = 32;
	}

	if (distance < 0)
		distance = 0;

	// corruption = round(rawTrade * distance * 3 / (10 * mult))
	const double num = static_cast<double>(rawTrade) * static_cast<double>(distance) * 3.0;
	const double den = 10.0 * static_cast<double>(mult);
	int corruption = RoundAwayFromZero(num / den);

	// Courthouse halves; under Communism the Palace also halves.
	if (HasBuilding(CivBld_Courthouse)
		|| (HasBuilding(CivBld_Palace) && government == CivGovernment::Communism))
	{
		corruption /= 2;
	}

	if (corruption < 0)
		corruption = 0;
	if (corruption > rawTrade)
		corruption = rawTrade;
	return corruption;
}

CivCityTradeBreakdown CivCity::ComputeTradeBreakdown(const CivCityTradeTurnParams& params) const
{
	CivCityTradeBreakdown b;
	b.rawTrade = std::max(0, params.rawTrade);
	b.corruption = ComputeCorruption(b.rawTrade, params.government,
		params.distanceToCapital, params.hasCapitalInEmpire);
	b.tradeTotal = b.rawTrade - b.corruption;
	if (b.tradeTotal < 0)
		b.tradeTotal = 0;

	// Rates are 0..10 and should sum to 10 (ClampBudgetRates).
	int taxesRate = params.budget.taxesRate;
	int luxRate = params.budget.luxuriesRate;
	if (taxesRate < 0) taxesRate = 0;
	if (taxesRate > 10) taxesRate = 10;
	if (luxRate < 0) luxRate = 0;
	if (luxRate > 10) luxRate = 10;
	// Keep lux within non-tax pie.
	if (luxRate > 10 - taxesRate)
		luxRate = 10 - taxesRate;

	// CivOne:
	//   TradeTaxes    = Round(TradeTotal / 10 * TaxesRate)
	//   TradeLuxuries = Round((TradeTotal - TradeTaxes) / (10 - TaxesRate) * LuxuriesRate)
	//   TradeScience  = TradeTotal - TradeLuxuries - TradeTaxes
	if (b.tradeTotal == 0)
	{
		b.baseTaxes = b.baseLuxuries = b.baseScience = 0;
	}
	else if (taxesRate >= 10)
	{
		b.baseTaxes = b.tradeTotal;
		b.baseLuxuries = 0;
		b.baseScience = 0;
	}
	else
	{
		b.baseTaxes = RoundAwayFromZero(
			(static_cast<double>(b.tradeTotal) / 10.0) * static_cast<double>(taxesRate));
		if (b.baseTaxes > b.tradeTotal)
			b.baseTaxes = b.tradeTotal;

		const int afterTax = b.tradeTotal - b.baseTaxes;
		if (taxesRate == 10 || luxRate <= 0 || afterTax <= 0)
		{
			b.baseLuxuries = 0;
		}
		else
		{
			// (10 - taxesRate) is the non-tax rate pie (lux + science).
			b.baseLuxuries = RoundAwayFromZero(
				(static_cast<double>(afterTax) / static_cast<double>(10 - taxesRate))
				* static_cast<double>(luxRate));
			if (b.baseLuxuries > afterTax)
				b.baseLuxuries = afterTax;
		}
		b.baseScience = b.tradeTotal - b.baseTaxes - b.baseLuxuries;
		if (b.baseScience < 0)
			b.baseScience = 0;
	}

	// Building / specialist / wonder bonuses (CivOne Luxuries / Taxes / Science getters).
	int taxes = ApplyHalfBonus(b.baseTaxes,
		HasBuilding(CivBld_MarketPlace), HasBuilding(CivBld_Bank));
	taxes += std::max(0, params.taxmen) * 2;

	int luxuries = ApplyHalfBonus(b.baseLuxuries,
		HasBuilding(CivBld_MarketPlace), HasBuilding(CivBld_Bank));
	luxuries += std::max(0, params.entertainers) * 2;

	int science = ApplyHalfBonus(b.baseScience,
		HasBuilding(CivBld_Library), HasBuilding(CivBld_University));
	// Copernicus: +100% (floor(science * 1.0) added → doubles).
	const bool copernicus = params.hasCopernicus || HasWonder(CivWonder_CopernicusObservatory);
	if (copernicus)
		science += static_cast<int>(std::floor(static_cast<double>(science) * 1.0));
	// SETI: +50%.
	if (params.empireHasSeti)
		science += static_cast<int>(std::floor(static_cast<double>(science) * 0.5));
	science += std::max(0, params.scientists) * 2;

	// Disorder: taxes not collected (CivOne NewTurn). Science still produced.
	b.taxes = params.inDisorder ? 0 : taxes;
	b.luxuries = luxuries; // used for happiness; still "produced" in disorder
	b.science = science;
	b.maintenance = TotalMaintenance();
	return b;
}

CivCityTradeBreakdown CivCity::ComputeTradeBreakdownFromMap(
	const CivMapData& map,
	const CivBudgetRates& budget,
	CivGovernment government,
	int distanceToCapital,
	bool hasCapitalInEmpire,
	bool inDisorder,
	bool empireHasSeti,
	int entertainers,
	int taxmen,
	int scientists) const
{
	const CivYields y = ComputeWorkedYields(map, government);

	CivCityTradeTurnParams p;
	p.rawTrade = y.trade;
	p.budget = budget;
	p.government = government;
	p.distanceToCapital = distanceToCapital;
	p.hasCapitalInEmpire = hasCapitalInEmpire;
	p.inDisorder = inDisorder;
	p.entertainers = entertainers;
	p.taxmen = taxmen;
	p.scientists = scientists;
	p.empireHasSeti = empireHasSeti;
	p.hasCopernicus = HasWonder(CivWonder_CopernicusObservatory);
	return ComputeTradeBreakdown(p);
}

CivCityTradeBreakdown CivCity::ProcessTradeTurnFromMap(
	const CivMapData& map,
	const CivBudgetRates& budget,
	CivGovernment government,
	int distanceToCapital,
	bool hasCapitalInEmpire,
	bool inDisorder,
	bool empireHasSeti,
	const std::vector<std::pair<int, int>>* occupiedAbs,
	int entertainers,
	int taxmen,
	int scientists)
{
	AutoAssignWorkedTiles(map, government, occupiedAbs);
	return ComputeTradeBreakdownFromMap(map, budget, government, distanceToCapital,
		hasCapitalInEmpire, inDisorder, empireHasSeti, entertainers, taxmen, scientists);
}

// ---------------------------------------------------------------------------
// Building id → city bitflag
// ---------------------------------------------------------------------------

CivCityBuilding CivBuildingIdToFlag(int buildingId)
{
	switch (static_cast<CivBuildingId>(buildingId))
	{
	case CivBuildingId::Palace:         return CivBld_Palace;
	case CivBuildingId::Barracks:       return CivBld_Barracks;
	case CivBuildingId::Granary:        return CivBld_Granary;
	case CivBuildingId::Temple:         return CivBld_Temple;
	case CivBuildingId::MarketPlace:    return CivBld_MarketPlace;
	case CivBuildingId::Library:        return CivBld_Library;
	case CivBuildingId::Courthouse:     return CivBld_Courthouse;
	case CivBuildingId::CityWalls:      return CivBld_CityWalls;
	case CivBuildingId::Aqueduct:       return CivBld_Aqueduct;
	case CivBuildingId::Bank:           return CivBld_Bank;
	case CivBuildingId::Cathedral:      return CivBld_Cathedral;
	case CivBuildingId::University:     return CivBld_University;
	case CivBuildingId::Colosseum:      return CivBld_Colosseum;
	case CivBuildingId::Factory:        return CivBld_Factory;
	case CivBuildingId::PowerPlant:     return CivBld_PowerPlant;
	case CivBuildingId::HydroPlant:     return CivBld_HydroPlant;
	case CivBuildingId::NuclearPlant:   return CivBld_NuclearPlant;
	case CivBuildingId::SSStructural:   return CivBld_SSStructural;
	case CivBuildingId::SSComponent:    return CivBld_SSComponent;
	case CivBuildingId::SSModule:       return CivBld_SSModule;
	default:                            return CivBld_None;
	}
}
