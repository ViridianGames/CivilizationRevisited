#ifndef _MAPVIEWERSTATE_H_
#define _MAPVIEWERSTATE_H_

#include "Geist/State.h"
#include "CivCity.h"
#include "CivFactions.h"
#include "CivPlayer.h"
#include "CivTerritory.h"
#include "CivTile.h"
#include "raylib.h"

#include <string>
#include <vector>

// Scrollable view of a Civ terrain map (Earth or randomly generated).
// Draws TER257 base tiles, special resource overlays, and a world minimap.
class MapViewerState : public State
{
public:
	static constexpr int kTileSize = 16;
	static constexpr int kTerrainCount = CivTerrain::Count;

	MapViewerState() = default;
	~MapViewerState() override = default;

	void Init(const std::string& configfile) override;
	void Shutdown() override;
	void Update() override;
	void Draw() override;
	void OnEnter() override;
	void OnExit() override;

	// When embedded in MainState: no ESC→title, quieter HUD, map input only.
	void SetEmbedded(bool embedded) { m_embedded = embedded; }
	bool IsEmbedded() const { return m_embedded; }

	// Left UI strip (Civ1-style). Map draws to the right of this.
	static constexpr int kSidebarWidth = 120;

	// Reload map/cities/units snapshot from globals / g_Game (after AI turns).
	void SyncFromGame();

	// Screen layout helpers (render coords).
	int MapLeft() const { return m_embedded ? kSidebarWidth : 0; }
	int MapViewW() const;
	int MapViewH() const;
	Rectangle MinimapRect() const;

	// Click helpers (virtual/render coords). Returns true if handled.
	bool HandleMinimapClick(float virtX, float virtY);
	// City under cursor. Ignores sidebar/minimap. owner/cityId = -1 if none.
	struct CityPick
	{
		int owner = -1;
		int cityId = -1;
		int x = 0;
		int y = 0;
		bool Valid() const { return owner >= 0 && cityId >= 0; }
	};
	CityPick PickCityAt(float virtX, float virtY) const;
	// Convenience: owner id only (-1 if none).
	int PickCityOwnerAt(float virtX, float virtY) const;
	void CenterOn(int mx, int my);

	// Which civ's fog of war to draw (-1 = human, else player index).
	void SetFogPlayerIndex(int playerIndex);
	int FogPlayerIndex() const { return m_fogPlayerIndex; }
	void SetFogEnabled(bool on) { m_fogEnabled = on; }
	bool FogEnabled() const { return m_fogEnabled; }

private:
	static constexpr int kRiverDirCount = 16; // N=1 E=2 S=4 W=8 bitfield

	// CivOne Direction bitfield (N=1 E=2 S=4 W=8 NW=16 NE=32 SW=64 SE=128).
	enum Dir : int
	{
		DirN = 1,
		DirE = 2,
		DirS = 4,
		DirW = 8,
		DirNW = 16,
		DirNE = 32,
		DirSW = 64,
		DirSE = 128
	};

	bool LoadTileAssets();
	void ClampCamera();
	void RebuildMinimap();
	void DrawMinimap();
	int ViewTilesX() const;
	int ViewTilesY() const;
	int TilePx() const { return kTileSize * m_scale; }

	Texture* TileTexture(int terrainId) const;
	Texture* RiverTexture(int mx, int my) const;
	Texture* SpecialTexture(const CivTile& tile) const;
	int RiverDirections(int mx, int my) const;
	// Same-type N/E/S/W mask for TER257 autotile columns (bits 0..15).
	int SameTypeMask(int mx, int my, uint8_t terrain) const;
	int LandNeighborMask(int mx, int my) const;
	int RiverNeighborMask(int mx, int my) const;
	const CivTile& TileAt(int mx, int my) const;

