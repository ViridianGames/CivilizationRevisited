#include "CustomizeWorldState.h"

#include "Geist/Engine.h"
#include "Geist/Globals.h"
#include "Geist/StateMachine.h"
#include "Geist/RNG.h"

#include "CivMapGenerator.h"
#include "GameGlobals.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace std;

namespace
{
	struct OptionMenu
	{
		const char* title;
		const char* choices[3];
	};

	const OptionMenu kMenus[CustomizeWorldState::StepCount] = {
		{ "LAND MASS:", { "Small", "Normal", "Large" } },
		{ "TEMPERATURE:", { "Cool", "Temperate", "Warm" } },
		{ "CLIMATE:", { "Arid", "Normal", "Wet" } },
		{ "AGE:", { "3 billion years", "4 billion years", "5 billion years" } },
	};

	const char* kHints[CustomizeWorldState::StepCount] = {
		"Small: more islands   Normal: mixed   Large: more continents",
		"Cool: arctic/tundra   Temperate: mixed   Warm: more desert",
		"Arid: desert/plains   Normal: mixed   Wet: jungle/swamp/rivers",
		"Younger: large terrain blobs   Older: broken-up features",
	};
}

void CustomizeWorldState::Init(const std::string& /*configfile*/)
{
	m_DrawCursor = true;
}

void CustomizeWorldState::Shutdown()
{
}

void CustomizeWorldState::OnEnter()
{
	m_step = StepLandMass;
	m_selected = 1;
	// Restore prior new-game world picks if any; else defaults.
	if (g_GameSetup.newGameFlow)
	{
		m_landMass = g_GameSetup.world.landMass;
		m_temperature = g_GameSetup.world.temperature;
		m_climate = g_GameSetup.world.climate;
		m_age = g_GameSetup.world.age;
	}
	else
	{
		m_landMass = 1;
		m_temperature = 1;
		m_climate = 1;
		m_age = 1;
	}
}

void CustomizeWorldState::OnExit()
{
}

void CustomizeWorldState::ActivateChoice(int choice)
{
	choice = std::clamp(choice, 0, 2);
	switch (m_step)
	{
	case StepLandMass: m_landMass = choice; break;
	case StepTemperature: m_temperature = choice; break;
	case StepClimate: m_climate = choice; break;
	case StepAge: m_age = choice; break;
	default: break;
	}

	if (m_step + 1 >= StepCount)
	{
		FinishAndGenerate();
		return;
	}
	m_step = static_cast<Step>(m_step + 1);
	m_selected = 1; // default Normal/Temperate/etc.
}

void CustomizeWorldState::FinishAndGenerate()
{
	CivWorldOptions opt;
	opt.landMass = m_landMass;
	opt.temperature = m_temperature;
	opt.climate = m_climate;
	opt.age = m_age;

	// New Game pipeline: store world options, then faction/color assignment.
	if (g_GameSetup.newGameFlow)
	{
		g_GameSetup.world = opt;
		g_StateMachine->MakeStateTransition(STATE_FACTIONSELECTSTATE);
		return;
	}

	// Random Map viewer path: generate immediately and show the map.
	if (!g_vitalRNG)
	{
		g_vitalRNG = std::make_unique<RNG>();
		g_vitalRNG->SeedFromSystemTimer();
	}
	else
	{
		g_vitalRNG->SeedFromSystemTimer();
	}

	if (!CivMapGenerator::Generate(g_ViewMap, opt, *g_vitalRNG))
	{
		DebugPrint("CustomizeWorld: map generation failed");
		g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
		return;
	}

	DebugPrint(string("Generated map: ") + g_ViewMap.title);
	g_StateMachine->MakeStateTransition(STATE_MAPVIEWERSTATE);
}

