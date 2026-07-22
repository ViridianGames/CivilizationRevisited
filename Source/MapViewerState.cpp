#include "MapViewerState.h"

#include "Geist/Engine.h"
#include "Geist/Globals.h"
#include "Geist/ResourceManager.h"
#include "Geist/StateMachine.h"

#include "CivGame.h"
#include "CivTerritory.h"
#include "CivUnits.h"
#include "GameGlobals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

namespace
{
	constexpr float kPanRepeatSeconds = 0.08f;
	constexpr const char* kEarthMapPath = "Images/civ_map/earth_terrain.bin";

	const char* kTerrainNames[MapViewerState::kTerrainCount] = {
		"desert", "plains", "grassland", "forest", "hills", "mountains",
		"tundra", "arctic", "swamp", "jungle", "ocean", "river",
	};

	Color TerrainMinimapColor(uint8_t t)
	{
		switch (t)
		{
		case CivTerrain::Desert: return Color{ 210, 180, 80, 255 };
		case CivTerrain::Plains: return Color{ 200, 180, 100, 255 };
		case CivTerrain::Grassland: return Color{ 60, 160, 50, 255 };
		case CivTerrain::Forest: return Color{ 20, 100, 20, 255 };
		case CivTerrain::Hills: return Color{ 140, 110, 60, 255 };
		case CivTerrain::Mountains: return Color{ 110, 110, 110, 255 };
		case CivTerrain::Tundra: return Color{ 200, 200, 210, 255 };
		case CivTerrain::Arctic: return Color{ 240, 240, 250, 255 };
		case CivTerrain::Swamp: return Color{ 90, 90, 50, 255 };
		case CivTerrain::Jungle: return Color{ 30, 120, 40, 255 };
		case CivTerrain::Ocean: return Color{ 40, 50, 160, 255 };
		case CivTerrain::River: return Color{ 60, 110, 210, 255 };
		default: return Color{ 255, 0, 255, 255 };
		}
	}

	void DrawTexScaled(Texture* tex, float dx, float dy, float size)
	{
		if (!tex || tex->id == 0)
			return;
		const Rectangle src{ 0, 0, static_cast<float>(tex->width), static_cast<float>(tex->height) };
		const Rectangle dst{ dx, dy, size, size };
		DrawTexturePro(*tex, src, dst, Vector2{ 0, 0 }, 0.0f, WHITE);
	}

	// Pixel-perfect 1px strokes (inclusive endpoints). Avoid DrawLine — it is
	// not grid-aligned and mis-joins with corner pixels.
	void DrawHStroke(int x0, int x1, int y, Color col)
	{
		if (x1 < x0)
			std::swap(x0, x1);
		const int w = x1 - x0 + 1;
		if (w <= 0)
			return;
		DrawRectangle(x0, y, w, 1, col);
	}

	void DrawVStroke(int x, int y0, int y1, Color col)
	{
		if (y1 < y0)
			std::swap(y0, y1);
		const int h = y1 - y0 + 1;
		if (h <= 0)
			return;
		DrawRectangle(x, y0, 1, h, col);
	}
}

void MapViewerState::Init(const std::string& /*configfile*/)
{
	m_DrawCursor = true;
	m_minimapImage = {};
	m_minimapTex = {};
	m_minimapReady = false;
}

void MapViewerState::Shutdown()
{
	m_map.Clear();
	if (m_minimapReady)
	{
		UnloadTexture(m_minimapTex);
		UnloadImage(m_minimapImage);
		m_minimapReady = false;
	}
	for (int t = 0; t < 28; ++t)
	{
		for (int c = 0; c < static_cast<int>(CivColor::Count); ++c)
		{
			if (m_unitSpriteReady[t][c])
			{
				UnloadTexture(m_unitSprites[t][c]);
				m_unitSpriteReady[t][c] = false;
			}
		}
	}
	if (m_fortifyReady)
	{
		UnloadTexture(m_fortifyOverlay);
		m_fortifyReady = false;
	}
	if (m_fortressReady)
	{
		UnloadTexture(m_fortressOverlay);
		m_fortressReady = false;
	}
	if (m_cityDigitsReady)
	{
		for (int i = 0; i < 10; ++i)
			UnloadTexture(m_cityDigits[i]);
		m_cityDigitsReady = false;
	}
}

void MapViewerState::OnEnter()
{
	m_camX = 0;
	m_camY = 0;
	m_scale = kMapScale; // fixed max zoom (no wheel zoom)
	m_panCooldown = 0.0f;

	if (!LoadTileAssets())
		DebugPrint("MapViewer: failed to load tile textures");

	m_cities.clear();

	if (g_ViewMap.valid && !g_ViewMap.tiles.empty())
	{
		m_map = g_ViewMap;
		// Snapshot cities from each player's list for drawing.
		for (const CivPlayer& pl : g_GameSetup.players)
			for (const CivCity& c : pl.cities)
				if (c.Valid())
					m_cities.push_back(c);
	}
	else
	{
		m_map.Clear();
		m_map.Resize(kCivMapW, kCivMapH, CivTerrain::Ocean);
		m_map.title = "Earth Map";
		ifstream in(kEarthMapPath, ios::binary);
		if (in)
		{
			vector<uint8_t> bytes(static_cast<size_t>(kCivMapW * kCivMapH));
			in.read(reinterpret_cast<char*>(bytes.data()), static_cast<streamsize>(bytes.size()));
			if (in.gcount() == static_cast<streamsize>(bytes.size()))
			{
				m_map.LoadTerrainBytes(bytes.data(), bytes.size(), kCivMapW, kCivMapH, 0);
				m_map.title = "Earth Map";
			}
			else
			{
				DebugPrint("MapViewer: earth_terrain.bin size mismatch");
				m_map.Clear();
			}
		}
		else
		{
			DebugPrint(string("MapViewer: missing ") + kEarthMapPath);
			m_map.Clear();
		}
	}

	// Center camera near human capital if present.
	if (!m_cities.empty() && g_GameSetup.PlayerCount() > 0)
	{
		const int human = g_GameSetup.HumanIndex();
		for (const CivCity& c : m_cities)
		{
			if (human >= 0 && c.owner == human)
			{
				CenterOn(c.x, c.y);
				break;
			}
		}
	}

	// Prefer live session map if a game is running.
	if (g_Game.started && !g_Game.map.tiles.empty())
	{
		m_map = g_Game.map;
		m_cities.clear();
		for (const CivPlayer& pl : g_Game.setup.players)
			for (const CivCity& c : pl.cities)
				if (c.Valid())
					m_cities.push_back(c);
		m_units.clear();
		for (const auto& u : g_Game.units)
		{
			if (!u.Valid())
				continue;
			m_units.push_back({ u.x, u.y, u.owner, u.typeId, u.fortify });
		}
	}

	// Fog on whenever a game session exists (human or observe-all-AI).
	m_fogEnabled = g_Game.started
		|| (g_GameSetup.Human() != nullptr)
		|| (g_GameSetup.PlayerCount() > 0);
	m_bordersEnabled = true;
	if (g_Game.started && g_Game.IsObserveMode() && m_fogPlayerIndex < 0)
		m_fogPlayerIndex = 0;
	RefreshTerritoryAndFog();

	RebuildMinimap();
	ClampCamera();
}

void MapViewerState::SyncFromGame()
{
	if (!g_Game.started)
		return;
	m_map = g_Game.map;
	// Keep fog maps on players in g_Game.setup.
	g_GameSetup = g_Game.setup;
	g_ViewMap = g_Game.map;

	m_cities.clear();
	for (const CivPlayer& pl : g_Game.setup.players)
		for (const CivCity& c : pl.cities)
			if (c.Valid())
				m_cities.push_back(c);
	m_units.clear();
	for (const auto& u : g_Game.units)
	{
		if (!u.Valid())
			continue;
		m_units.push_back({ u.x, u.y, u.owner, u.typeId, u.fortify });
	}
	RefreshTerritoryAndFog();
	RebuildMinimap();
}

void MapViewerState::RefreshTerritoryAndFog()
{
	if (m_map.tiles.empty())
	{
		m_territory.Clear();
		return;
	}

	CivGameSetup& setup = g_Game.started ? g_Game.setup : g_GameSetup;

	// Continents required so ownership cannot jump across open ocean.
	m_map.ComputeContinents();

	// History/replay map: land within Euclidean radius 5 of each city
	// (same continent only).
	m_territory.Compute(m_map, setup);
	CivApplyTerritoryToMap(m_map, m_territory);

	// Current vision from cities + units; explored accumulates.
	CivRebuildAllVisibility(setup, m_map, kCivCityVisionRange);
	// Unit vision (range 1).
	if (g_Game.started)
	{
		for (const auto& u : g_Game.units)
		{
			if (!u.Valid())
				continue;
			CivPlayer* p = setup.PlayerAt(u.owner);
			if (!p)
				continue;
			CivAddVision(*p, u.x, u.y, kCivUnitVisionRange, m_map.width, m_map.height);
		}
	}
}

