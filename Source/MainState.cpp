#include "MainState.h"

#include "Geist/Engine.h"
#include "Geist/Globals.h"
#include "Geist/ResourceManager.h"
#include "Geist/StateMachine.h"

#include "CivAdvances.h"
#include "CivBuildings.h"
#include "CivCity.h"
#include "CivGame.h"
#include "CivPlayer.h"
#include "CivUnits.h"
#include "GameGlobals.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace std;

namespace
{
	const char* kTabNames[MainState::TabCount] = {
		"Cities", "Units", "Adv", "Wndrs"
	};

	// Placeholder wonder names by id until a full wonder table exists.
	const char* kWonderNames[] = {
		"Pyramids", "Hanging Gardens", "Colossus", "Lighthouse", "Great Library",
		"Oracle", "Great Wall", "Magellan's Expedition", "Michelangelo's Chapel",
		"Copernicus' Observatory", "Shakespeare's Theatre", "Isaac Newton's College",
		"J.S. Bach's Cathedral", "Darwin's Voyage", "Hoover Dam", "Women's Suffrage",
		"Manhattan Project", "United Nations", "Apollo Program", "SETI Program",
		"Cure for Cancer",
	};
	constexpr int kWonderNameCount = sizeof(kWonderNames) / sizeof(kWonderNames[0]);

	const char* WonderName(int id)
	{
		if (id >= 0 && id < kWonderNameCount)
			return kWonderNames[id];
		return "Unknown Wonder";
	}

	float LineH()
	{
		return static_cast<float>(g_smallFont->baseSize) + 3.0f;
	}

	// CivOne Pattern.PanelGrey — SP299[288,120,32,16], tiled.
	void DrawPanelGrey(int x, int y, int w, int h)
	{
		Texture* tex = g_ResourceManager->GetTexture("Images/civ_tiles/panel_grey.png", false);
		if (!tex || tex->id == 0)
		{
			DrawRectangle(x, y, w, h, Color{ 136, 136, 140, 255 });
			return;
		}
		const int tw = tex->width > 0 ? tex->width : 32;
		const int th = tex->height > 0 ? tex->height : 16;
		for (int py = y; py < y + h; py += th)
		{
			for (int px = x; px < x + w; px += tw)
			{
				const int dw = std::min(tw, x + w - px);
				const int dh = std::min(th, y + h - py);
				const Rectangle src{ 0, 0, static_cast<float>(dw), static_cast<float>(dh) };
				const Rectangle dst{ static_cast<float>(px), static_cast<float>(py),
					static_cast<float>(dw), static_cast<float>(dh) };
				DrawTexturePro(*tex, src, dst, Vector2{ 0, 0 }, 0.0f, WHITE);
			}
		}
	}
}

void MainState::Init(const std::string& /*configfile*/)
{
	m_DrawCursor = true;
	m_mapView.Init("");
	m_mapView.SetEmbedded(true);
}

void MainState::Shutdown()
{
	m_mapView.Shutdown();
}

void MainState::OnEnter()
{
	m_playerIndex = std::max(0, g_GameSetup.HumanIndex());
	if (m_playerIndex < 0)
		m_playerIndex = 0;
	m_selectedCityId = -1;
	m_tab = TabCities;
	m_scroll = 0;
	m_mapView.OnEnter();
	// Fog follows the observed civ (first / human on enter).
	m_mapView.SetFogPlayerIndex(m_playerIndex);
	if (const CivPlayer* p = g_GameSetup.PlayerAt(m_playerIndex))
	{
		if (const CivCity* cap = p->Capital())
		{
			m_mapView.CenterOn(cap->x, cap->y);
			m_selectedCityId = cap->id;
		}
		else if (p->startX >= 0 && p->startY >= 0)
		{
			// Pre-founding: center on starting settlers.
			m_mapView.CenterOn(p->startX, p->startY);
			m_selectedCityId = -1;
		}
		else if (g_Game.started)
		{
			for (const auto& u : g_Game.units)
			{
				if (u.Valid() && u.owner == p->id)
				{
					m_mapView.CenterOn(u.x, u.y);
					break;
				}
			}
		}
	}
}