void CustomizeWorldState::Update()
{
	if (IsKeyPressed(KEY_ESCAPE))
	{
		if (m_step == StepLandMass)
		{
			if (g_GameSetup.newGameFlow)
				g_StateMachine->MakeStateTransition(STATE_DIFFICULTYSTATE);
			else
				g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
			return;
		}
		m_step = static_cast<Step>(m_step - 1);
		m_selected = 1;
		return;
	}

	if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
		m_selected = (m_selected + 2) % 3;
	if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
		m_selected = (m_selected + 1) % 3;

	const float rw = static_cast<float>(g_Engine->m_RenderWidth);
	const float rh = static_cast<float>(g_Engine->m_RenderHeight);
	const float menuY = rh * 0.42f;
	const float rowH = static_cast<float>(g_font->baseSize) + 10.0f;
	const float scaleX = g_Engine->m_ScreenWidth / static_cast<float>(g_Engine->m_RenderWidth);
	const float scaleY = g_Engine->m_ScreenHeight / static_cast<float>(g_Engine->m_RenderHeight);
	const float virtMouseX = GetMouseX() / scaleX;
	const float virtMouseY = GetMouseY() / scaleY;

	const auto& menu = kMenus[m_step];
	for (int i = 0; i < 3; ++i)
	{
		const Vector2 size = MeasureTextEx(*g_font, menu.choices[i], g_font->baseSize, 1);
		const float x0 = rw * 0.5f - size.x * 0.5f - 16.0f;
		const float y0 = menuY + i * rowH - 2.0f;
		const float x1 = x0 + size.x + 32.0f;
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

void CustomizeWorldState::Draw()
{
	const float rw = static_cast<float>(g_Engine->m_RenderWidth);
	const float rh = static_cast<float>(g_Engine->m_RenderHeight);
	const float cx = rw * 0.5f;

	DrawRectangle(0, 0, g_Engine->m_RenderWidth, g_Engine->m_RenderHeight, Color{ 12, 28, 20, 255 });

	// Soft vignette frame
	DrawRectangleLinesEx(Rectangle{ 4, 4, rw - 8, rh - 8 }, 1.0f, Color{ 40, 90, 60, 180 });

	const char* header = "Customize World";
	const Vector2 hs = MeasureTextEx(*g_font, header, g_font->baseSize, 1);
	DrawOutlinedText(g_font, header, { cx - hs.x * 0.5f, 12.0f }, g_font->baseSize, 1, Color{ 220, 240, 200, 255 });

	// Progress: completed choices
	static const char* kStepNames[] = { "Land", "Temp", "Climate", "Age" };
	const int vals[] = { m_landMass, m_temperature, m_climate, m_age };
	float progressX = 12.0f;
	for (int i = 0; i < StepCount; ++i)
	{
		Color col = (i < m_step) ? Color{ 160, 200, 140, 255 }
		          : (i == m_step) ? Color{ 255, 220, 120, 255 }
		          : Color{ 100, 120, 100, 200 };
		string label = kStepNames[i];
		if (i < m_step)
			label += string(": ") + kMenus[i].choices[vals[i]];
		DrawOutlinedText(g_smallFont, label, { progressX, 32.0f }, g_smallFont->baseSize, 1, col);
		progressX += MeasureTextEx(*g_smallFont, label.c_str(), g_smallFont->baseSize, 1).x + 14.0f;
	}

	const auto& menu = kMenus[m_step];
	const Vector2 ts = MeasureTextEx(*g_font, menu.title, g_font->baseSize, 1);
	DrawOutlinedText(g_font, menu.title, { cx - ts.x * 0.5f, rh * 0.30f },
		g_font->baseSize, 1, Color{ 180, 220, 160, 255 });

	const float menuY = rh * 0.42f;
	const float rowH = static_cast<float>(g_font->baseSize) + 10.0f;
	for (int i = 0; i < 3; ++i)
	{
		const bool selected = (i == m_selected);
		const char* label = menu.choices[i];
		const Vector2 size = MeasureTextEx(*g_font, label, g_font->baseSize, 1);
		const float x = cx - size.x * 0.5f;
		const float y = menuY + i * rowH;

		if (selected)
		{
			DrawRectangle(static_cast<int>(x - 14), static_cast<int>(y - 2),
				static_cast<int>(size.x + 28), static_cast<int>(rowH - 2),
				Color{ 30, 70, 45, 230 });
			const float markerW = MeasureTextEx(*g_font, ">", g_font->baseSize, 1).x;
			DrawOutlinedText(g_font, ">", { x - markerW - 6.0f, y },
				g_font->baseSize, 1, Color{ 255, 220, 120, 255 });
			DrawOutlinedText(g_font, label, { x, y }, g_font->baseSize, 1, Color{ 255, 220, 120, 255 });
		}
		else
		{
			DrawOutlinedText(g_font, label, { x, y }, g_font->baseSize, 1, Color{ 190, 210, 190, 255 });
		}
	}

	const char* hint = kHints[m_step];
	const Vector2 hsz = MeasureTextEx(*g_smallFont, hint, g_smallFont->baseSize, 1);
	DrawOutlinedText(g_smallFont, hint,
		{ cx - hsz.x * 0.5f, rh - 36.0f },
		g_smallFont->baseSize, 1, Color{ 150, 180, 150, 255 });

	const char* nav = "Enter/click: choose   ESC: back";
	const Vector2 ns = MeasureTextEx(*g_smallFont, nav, g_smallFont->baseSize, 1);
	DrawOutlinedText(g_smallFont, nav,
		{ cx - ns.x * 0.5f, rh - 18.0f },
		g_smallFont->baseSize, 1, Color{ 140, 160, 140, 255 });
}
