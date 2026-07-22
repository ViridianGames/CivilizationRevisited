#ifndef _CIVCITY_H_
#define _CIVCITY_H_

#include "CivTerrainYields.h"
#include "CivTile.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Building ids we care about early (expand as buildings are added).
// Bitflags keep the object compact until a full building list exists.
enum CivCityBuilding : uint32_t
{
	CivBld_None        = 0,
	CivBld_Palace      = 1u << 0,  // capital
	CivBld_Barracks    = 1u << 1,
	CivBld_Granary     = 1u << 2,
	CivBld_Temple      = 1u << 3,
	CivBld_MarketPlace = 1u << 4,
	CivBld_Library     = 1u << 5,
	CivBld_Courthouse  = 1u << 6,
	CivBld_CityWalls   = 1u << 7,  // fortified look on map
	CivBld_Aqueduct    = 1u << 8,
	CivBld_Bank        = 1u << 9,
	CivBld_Cathedral   = 1u << 10,
	CivBld_University  = 1u << 11,
	CivBld_Colosseum   = 1u << 12,
	CivBld_Factory     = 1u << 13,
	CivBld_PowerPlant  = 1u << 14,
	CivBld_HydroPlant  = 1u << 15,
	CivBld_NuclearPlant= 1u << 16,
	CivBld_SSStructural= 1u << 17,
	CivBld_SSComponent = 1u << 18,
	CivBld_SSModule    = 1u << 19,
};

// What the city is currently producing (unit or building). Expand later.
enum class CivProductionKind : uint8_t
{
	None = 0,
	Unit,
	Building,
	Wonder,
};

struct CivProduction
{
	CivProductionKind kind = CivProductionKind::None;
	int id = -1; // unit type id, building id, or wonder id
};

// Outcome flags for one food/growth turn (can combine).
enum CivCityFoodEvent : uint32_t
{
	CivFoodEvt_None             = 0,
	CivFoodEvt_Grew             = 1u << 0,
	CivFoodEvt_Starved          = 1u << 1,
	CivFoodEvt_AqueductBlocked  = 1u << 2,
	CivFoodEvt_GranaryApplied   = 1u << 3,
	CivFoodEvt_WeLovePresident  = 1u << 4,
	CivFoodEvt_Destroyed        = 1u << 5,
};

// Result of ProcessFoodTurn — UI / logs can read this without re-deriving state.
struct CivCityFoodTurnResult
{
	uint32_t events = CivFoodEvt_None;
	int foodIncome = 0;       // surplus (can be negative) before disorder zeroing
	int foodApplied = 0;      // amount actually added to storage (0 if disorder)
	int foodBefore = 0;
	int foodAfter = 0;
	int sizeBefore = 0;
	int sizeAfter = 0;
	int foodRequiredBefore = 0;
	int foodRequiredAfter = 0;

	bool Has(CivCityFoodEvent e) const { return (events & e) != 0; }
};

// Inputs for a city food turn that live outside the city object.
struct CivCityFoodTurnParams
{
	// Sum of FoodValue over worked tiles (center + assigned). Caller or helper fills this.
	int foodFromTiles = 0;
	// Settlers with this city as home (for food upkeep).
	int homeSettlers = 0;
	// Anarchy / Despotism → settlers cost 1 food; other govs cost 2.
	bool despoticGovernment = true;
	// Republic / Democracy: free size+1 when celebration conditions hold.
	bool allowWeLovePresident = false;
	// Civil disorder: no food income this turn (still check storage / famine from prior).
	bool inDisorder = false;
};

// Result of completing a Settlers build.
struct CivCitySettlerBuildResult
{
	bool built = false;       // false if blocked (Chieftain + size 1)
	bool cityDestroyed = false;
	int sizeBefore = 0;
	int sizeAfter = 0;
};