MainState::EmpireEcon MainState::ComputeEmpireEcon(const CivPlayer& player) const
{
	EmpireEcon e;
	const CivMapData& map = g_Game.started ? g_Game.map : g_ViewMap;
	if (map.tiles.empty())
		return e;

	CivPlayer pl = player; // copy so AutoAssign / budget clamp are safe
	pl.ClampBudgetRates();
	const CivBudgetRates budget = pl.BudgetRates();
	const CivCity* capital = pl.Capital();
	const bool hasCapital = capital != nullptr;
	const bool empireSeti = pl.HasWonder(CivWonder_SETIProgram);
	const bool despotic = CivGovIsDespotic(pl.government);

	for (CivCity& c : pl.cities)
	{
		if (!c.Valid())
			continue;
		c.AutoAssignWorkedTiles(map, pl.government, nullptr);
		const CivYields y = c.ComputeWorkedYields(map, pl.government);

		int homeSettlers = 0;
		int homeSupport = 0;
		if (g_Game.started)
		{
			for (const auto& u : g_Game.units)
			{
				if (!u.Valid() || u.owner != pl.id || u.homeCityId != c.id)
					continue;
				if (u.IsSettlers())
					++homeSettlers;
				if (u.RequiresShieldSupport())
					++homeSupport;
			}
		}
		e.foodSurplus += c.FoodIncome(y.food, homeSettlers, despotic);
		const int support = CivCity::ComputeShieldSupportCost(pl.government, c.size, homeSupport);
		e.shields += c.ComputeShieldIncome(y.shields, support, c.InDisorder());

		int dist = hasCapital
			? CivMapDistance(c.x, c.y, capital->x, capital->y, map.width)
			: 32;
		const CivCityTradeBreakdown b = c.ComputeTradeBreakdownFromMap(
			map, budget, pl.government, dist, hasCapital, c.InDisorder(), empireSeti);
		e.rawTrade += b.rawTrade;
		e.corruption += b.corruption;
		e.goldGross += b.taxes;
		e.maintenance += b.maintenance;
		e.science += b.science;
		e.luxuries += b.luxuries;
	}
	e.goldNet = e.goldGross - e.maintenance;
	return e;
}

void MainState::ComputeCityYields(const CivPlayer& player, const CivCity& city,
	int& outFood, int& outShields, int& outTrade) const
{
	outFood = outShields = outTrade = 0;
	const CivMapData& map = g_Game.started ? g_Game.map : g_ViewMap;
	if (map.tiles.empty() || !city.Valid())
		return;

	CivCity c = city;
	c.AutoAssignWorkedTiles(map, player.government, nullptr);
	const CivYields y = c.ComputeWorkedYields(map, player.government);

	int homeSettlers = 0;
	int homeSupport = 0;
	if (g_Game.started)
	{
		for (const auto& u : g_Game.units)
		{
			if (!u.Valid() || u.owner != player.id || u.homeCityId != c.id)
				continue;
			if (u.IsSettlers())
				++homeSettlers;
			if (u.RequiresShieldSupport())
				++homeSupport;
		}
	}
	const bool despotic = CivGovIsDespotic(player.government);
	outFood = c.FoodIncome(y.food, homeSettlers, despotic);
	const int support = CivCity::ComputeShieldSupportCost(player.government, c.size, homeSupport);
	outShields = c.ComputeShieldIncome(y.shields, support, c.InDisorder());
	outTrade = y.trade;
}

void MainState::OnExit()
{
	m_mapView.OnExit();
}

int MainState::VisibleLines(float contentH) const
{
	return std::max(1, static_cast<int>(contentH / LineH()));
}

void MainState::ClampScroll(int lineCount, int visibleLines)
{
	const int maxScroll = std::max(0, lineCount - visibleLines);
	m_scroll = std::clamp(m_scroll, 0, maxScroll);
}

float MainState::ObserverTop() const
{
	const Rectangle mm = m_mapView.MinimapRect();
	return mm.y + mm.height + 6.0f;
}

float MainState::ObserverTabY() const
{
	// Keep in sync with DrawObserverPanel header block (tribe, leader, rates, econ).
	const float top = ObserverTop();
	const float lh = LineH();
	const CivPlayer* player = g_GameSetup.PlayerAt(m_playerIndex);
	if (!player)
		return top + lh + 2.0f;
	// 6 header lines when a civ is selected.
	return top + 6.0f * lh + 2.0f;
}

