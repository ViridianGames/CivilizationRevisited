#include "FactionSelectState.h"

#include "Geist/Engine.h"
#include "Geist/Globals.h"
#include "Geist/StateMachine.h"
#include "Geist/RNG.h"

#include "CivGame.h"
#include "CivMapGenerator.h"
#include "GameGlobals.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace std;

void FactionSelectState::Init(const std::string& /*configfile*/)
{
	m_DrawCursor = true;
}

void FactionSelectState::Shutdown()
{
}

void FactionSelectState::OnEnter()
{
	m_numSlots = std::clamp(g_GameSetup.numCivilizations, 3, 8);
	for (int i = 0; i < 8; ++i)
		m_slots[i] = CivPlayerSlot{};
	m_slot = 0;
	m_phase = PhasePickColor;
	m_selected = 0;
}

void FactionSelectState::OnExit()
{
}

bool FactionSelectState::ColorTaken(CivColor c, int exceptSlot) const
{
	for (int i = 0; i < m_slot; ++i)
	{
		if (i == exceptSlot)
			continue;
		if (m_slots[i].active && m_slots[i].color == c)
			return true;
	}
	return false;
}

bool FactionSelectState::FactionTaken(CivFactionId f, int exceptSlot) const
{
	for (int i = 0; i < m_slot; ++i)
	{
		if (i == exceptSlot)
			continue;
		if (m_slots[i].active && m_slots[i].faction == f)
			return true;
	}
	return false;
}

CivColor FactionSelectState::NextFreeColor() const
{
	for (int i = 0; i < static_cast<int>(CivColor::Count); ++i)
	{
		const CivColor c = static_cast<CivColor>(i);
		if (!ColorTaken(c))
			return c;
	}
	return CivColor::White;
}

CivFactionId FactionSelectState::NextFreeFaction() const
{
	int n = 0;
	const CivFactionDef* list = CivFactionList(&n);
	for (int i = 0; i < n; ++i)
	{
		if (!FactionTaken(list[i].id))
			return list[i].id;
	}
	return CivFactionId::Romans;
}

void FactionSelectState::BeginSlot(int slot)
{
	m_slot = slot;
	m_phase = PhasePickColor;
	// Prefer first free color in list for selection cursor.
	m_selected = static_cast<int>(NextFreeColor());
}

void FactionSelectState::ConfirmColor(CivColor color)
{
	if (ColorTaken(color))
		return;
	m_slots[m_slot].color = color;
	m_phase = PhasePickFaction;
	// Cursor on first free faction.
	m_selected = static_cast<int>(NextFreeFaction());
}

void FactionSelectState::ConfirmFaction(CivFactionId faction)
{
	if (FactionTaken(faction))
		return;

	m_slots[m_slot].faction = faction;
	m_slots[m_slot].active = true;
	m_slots[m_slot].human = (m_slot == 0);

	if (m_slot + 1 >= m_numSlots)
	{
		FinishAndStart();
		return;
	}

	// After human picks, auto-fill AI opponents with free colors/factions.
	if (m_slot == 0)
	{
		AutoFillRemaining();
		FinishAndStart();
		return;
	}

	BeginSlot(m_slot + 1);
}

void FactionSelectState::AutoFillRemaining()
{
	bool colorUsed[8] = {};
	bool factionUsed[16] = {};
	colorUsed[static_cast<int>(m_slots[0].color)] = true;
	factionUsed[static_cast<int>(m_slots[0].faction)] = true;

	int nFac = 0;
	const CivFactionDef* facList = CivFactionList(&nFac);

	for (int s = 1; s < m_numSlots; ++s)
	{
		CivColor col = CivColor::White;
		for (int c = 0; c < 8; ++c)
		{
			if (!colorUsed[c])
			{
				col = static_cast<CivColor>(c);
				break;
			}
		}
		CivFactionId fac = CivFactionId::Romans;
		for (int f = 0; f < nFac; ++f)
		{
			if (!factionUsed[static_cast<int>(facList[f].id)])
			{
				fac = facList[f].id;
				break;
			}
		}
		m_slots[s].color = col;
		m_slots[s].faction = fac;
		m_slots[s].human = false;
		m_slots[s].active = true;
		colorUsed[static_cast<int>(col)] = true;
		factionUsed[static_cast<int>(fac)] = true;
	}
}