	// Land: land base + TER257[mask, terrain]. Ocean: base + coasts. River: SP257 dirs.
	void DrawTerrainCell(int mx, int my, float dstX, float dstY, float tilePx) const;
	// Draw CivOne-style ocean/lake coast overlays into a 16x16 tile dest.
	void DrawOceanCoasts(int mx, int my, float dstX, float dstY, float tilePx) const;
	// Irrigation, mine, road/rail (directional), fortress from SP257.
	void DrawTileImprovements(int mx, int my, float dstX, float dstY, float tilePx) const;
	// Neighbor mask for road/rail connectivity (8-way clockwise bits).
	int RoadConnectionMask(int mx, int my, bool rail) const;
	bool TileConnectsRoad(int mx, int my, bool rail) const;
	void DrawCities() const;
	void DrawUnits() const;
	void DrawTerritoryBorders() const;
	void RefreshTerritoryAndFog();
	// Viewing player for fog (human), or null if fog disabled / no game.
	const CivPlayer* FogViewer() const;
	bool TileExplored(int mx, int my) const;
	bool TileVisible(int mx, int my) const;
	void DrawSheetSrc(Texture* sheet, float sx, float sy, float sw, float sh,
		float dx, float dy, float dw, float dh) const;
	// SP257 unit sprite recolored for faction (cached). typeId 0..27, color 0..7.
	Texture* UnitSprite(int typeId, CivColor color) const;
	void EnsureFortifyOverlay() const;
	void EnsureFortressOverlay() const;
	// Digits 0–9 from original FONTS.CV font 0 (same glyphs Civ1 uses for city size).
	void EnsureCityDigits() const;
	void DrawCitySizeNumber(int size, float fillX, float fillY, float fillS) const;
	static bool DirAnd(int mask, int bits) { return (mask & bits) == bits; }
	static bool DirNot(int mask, int bits) { return (mask & bits) == 0; }

	CivMapData m_map;
	CivTerritoryMap m_territory;
	std::vector<CivCity> m_cities;
	// Snapshot of unit positions for drawing (ids only; full list lives in g_Game).
	struct UnitDot
	{
		int x = 0;
		int y = 0;
		int owner = -1;
		int typeId = 0;
		bool fortify = false;
	};
	std::vector<UnitDot> m_units;
	// Lazy unit sprite cache: [typeId][color]. Built from SP257 with faction recolor.
	mutable Texture2D m_unitSprites[28][static_cast<int>(CivColor::Count)]{};
	mutable bool m_unitSpriteReady[28][static_cast<int>(CivColor::Count)]{};
	mutable Texture2D m_fortifyOverlay{};
	mutable bool m_fortifyReady = false;
	// Fortress tile improvement (SP257 cyan key cleared).
	mutable Texture2D m_fortressOverlay{};
	mutable bool m_fortressReady = false;
	// City population digits (FONTS.CV font 0).
	mutable Texture2D m_cityDigits[10]{};
	mutable bool m_cityDigitsReady = false;
	// Fog on by default; F toggles. Borders B toggles.
	bool m_fogEnabled = true;
	bool m_bordersEnabled = true;
	// Player index whose explored/visible maps drive fog (-1 → human or 0).
	int m_fogPlayerIndex = -1;
	std::string m_tilePaths[kTerrainCount];
	std::string m_riverPaths[kRiverDirCount];
	// SP257 specials: one per base terrain id (0..10). Grassland shield separate.
	std::string m_specialPaths[kTerrainCount];
	std::string m_shieldPath;
	std::string m_hutPath;
	std::string m_cityIconPath;
	std::string m_cityWallsPath;
	std::string m_landBasePath;
	std::string m_oceanBasePath;
	std::string m_ter257Path;
	std::string m_sp257Path;
	std::string m_sp299Path;
	std::string m_coastCornerPaths[4];

	// Fixed display scale (no zoom): 1× (1 tile pixel → 1 screen pixel).
	static constexpr int kMapScale = 1;

	int m_camX = 0;
	int m_camY = 0;
	int m_scale = kMapScale;
	float m_panCooldown = 0.0f;
	bool m_embedded = false;

	Image m_minimapImage{};
	Texture2D m_minimapTex{};
	bool m_minimapReady = false;
};

using EarthMapViewerState = MapViewerState;

#endif