void MainState::Update()
{
	if (m_logTimer > 0.0f)
		m_logTimer -= GetFrameTime();

	if (IsKeyPressed(KEY_ESCAPE))
	{
		g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
		return;
	}

	// Enter / Space — end turn (human game or all-AI observe).
	const bool canEndTurn = g_Game.started
		&& (g_Game.IsHumanTurn() || g_Game.IsObserveMode() || g_Game.setup.HumanIndex() < 0);
	if (canEndTurn && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)))
	{
		if (!g_vitalRNG)
		{
			g_vitalRNG = std::make_unique<RNG>();
			g_vitalRNG->SeedFromSystemTimer();
		}
		g_Game.EndHumanTurn(*g_vitalRNG);
		g_GameSetup = g_Game.setup;
		g_ViewMap = g_Game.map;
		m_mapView.SyncFromGame();
		// Keep observing the same civ after the year resolves.
		m_mapView.SetFogPlayerIndex(m_playerIndex);
		m_logTimer = 4.0f;
		return;
	}

	// Tabs: 1-4 (narrow sidebar labels)
	if (IsKeyPressed(KEY_ONE)) { m_tab = TabCities; m_scroll = 0; }
	if (IsKeyPressed(KEY_TWO)) { m_tab = TabUnits; m_scroll = 0; }
	if (IsKeyPressed(KEY_THREE)) { m_tab = TabAdvances; m_scroll = 0; }
	if (IsKeyPressed(KEY_FOUR)) { m_tab = TabWonders; m_scroll = 0; }

	// [ ] cycle observed civilization (and its fog).
	const int nPlayers = g_GameSetup.PlayerCount();
	if (nPlayers > 0)
	{
		auto focusObservedCiv = [&]() {
			m_mapView.SetFogPlayerIndex(m_playerIndex);
			m_selectedCityId = -1;
			if (const CivPlayer* p = g_GameSetup.PlayerAt(m_playerIndex))
			{
				if (const CivCity* cap = p->Capital())
				{
					m_selectedCityId = cap->id;
					m_mapView.CenterOn(cap->x, cap->y);
				}
				else if (p->startX >= 0)
					m_mapView.CenterOn(p->startX, p->startY);
				else if (g_Game.started)
				{
					for (const auto& u : g_Game.units)
					{
						if (u.Valid() && u.owner == p->id)
						{
							m_mapView.CenterOn(u.x, u.y);
							break;
						}
					}
				}
			}
		};
		if (IsKeyPressed(KEY_LEFT_BRACKET))
		{
			m_playerIndex = (m_playerIndex + nPlayers - 1) % nPlayers;
			m_scroll = 0;
			focusObservedCiv();
		}
		if (IsKeyPressed(KEY_RIGHT_BRACKET))
		{
			m_playerIndex = (m_playerIndex + 1) % nPlayers;
			m_scroll = 0;
			focusObservedCiv();
		}
	}

	const float scaleX = g_Engine->m_ScreenWidth / static_cast<float>(g_Engine->m_RenderWidth);
	const float scaleY = g_Engine->m_ScreenHeight / static_cast<float>(g_Engine->m_RenderHeight);
	const float virtX = GetMouseX() / scaleX;
	const float virtY = GetMouseY() / scaleY;

	// Mouse wheel scrolls observer list when over sidebar.
	if (virtX < MapViewerState::kSidebarWidth)
	{
		const float wheel = GetMouseWheelMove();
		if (wheel != 0.0f)
		{
			m_scroll -= (wheel > 0.0f) ? 1 : -1;
			m_scroll = std::max(0, m_scroll);
		}

		// Tab clicks (same geometry as DrawObserverPanel / DrawTabBar).
		const float ox = 4.0f;
		const float tabY = ObserverTabY();
		const float ow = MapViewerState::kSidebarWidth - 8.0f;
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
			&& virtY >= tabY && virtY < tabY + kTabH
			&& virtX >= ox && virtX < ox + ow)
		{
			const float tabW = ow / static_cast<float>(TabCount);
			const int hit = static_cast<int>((virtX - ox) / tabW);
			if (hit >= 0 && hit < TabCount)
			{
				m_tab = static_cast<ObserverTab>(hit);
				m_scroll = 0;
			}
		}
	}

	// Map input: pan, fog/borders, minimap jump.
	m_mapView.Update();

	// Click a city on the map: observe that civ + select city for yield detail.
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		const MapViewerState::CityPick pick = m_mapView.PickCityAt(virtX, virtY);
		if (pick.Valid())
		{
			for (int i = 0; i < g_GameSetup.PlayerCount(); ++i)
			{
				const CivPlayer* p = g_GameSetup.PlayerAt(i);
				if (p && p->id == pick.owner)
				{
					m_playerIndex = i;
					m_selectedCityId = pick.cityId;
					m_scroll = 0;
					m_tab = TabCities;
					// Fog of war switches to the newly observed civilization.
					m_mapView.SetFogPlayerIndex(m_playerIndex);
					break;
				}
			}
		}
	}
}