void FactionSelectState::FinishAndStart()
{
	// Build full CivPlayer objects (human + AI) from slots.
	g_GameSetup.BuildPlayersFromSlots(m_slots, m_numSlots);

	if (!g_vitalRNG)
	{
		g_vitalRNG = std::make_unique<RNG>();
		g_vitalRNG->SeedFromSystemTimer();
	}
	else
	{
		g_vitalRNG->SeedFromSystemTimer();
	}

	// Starting techs: Roads + Irrigation + Mining; chance of Pottery/Alphabet/etc.
	g_GameSetup.GrantAllStartingAdvances(*g_vitalRNG);

	if (!CivMapGenerator::Generate(g_ViewMap, g_GameSetup.world, *g_vitalRNG))
	{
		DebugPrint("FactionSelect: map generation failed");
		g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
		return;
	}

	const int placed = CivPlaceStartingCities(g_ViewMap, g_GameSetup, *g_vitalRNG);
	char buf[192];
	snprintf(buf, sizeof(buf), "New game: %d civs, %d start sites, map '%s'",
		g_GameSetup.PlayerCount(), placed, g_ViewMap.title.c_str());
	DebugPrint(buf);
	for (const CivPlayer& p : g_GameSetup.players)
	{
		string techList;
		for (int id : p.advances)
		{
			if (!techList.empty())
				techList += ", ";
			techList += CivAdvanceName(id);
		}
		DebugPrint(p.tribeName + " starts with: " + techList);
	}

	if (const CivPlayer* human = g_GameSetup.Human())
		g_ViewMap.title = human->tribeName + " — " + CivDifficultyName(g_GameSetup.difficulty);
	else
		g_ViewMap.title = string("Game — ") + CivDifficultyName(g_GameSetup.difficulty);

	// Boot live session: map + setup + starting units + first-turn economy.
	g_Game.Reset();
	g_Game.map = g_ViewMap;
	g_Game.setup = g_GameSetup;
	g_Game.StartNewGame(*g_vitalRNG);
	// Mirror session back to globals the UI already uses.
	g_GameSetup = g_Game.setup;
	g_ViewMap = g_Game.map;

	// Enter main game shell (Civ Observer, etc.).
	g_StateMachine->MakeStateTransition(STATE_MAINSTATE);
}

