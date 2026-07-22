#include "TitleState.h"

#include "Geist/Engine.h"
#include "Geist/Globals.h"
#include "Geist/StateMachine.h"
#include "Geist/RNG.h"

#include "CivGame.h"
#include "CivTerrain.h"
#include "GameGlobals.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace std;

namespace
{
	// Special target: start all-AI observe session (not a state id).
	constexpr int kTargetObserveGame = -2;

	struct MenuItem
	{
		const char* label;
		int targetState; // -1 = quit, -2 = observe game
	};

	const vector<MenuItem>& MenuItems()
	{
		static const vector<MenuItem> items = {
			{ "New Game", STATE_DIFFICULTYSTATE },
			{ "Observe Game", kTargetObserveGame },
			{ "Earth Map", STATE_EARTHMAPVIEWERSTATE },
		};
		return items;
	}

	void PrepareEarthMap()
	{
		g_ViewMap.Clear();
		g_ViewMap.title = "Earth Map";

		ifstream in("Images/civ_map/earth_terrain.bin", ios::binary);
		if (!in)
		{
			DebugPrint("Title: missing Images/civ_map/earth_terrain.bin");
			g_ViewMap.valid = false;
			return;
		}
		vector<uint8_t> bytes(static_cast<size_t>(kCivMapW * kCivMapH));
		in.read(reinterpret_cast<char*>(bytes.data()), static_cast<streamsize>(bytes.size()));
		if (in.gcount() != static_cast<streamsize>(bytes.size()))
		{
			DebugPrint("Title: earth_terrain.bin size mismatch");
			g_ViewMap.valid = false;
			return;
		}
		// masterWord 0: deterministic specials on Earth for viewing.
		if (!g_ViewMap.LoadTerrainBytes(bytes.data(), bytes.size(), kCivMapW, kCivMapH, 0))
		{
			g_ViewMap.valid = false;
			return;
		}
		g_ViewMap.title = "Earth Map";
		g_ViewMap.valid = true;
	}

	void StartObserveFromTitle()
	{
		if (!g_vitalRNG)
		{
			g_vitalRNG = std::make_unique<RNG>();
			g_vitalRNG->SeedFromSystemTimer();
		}
		else
			g_vitalRNG->SeedFromSystemTimer();

		if (!g_Game.StartObserveGame(*g_vitalRNG))
		{
			DebugPrint("Observe Game: map generation failed");
			return;
		}
		g_GameSetup = g_Game.setup;
		g_ViewMap = g_Game.map;
		g_StateMachine->MakeStateTransition(STATE_MAINSTATE);
	}

	void ActivateMenuItem(int target)
	{
		if (target < 0)
		{
			if (target == kTargetObserveGame)
				StartObserveFromTitle();
			else
				g_Engine->m_Done = true;
			return;
		}
		if (target == STATE_EARTHMAPVIEWERSTATE)
			PrepareEarthMap();
		if (target == STATE_DIFFICULTYSTATE)
		{
			g_GameSetup.Reset();
			g_GameSetup.newGameFlow = true;
		}
		g_StateMachine->MakeStateTransition(target);
	}
}

void TitleState::Init(const std::string& /*configfile*/)
{
	m_selected = 0;
	m_DrawCursor = true;
}

void TitleState::Shutdown()
{
}

void TitleState::OnEnter()
{
	m_selected = 0;
}

void TitleState::OnExit()
{
}