// Trade → gold / science / luxuries for one city (CivOne City.cs formulas).
struct CivCityTradeBreakdown
{
	int rawTrade = 0;       // sum of tile trade before corruption
	int corruption = 0;
	int tradeTotal = 0;     // rawTrade - corruption (usable trade)
	int taxes = 0;          // gold from tax rate (+ market/bank / taxmen)
	int luxuries = 0;       // luxury arrows (+ market/bank / entertainers)
	int science = 0;        // beakers (+ library/university / scientists / wonders)
	int maintenance = 0;    // gold upkeep of buildings this city

	// Base split of tradeTotal before building/specialist bonuses.
	int baseTaxes = 0;
	int baseLuxuries = 0;
	int baseScience = 0;
};

// Budget rates 0..10 that sum to 10 (classic tax screen).
struct CivBudgetRates
{
	int luxuriesRate = 0;
	int taxesRate = 5;
	int scienceRate = 5; // residual; used for UI; science is trade leftover after tax+lux
};

// Inputs for computing / applying a city's trade income.
struct CivCityTradeTurnParams
{
	int rawTrade = 0;
	CivBudgetRates budget{};
	CivGovernment government = CivGovernment::Despotism;
	// Distance to capital (Chebyshev with X-wrap). Ignored under Communism (fixed 10)
	// and Democracy (no corruption). Capital with Palace → 0 corruption (non-commie).
	int distanceToCapital = 0;
	bool hasCapitalInEmpire = true; // if false and no palace, distance treated as 32
	bool inDisorder = false;        // no tax gold (science still counts, CivOne)
	// Specialists (+2 each to their bucket).
	int entertainers = 0;
	int taxmen = 0;
	int scientists = 0;
	// Empire-wide wonder: SETI Program (+50% science).
	bool empireHasSeti = false;
	// City wonder: Copernicus' Observatory (doubles science) — also checked via HasWonder.
	bool hasCopernicus = false;
};

// Relative offset of a worked non-center tile (city center is always worked).
struct CivWorkedTile
{
	int8_t dx = 0;
	int8_t dy = 0;
};

// Per-city game state. Mirrors classic Civ city data enough for map, growth,
// production, and buildings — economy formulas come later.
class CivCity
{
public:
	// --- identity / map ---
	int id = -1;              // unique id within the game (-1 = unset)
	int x = 0;
	int y = 0;
	int owner = -1;           // CivPlayer::id
	std::string name;

	// --- population ---
	int size = 1;             // population points (1..n)
	int food = 0;             // stored food toward next growth
	int shields = 0;          // stored production toward current build

	// --- production ---
	CivProduction production{};

	// --- buildings / wonders (bit packs for now) ---
	uint32_t buildings = CivBld_None;
	// Wonder ids present in this city (small list; most cities have 0–2).
	std::vector<int> wonders;

	// --- citizen mood (cached / set by happiness calc later) ---
	int happy = 0;
	int content = 0;
	int unhappy = 0;
	bool wasInDisorder = false;

	// --- worked tiles (non-center; center is always worked) ---
	// City square always yields. Population `size` also works that many extra
	// BFC tiles (size 1 → center + 1; each growth unlocks one more).
	std::vector<CivWorkedTile> workedTiles;

	// --- flags ---
	bool capital = false;     // has Palace / is capital
	bool coastal = false;     // next to ocean (ship building)
	bool destroyed = false;

	// --- helpers ---

	bool Valid() const { return id >= 0 && !destroyed && size > 0; }

	bool HasBuilding(CivCityBuilding b) const
	{
		return (buildings & b) != 0;
	}

	void SetBuilding(CivCityBuilding b, bool on = true)
	{
		if (on)
			buildings |= b;
		else
			buildings &= ~b;
		if (b == CivBld_Palace)
			capital = on;
	}

	// Map drawing: fortified if city walls present.
	bool Fortified() const { return HasBuilding(CivBld_CityWalls); }

	bool InDisorder() const
	{
		return size > 0 && unhappy > happy;
	}