void MainState::DrawSidebarBackground() const
{
	const int rh = g_Engine->m_RenderHeight;
	DrawPanelGrey(0, 0, MapViewerState::kSidebarWidth, rh);
	// Subtle right edge like a panel divider.
	DrawRectangle(MapViewerState::kSidebarWidth - 1, 0, 1, rh, Color{ 40, 40, 40, 255 });
}

void MainState::DrawTabBar(float x, float y, float w)
{
	const float tabW = w / static_cast<float>(TabCount);
	const int th = static_cast<int>(kTabH);
	for (int i = 0; i < TabCount; ++i)
	{
		const bool selected = (static_cast<int>(m_tab) == i);
		const float tx = x + i * tabW;
		DrawRectangle(static_cast<int>(tx), static_cast<int>(y),
			static_cast<int>(tabW - 1), th,
			selected ? Color{ 50, 70, 120, 220 } : Color{ 20, 20, 28, 160 });
		DrawRectangleLines(static_cast<int>(tx), static_cast<int>(y),
			static_cast<int>(tabW - 1), th,
			selected ? Color{ 220, 220, 240, 255 } : Color{ 60, 60, 70, 255 });

		const Vector2 ts = MeasureTextEx(*g_smallFont, kTabNames[i], g_smallFont->baseSize, 1);
		DrawTextEx(*g_smallFont, kTabNames[i],
			{ tx + (tabW - 1 - ts.x) * 0.5f, y + 2.0f },
			static_cast<float>(g_smallFont->baseSize), 1,
			selected ? Color{ 255, 240, 160, 255 } : Color{ 200, 200, 210, 255 });
	}
}