void MapViewerState::SetFogPlayerIndex(int playerIndex)
{
	m_fogPlayerIndex = playerIndex;
	// Minimap dots / dimming depend on which civ is observed.
	RebuildMinimap();
}

const CivPlayer* MapViewerState::FogViewer() const
{
	if (!m_fogEnabled)
		return nullptr;

	const CivGameSetup& setup = g_Game.started ? g_Game.setup : g_GameSetup;

	// Explicit observe / selected civ takes priority.
	if (m_fogPlayerIndex >= 0)
	{
		const CivPlayer* p = setup.PlayerAt(m_fogPlayerIndex);
		if (p && p->HasFog())
			return p;
	}

	// Normal game: human fog.
	if (const CivPlayer* human = setup.Human())
	{
		if (human->HasFog())
			return human;
	}

	// Observe fallback: first living player with fog maps.
	for (int i = 0; i < setup.PlayerCount(); ++i)
	{
		const CivPlayer* p = setup.PlayerAt(i);
		if (p && p->IsAlive() && p->HasFog())
			return p;
	}
	return nullptr;
}

bool MapViewerState::TileExplored(int mx, int my) const
{
	const CivPlayer* v = FogViewer();
	if (!v)
		return true; // no fog → everything known
	return v->IsExplored(mx, my);
}

bool MapViewerState::TileVisible(int mx, int my) const
{
	const CivPlayer* v = FogViewer();
	if (!v)
		return true;
	return v->IsVisible(mx, my);
}

void MapViewerState::OnExit()
{
}

bool MapViewerState::LoadTileAssets()
{
	auto loadPoint = [](const string& path) -> Texture* {
		Texture* tex = g_ResourceManager->GetTexture(path, false);
		if (tex && tex->id != 0)
		{
			SetTextureFilter(*tex, TEXTURE_FILTER_POINT);
			SetTextureWrap(*tex, TEXTURE_WRAP_CLAMP);
		}
		return tex;
	};

	for (int i = 0; i < kTerrainCount; ++i)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "Images/civ_tiles/base_%02d_%s.png", i, kTerrainNames[i]);
		m_tilePaths[i] = buf;
		loadPoint(m_tilePaths[i]);

		snprintf(buf, sizeof(buf), "Images/civ_tiles/special_%02d_%s.png", i, kTerrainNames[i]);
		m_specialPaths[i] = buf;
		loadPoint(m_specialPaths[i]);
	}

	for (int d = 0; d < kRiverDirCount; ++d)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "Images/civ_tiles/river_d%02d.png", d);
		m_riverPaths[d] = buf;
		loadPoint(m_riverPaths[d]);
	}

	m_shieldPath = "Images/civ_tiles/special_shield.png";
	m_hutPath = "Images/civ_tiles/special_hut.png";
	m_cityIconPath = "Images/civ_tiles/city_icon.png";
	m_cityWallsPath = "Images/civ_tiles/city_walls.png";
	m_landBasePath = "Images/civ_tiles/land_base.png";
	m_oceanBasePath = "Images/civ_tiles/ocean_base.png";
	m_ter257Path = "Images/civ_tiles/ter257_sheet.png";
	m_sp257Path = "Images/civ_tiles/sp257_sheet.png";
	m_sp299Path = "Images/civ_tiles/sp299_sheet.png";
	loadPoint(m_shieldPath);
	loadPoint(m_hutPath);
	loadPoint(m_cityIconPath);
	loadPoint(m_cityWallsPath);
	loadPoint(m_landBasePath);
	loadPoint(m_oceanBasePath);
	loadPoint(m_ter257Path);
	loadPoint(m_sp257Path);
	loadPoint(m_sp299Path);
	for (int i = 0; i < 4; ++i)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "Images/civ_tiles/coast_corner_%d.png", i);
		m_coastCornerPaths[i] = buf;
		loadPoint(m_coastCornerPaths[i]);
	}
	return true;
}

void MapViewerState::RebuildMinimap()
{
	if (m_minimapReady)
	{
		UnloadTexture(m_minimapTex);
		UnloadImage(m_minimapImage);
		m_minimapReady = false;
	}

	if (m_map.tiles.empty() || m_map.width <= 0 || m_map.height <= 0)
		return;

	const int w = m_map.width;
	const int h = m_map.height;
	m_minimapImage = GenImageColor(w, h, BLACK);
	const CivPlayer* fog = FogViewer();
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x)
		{
			// Unexplored stays black on the minimap.
			if (fog && !fog->IsExplored(x, y))
			{
				ImageDrawPixel(&m_minimapImage, x, y, BLACK);
				continue;
			}
			const CivTile& tile = m_map.tiles[m_map.Index(x, y)];
			Color c = TerrainMinimapColor(tile.terrain);
			// Brighten specials slightly so they read on the mini map.
			if (tile.HasResource())
			{
				c.r = static_cast<unsigned char>(std::min(255, c.r + 40));
				c.g = static_cast<unsigned char>(std::min(255, c.g + 40));
				c.b = static_cast<unsigned char>(std::min(255, c.b + 20));
			}
			// Dim tiles outside current LOS (explored memory).
			if (fog && !fog->IsVisible(x, y))
			{
				c.r = static_cast<unsigned char>(c.r / 3);
				c.g = static_cast<unsigned char>(c.g / 3);
				c.b = static_cast<unsigned char>(c.b / 3);
			}
			ImageDrawPixel(&m_minimapImage, x, y, c);
		}
	// City dots on minimap (only if explored / own).
	for (const CivCity& city : m_cities)
	{
		if (city.x < 0 || city.y < 0 || city.x >= w || city.y >= h)
			continue;
		if (fog && !fog->IsExplored(city.x, city.y)
			&& !(fog->id == city.owner))
			continue;
		Color col = WHITE;
		if (const CivPlayer* pl = g_GameSetup.PlayerAt(city.owner))
			col = pl->ColorRgb();
		ImageDrawPixel(&m_minimapImage, city.x, city.y, col);
	}
	m_minimapTex = LoadTextureFromImage(m_minimapImage);
	SetTextureFilter(m_minimapTex, TEXTURE_FILTER_POINT);
	SetTextureWrap(m_minimapTex, TEXTURE_WRAP_CLAMP);
	m_minimapReady = true;
}

const CivTile& MapViewerState::TileAt(int mx, int my) const
{
	return m_map.TileAt(mx, my);
}

int MapViewerState::RiverDirections(int mx, int my) const
{
	auto connects = [&](int x, int y) -> bool {
		return CivTerrain::IsRiverOrOcean(TileAt(x, y).terrain);
	};
	int dirs = 0;
	if (connects(mx, my - 1)) dirs |= 1;
	if (connects(mx + 1, my)) dirs |= 2;
	if (connects(mx, my + 1)) dirs |= 4;
	if (connects(mx - 1, my)) dirs |= 8;
	return dirs;
}

int MapViewerState::SameTypeMask(int mx, int my, uint8_t terrain) const
{
	// CivOne land autotile: only cardinal neighbors of the exact same terrain type.
	// Column in TER257 = this mask (N=1 E=2 S=4 W=8).
	int mask = 0;
	if (my > 0 && TileAt(mx, my - 1).terrain == terrain)
		mask |= DirN;
	if (TileAt(mx + 1, my).terrain == terrain)
		mask |= DirE;
	if (my + 1 < m_map.height && TileAt(mx, my + 1).terrain == terrain)
		mask |= DirS;
	if (TileAt(mx - 1, my).terrain == terrain)
		mask |= DirW;
	return mask & 15;
}

void MapViewerState::DrawTerrainCell(int mx, int my, float dstX, float dstY, float tilePx) const
{
	const CivTile& tile = TileAt(mx, my);
	const uint8_t t = tile.terrain;

	if (t == CivTerrain::Ocean)
	{
		// Solid ocean base + coast/lake shore pieces.
		if (Texture* base = g_ResourceManager->GetTexture(m_oceanBasePath, false))
			DrawTexScaled(base, dstX, dstY, tilePx);
		else if (Texture* fallback = TileTexture(CivTerrain::Ocean))
			DrawTexScaled(fallback, dstX, dstY, tilePx);
		else
			DrawRectangle(static_cast<int>(dstX), static_cast<int>(dstY),
				static_cast<int>(tilePx), static_cast<int>(tilePx), DARKBLUE);
		DrawOceanCoasts(mx, my, dstX, dstY, tilePx);
		return;
	}

	if (t == CivTerrain::River)
	{
		// SP257 river directions (already pre-composited on land base).
		if (Texture* river = RiverTexture(mx, my))
			DrawTexScaled(river, dstX, dstY, tilePx);
		else if (Texture* land = g_ResourceManager->GetTexture(m_landBasePath, false))
			DrawTexScaled(land, dstX, dstY, tilePx);
		return;
	}

	// Land: LandBase + TER257[sameTypeMask * 16, terrainId * 16].
	if (Texture* land = g_ResourceManager->GetTexture(m_landBasePath, false))
		DrawTexScaled(land, dstX, dstY, tilePx);

	Texture* ter = g_ResourceManager->GetTexture(m_ter257Path, false);
	if (ter && ter->id != 0 && t < CivTerrain::Ocean)
	{
		const int mask = SameTypeMask(mx, my, t);
		const float sx = static_cast<float>(mask * kTileSize);
		const float sy = static_cast<float>(t * kTileSize);
		DrawSheetSrc(ter, sx, sy, static_cast<float>(kTileSize), static_cast<float>(kTileSize),
			dstX, dstY, tilePx, tilePx);
	}
	else
	{
		// Fallback: pre-composited column-0 base tile.
		if (Texture* fallback = TileTexture(t))
			DrawTexScaled(fallback, dstX, dstY, tilePx);
	}
}