	// Classic: food needed to grow = (size + 1) * 10.
	int FoodRequired() const { return (size + 1) * 10; }

	// Citizens eat 2 food each (settlers are separate — see FoodCosts).
	int FoodUpkeep() const { return size * 2; }

	// Full food costs: citizens + home settlers (gov-dependent).
	// Despotic (Anarchy/Despotism): +1 per settler; else +2 per settler.
	int FoodCosts(int homeSettlers, bool despoticGovernment) const;

	// foodFromTiles - FoodCosts (can be negative → famine path).
	int FoodIncome(int foodFromTiles, int homeSettlers, bool despoticGovernment) const;

	// Can grow past size 10 only with an Aqueduct.
	bool NeedsAqueductToGrow() const
	{
		return size == 10 && !HasBuilding(CivBld_Aqueduct);
	}

	// Celebration: no unhappy, happy >= content, size >= 3 (caller checks gov).
	bool QualifiesWeLovePresident() const
	{
		return size >= 3 && unhappy == 0 && happy >= content && food > 0;
	}

	bool HasWonder(int wonderId) const
	{
		for (int w : wonders)
			if (w == wonderId)
				return true;
		return false;
	}

	void AddWonder(int wonderId)
	{
		if (!HasWonder(wonderId))
			wonders.push_back(wonderId);
	}

	void ClearProduction()
	{
		production = CivProduction{};
		shields = 0;
	}

	void SetProductionUnit(int unitTypeId)
	{
		production.kind = CivProductionKind::Unit;
		production.id = unitTypeId;
		shields = 0;
	}

	void SetProductionBuilding(int buildingId)
	{
		production.kind = CivProductionKind::Building;
		production.id = buildingId;
		shields = 0;
	}

	// Cap stored food at the current box size (when size changes externally).
	void ClampFoodToRequired();

	// Apply size change and clamp food / trim worked tiles.
	// Returns false if city is destroyed (size <= 0).
	bool SetSize(int newSize);

	// Classic NewTurn food path: We Love the President → add income → starve/grow
	// with Granary half-box and Aqueduct gate at size 10.
	CivCityFoodTurnResult ProcessFoodTurn(const CivCityFoodTurnParams& params);

	// When a Settlers unit is completed in this city.
	// Chieftain + size 1: build blocked (returns built=false).
	// Sole remaining city at size 1: temporary +1 then -1 so the city survives.
	CivCitySettlerBuildResult CompleteSettlersBuild(bool soleCity, bool chieftainDifficulty);

	// Finish a building: set its bitflag, clear production/shields. Returns false if unknown id.
	bool CompleteBuilding(int buildingId);

	// --- map / yield helpers (need map + government) ---

	// True if (dx,dy) is in the classic 5×5 BFC minus corners (and not 0,0).
	static bool IsCityRadiusOffset(int dx, int dy);

	// Absolute map coords for a worked offset (X wraps on the map).
	void WorkedTileAbs(const CivMapData& map, int dx, int dy, int& outX, int& outY) const;

	// Yield for one map tile as this city would count it.
	// City center forces irrigation + road (classic free city-square bonuses).
	static CivYields TileYieldAt(const CivMapData& map, int tileX, int tileY,
		CivGovernment government, bool cityCenter);

	// Sum yields of center + all workedTiles.
	CivYields ComputeWorkedYields(const CivMapData& map, CivGovernment government) const;

	// Auto-assign best BFC tiles (food, then shields, then trade) up to `size`
	// additional squares. Does not clear tiles already assigned if still valid;
	// trims extras when oversized; fills vacancies. Optional occupied mask:
	// pair list of absolute (x,y) already taken by other cities' workers.
	void AutoAssignWorkedTiles(const CivMapData& map, CivGovernment government,
		const std::vector<std::pair<int, int>>* occupiedAbs = nullptr);

