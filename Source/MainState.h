#ifndef _MAINSTATE_H_
#define _MAINSTATE_H_

#include "Geist/State.h"
#include "MapViewerState.h"

// Main game shell: left UI strip (minimap + single-civ observer) + world map.
class MainState : public State
{
public:
	enum ObserverTab
	{
		TabCities = 0,
		TabUnits,
		TabAdvances,
		TabWonders,
		TabCount
	};

	MainState() = default;
	~MainState() override = default;

	void Init(const std::string& configfile) override;
	void Shutdown() override;
	void Update() override;
	void Draw() override;
	void OnEnter() override;
	void OnExit() override;

private:
	void DrawSidebarBackground() const;
	void DrawObserverPanel();
	void DrawTabBar(float x, float y, float w);
	void DrawContent(float x, float y, float w, float h);
	void DrawCitiesTab(const class CivPlayer& player, float x, float y, float w, float h);
	void DrawUnitsTab(const class CivPlayer& player, float x, float y, float w, float h);
	void DrawAdvancesTab(const class CivPlayer& player, float x, float y, float w, float h);
	void DrawWondersTab(const class CivPlayer& player, float x, float y, float w, float h);

	void ClampScroll(int lineCount, int visibleLines);
	int VisibleLines(float contentH) const;

	// Bottom of minimap → start of observer content (render coords).
	float ObserverTop() const;
	// Y of the tab bar (must match DrawObserverPanel layout).
	float ObserverTabY() const;
	static constexpr float kTabH = 14.0f;

	// Empire economy preview (does not mutate gold/science).
	struct EmpireEcon
	{
		int foodSurplus = 0;
		int shields = 0;
		int goldNet = 0;      // taxes - maintenance
		int goldGross = 0;    // taxes before maint
		int maintenance = 0;
		int science = 0;
		int luxuries = 0;
		int rawTrade = 0;
		int corruption = 0;
	};
	EmpireEcon ComputeEmpireEcon(const class CivPlayer& player) const;
	// Per-city food / shields / trade for display (uses map + auto-assign copy).
	void ComputeCityYields(const class CivPlayer& player, const class CivCity& city,
		int& outFood, int& outShields, int& outTrade) const;

	MapViewerState m_mapView;

	int m_playerIndex = 0;
	// Selected city for detail (-1 = none / empire only).
	int m_selectedCityId = -1;
	ObserverTab m_tab = TabCities;
	int m_scroll = 0;
	// Brief flash of last turn log.
	float m_logTimer = 0.0f;
};

#endif