Texture* MapViewerState::TileTexture(int terrainId) const
{
	if (terrainId < 0 || terrainId >= kTerrainCount)
		terrainId = CivTerrain::Ocean;
	return g_ResourceManager->GetTexture(m_tilePaths[terrainId], false);
}

Texture* MapViewerState::RiverTexture(int mx, int my) const
{
	const int d = RiverDirections(mx, my) & (kRiverDirCount - 1);
	return g_ResourceManager->GetTexture(m_riverPaths[d], false);
}

Texture* MapViewerState::SpecialTexture(const CivTile& tile) const
{
	const CivResource res = tile.Resource();
	if (res == CivResource::None)
		return nullptr;
	if (res == CivResource::Shield)
		return g_ResourceManager->GetTexture(m_shieldPath, false);
	// SP257 specials are indexed by terrain id (matches CivOne Terrain enum 0..10).
	if (tile.terrain >= 0 && tile.terrain < kTerrainCount && tile.terrain != CivTerrain::River)
		return g_ResourceManager->GetTexture(m_specialPaths[tile.terrain], false);
	return nullptr;
}

int MapViewerState::LandNeighborMask(int mx, int my) const
{
	// Any non-ocean neighbor counts as land (rivers count as land for coasts).
	struct Off { int dx, dy, bit; };
	static const Off kOff[] = {
		{ 0, -1, DirN }, { 1, 0, DirE }, { 0, 1, DirS }, { -1, 0, DirW },
		{ -1, -1, DirNW }, { 1, -1, DirNE }, { -1, 1, DirSW }, { 1, 1, DirSE },
	};
	int mask = 0;
	for (const Off& o : kOff)
	{
		const int ny = my + o.dy;
		if (ny < 0 || ny >= m_map.height)
			continue;
		if (!CivTerrain::IsOcean(TileAt(mx + o.dx, ny).terrain))
			mask |= o.bit;
	}
	return mask;
}

int MapViewerState::RiverNeighborMask(int mx, int my) const
{
	// Cardinal rivers only (for river-mouth overlays on ocean).
	struct Off { int dx, dy, bit; };
	static const Off kOff[] = {
		{ 0, -1, DirN }, { 1, 0, DirE }, { 0, 1, DirS }, { -1, 0, DirW },
	};
	int mask = 0;
	for (const Off& o : kOff)
	{
		const int ny = my + o.dy;
		if (ny < 0 || ny >= m_map.height)
			continue;
		if (TileAt(mx + o.dx, ny).terrain == CivTerrain::River)
			mask |= o.bit;
	}
	return mask;
}

void MapViewerState::DrawSheetSrc(Texture* sheet, float sx, float sy, float sw, float sh,
	float dx, float dy, float dw, float dh) const
{
	if (!sheet || sheet->id == 0)
		return;
	const Rectangle src{ sx, sy, sw, sh };
	const Rectangle dst{ dx, dy, dw, dh };
	DrawTexturePro(*sheet, src, dst, Vector2{ 0, 0 }, 0.0f, WHITE);
}

bool MapViewerState::TileConnectsRoad(int mx, int my, bool rail) const
{
	if (my < 0 || my >= m_map.height)
		return false;
	// X wraps.
	while (mx < 0)
		mx += m_map.width;
	mx %= m_map.width;

	// Cities always connect (roads run into city tiles).
	for (const CivCity& c : m_cities)
	{
		if (c.Valid() && c.x == mx && c.y == my)
			return true;
	}

	const CivTile& t = TileAt(mx, my);
	if (t.IsOcean())
		return false;
	if (rail)
		return t.HasRail();
	// Road layer also joins to railroad tiles.
	return t.HasRoad() || t.HasRail();
}

int MapViewerState::RoadConnectionMask(int mx, int my, bool rail) const
{
	// CivOne MapTile.GetRoad / GetRailRoad: Clockwise N,NE,E,SE,S,SW,W,NW.
	struct Off { int dx, dy, bit; };
	static const Off kCW[8] = {
		{ 0, -1, DirN }, { 1, -1, DirNE }, { 1, 0, DirE }, { 1, 1, DirSE },
		{ 0, 1, DirS }, { -1, 1, DirSW }, { -1, 0, DirW }, { -1, -1, DirNW },
	};
	int mask = 0;
	for (const Off& o : kCW)
	{
		if (TileConnectsRoad(mx + o.dx, my + o.dy, rail))
			mask |= o.bit;
	}
	return mask;
}

void MapViewerState::EnsureFortressOverlay() const
{
	if (m_fortressReady)
		return;
	// SP257[224,112,16,16], palette cyan (idx 3) → transparent.
	Image sheet = LoadImage(m_sp257Path.c_str());
	if (sheet.data == nullptr)
		sheet = LoadImage("Images/civ_extract/SP257_320x200.png");
	if (sheet.data == nullptr)
		return;
	Image fort = ImageFromImage(sheet, Rectangle{ 224, 112, 16, 16 });
	UnloadImage(sheet);
	if (fort.data == nullptr)
		return;
	ImageColorReplace(&fort, CivVgaPaletteColor(3), BLANK);
	m_fortressOverlay = LoadTextureFromImage(fort);
	UnloadImage(fort);
	if (m_fortressOverlay.id != 0)
	{
		SetTextureFilter(m_fortressOverlay, TEXTURE_FILTER_POINT);
		SetTextureWrap(m_fortressOverlay, TEXTURE_WRAP_CLAMP);
		m_fortressReady = true;
	}
}

void MapViewerState::EnsureCityDigits() const
{
	if (m_cityDigitsReady)
		return;

	// Prefer pre-sliced PNGs if present; else parse original FONTS.CV font 0.
	bool allOk = true;
	for (int d = 0; d < 10; ++d)
	{
		char path[64];
		snprintf(path, sizeof(path), "Images/civ_tiles/digits/digit_%d.png", d);
		Image img = LoadImage(path);
		if (img.data == nullptr)
		{
			allOk = false;
			// unload any already loaded
			for (int j = 0; j < d; ++j)
				UnloadTexture(m_cityDigits[j]);
			break;
		}
		m_cityDigits[d] = LoadTextureFromImage(img);
		UnloadImage(img);
		if (m_cityDigits[d].id == 0)
		{
			allOk = false;
			for (int j = 0; j <= d; ++j)
				if (m_cityDigits[j].id != 0)
					UnloadTexture(m_cityDigits[j]);
			break;
		}
		SetTextureFilter(m_cityDigits[d], TEXTURE_FILTER_POINT);
		SetTextureWrap(m_cityDigits[d], TEXTURE_WRAP_CLAMP);
	}
	if (allOk)
	{
		m_cityDigitsReady = true;
		return;
	}

	// Parse FONTS.CV (CivOne Fontset format).
	// Header: u16 fontCount, then fontCount × u16 offsets.
	// At each offset: glyph bitmaps; metadata at offset-8..offset-2.
	FILE* f = fopen("Data/CIV/FONTS.CV", "rb");
	if (!f)
		return;
	fseek(f, 0, SEEK_END);
	const long fileLen = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fileLen <= 0 || fileLen > 2 * 1024 * 1024)
	{
		fclose(f);
		return;
	}
	std::vector<uint8_t> bytes(static_cast<size_t>(fileLen));
	if (fread(bytes.data(), 1, bytes.size(), f) != bytes.size())
	{
		fclose(f);
		return;
	}
	fclose(f);

	auto u16 = [&](int i) -> uint16_t {
		return static_cast<uint16_t>(bytes[static_cast<size_t>(i)]
			| (bytes[static_cast<size_t>(i + 1)] << 8));
	};
	const int fontCount = u16(0);
	if (fontCount <= 0 || fontCount > 32)
		return;
	const int offset = u16(2); // font 0
	if (offset < 16 || offset >= static_cast<int>(bytes.size()))
		return;

	const int first = bytes[static_cast<size_t>(offset - 8)];
	const int last = bytes[static_cast<size_t>(offset - 7)];
	const int charByteLength = bytes[static_cast<size_t>(offset - 6)];
	const int top = bytes[static_cast<size_t>(offset - 5)];
	const int bottom = bytes[static_cast<size_t>(offset - 4)];
	const int height = 1 + bottom - top;
	const int charCount = 1 + last - first;
	if (charByteLength <= 0 || height <= 0 || charCount <= 0)
		return;

	int index = offset;
	int i = 0;
	for (int c = first; c <= last; ++c)
	{
		++i;
		std::vector<uint8_t> raw(static_cast<size_t>(height * charByteLength));
		for (int row = 0; row < height; ++row)
		{
			for (int col = 0; col < charByteLength; ++col)
			{
				const int ind = row * charByteLength + col;
				const int bin = index + (row * (charByteLength * charCount)) + col;
				if (bin < 0 || bin >= static_cast<int>(bytes.size()))
					return;
				raw[static_cast<size_t>(ind)] = bytes[static_cast<size_t>(bin)];
			}
		}
		const int widthPos = offset - 9 - charCount + i;
		int charWidth = (widthPos >= 0 && widthPos < static_cast<int>(bytes.size()))
			? bytes[static_cast<size_t>(widthPos)]
			: charByteLength * 8;
		if (charWidth > charByteLength * 8)
			charWidth = charByteLength * 8;

		if (c >= '0' && c <= '9')
		{
			const int d = c - '0';
			Image img = GenImageColor(charWidth, height, BLANK);
			int b = 0, bit = 0;
			for (int y = 0; y < height; ++y)
			{
				if (bit > 0)
				{
					bit = 0;
					++b;
				}
				for (int x = 0; x < charByteLength * 8; ++x)
				{
					if (x < charWidth)
					{
						const bool on = (raw[static_cast<size_t>(b)] & (0x80 >> bit)) != 0;
						if (on)
							ImageDrawPixel(&img, x, y, BLACK);
					}
					if (++bit == 8)
					{
						bit = 0;
						++b;
					}
				}
			}
			m_cityDigits[d] = LoadTextureFromImage(img);
			UnloadImage(img);
			if (m_cityDigits[d].id != 0)
			{
				SetTextureFilter(m_cityDigits[d], TEXTURE_FILTER_POINT);
				SetTextureWrap(m_cityDigits[d], TEXTURE_WRAP_CLAMP);
			}
		}
		index += charByteLength;
	}
	m_cityDigitsReady = true;
	// Persist slices for faster future loads.
	// (optional — PNGs already generated offline)
}