	// Convenience: reassign tiles, then ProcessFoodTurn with food from the map.
	CivCityFoodTurnResult ProcessFoodTurnFromMap(
		const CivMapData& map,
		CivGovernment government,
		int homeSettlers,
		bool inDisorder,
		const std::vector<std::pair<int, int>>* occupiedAbs = nullptr);

	// --- trade / gold / science / luxuries ---

	// Sum of building maintenance gold for buildings we track as bitflags.
	int TotalMaintenance() const;

	// Corruption lost from raw trade (before tax/lux/science split).
	int ComputeCorruption(int rawTrade, CivGovernment government,
		int distanceToCapital, bool hasCapitalInEmpire) const;

	// Full breakdown: corruption, rate split, building multipliers, specialists.
	CivCityTradeBreakdown ComputeTradeBreakdown(const CivCityTradeTurnParams& params) const;

	// Same, but raw trade taken from worked tiles on the map (does not reassign).
	CivCityTradeBreakdown ComputeTradeBreakdownFromMap(
		const CivMapData& map,
		const CivBudgetRates& budget,
		CivGovernment government,
		int distanceToCapital,
		bool hasCapitalInEmpire,
		bool inDisorder,
		bool empireHasSeti = false,
		int entertainers = 0,
		int taxmen = 0,
		int scientists = 0) const;

	// Auto-assign tiles then compute trade breakdown from the map.
	CivCityTradeBreakdown ProcessTradeTurnFromMap(
		const CivMapData& map,
		const CivBudgetRates& budget,
		CivGovernment government,
		int distanceToCapital,
		bool hasCapitalInEmpire,
		bool inDisorder,
		bool empireHasSeti = false,
		const std::vector<std::pair<int, int>>* occupiedAbs = nullptr,
		int entertainers = 0,
		int taxmen = 0,
		int scientists = 0);

	// Found a new city (capital or otherwise).
	static CivCity Found(int cityId, int mapX, int mapY, int ownerPlayer,
		const std::string& cityName, bool isCapital = false)
	{
		CivCity c;
		c.id = cityId;
		c.x = mapX;
		c.y = mapY;
		c.owner = ownerPlayer;
		c.name = cityName;
		c.size = 1;
		c.food = 0;
		c.shields = 0;
		c.capital = isCapital;
		if (isCapital)
			c.buildings = CivBld_Palace;
		return c;
	}
};

// Map building bitflag from CivBuildingId (for production completion later).
CivCityBuilding CivBuildingIdToFlag(int buildingId);

// CivOne government CorruptionMultiplier (higher → less corruption). 0 = none (Democracy).
int CivCorruptionMultiplier(CivGovernment government);

// Map distance for corruption: max(wrapX(|dx|), |dy|) — CivOne Common.DistanceToTile style.
int CivMapDistance(int x1, int y1, int x2, int y2, int mapWidth);

// Placeholder wonder ids matching MainState name table until a full catalog lands.
enum CivWonderId : int
{
	CivWonder_Pyramids = 0,
	CivWonder_HangingGardens = 1,
	CivWonder_Colossus = 2,
	CivWonder_Lighthouse = 3,
	CivWonder_GreatLibrary = 4,
	CivWonder_Oracle = 5,
	CivWonder_GreatWall = 6,
	CivWonder_MagellansExpedition = 7,
	CivWonder_MichelangelosChapel = 8,
	CivWonder_CopernicusObservatory = 9,
	CivWonder_ShakespearesTheatre = 10,
	CivWonder_IsaacNewtonsCollege = 11,
	CivWonder_JSBachsCathedral = 12,
	CivWonder_DarwinsVoyage = 13,
	CivWonder_HooverDam = 14,
	CivWonder_WomensSuffrage = 15,
	CivWonder_ManhattanProject = 16,
	CivWonder_UnitedNations = 17,
	CivWonder_ApolloProgram = 18,
	CivWonder_SETIProgram = 19,
	CivWonder_CureForCancer = 20,
};

#endif
