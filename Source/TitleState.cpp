#include <string>
#include "Geist/Engine.h"
#include "Geist/Globals.h"

#include "GameGlobals.h"

using namespace std;

void TitleState::Init(const std::string& configfile)
{
}

void TitleState::Shutdown()
{
}

void TitleState::OnEnter()
{
}

void TitleState::OnExit()
{
}

void TitleState::Update()
{
	if (IsKeyPressed(KEY_ESCAPE))
	{
		g_Engine->m_Done = true;
	}
}

void TitleState::Draw()
{
	DrawRectangle(0, 0, g_Engine->m_RenderWidth, g_Engine->m_RenderHeight, Color{ 8, 12, 28, 255 });

	const float cx = static_cast<float>(g_Engine->m_RenderWidth) * 0.5f;
	const float cy = static_cast<float>(g_Engine->m_RenderHeight) * 0.35f;

	// Simple centered title
	const char* line1 = "Civilization";
	const char* line2 = "Revisited";
	Vector2 s1 = MeasureTextEx(*g_font, line1, g_font->baseSize, 1);
	Vector2 s2 = MeasureTextEx(*g_font, line2, g_font->baseSize, 1);
	DrawOutlinedText(g_font, line1, { cx - s1.x * 0.5f, cy }, g_font->baseSize, 1, WHITE);
	DrawOutlinedText(g_font, line2, { cx - s2.x * 0.5f, cy + g_font->baseSize + 4.0f }, g_font->baseSize, 1, WHITE);

	const char* hint = "ESC: quit";
	Vector2 sh = MeasureTextEx(*g_smallFont, hint, g_smallFont->baseSize, 1);
	DrawOutlinedText(g_smallFont, hint,
		{ cx - sh.x * 0.5f, static_cast<float>(g_Engine->m_RenderHeight) - 24.0f },
		g_smallFont->baseSize, 1, Color{ 180, 190, 210, 255 });
}