void MapViewerState::DrawCitySizeNumber(int size, float fillX, float fillY, float fillS) const
{
	EnsureCityDigits();
	if (!m_cityDigitsReady)
		return;

	// Build digit list.
	int n = std::max(1, size);
	int digs[8];
	int nd = 0;
	{
		int tmp[8];
		int nt = 0;
		int v = n;
		if (v == 0)
			tmp[nt++] = 0;
		while (v > 0 && nt < 8)
		{
			tmp[nt++] = v % 10;
			v /= 10;
		}
		for (int i = nt - 1; i >= 0; --i)
			digs[nd++] = tmp[i];
	}

	constexpr int kGap = 1; // FONTS.CV spaceX for font 0
	int totalW = 0;
	for (int i = 0; i < nd; ++i)
	{
		if (m_cityDigits[digs[i]].id == 0)
			return;
		totalW += m_cityDigits[digs[i]].width;
		if (i + 1 < nd)
			totalW += kGap;
	}
	const int maxH = m_cityDigits[0].height > 0 ? m_cityDigits[0].height : 8;

	// 1:1 pixel scale inside the city fill (Civ1 is 1x on the map tile).
	const float originX = fillX + (fillS - static_cast<float>(totalW)) * 0.5f;
	const float originY = fillY + (fillS - static_cast<float>(maxH)) * 0.5f;

	float x = originX;
	for (int i = 0; i < nd; ++i)
	{
		const Texture2D& tex = m_cityDigits[digs[i]];
		const Rectangle src{ 0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height) };
		const Rectangle dst{ x, originY, static_cast<float>(tex.width), static_cast<float>(tex.height) };
		DrawTexturePro(tex, src, dst, Vector2{ 0, 0 }, 0.0f, WHITE);
		x += static_cast<float>(tex.width + kGap);
	}
}

void MapViewerState::DrawTileImprovements(int mx, int my, float dstX, float dstY, float tilePx) const
{
	const CivTile& tile = TileAt(mx, my);
	if (tile.IsOcean())
		return;
	if (!tile.HasIrrigation() && !tile.HasMine() && !tile.HasRoad() && !tile.HasRail()
		&& !tile.HasFortress())
		return;

	Texture* sp = g_ResourceManager->GetTexture(m_sp257Path, false);
	if (!sp || sp->id == 0)
		sp = g_ResourceManager->GetTexture("Images/civ_extract/SP257_320x200.png", false);
	if (!sp || sp->id == 0)
		return;

	// Order: irrigation → mine → road/rail → fortress (Civ1 stack).
	if (tile.HasIrrigation())
		DrawSheetSrc(sp, 64, 32, 16, 16, dstX, dstY, tilePx, tilePx);
	if (tile.HasMine())
		DrawSheetSrc(sp, 80, 32, 16, 16, dstX, dstY, tilePx, tilePx);

	// Road or railroad (rail replaces road visually).
	const bool drawRail = tile.HasRail();
	const bool drawRoad = tile.HasRoad() || drawRail;
	if (drawRoad)
	{
		const int mask = RoadConnectionMask(mx, my, drawRail);
		// Clockwise order matches CivOne GetRoad / GetRailRoad sprite columns.
		struct Off { int bit; int col; };
		static const Off kCW[8] = {
			{ DirN, 0 }, { DirNE, 1 }, { DirE, 2 }, { DirSE, 3 },
			{ DirS, 4 }, { DirSW, 5 }, { DirW, 6 }, { DirNW, 7 },
		};
		if (mask == 0)
		{
			// Isolated stub: 2×2 center (palette brown for road, grey for rail).
			const float s = tilePx / 16.0f;
			const Color blob = drawRail
				? CivVgaPaletteColor(5)   // near-black grey
				: CivVgaPaletteColor(6);  // brown
			DrawRectangle(
				static_cast<int>(dstX + 7.0f * s),
				static_cast<int>(dstY + 7.0f * s),
				std::max(1, static_cast<int>(2.0f * s)),
				std::max(1, static_cast<int>(2.0f * s)),
				blob);
		}
		else
		{
			for (const Off& o : kCW)
			{
				if ((mask & o.bit) == 0)
					continue;
				if (drawRail)
				{
					// Rail: SP257[128 + i*16, 96]
					const float sx = static_cast<float>(128 + o.col * 16);
					DrawSheetSrc(sp, sx, 96, 16, 16, dstX, dstY, tilePx, tilePx);
				}
				else
				{
					// Road: SP257[i*16, 48]
					const float sx = static_cast<float>(o.col * 16);
					DrawSheetSrc(sp, sx, 48, 16, 16, dstX, dstY, tilePx, tilePx);
				}
			}
		}
	}

	if (tile.HasFortress())
	{
		EnsureFortressOverlay();
		if (m_fortressReady && m_fortressOverlay.id != 0)
		{
			const Rectangle src{ 0, 0, 16, 16 };
			const Rectangle dst{ dstX, dstY, tilePx, tilePx };
			DrawTexturePro(m_fortressOverlay, src, dst, Vector2{ 0, 0 }, 0.0f, WHITE);
		}
	}
}

