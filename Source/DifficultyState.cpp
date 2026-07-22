#include "DifficultyState.h"

#include "Geist/Engine.h"
#include "Geist/Globals.h"
#include "Geist/StateMachine.h"

#include "CivFactions.h"
#include "GameGlobals.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace std;

namespace
{
	const char* kDiffNames[] = {
		"Chieftain (easiest)",
		"Warlord",
		"Prince",
		"King",
		"Emperor",
		"Deity (toughest)",
	};
	constexpr int kDiffCount = 6;

	// Original offered 3–7; we allow 3–8 so all eight colors (incl. red) can be used.
	constexpr int kCivMin = 3;
	constexpr int kCivMax = 8;
	constexpr int kCivChoices = kCivMax - kCivMin + 1; // 6 choices

	string CivLabel(int n)
	{
		return to_string(n) + " Civilizations";
	}
}

void DifficultyState::Init(const std::string& /*configfile*/)
{
	m_DrawCursor = true;
}

void DifficultyState::Shutdown()
{
}

void DifficultyState::OnEnter()
{
	m_step = StepDifficulty;
	m_selected = 2;
	m_difficulty = 2;
	m_numCivs = 7;
	g_GameSetup.newGameFlow = true;
}

void DifficultyState::OnExit()
{
}

void DifficultyState::ActivateChoice(int choice)
{
	if (m_step == StepDifficulty)
	{
		m_difficulty = std::clamp(choice, 0, kDiffCount - 1);
		m_step = StepCompetition;
		// Default selection: 7 civs → index (7-3)=4
		m_selected = m_numCivs - kCivMin;
		return;
	}

	// Competition
	m_numCivs = std::clamp(kCivMin + choice, kCivMin, kCivMax);
	Finish();
}

void DifficultyState::Finish()
{
	g_GameSetup.difficulty = static_cast<CivDifficulty>(m_difficulty);
	g_GameSetup.numCivilizations = m_numCivs;
	g_GameSetup.newGameFlow = true;
	// World options next (same menus as random map).
	g_StateMachine->MakeStateTransition(STATE_CUSTOMIZEWORLDSTATE);
}

void DifficultyState::Update()
{
	const int itemCount = (m_step == StepDifficulty) ? kDiffCount : kCivChoices;

	if (IsKeyPressed(KEY_ESCAPE))
	{
		if (m_step == StepDifficulty)
		{
			g_GameSetup.Reset();
			g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
			return;
		}
		m_step = StepDifficulty;
		m_selected = m_difficulty;
		return;
	}

	if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
		m_selected = (m_selected + itemCount - 1) % itemCount;
	if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
		m_selected = (m_selected + 1) % itemCount;

	const float rw = static_cast<float>(g_Engine->m_RenderWidth);
	const float rh = static_cast<float>(g_Engine->m_RenderHeight);
	const float menuY = rh * 0.36f;
	const float rowH = static_cast<float>(g_font->baseSize) + 8.0f;
	const float scaleX = g_Engine->m_ScreenWidth / static_cast<float>(g_Engine->m_RenderWidth);
	const float scaleY = g_Engine->m_ScreenHeight / static_cast<float>(g_Engine->m_RenderHeight);
	const float virtMouseX = GetMouseX() / scaleX;
	const float virtMouseY = GetMouseY() / scaleY;

	for (int i = 0; i < itemCount; ++i)
	{
		string label = (m_step == StepDifficulty) ? kDiffNames[i] : CivLabel(kCivMin + i);
		const Vector2 size = MeasureTextEx(*g_font, label.c_str(), g_font->baseSize, 1);
		const float x0 = rw * 0.5f - size.x * 0.5f - 14.0f;
		const float y0 = menuY + i * rowH - 2.0f;
		const float x1 = x0 + size.x + 28.0f;
		const float y1 = y0 + rowH;
		if (virtMouseX >= x0 && virtMouseX <= x1 && virtMouseY >= y0 && virtMouseY <= y1)
		{
			m_selected = i;
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			{
				ActivateChoice(i);
				return;
			}
		}
	}

	if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_KP_ENTER))
		ActivateChoice(m_selected);
}

void DifficultyState::Draw()
{
	const float rw = static_cast<float>(g_Engine->m_RenderWidth);
	const float rh = static_cast<float>(g_Engine->m_RenderHeight);
	const float cx = rw * 0.5f;

	DrawRectangle(0, 0, g_Engine->m_RenderWidth, g_Engine->m_RenderHeight, Color{ 20, 18, 12, 255 });
	DrawRectangleLinesEx(Rectangle{ 4, 4, rw - 8, rh - 8 }, 1.0f, Color{ 120, 100, 50, 180 });

	const char* header = "New Game";
	const Vector2 hs = MeasureTextEx(*g_font, header, g_font->baseSize, 1);
	DrawOutlinedText(g_font, header, { cx - hs.x * 0.5f, 10.0f }, g_font->baseSize, 1,
		Color{ 255, 230, 160, 255 });

	const char* title = (m_step == StepDifficulty) ? "Difficulty Level..." : "Level of Competition...";
	const Vector2 ts = MeasureTextEx(*g_font, title, g_font->baseSize, 1);
	DrawOutlinedText(g_font, title, { cx - ts.x * 0.5f, 28.0f }, g_font->baseSize, 1,
		Color{ 220, 200, 140, 255 });

	if (m_step == StepCompetition)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "Difficulty: %s", kDiffNames[m_difficulty]);
		const Vector2 bs = MeasureTextEx(*g_smallFont, buf, g_smallFont->baseSize, 1);
		DrawOutlinedText(g_smallFont, buf, { cx - bs.x * 0.5f, 44.0f }, g_smallFont->baseSize, 1,
			Color{ 180, 170, 130, 255 });
	}

	const int itemCount = (m_step == StepDifficulty) ? kDiffCount : kCivChoices;
	const float menuY = rh * 0.36f;
	const float rowH = static_cast<float>(g_font->baseSize) + 8.0f;

	for (int i = 0; i < itemCount; ++i)
	{
		string label = (m_step == StepDifficulty) ? kDiffNames[i] : CivLabel(kCivMin + i);
		const bool selected = (i == m_selected);
		const Vector2 size = MeasureTextEx(*g_font, label.c_str(), g_font->baseSize, 1);
		const float x = cx - size.x * 0.5f;
		const float y = menuY + i * rowH;

		if (selected)
		{
			DrawRectangle(static_cast<int>(x - 12), static_cast<int>(y - 2),
				static_cast<int>(size.x + 24), static_cast<int>(rowH - 2),
				Color{ 70, 55, 25, 230 });
			DrawOutlinedText(g_font, ">", { x - 12.0f, y }, g_font->baseSize, 1,
				Color{ 255, 220, 100, 255 });
			DrawOutlinedText(g_font, label, { x, y }, g_font->baseSize, 1,
				Color{ 255, 230, 120, 255 });
		}
		else
		{
			DrawOutlinedText(g_font, label, { x, y }, g_font->baseSize, 1,
				Color{ 200, 190, 160, 255 });
		}
	}

	const char* hint = (m_step == StepDifficulty)
		? "Enter/click: choose   ESC: cancel"
		: "Includes you + AI opponents   ESC: back";
	const Vector2 ns = MeasureTextEx(*g_smallFont, hint, g_smallFont->baseSize, 1);
	DrawOutlinedText(g_smallFont, hint, { cx - ns.x * 0.5f, rh - 16.0f },
		g_smallFont->baseSize, 1, Color{ 160, 150, 120, 255 });
}