void FactionSelectState::Update()
{
	if (IsKeyPressed(KEY_ESCAPE))
	{
		if (m_phase == PhasePickFaction)
		{
			m_phase = PhasePickColor;
			m_selected = static_cast<int>(m_slots[m_slot].color);
			return;
		}
		if (m_slot == 0)
		{
			// Back to world customize.
			g_StateMachine->MakeStateTransition(STATE_CUSTOMIZEWORLDSTATE);
			return;
		}
		BeginSlot(m_slot - 1);
		return;
	}

	int itemCount = 0;
	if (m_phase == PhasePickColor)
		itemCount = static_cast<int>(CivColor::Count);
	else
	{
		int n = 0;
		CivFactionList(&n);
		itemCount = n;
	}

	if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
		m_selected = (m_selected + itemCount - 1) % itemCount;
	if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
		m_selected = (m_selected + 1) % itemCount;

	// Skip taken entries with left/right optionally — also skip on confirm.
	const float rw = static_cast<float>(g_Engine->m_RenderWidth);
	const float rh = static_cast<float>(g_Engine->m_RenderHeight);
	const float menuY = 52.0f;
	const float rowH = static_cast<float>(g_font->baseSize) + 6.0f;
	const float scaleX = g_Engine->m_ScreenWidth / static_cast<float>(g_Engine->m_RenderWidth);
	const float scaleY = g_Engine->m_ScreenHeight / static_cast<float>(g_Engine->m_RenderHeight);
	const float virtMouseX = GetMouseX() / scaleX;
	const float virtMouseY = GetMouseY() / scaleY;

	for (int i = 0; i < itemCount; ++i)
	{
		const float y0 = menuY + i * rowH - 1.0f;
		const float y1 = y0 + rowH;
		if (virtMouseY >= y0 && virtMouseY <= y1 && virtMouseX > 20 && virtMouseX < rw - 20)
		{
			m_selected = i;
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			{
				if (m_phase == PhasePickColor)
				{
					const CivColor c = static_cast<CivColor>(i);
					if (!ColorTaken(c))
						ConfirmColor(c);
				}
				else
				{
					const CivFactionId f = static_cast<CivFactionId>(i);
					if (!FactionTaken(f))
						ConfirmFaction(f);
				}
				return;
			}
		}
	}

	if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_KP_ENTER))
	{
		if (m_phase == PhasePickColor)
		{
			const CivColor c = static_cast<CivColor>(m_selected);
			if (!ColorTaken(c))
				ConfirmColor(c);
			else
			{
				// Advance to next free color.
				for (int k = 0; k < itemCount; ++k)
				{
					const int idx = (m_selected + 1 + k) % itemCount;
					if (!ColorTaken(static_cast<CivColor>(idx)))
					{
						m_selected = idx;
						break;
					}
				}
			}
		}
		else
		{
			const CivFactionId f = static_cast<CivFactionId>(m_selected);
			if (!FactionTaken(f))
				ConfirmFaction(f);
			else
			{
				for (int k = 0; k < itemCount; ++k)
				{
					const int idx = (m_selected + 1 + k) % itemCount;
					if (!FactionTaken(static_cast<CivFactionId>(idx)))
					{
						m_selected = idx;
						break;
					}
				}
			}
		}
	}
}