void MapViewerState::DrawOceanCoasts(int mx, int my, float dstX, float dstY, float tilePx) const
{
	// Port of CivOne MapTile.GetOceanLayer (GFX256 path).
	const int land = LandNeighborMask(mx, my);
	const int rivers = RiverNeighborMask(mx, my);
	if (land == 0 && rivers == 0)
		return;

	const float s = tilePx / 16.0f; // scale factor from source pixels to screen

	// 1) Special 16x16 corner cases from SP299 (when only a SE/NW/NE/SW land pair).
	bool drewCorner = false;
	if (DirAnd(land, DirS | DirE) && DirNot(land, DirN | DirW | DirSW | DirNE))
	{
		if (Texture* t = g_ResourceManager->GetTexture(m_coastCornerPaths[0], false))
			DrawTexScaled(t, dstX, dstY, tilePx);
		drewCorner = true;
	}
	else if (DirAnd(land, DirN | DirW) && DirNot(land, DirS | DirE | DirNE | DirSW))
	{
		if (Texture* t = g_ResourceManager->GetTexture(m_coastCornerPaths[1], false))
			DrawTexScaled(t, dstX, dstY, tilePx);
		drewCorner = true;
	}
	else if (DirAnd(land, DirN | DirE) && DirNot(land, DirS | DirW | DirNW | DirSE))
	{
		if (Texture* t = g_ResourceManager->GetTexture(m_coastCornerPaths[2], false))
			DrawTexScaled(t, dstX, dstY, tilePx);
		drewCorner = true;
	}
	else if (DirAnd(land, DirS | DirW) && DirNot(land, DirN | DirE | DirSE | DirNW))
	{
		if (Texture* t = g_ResourceManager->GetTexture(m_coastCornerPaths[3], false))
			DrawTexScaled(t, dstX, dstY, tilePx);
		drewCorner = true;
	}

	Texture* ter = g_ResourceManager->GetTexture(m_ter257Path, false);
	if (!ter || ter->id == 0)
		return;

	auto blit8 = [&](float sx, float sy, float ox, float oy) {
		DrawSheetSrc(ter, sx, sy, 8, 8, dstX + ox * s, dstY + oy * s, 8 * s, 8 * s);
	};
	auto blit16 = [&](float sx, float sy) {
		DrawSheetSrc(ter, sx, sy, 16, 16, dstX, dstY, tilePx, tilePx);
	};

	// 2) Cardinal coast segments (TER257 y=176 / y=184 half-tiles), unless corner handled it.
	if (!drewCorner)
	{
		if (DirAnd(land, DirN))
		{
			const float xw = DirAnd(land, DirW) ? 80.0f : DirAnd(land, DirNW) ? 96.0f : 64.0f;
			const float xe = DirAnd(land, DirE) ? 88.0f : DirAnd(land, DirNE) ? 56.0f : 24.0f;
			blit8(xw, 176, 0, 0);
			blit8(xe, 176, 8, 0);
		}
		if (DirAnd(land, DirE))
		{
			const float xn = DirAnd(land, DirN) ? 88.0f : DirAnd(land, DirNE) ? 104.0f : 72.0f;
			const float xs = DirAnd(land, DirS) ? 88.0f : DirAnd(land, DirSE) ? 56.0f : 24.0f;
			blit8(xn, 176, 8, 0);
			blit8(xs, 184, 8, 8);
		}
		if (DirAnd(land, DirS))
		{
			const float xw = DirAnd(land, DirW) ? 80.0f : DirAnd(land, DirSW) ? 48.0f : 16.0f;
			const float xe = DirAnd(land, DirE) ? 88.0f : DirAnd(land, DirSE) ? 104.0f : 72.0f;
			blit8(xw, 184, 0, 8);
			blit8(xe, 184, 8, 8);
		}
		if (DirAnd(land, DirW))
		{
			const float xn = DirAnd(land, DirN) ? 80.0f : DirAnd(land, DirNW) ? 48.0f : 16.0f;
			const float xs = DirAnd(land, DirS) ? 80.0f : DirAnd(land, DirSW) ? 96.0f : 64.0f;
			blit8(xn, 176, 0, 0);
			blit8(xs, 184, 0, 8);
		}
	}

	// 3) Diagonal-only land (land on corner but not on either adjacent cardinal).
	if (DirAnd(land, DirNW) && DirNot(land, DirN | DirW))
		blit8(32, 176, 0, 0);
	if (DirAnd(land, DirNE) && DirNot(land, DirN | DirE))
		blit8(40, 176, 8, 0);
	if (DirAnd(land, DirSW) && DirNot(land, DirS | DirW))
		blit8(32, 184, 0, 8);
	if (DirAnd(land, DirSE) && DirNot(land, DirS | DirE))
		blit8(40, 184, 8, 8);

	// 4) River mouths into ocean.
	if (DirAnd(rivers, DirN))
		blit16(128, 176);
	if (DirAnd(rivers, DirE))
		blit16(144, 176);
	if (DirAnd(rivers, DirS))
		blit16(160, 176);
	if (DirAnd(rivers, DirW))
		blit16(176, 176);
}

void MapViewerState::DrawCities() const
{
	if (m_cities.empty() || m_map.tiles.empty())
		return;

	const int tilePx = TilePx();
	const int viewTilesX = ViewTilesX();
	const int viewTilesY = ViewTilesY();
	const float s = static_cast<float>(m_scale); // 1 map-pixel → screen pixels
	const int mapLeft = MapLeft();

	Texture* cityIcon = g_ResourceManager->GetTexture(m_cityIconPath, false);
	Texture* cityWalls = g_ResourceManager->GetTexture(m_cityWallsPath, false);

	const CivPlayer* fog = FogViewer();

	for (const CivCity& city : m_cities)
	{
		// Fog: only show cities on currently visible tiles (or own cities).
		if (fog)
		{
			const bool own = (fog->id == city.owner);
			if (!own && !fog->IsVisible(city.x, city.y))
				continue;
		}

		// Screen tile origin (handle horizontal wrap vs camera).
		int relX = city.x - m_camX;
		while (relX < 0)
			relX += m_map.width;
		while (relX >= m_map.width)
			relX -= m_map.width;
		if (relX >= viewTilesX)
			continue;
		const int relY = city.y - m_camY;
		if (relY < 0 || relY >= viewTilesY)
			continue;

		const float dx = static_cast<float>(mapLeft + relX * tilePx);
		const float dy = static_cast<float>(relY * tilePx);

		// City colors (CivOne Icons.City):
		//   white (pal 15) L-frame, ColourDark rim, ColourLight fill,
		//   building icon with black (pal 5) → ColourDark.
		Color light = CivCityEdgeWhite();
		Color dark = CivVgaPaletteColor(7);
		if (const CivPlayer* pl = g_GameSetup.PlayerAt(city.owner))
		{
			light = pl->ColorRgb();
			dark = pl->ColorDarkRgb();
		}
		const Color edge = CivCityEdgeWhite();

		// 1) White base (forms left + bottom L once dark/light overpaint).
		DrawRectangle(static_cast<int>(dx + s), static_cast<int>(dy + s),
			14 * m_scale, 14 * m_scale, edge);
		// 2) Dark rim (top + right show; left/bottom stay white).
		DrawRectangle(static_cast<int>(dx + 2 * s), static_cast<int>(dy + s),
			13 * m_scale, 13 * m_scale, dark);
		// 3) Light fill (primary faction color).
		DrawRectangle(static_cast<int>(dx + 2 * s), static_cast<int>(dy + 2 * s),
			12 * m_scale, 12 * m_scale, light);

		// 4) City building sprite (white silhouette × ColourDark tint).
		if (cityIcon && cityIcon->id != 0)
		{
			const Rectangle src{ 0, 0, static_cast<float>(cityIcon->width), static_cast<float>(cityIcon->height) };
			const Rectangle dst{ dx + s, dy + s, 15.0f * s, 15.0f * s };
			DrawTexturePro(*cityIcon, src, dst, Vector2{ 0, 0 }, 0.0f, dark);
		}

		// 4) Fortification / city walls overlay 15×15 at source (209,113).
		if (city.Fortified() && cityWalls && cityWalls->id != 0)
		{
			const Rectangle src{ 0, 0, static_cast<float>(cityWalls->width), static_cast<float>(cityWalls->height) };
			const Rectangle dst{ dx + s, dy + s, 15.0f * s, 15.0f * s };
			DrawTexturePro(*cityWalls, src, dst, Vector2{ 0, 0 }, 0.0f, WHITE);
		}

		// 5) City size — original FONTS.CV font 0 glyphs (CivOne Icons.City).
		// Centered on the light fill (12×12 @ +2,+2).
		DrawCitySizeNumber(std::max(1, city.size), dx + 2.0f * s, dy + 2.0f * s, 12.0f * s);

		// 6) City name under the tile.
		const Vector2 ns = MeasureTextEx(*g_smallFont, city.name.c_str(), g_smallFont->baseSize, 1);
		DrawOutlinedText(g_smallFont, city.name,
			{ dx + (tilePx - ns.x) * 0.5f, dy + tilePx + 1.0f },
			g_smallFont->baseSize, 1, WHITE);
	}
}