void MainState::DrawCitiesTab(const CivPlayer& player, float x, float y, float w, float h)
{
	vector<string> lines;

	// Selected city yields (per turn).
	const CivCity* sel = player.FindCityById(m_selectedCityId);
	if (sel && sel->Valid())
	{
		int food = 0, sh = 0, tr = 0;
		ComputeCityYields(player, *sel, food, sh, tr);
		char title[96];
		snprintf(title, sizeof(title), "%s sz%d%s",
			sel->name.c_str(), sel->size, sel->capital ? " *" : "");
		lines.emplace_back(title);
		char ybuf[96];
		snprintf(ybuf, sizeof(ybuf), "Food %+d/t", food);
		lines.emplace_back(ybuf);
		snprintf(ybuf, sizeof(ybuf), "Prod %d/t", sh);
		lines.emplace_back(ybuf);
		snprintf(ybuf, sizeof(ybuf), "Trade %d/t", tr);
		lines.emplace_back(ybuf);
		snprintf(ybuf, sizeof(ybuf), "Gov %s", CivGovernmentName(player.government));
		lines.emplace_back(ybuf);
		snprintf(ybuf, sizeof(ybuf), "Mood H%d C%d U%d%s",
			sel->happy, sel->content, sel->unhappy,
			sel->InDisorder() ? " DISORDER" : "");
		lines.emplace_back(ybuf);
		if (sel->production.kind == CivProductionKind::Unit)
		{
			char pbuf[96];
			snprintf(pbuf, sizeof(pbuf), "Prod: %s %d/%d",
				CivUnitName(sel->production.id),
				sel->shields,
				CivUnitShieldCost(sel->production.id));
			lines.emplace_back(pbuf);
		}
		else if (sel->production.kind == CivProductionKind::Building)
		{
			char pbuf[96];
			snprintf(pbuf, sizeof(pbuf), "Prod: %s %d/%d",
				CivBuildingName(sel->production.id),
				sel->shields,
				CivBuildingShieldCost(sel->production.id));
			lines.emplace_back(pbuf);
		}
		else
			lines.emplace_back("Prod: (none)");

		// Buildings present in this city.
		lines.emplace_back("-- Buildings --");
		bool anyBld = false;
		for (int bi = 0; bi < CivBuildingCount(); ++bi)
		{
			const CivCityBuilding flag = CivBuildingIdToFlag(bi);
			if (flag == CivBld_None)
				continue;
			if (!sel->HasBuilding(flag))
				continue;
			anyBld = true;
			lines.emplace_back(CivBuildingName(bi));
		}
		if (!anyBld)
			lines.emplace_back("(none)");

		lines.emplace_back("----");
	}
	else
	{
		lines.emplace_back("(click a city)");
		lines.emplace_back("----");
	}

	// Short list of all cities.
	for (const CivCity& c : player.cities)
	{
		if (!c.Valid())
			continue;
		const bool hi = (c.id == m_selectedCityId);
		char buf[96];
		snprintf(buf, sizeof(buf), "%s%s sz%d",
			hi ? ">" : " ",
			c.name.c_str(), c.size);
		lines.emplace_back(buf);
	}
	if (player.cities.empty())
		lines.emplace_back("(no cities)");

	const int vis = VisibleLines(h);
	ClampScroll(static_cast<int>(lines.size()), vis);
	const float lh = LineH();
	for (int i = 0; i < vis; ++i)
	{
		const int idx = m_scroll + i;
		if (idx >= static_cast<int>(lines.size()))
			break;
		const bool hi = (idx < static_cast<int>(lines.size())
			&& !lines[static_cast<size_t>(idx)].empty()
			&& lines[static_cast<size_t>(idx)][0] == '>');
		DrawTextEx(*g_smallFont, lines[static_cast<size_t>(idx)].c_str(),
			{ x + 2.0f, y + i * lh }, static_cast<float>(g_smallFont->baseSize), 1,
			hi ? Color{ 80, 40, 0, 255 } : Color{ 20, 20, 24, 255 });
	}
	(void)w;
}

void MainState::DrawUnitsTab(const CivPlayer& player, float x, float y, float w, float h)
{
	vector<string> lines;
	bool anyFielded = false;
	if (g_Game.started)
	{
		for (const auto& u : g_Game.units)
		{
			if (!u.Valid() || u.owner != player.id)
				continue;
			anyFielded = true;
			char buf[96];
			snprintf(buf, sizeof(buf), "%s (%d,%d)%s",
				CivUnitName(u.typeId), u.x, u.y, u.fortify ? " F" : "");
			lines.emplace_back(buf);
		}
	}
	if (!anyFielded)
		lines.emplace_back("(none on map)");

	lines.emplace_back("");
	lines.emplace_back("-- Buildable --");
	bool any = false;
	for (int i = 0; i < CivUnitCount(); ++i)
	{
		if (!CivUnitAvailable(i, player.advances))
			continue;
		any = true;
		const CivUnitDef& u = CivUnit(i);
		char buf[96];
		snprintf(buf, sizeof(buf), "%s A%d/D%d/M%d",
			u.name, u.attack, u.defense, u.move);
		lines.emplace_back(buf);
	}
	if (!any)
		lines.emplace_back("(none)");

	const int vis = VisibleLines(h);
	ClampScroll(static_cast<int>(lines.size()), vis);
	const float lh = LineH();
	for (int i = 0; i < vis; ++i)
	{
		const int idx = m_scroll + i;
		if (idx >= static_cast<int>(lines.size()))
			break;
		DrawTextEx(*g_smallFont, lines[static_cast<size_t>(idx)].c_str(),
			{ x + 2.0f, y + i * lh }, static_cast<float>(g_smallFont->baseSize), 1,
			Color{ 20, 20, 24, 255 });
	}
	(void)w;
}

