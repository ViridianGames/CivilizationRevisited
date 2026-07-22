#include "CivFactions.h"
#include "CivMapGenerator.h"
#include "CivPlayer.h"
#include "CivTile.h"
#include "CustomizeWorldState.h"
#include "DifficultyState.h"
#include "FactionSelectState.h"
#include "MapViewerState.h"
#include "MainState.h"
#include "OptionsState.h"
#include "TitleState.h"
#include "raylib.h"
#include "Geist/RNG.h"

enum GameStates
{
    STATE_TITLESTATE = 0,
    STATE_MAINSTATE,
    STATE_OPTIONSSTATE,
    STATE_MAPVIEWERSTATE,
    STATE_CUSTOMIZEWORLDSTATE,
    STATE_DIFFICULTYSTATE,
    STATE_FACTIONSELECTSTATE,
    STATE_LASTSTATE
};

// Earth uses the same map viewer; title preloads earth data into g_ViewMap.
constexpr int STATE_EARTHMAPVIEWERSTATE = STATE_MAPVIEWERSTATE;

// Active map for MapViewerState (Earth loader or random generator writes here).
inline CivMapData g_ViewMap;

// New-game setup (difficulty, civs, factions, cities).
inline CivGameSetup g_GameSetup;

struct ConsoleString
{
    std::string m_String;
    Color m_Color;
    unsigned int m_StartTime;
};

inline float g_drawScale = 1.0f;

inline std::unique_ptr<RNG> g_vitalRNG;
inline std::unique_ptr<RNG> g_nonVitalRNG;

inline std::shared_ptr<Font> g_font;
inline std::shared_ptr<Font> g_smallFont;

void DrawOutlinedText(std::shared_ptr<Font> font, const std::string& text, Vector2 position, float fontSize, int spacing, Color color);

void DrawParagraph(std::shared_ptr<Font> font, const std::string& text, Vector2 position, float maxwidth, float fontSize, int spacing, Color color, bool outlined = false);

void DebugPrint(std::string text);