// CivOne unit sprites: SP257 / SPRITES at (id%20)*16, y=160 (id<20) or 176.
// Clear top row + left column; remap palette 10→ColourLight, 2→ColourDark.
Texture* MapViewerState::UnitSprite(int typeId, CivColor color) const
{
	if (typeId < 0 || typeId >= 28)
		return nullptr;
	const int ci = static_cast<int>(color);
	if (ci < 0 || ci >= static_cast<int>(CivColor::Count))
		return nullptr;
	if (m_unitSpriteReady[typeId][ci])
		return &m_unitSprites[typeId][ci];

	Image sheet = LoadImage(m_sp257Path.c_str());
	if (sheet.data == nullptr)
	{
		// Fallback path used by extract layout.
		sheet = LoadImage("Images/civ_extract/SP257_320x200.png");
	}
	if (sheet.data == nullptr)
		return nullptr;

	const int xx = (typeId % 20) * 16;
	const int yy = typeId < 20 ? 160 : 176;
	Image unit = ImageFromImage(sheet, Rectangle{
		static_cast<float>(xx), static_cast<float>(yy), 16.0f, 16.0f });
	UnloadImage(sheet);
	if (unit.data == nullptr)
		return nullptr;

	// Transparent top edge + left column (CivOne).
	for (int x = 0; x < 16; ++x)
		ImageDrawPixel(&unit, x, 0, BLANK);
	for (int y = 1; y < 16; ++y)
		ImageDrawPixel(&unit, 0, y, BLANK);

	const Color srcLight = CivVgaPaletteColor(10); // green base fill
	const Color srcDark = CivVgaPaletteColor(2);  // green base rim
	const Color dstLight = CivColorRgb(color);
	const Color dstDark = CivColorDarkRgb(color);

	// CivOne special remaps before applying player colours.
	if (CivColorLightIndex(color) == 15)
	{
		// White player: free index 15 by moving existing whites to cyan.
		ImageColorReplace(&unit, CivVgaPaletteColor(15), CivVgaPaletteColor(11));
	}
	else if (CivColorDarkIndex(color) == 8)
	{
		// Grey player: free index 7 by moving greys to dark cyan.
		ImageColorReplace(&unit, CivVgaPaletteColor(7), CivVgaPaletteColor(3));
	}

	ImageColorReplace(&unit, srcLight, dstLight);
	ImageColorReplace(&unit, srcDark, dstDark);

	m_unitSprites[typeId][ci] = LoadTextureFromImage(unit);
	UnloadImage(unit);
	if (m_unitSprites[typeId][ci].id == 0)
		return nullptr;
	SetTextureFilter(m_unitSprites[typeId][ci], TEXTURE_FILTER_POINT);
	SetTextureWrap(m_unitSprites[typeId][ci], TEXTURE_WRAP_CLAMP);
	m_unitSpriteReady[typeId][ci] = true;
	return &m_unitSprites[typeId][ci];
}

void MapViewerState::EnsureFortifyOverlay() const
{
	if (m_fortifyReady)
		return;
	// CivOne Generic.Fortify: SP257[208,112,16,16], colour 3 → transparent.
	Image sheet = LoadImage(m_sp257Path.c_str());
	if (sheet.data == nullptr)
		sheet = LoadImage("Images/civ_extract/SP257_320x200.png");
	if (sheet.data == nullptr)
		return;
	Image fort = ImageFromImage(sheet, Rectangle{ 208, 112, 16, 16 });
	UnloadImage(sheet);
	if (fort.data == nullptr)
		return;
	ImageColorReplace(&fort, CivVgaPaletteColor(3), BLANK);
	m_fortifyOverlay = LoadTextureFromImage(fort);
	UnloadImage(fort);
	if (m_fortifyOverlay.id != 0)
	{
		SetTextureFilter(m_fortifyOverlay, TEXTURE_FILTER_POINT);
		SetTextureWrap(m_fortifyOverlay, TEXTURE_WRAP_CLAMP);
		m_fortifyReady = true;
	}
}

void MapViewerState::DrawUnits() const
{
	const int tilePx = TilePx();
	const int viewTilesX = ViewTilesX();
	const int viewTilesY = ViewTilesY();
	const float s = static_cast<float>(m_scale);
	const int mapLeft = MapLeft();
	const CivPlayer* fog = FogViewer();
	const CivGameSetup& setup = g_Game.started ? g_Game.setup : g_GameSetup;

	// One sprite per tile (last unit wins). Drawn under cities so city tiles
	// show the city icon instead (Civ1 map style).
	struct Drawn
	{
		int x, y, owner, typeId;
		bool fortify;
	};
	std::vector<Drawn> drawList;
	drawList.reserve(m_units.size());

	for (const UnitDot& u : m_units)
	{
		if (fog && fog->id != u.owner && !fog->IsVisible(u.x, u.y))
			continue;

		// Replace any existing unit already queued for this tile.
		bool replaced = false;
		for (Drawn& d : drawList)
		{
			if (d.x == u.x && d.y == u.y)
			{
				d = { u.x, u.y, u.owner, u.typeId, u.fortify };
				replaced = true;
				break;
			}
		}
		if (!replaced)
			drawList.push_back({ u.x, u.y, u.owner, u.typeId, u.fortify });
	}

	EnsureFortifyOverlay();

	for (const Drawn& u : drawList)
	{
		int relX = u.x - m_camX;
		while (relX < 0)
			relX += m_map.width;
		while (relX >= m_map.width)
			relX -= m_map.width;
		if (relX >= viewTilesX)
			continue;
		const int relY = u.y - m_camY;
		if (relY < 0 || relY >= viewTilesY)
			continue;

		const float dx = static_cast<float>(mapLeft + relX * tilePx);
		const float dy = static_cast<float>(relY * tilePx);

		CivColor col = CivColor::White;
		if (const CivPlayer* pl = setup.PlayerAt(u.owner))
			col = pl->color;

		Texture* tex = UnitSprite(u.typeId, col);
		if (tex && tex->id != 0)
		{
			const Rectangle src{ 0, 0, 16, 16 };
			const Rectangle dst{ dx, dy, 16.0f * s, 16.0f * s };
			DrawTexturePro(*tex, src, dst, Vector2{ 0, 0 }, 0.0f, WHITE);
		}
		else
		{
			// Fallback marker if sheet missing.
			const Color rgb = CivColorRgb(col);
			DrawRectangle(static_cast<int>(dx + s), static_cast<int>(dy + s),
				std::max(3, m_scale * 3), std::max(3, m_scale * 3), rgb);
		}

		if (u.fortify && m_fortifyReady && m_fortifyOverlay.id != 0)
		{
			const Rectangle src{ 0, 0, 16, 16 };
			const Rectangle dst{ dx, dy, 16.0f * s, 16.0f * s };
			DrawTexturePro(m_fortifyOverlay, src, dst, Vector2{ 0, 0 }, 0.0f, WHITE);
		}
	}
}

void MapViewerState::DrawTerritoryBorders() const
{
	if (!m_bordersEnabled || !m_territory.Valid())
		return;

	const int tilePx = TilePx();
	const int viewTilesX = ViewTilesX();
	const int viewTilesY = ViewTilesY();
	const int mapLeft = MapLeft();
	// Soft corners: convex (outer) shortens by 2 + diagonal pixel on this tile.
	// Concave (inner) shortens by 1 + diagonal pixel — edges live on two tiles
	// that already meet with a 1px offset, so inset 1 matches the same look.
	constexpr int kConvex = 2;
	constexpr int kConcave = 1;

	const CivPlayer* fog = FogViewer();
	const CivGameSetup& setup = g_Game.started ? g_Game.setup : g_GameSetup;

	for (int ty = 0; ty < viewTilesY; ++ty)
	{
		const int my = m_camY + ty;
		if (my < 0 || my >= m_map.height)
			continue;
		for (int tx = 0; tx < viewTilesX; ++tx)
		{
			int mx = m_camX + tx;
			while (mx < 0)
				mx += m_map.width;
			mx %= m_map.width;

			if (fog && !fog->IsVisible(mx, my))
				continue;

			const int8_t owner = m_territory.OwnerAt(mx, my);
			if (owner < 0)
				continue;

			Color col = CivCityEdgeWhite();
			if (const CivPlayer* pl = setup.PlayerAt(owner))
				col = pl->ColorRgb();
			col.a = 255;

			const int dx = mapLeft + tx * tilePx;
			const int dy = ty * tilePx;
			const int T = tilePx;

			const bool bN = m_territory.IsBorderWith(mx, my, mx, my - 1);
			const bool bS = m_territory.IsBorderWith(mx, my, mx, my + 1);
			const bool bW = m_territory.IsBorderWith(mx, my, mx - 1, my);
			const bool bE = m_territory.IsBorderWith(mx, my, mx + 1, my);

			// Convex: both edges on this tile.
			const bool xNW = bN && bW;
			const bool xNE = bN && bE;
			const bool xSW = bS && bW;
			const bool xSE = bS && bE;

			// Concave: edge ends here; the perpendicular edge is on a neighbor
			// (diagonal cell is the "bay" / missing corner of a 2x2).
			const bool vNW = bN && !bW && m_territory.IsBorderWith(mx - 1, my - 1, mx, my - 1);
			const bool vNE = bN && !bE && m_territory.IsBorderWith(mx + 1, my - 1, mx, my - 1);
			const bool vSW = bS && !bW && m_territory.IsBorderWith(mx - 1, my + 1, mx, my + 1);
			const bool vSE = bS && !bE && m_territory.IsBorderWith(mx + 1, my + 1, mx, my + 1);
			const bool hNW = bW && !bN && m_territory.IsBorderWith(mx - 1, my - 1, mx - 1, my);
			const bool hNE = bE && !bN && m_territory.IsBorderWith(mx + 1, my - 1, mx + 1, my);
			const bool hSW = bW && !bS && m_territory.IsBorderWith(mx - 1, my + 1, mx - 1, my);
			const bool hSE = bE && !bS && m_territory.IsBorderWith(mx + 1, my + 1, mx + 1, my);

			auto inset = [](bool convex, bool concave) -> int {
				if (convex)
					return kConvex;
				if (concave)
					return kConcave;
				return 0;
			};

			if (bN)
			{
				const int x0 = dx + inset(xNW, vNW);
				const int x1 = dx + T - 1 - inset(xNE, vNE);
				if (x1 >= x0)
					DrawHStroke(x0, x1, dy, col);
			}
			if (bS)
			{
				const int x0 = dx + inset(xSW, vSW);
				const int x1 = dx + T - 1 - inset(xSE, vSE);
				if (x1 >= x0)
					DrawHStroke(x0, x1, dy + T - 1, col);
			}
			if (bW)
			{
				const int y0 = dy + inset(xNW, hNW);
				const int y1 = dy + T - 1 - inset(xSW, hSW);
				if (y1 >= y0)
					DrawVStroke(dx, y0, y1, col);
			}
			if (bE)
			{
				const int y0 = dy + inset(xNE, hNE);
				const int y1 = dy + T - 1 - inset(xSE, hSE);
				if (y1 >= y0)
					DrawVStroke(dx + T - 1, y0, y1, col);
			}

			// Convex: one square on the diagonal inside this tile.
			if (xNW)
				DrawRectangle(dx + 1, dy + 1, 1, 1, col);
			if (xNE)
				DrawRectangle(dx + T - 2, dy + 1, 1, 1, col);
			if (xSE)
				DrawRectangle(dx + T - 2, dy + T - 2, 1, 1, col);
			if (xSW)
				DrawRectangle(dx + 1, dy + T - 2, 1, 1, col);

			// Concave: one square at the elbow. The two tiles that form the
			// corner each place the same pixel (safe to draw twice).
			if (vNW)
				DrawRectangle(dx, dy - 1, 1, 1, col);
			if (vNE)
				DrawRectangle(dx + T - 1, dy - 1, 1, 1, col);
			if (vSW)
				DrawRectangle(dx, dy + T, 1, 1, col);
			if (vSE)
				DrawRectangle(dx + T - 1, dy + T, 1, 1, col);
			if (hNW)
				DrawRectangle(dx - 1, dy, 1, 1, col);
			if (hNE)
				DrawRectangle(dx + T, dy, 1, 1, col);
			if (hSW)
				DrawRectangle(dx - 1, dy + T - 1, 1, 1, col);
			if (hSE)
				DrawRectangle(dx + T, dy + T - 1, 1, 1, col);
		}
	}
}