void MainState::DrawAdvancesTab(const CivPlayer& player, float x, float y, float w, float h)
{
	vector<string> lines;
	if (player.currentResearch >= 0)
	{
		char buf[96];
		snprintf(buf, sizeof(buf), "R: %s (%d)",
			CivAdvanceName(player.currentResearch), player.science);
		lines.emplace_back(buf);
	}
	else
		lines.emplace_back("R: (none)");

	lines.emplace_back("");
	lines.emplace_back("-- Known --");
	if (player.advances.empty())
		lines.emplace_back("(none)");
	else
	{
		for (int id : player.advances)
			lines.emplace_back(CivAdvanceName(id));
	}

	const vector<int> avail = CivAdvancesAvailable(player.advances);
	if (!avail.empty())
	{
		lines.emplace_back("");
		lines.emplace_back("-- Available --");
		for (int id : avail)
			lines.emplace_back(CivAdvanceName(id));
	}

	const int vis = VisibleLines(h);
	ClampScroll(static_cast<int>(lines.size()), vis);
	const float lh = LineH();
	for (int i = 0; i < vis; ++i)
	{
		const int idx = m_scroll + i;
		if (idx >= static_cast<int>(lines.size()))
			break;
		DrawTextEx(*g_smallFont, lines[static_cast<size_t>(idx)].c_str(),
			{ x + 2.0f, y + i * lh }, static_cast<float>(g_smallFont->baseSize), 1,
			Color{ 20, 20, 24, 255 });
	}
	(void)w;
}

void MainState::DrawWondersTab(const CivPlayer& player, float x, float y, float w, float h)
{
	vector<string> lines;
	for (const CivCity& c : player.cities)
	{
		if (!c.Valid())
			continue;
		for (int wid : c.wonders)
		{
			char buf[96];
			snprintf(buf, sizeof(buf), "%s @ %s", WonderName(wid), c.name.c_str());
			lines.emplace_back(buf);
		}
	}
	if (lines.empty())
		lines.emplace_back("(no wonders)");

	const int vis = VisibleLines(h);
	ClampScroll(static_cast<int>(lines.size()), vis);
	const float lh = LineH();
	for (int i = 0; i < vis; ++i)
	{
		const int idx = m_scroll + i;
		if (idx >= static_cast<int>(lines.size()))
			break;
		DrawTextEx(*g_smallFont, lines[static_cast<size_t>(idx)].c_str(),
			{ x + 2.0f, y + i * lh }, static_cast<float>(g_smallFont->baseSize), 1,
			Color{ 20, 20, 24, 255 });
	}
	(void)w;
}

void MainState::DrawContent(float x, float y, float w, float h)
{
	const CivPlayer* player = g_GameSetup.PlayerAt(m_playerIndex);
	if (!player)
	{
		DrawTextEx(*g_smallFont, "No player.",
			{ x + 2, y + 2 }, static_cast<float>(g_smallFont->baseSize), 1,
			Color{ 20, 20, 24, 255 });
		return;
	}

	switch (m_tab)
	{
	case TabCities:   DrawCitiesTab(*player, x, y, w, h); break;
	case TabUnits:    DrawUnitsTab(*player, x, y, w, h); break;
	case TabAdvances: DrawAdvancesTab(*player, x, y, w, h); break;
	case TabWonders:  DrawWondersTab(*player, x, y, w, h); break;
	default: break;
	}
}