void TitleState::Update()
{
	const auto& items = MenuItems();
	if (items.empty())
		return;

	if (IsKeyPressed(KEY_ESCAPE))
	{
		g_Engine->m_Done = true;
		return;
	}

	if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
		m_selected = (m_selected + static_cast<int>(items.size()) - 1) % static_cast<int>(items.size());
	if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
		m_selected = (m_selected + 1) % static_cast<int>(items.size());

	const bool activate = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_KP_ENTER);

	const float rw = static_cast<float>(g_Engine->m_RenderWidth);
	const float rh = static_cast<float>(g_Engine->m_RenderHeight);
	const float menuY = rh * 0.55f;
	const float rowH = static_cast<float>(g_font->baseSize) + 8.0f;
	const float scaleX = g_Engine->m_ScreenWidth / static_cast<float>(g_Engine->m_RenderWidth);
	const float scaleY = g_Engine->m_ScreenHeight / static_cast<float>(g_Engine->m_RenderHeight);
	const float virtMouseX = GetMouseX() / scaleX;
	const float virtMouseY = GetMouseY() / scaleY;

	for (int i = 0; i < static_cast<int>(items.size()); ++i)
	{
		const Vector2 size = MeasureTextEx(*g_font, items[static_cast<size_t>(i)].label, g_font->baseSize, 1);
		const float x0 = rw * 0.5f - size.x * 0.5f - 12.0f;
		const float y0 = menuY + i * rowH - 2.0f;
		const float x1 = x0 + size.x + 24.0f;
		const float y1 = y0 + rowH;
		if (virtMouseX >= x0 && virtMouseX <= x1 && virtMouseY >= y0 && virtMouseY <= y1)
		{
			m_selected = i;
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			{
				ActivateMenuItem(items[static_cast<size_t>(i)].targetState);
				return;
			}
		}
	}

	if (activate)
		ActivateMenuItem(items[static_cast<size_t>(m_selected)].targetState);
}

void TitleState::Draw()
{
	const float rw = static_cast<float>(g_Engine->m_RenderWidth);
	const float rh = static_cast<float>(g_Engine->m_RenderHeight);

	DrawRectangle(0, 0, g_Engine->m_RenderWidth, g_Engine->m_RenderHeight, Color{ 8, 12, 28, 255 });

	const float cx = rw * 0.5f;
	const float cy = rh * 0.28f;

	const char* line1 = "Civilization";
	const char* line2 = "Revisited";
	const Vector2 s1 = MeasureTextEx(*g_font, line1, g_font->baseSize, 1);
	const Vector2 s2 = MeasureTextEx(*g_font, line2, g_font->baseSize, 1);
	DrawOutlinedText(g_font, line1, { cx - s1.x * 0.5f, cy }, g_font->baseSize, 1, WHITE);
	DrawOutlinedText(g_font, line2, { cx - s2.x * 0.5f, cy + g_font->baseSize + 4.0f }, g_font->baseSize, 1, WHITE);

	const auto& items = MenuItems();
	const float menuY = rh * 0.55f;
	const float rowH = static_cast<float>(g_font->baseSize) + 8.0f;

	for (int i = 0; i < static_cast<int>(items.size()); ++i)
	{
		const bool selected = (i == m_selected);
		const char* label = items[static_cast<size_t>(i)].label;
		const Vector2 size = MeasureTextEx(*g_font, label, g_font->baseSize, 1);
		const float x = cx - size.x * 0.5f;
		const float y = menuY + i * rowH;

		if (selected)
		{
			DrawRectangle(static_cast<int>(x - 12), static_cast<int>(y - 2),
				static_cast<int>(size.x + 24), static_cast<int>(rowH - 2),
				Color{ 30, 50, 90, 220 });
			const float markerW = MeasureTextEx(*g_font, ">", g_font->baseSize, 1).x;
			DrawOutlinedText(g_font, ">", { x - markerW - 6.0f, y },
				g_font->baseSize, 1, Color{ 255, 220, 120, 255 });
			DrawOutlinedText(g_font, label, { x, y }, g_font->baseSize, 1, Color{ 255, 220, 120, 255 });
		}
		else
		{
			DrawOutlinedText(g_font, label, { x, y }, g_font->baseSize, 1, Color{ 200, 210, 230, 255 });
		}
	}

	const char* hint = "Enter/click: select   ESC: quit";
	const Vector2 sh = MeasureTextEx(*g_smallFont, hint, g_smallFont->baseSize, 1);
	DrawOutlinedText(g_smallFont, hint,
		{ cx - sh.x * 0.5f, rh - 20.0f },
		g_smallFont->baseSize, 1, Color{ 180, 190, 210, 255 });
}