int MapViewerState::MapViewW() const
{
	return std::max(1, static_cast<int>(g_Engine->m_RenderWidth) - MapLeft());
}

int MapViewerState::MapViewH() const
{
	return std::max(1, static_cast<int>(g_Engine->m_RenderHeight));
}

int MapViewerState::ViewTilesX() const
{
	const int tp = TilePx();
	return std::max(1, (MapViewW() + tp - 1) / tp + 1);
}

int MapViewerState::ViewTilesY() const
{
	const int tp = TilePx();
	return std::max(1, (MapViewH() + tp - 1) / tp + 1);
}

void MapViewerState::ClampCamera()
{
	const int viewTilesY = std::max(1, MapViewH() / TilePx());
	if (m_map.width > 0)
	{
		while (m_camX < 0)
			m_camX += m_map.width;
		m_camX %= m_map.width;
	}
	const int maxY = std::max(0, m_map.height - viewTilesY);
	m_camY = std::clamp(m_camY, 0, maxY);
}

void MapViewerState::CenterOn(int mx, int my)
{
	const int vtx = std::max(1, MapViewW() / TilePx());
	const int vty = std::max(1, MapViewH() / TilePx());
	m_camX = mx - vtx / 2;
	m_camY = my - vty / 2;
	ClampCamera();
}

Rectangle MapViewerState::MinimapRect() const
{
	// Embedded: fixed 1× (1 map tile → 1 minimap pixel). Standalone may scale up.
	const int mapW = std::max(1, m_map.width);
	const int mapH = std::max(1, m_map.height);

	int scale = 1;
	if (!m_embedded)
	{
		const int maxW = static_cast<int>(g_Engine->m_RenderWidth) * 2 / 5;
		const int maxH = static_cast<int>(g_Engine->m_RenderHeight) / 2;
		scale = std::min(maxW / mapW, maxH / mapH);
		if (scale < 1)
			scale = 1;
	}
	const int mw = mapW * scale;
	const int mh = mapH * scale;
	// Center in the sidebar strip when smaller than full width.
	const float x = m_embedded
		? static_cast<float>((kSidebarWidth - mw) / 2)
		: 4.0f;
	const float y = 2.0f;
	return Rectangle{ x, y, static_cast<float>(mw), static_cast<float>(mh) };
}

bool MapViewerState::HandleMinimapClick(float virtX, float virtY)
{
	if (!m_minimapReady || m_map.tiles.empty())
		return false;
	const Rectangle r = MinimapRect();
	if (virtX < r.x || virtY < r.y || virtX >= r.x + r.width || virtY >= r.y + r.height)
		return false;

	const float u = (virtX - r.x) / r.width;
	const float v = (virtY - r.y) / r.height;
	const int mx = std::clamp(static_cast<int>(u * m_map.width), 0, m_map.width - 1);
	const int my = std::clamp(static_cast<int>(v * m_map.height), 0, m_map.height - 1);
	CenterOn(mx, my);
	return true;
}

MapViewerState::CityPick MapViewerState::PickCityAt(float virtX, float virtY) const
{
	CityPick none{};
	if (m_map.tiles.empty())
		return none;
	// Ignore sidebar / minimap clicks.
	if (virtX < static_cast<float>(MapLeft()))
		return none;
	const Rectangle mm = MinimapRect();
	if (virtX >= mm.x && virtX < mm.x + mm.width && virtY >= mm.y && virtY < mm.y + mm.height)
		return none;

	const int tp = TilePx();
	const int relX = static_cast<int>((virtX - MapLeft()) / tp);
	const int relY = static_cast<int>(virtY / tp);
	if (relX < 0 || relY < 0)
		return none;
	if (relX >= ViewTilesX() || relY >= ViewTilesY())
		return none;

	int mx = m_camX + relX;
	while (mx < 0)
		mx += m_map.width;
	mx %= m_map.width;
	const int my = m_camY + relY;
	if (my < 0 || my >= m_map.height)
		return none;

	const CivPlayer* fog = FogViewer();
	for (const CivCity& c : m_cities)
	{
		if (!c.Valid() || c.x != mx || c.y != my)
			continue;
		if (fog && fog->id != c.owner && !fog->IsVisible(mx, my))
			return none;
		CityPick pick;
		pick.owner = c.owner;
		pick.cityId = c.id;
		pick.x = c.x;
		pick.y = c.y;
		return pick;
	}
	return none;
}

int MapViewerState::PickCityOwnerAt(float virtX, float virtY) const
{
	return PickCityAt(virtX, virtY).owner;
}

void MapViewerState::DrawMinimap()
{
	if (!m_minimapReady)
		return;

	const Rectangle r = MinimapRect();
	DrawRectangle(static_cast<int>(r.x) - 2, static_cast<int>(r.y) - 2,
		static_cast<int>(r.width) + 4, static_cast<int>(r.height) + 4,
		Color{ 0, 0, 0, 220 });
	DrawRectangleLines(static_cast<int>(r.x) - 2, static_cast<int>(r.y) - 2,
		static_cast<int>(r.width) + 4, static_cast<int>(r.height) + 4,
		Color{ 40, 40, 40, 255 });

	const Rectangle src{ 0, 0, static_cast<float>(m_minimapTex.width), static_cast<float>(m_minimapTex.height) };
	DrawTexturePro(m_minimapTex, src, r, Vector2{ 0, 0 }, 0.0f, WHITE);

	const int viewTilesX = std::max(1, MapViewW() / TilePx());
	const int viewTilesY = std::max(1, MapViewH() / TilePx());
	const float sx = r.width / static_cast<float>(m_map.width);
	const float sy = r.height / static_cast<float>(m_map.height);
	const float vx = r.x + m_camX * sx;
	const float vy = r.y + m_camY * sy;
	const float vw = viewTilesX * sx;
	const float vh = viewTilesY * sy;
	DrawRectangleLinesEx(Rectangle{ vx, vy, vw, vh }, 1.0f, Color{ 255, 240, 80, 255 });
	if (m_camX + viewTilesX > m_map.width)
	{
		const float wrapW = (m_camX + viewTilesX - m_map.width) * sx;
		DrawRectangleLinesEx(Rectangle{ r.x, vy, wrapW, vh }, 1.0f, Color{ 255, 240, 80, 255 });
	}
}