void MainState::DrawObserverPanel()
{
	const float ox = 4.0f;
	const float ow = static_cast<float>(MapViewerState::kSidebarWidth) - 8.0f;
	const float top = ObserverTop();
	const float rh = static_cast<float>(g_Engine->m_RenderHeight);
	const float lh = LineH();
	const Color ink{ 16, 16, 20, 255 };
	const Color dim{ 40, 40, 48, 255 };

	const CivPlayer* player = g_GameSetup.PlayerAt(m_playerIndex);
	float y = top;

	// Header: faction color chip + tribe / leader (line count must match ObserverTabY).
	if (player)
	{
		const Color col = player->ColorRgb();
		DrawRectangle(static_cast<int>(ox), static_cast<int>(y), 10, 10, col);
		DrawRectangleLines(static_cast<int>(ox), static_cast<int>(y), 10, 10, BLACK);

		DrawTextEx(*g_smallFont, player->tribeName.c_str(),
			{ ox + 14.0f, y }, static_cast<float>(g_smallFont->baseSize), 1, ink);
		y += lh;

		char sub[96];
		snprintf(sub, sizeof(sub), "%s $%d %s",
			player->leaderName.c_str(),
			player->gold,
			player->human ? "YOU" : "AI");
		DrawTextEx(*g_smallFont, sub,
			{ ox, y }, static_cast<float>(g_smallFont->baseSize), 1, dim);
		y += lh;

		// Tax rate breakdown (rates sum to 10 → show as ×10 %).
		char rates[96];
		snprintf(rates, sizeof(rates), "Tax %d%% Sci %d%% Lux %d%%",
			player->taxesRate * 10,
			player->scienceRate * 10,
			player->luxuriesRate * 10);
		DrawTextEx(*g_smallFont, rates,
			{ ox, y }, static_cast<float>(g_smallFont->baseSize), 1, ink);
		y += lh;

		// Empire income this turn (preview).
		const EmpireEcon e = ComputeEmpireEcon(*player);
		char econ[96];
		snprintf(econ, sizeof(econ), "Gold %+d/t", e.goldNet);
		DrawTextEx(*g_smallFont, econ,
			{ ox, y }, static_cast<float>(g_smallFont->baseSize), 1, ink);
		y += lh;
		snprintf(econ, sizeof(econ), "  ($%d-maint%d)", e.goldGross, e.maintenance);
		DrawTextEx(*g_smallFont, econ,
			{ ox, y }, static_cast<float>(g_smallFont->baseSize), 1, dim);
		y += lh;
		snprintf(econ, sizeof(econ), "Sci %+d  Lux %d/t", e.science, e.luxuries);
		DrawTextEx(*g_smallFont, econ,
			{ ox, y }, static_cast<float>(g_smallFont->baseSize), 1, ink);
		y += lh;
	}
	else
	{
		DrawTextEx(*g_smallFont, "Click a city",
			{ ox, y }, static_cast<float>(g_smallFont->baseSize), 1, dim);
		y += lh;
	}

	// Shared layout with Update() hit-testing.
	const float tabY = ObserverTabY();
	DrawTabBar(ox, tabY, ow);

	const float listY = tabY + kTabH + 2.0f;
	const float listH = rh - listY - 14.0f;
	if (listH > 8.0f)
		DrawContent(ox, listY, ow, listH);

	// Hint
	DrawTextEx(*g_smallFont, "[]/city: civ",
		{ ox, rh - 12.0f }, static_cast<float>(g_smallFont->baseSize), 1,
		Color{ 48, 48, 56, 255 });
}

void MainState::Draw()
{
	const int rw = g_Engine->m_RenderWidth;
	const int rh = g_Engine->m_RenderHeight;

	if (g_GameSetup.PlayerCount() == 0)
	{
		DrawRectangle(0, 0, rw, rh, Color{ 8, 10, 18, 255 });
		const char* msg = "No active game. Start a New Game from the title screen.";
		const Vector2 ms = MeasureTextEx(*g_font, msg, g_font->baseSize, 1);
		DrawOutlinedText(g_font, msg,
			{ (rw - ms.x) * 0.5f, rh * 0.45f },
			g_font->baseSize, 1, Color{ 200, 180, 140, 255 });
		return;
	}

	// 1) Grey mottled left strip (under minimap + observer).
	DrawSidebarBackground();

	// 2) Map (right of sidebar) + minimap (top-left of strip).
	m_mapView.Draw();

	// 3) Observer for the selected civ (below minimap).
	DrawObserverPanel();

	// Turn / year on map area bottom.
	if (g_Game.started)
	{
		const float mapLeft = static_cast<float>(MapViewerState::kSidebarWidth);
		char turnBuf[128];
		if (g_Game.IsObserveMode())
			snprintf(turnBuf, sizeof(turnBuf), "OBSERVE  Turn %d  %s  |  SPACE: Advance year  |  F: fog",
				g_Game.gameTurn, g_Game.YearString().c_str());
		else
			snprintf(turnBuf, sizeof(turnBuf), "Turn %d  %s  |  SPACE: End Turn",
				g_Game.gameTurn, g_Game.YearString().c_str());
		DrawOutlinedText(g_smallFont, turnBuf,
			{ mapLeft + 4.0f, static_cast<float>(rh - 24) },
			g_smallFont->baseSize, 1, Color{ 200, 220, 160, 255 });

		if (m_logTimer > 0.0f && !g_Game.lastLog.empty())
		{
			DrawOutlinedText(g_smallFont, g_Game.lastLog,
				{ mapLeft + 4.0f, static_cast<float>(rh - 36) },
				g_smallFont->baseSize, 1, Color{ 255, 230, 140, 255 });
		}
	}
}