void FactionSelectState::Draw()
{
	const float rw = static_cast<float>(g_Engine->m_RenderWidth);
	const float rh = static_cast<float>(g_Engine->m_RenderHeight);
	const float cx = rw * 0.5f;

	DrawRectangle(0, 0, g_Engine->m_RenderWidth, g_Engine->m_RenderHeight, Color{ 14, 16, 28, 255 });
	DrawRectangleLinesEx(Rectangle{ 3, 3, rw - 6, rh - 6 }, 1.0f, Color{ 80, 90, 140, 200 });

	const char* header = (m_slot == 0) ? "Choose Your Empire" : "Opponent Setup";
	const Vector2 hs = MeasureTextEx(*g_font, header, g_font->baseSize, 1);
	DrawOutlinedText(g_font, header, { cx - hs.x * 0.5f, 6.0f }, g_font->baseSize, 1, WHITE);

	char sub[96];
	if (m_phase == PhasePickColor)
		snprintf(sub, sizeof(sub), "Pick a color  (%d civilizations)", m_numSlots);
	else
		snprintf(sub, sizeof(sub), "Pick a civilization  (color: %s)",
			CivColorName(m_slots[m_slot].color));
	const Vector2 ss = MeasureTextEx(*g_smallFont, sub, g_smallFont->baseSize, 1);
	DrawOutlinedText(g_smallFont, sub, { cx - ss.x * 0.5f, 20.0f }, g_smallFont->baseSize, 1,
		Color{ 180, 190, 220, 255 });

	// Already-assigned summary strip.
	float ax = 8.0f;
	for (int i = 0; i < m_slot; ++i)
	{
		if (!m_slots[i].active)
			continue;
		const Color col = CivColorRgb(m_slots[i].color);
		DrawRectangle(static_cast<int>(ax), 34, 8, 8, col);
		DrawRectangleLines(static_cast<int>(ax), 34, 8, 8, BLACK);
		const char* nm = CivFaction(m_slots[i].faction).name;
		DrawOutlinedText(g_smallFont, nm, { ax + 10.0f, 33.0f }, g_smallFont->baseSize, 1,
			Color{ 200, 200, 210, 255 });
		ax += MeasureTextEx(*g_smallFont, nm, g_smallFont->baseSize, 1).x + 18.0f;
	}

	const float menuY = 52.0f;
	const float rowH = static_cast<float>(g_font->baseSize) + 6.0f;

	if (m_phase == PhasePickColor)
	{
		for (int i = 0; i < static_cast<int>(CivColor::Count); ++i)
		{
			const CivColor c = static_cast<CivColor>(i);
			const bool taken = ColorTaken(c);
			const bool selected = (i == m_selected);
			const float y = menuY + i * rowH;
			const Color rgb = CivColorRgb(c);

			DrawRectangle(24, static_cast<int>(y + 1), 12, 10, rgb);
			DrawRectangleLines(24, static_cast<int>(y + 1), 12, 10, BLACK);

			string label = CivColorName(c);
			if (taken)
				label += "  (taken)";
			Color textCol = taken ? Color{ 100, 100, 110, 255 }
				: selected ? Color{ 255, 230, 120, 255 }
				: Color{ 200, 210, 230, 255 };
			if (selected && !taken)
			{
				DrawRectangle(40, static_cast<int>(y - 1), 120, static_cast<int>(rowH - 1),
					Color{ 40, 50, 90, 220 });
				DrawOutlinedText(g_font, ">", { 44.0f, y }, g_font->baseSize, 1, textCol);
			}
			DrawOutlinedText(g_font, label, { 58.0f, y }, g_font->baseSize, 1, textCol);
		}
	}
	else
	{
		int nFac = 0;
		const CivFactionDef* facs = CivFactionList(&nFac);
		// Two columns if many factions (14) so they fit 270px height.
		const int cols = 2;
		const int rows = (nFac + cols - 1) / cols;
		for (int i = 0; i < nFac; ++i)
		{
			const bool taken = FactionTaken(facs[i].id);
			const bool selected = (i == m_selected);
			const int col = i / rows;
			const int row = i % rows;
			const float x = 20.0f + col * (rw * 0.48f);
			const float y = menuY + row * rowH;

			string label = string(facs[i].name) + "  (" + facs[i].leader + ")";
			if (taken)
				label += " *";
			Color textCol = taken ? Color{ 100, 100, 110, 255 }
				: selected ? Color{ 255, 230, 120, 255 }
				: Color{ 200, 210, 230, 255 };
			if (selected && !taken)
			{
				const Vector2 sz = MeasureTextEx(*g_font, label.c_str(), g_font->baseSize, 1);
				DrawRectangle(static_cast<int>(x - 4), static_cast<int>(y - 1),
					static_cast<int>(sz.x + 12), static_cast<int>(rowH - 1),
					Color{ 40, 50, 90, 220 });
			}
			// Color swatch of current pick
			if (selected)
			{
				const Color rgb = CivColorRgb(m_slots[m_slot].color);
				DrawRectangle(static_cast<int>(x - 16), static_cast<int>(y + 1), 10, 10, rgb);
				DrawRectangleLines(static_cast<int>(x - 16), static_cast<int>(y + 1), 10, 10, BLACK);
			}
			DrawOutlinedText(g_font, label, { x, y }, g_font->baseSize, 1, textCol);
		}
	}

	const char* hint = "Enter/click: choose   ESC: back   Colors are free — red is playable";
	const Vector2 ns = MeasureTextEx(*g_smallFont, hint, g_smallFont->baseSize, 1);
	DrawOutlinedText(g_smallFont, hint, { cx - ns.x * 0.5f, rh - 14.0f },
		g_smallFont->baseSize, 1, Color{ 150, 160, 190, 255 });
}