void MapViewerState::Update()
{
	if (!m_embedded && IsKeyPressed(KEY_ESCAPE))
	{
		g_StateMachine->MakeStateTransition(STATE_TITLESTATE);
		return;
	}

	// F: toggle fog of war. B: toggle territory borders.
	if (IsKeyPressed(KEY_F))
	{
		// Allow toggle whenever any civ has fog data (human or observe mode).
		const CivGameSetup& setup = g_Game.started ? g_Game.setup : g_GameSetup;
		bool anyFog = false;
		for (int i = 0; i < setup.PlayerCount(); ++i)
		{
			if (const CivPlayer* p = setup.PlayerAt(i))
			{
				if (p->HasFog())
				{
					anyFog = true;
					break;
				}
			}
		}
		if (anyFog)
		{
			m_fogEnabled = !m_fogEnabled;
			RebuildMinimap();
		}
	}
	if (IsKeyPressed(KEY_B))
		m_bordersEnabled = !m_bordersEnabled;

	// Zoom locked at kMapScale (max); mouse wheel ignored.

	const float scaleX = g_Engine->m_ScreenWidth / static_cast<float>(g_Engine->m_RenderWidth);
	const float scaleY = g_Engine->m_ScreenHeight / static_cast<float>(g_Engine->m_RenderHeight);
	const float virtMouseX = GetMouseX() / scaleX;
	const float virtMouseY = GetMouseY() / scaleY;

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		HandleMinimapClick(virtMouseX, virtMouseY);

	const bool left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
	const bool right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
	const bool up = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
	const bool down = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);

	const bool justLeft = IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT);
	const bool justRight = IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT);
	const bool justUp = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
	const bool justDown = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);

	int dx = 0;
	int dy = 0;
	if (justLeft) dx -= 1;
	if (justRight) dx += 1;
	if (justUp) dy -= 1;
	if (justDown) dy += 1;

	if (dx != 0 || dy != 0)
	{
		m_camX += dx;
		m_camY += dy;
		ClampCamera();
		m_panCooldown = kPanRepeatSeconds;
	}
	else if (left || right || up || down)
	{
		m_panCooldown -= GetFrameTime();
		if (m_panCooldown <= 0.0f)
		{
			if (left) m_camX -= 1;
			if (right) m_camX += 1;
			if (up) m_camY -= 1;
			if (down) m_camY += 1;
			ClampCamera();
			m_panCooldown = kPanRepeatSeconds;
		}
	}
	else
	{
		m_panCooldown = 0.0f;
	}
}

void MapViewerState::Draw()
{
	const int rw = g_Engine->m_RenderWidth;
	const int rh = g_Engine->m_RenderHeight;
	const int mapLeft = MapLeft();
	// Map area only (sidebar is drawn by MainState when embedded).
	DrawRectangle(mapLeft, 0, rw - mapLeft, rh, Color{ 8, 16, 48, 255 });

	if (m_map.tiles.empty())
	{
		const char* msg = "Map assets missing — run tools/slice_civ_terrain.py";
		const Vector2 ms = MeasureTextEx(*g_font, msg, g_font->baseSize, 1);
		DrawOutlinedText(g_font, msg,
			{ mapLeft + (rw - mapLeft - ms.x) * 0.5f, rh * 0.5f },
			g_font->baseSize, 1, RED);
		return;
	}

	const int tilePx = TilePx();
	const int viewTilesX = ViewTilesX();
	const int viewTilesY = ViewTilesY();

	const CivPlayer* fog = FogViewer();

	for (int ty = 0; ty < viewTilesY; ++ty)
	{
		const int my = m_camY + ty;
		if (my < 0 || my >= m_map.height)
			continue;
		for (int tx = 0; tx < viewTilesX; ++tx)
		{
			int mx = m_camX + tx;
			while (mx < 0)
				mx += m_map.width;
			mx %= m_map.width;

			const float dx = static_cast<float>(mapLeft + tx * tilePx);
			const float dy = static_cast<float>(ty * tilePx);
			const float sz = static_cast<float>(tilePx);

			// Unexplored: pure black (not permanent forever-visible).
			if (fog && !fog->IsExplored(mx, my))
			{
				DrawRectangle(static_cast<int>(dx), static_cast<int>(dy),
					tilePx, tilePx, BLACK);
				continue;
			}

			const CivTile& tile = m_map.tiles[m_map.Index(mx, my)];

			// Land autotile / ocean coasts / river dirs.
			DrawTerrainCell(mx, my, dx, dy, sz);

			// Tile improvements (irrigation / mine / road / rail / fortress).
			// Shown on explored tiles (terrain memory); dimmed with fog below.
			DrawTileImprovements(mx, my, dx, dy, sz);

			// Resource special overlay (coal, gems, fish, shield, …).
			// Only show specials / huts on currently visible tiles.
			const bool lit = !fog || fog->IsVisible(mx, my);
			if (lit)
			{
				if (Texture* special = SpecialTexture(tile))
					DrawTexScaled(special, dx, dy, sz);

				// Goody hut
				if (tile.hut)
				{
					if (Texture* hut = g_ResourceManager->GetTexture(m_hutPath, false))
						DrawTexScaled(hut, dx, dy, sz);
				}
			}

			// Explored but not in current LOS: dim (remember terrain, no live detail).
			if (fog && !fog->IsVisible(mx, my))
			{
				DrawRectangle(static_cast<int>(dx), static_cast<int>(dy),
					tilePx, tilePx, Color{ 0, 0, 0, 150 });
			}
		}
	}

	DrawTerritoryBorders();
	// Units under cities: city icon covers stacked garrison (Civ1 map style).
	DrawUnits();
	DrawCities();
	DrawMinimap();

	// Hover resource name under cursor (optional HUD, map area only).
	const float scaleX = g_Engine->m_ScreenWidth / static_cast<float>(g_Engine->m_RenderWidth);
	const float scaleY = g_Engine->m_ScreenHeight / static_cast<float>(g_Engine->m_RenderHeight);
	const float virtMX = GetMouseX() / scaleX;
	const float virtMY = GetMouseY() / scaleY;
	const int hoverTx = static_cast<int>((virtMX - mapLeft) / tilePx);
	const int hoverTy = static_cast<int>(virtMY / tilePx);
	string hoverInfo;
	if (virtMX >= mapLeft && hoverTx >= 0 && hoverTy >= 0 && hoverTy < viewTilesY)
	{
		int mx = m_camX + hoverTx;
		while (mx < 0) mx += m_map.width;
		mx %= m_map.width;
		const int my = m_camY + hoverTy;
		if (my >= 0 && my < m_map.height)
		{
			ostringstream os;
			os << "(" << mx << "," << my << ")";
			if (fog && !fog->IsExplored(mx, my))
			{
				os << " unexplored";
			}
			else
			{
				const CivTile& ht = m_map.tiles[m_map.Index(mx, my)];
				os << " " << kTerrainNames[ht.terrain];
				if (fog && !fog->IsVisible(mx, my))
					os << " [fog]";
				if ((!fog || fog->IsVisible(mx, my)) && ht.HasResource())
					os << " [" << CivTile::ResourceName(ht.Resource()) << "]";
				if ((!fog || fog->IsVisible(mx, my)) && ht.hut)
					os << " hut";
				const int8_t terrOwner = m_territory.OwnerAt(mx, my);
				if (terrOwner >= 0)
				{
					if (const CivPlayer* pl = g_GameSetup.PlayerAt(terrOwner))
						os << "  land:" << pl->tribeName;
				}
				for (const CivCity& c : m_cities)
				{
					if (c.x == mx && c.y == my)
					{
						// Don't leak enemy city names through fog.
						if (!fog || fog->IsVisible(mx, my) || fog->id == c.owner)
						{
							os << "  city:" << c.name;
							if (const CivPlayer* pl = g_GameSetup.PlayerAt(c.owner))
								os << " (" << pl->tribeName << ")";
						}
						break;
					}
				}
			}
			hoverInfo = os.str();
		}
	}

	// Map-area HUD (skip when embedded — MainState owns the sidebar).
	if (!m_embedded)
	{
		ostringstream title;
		title << m_map.title << "   cam (" << m_camX << "," << m_camY << ")";
		DrawOutlinedText(g_font, title.str(), { 6.0f, static_cast<float>(MinimapRect().y + MinimapRect().height + 6) },
			g_font->baseSize, 1, WHITE);
		if (!hoverInfo.empty())
			DrawOutlinedText(g_smallFont, hoverInfo,
				{ 6.0f, static_cast<float>(MinimapRect().y + MinimapRect().height + 18) },
				g_smallFont->baseSize, 1, Color{ 220, 230, 180, 255 });

		const char* help = "WASD/Arrows: pan   Click minimap: jump   ESC: back";
		const Vector2 hs = MeasureTextEx(*g_smallFont, help, g_smallFont->baseSize, 1);
		DrawOutlinedText(g_smallFont, help,
			{ (rw - hs.x) * 0.5f, static_cast<float>(rh - 12) },
			g_smallFont->baseSize, 1, Color{ 180, 190, 210, 255 });
	}
	else if (!hoverInfo.empty())
	{
		DrawOutlinedText(g_smallFont, hoverInfo,
			{ static_cast<float>(mapLeft + 4), static_cast<float>(rh - 12) },
			g_smallFont->baseSize, 1, Color{ 220, 230, 180, 255 });
	}
}
