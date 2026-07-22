#ifndef _CIVGAME_H_
#define _CIVGAME_H_

#include "CivAI.h"
#include "CivDifficulty.h"
#include "CivPlayer.h"
#include "CivTile.h"
#include "CivUnitInstance.h"
#include "Geist/RNG.h"

#include <string>
#include <vector>

// Runtime game session (extends setup with turns, units, year).
// Classic Civ1: one AI type; difficulty only changes bonuses / soft limits.

struct CivGame
{
	CivMapData map;
	CivGameSetup setup;

	int gameTurn = 0;              // 0 = 4000 BC era start
	int currentPlayerIndex = 0;
	bool started = false;
	// All-AI spectator session (title "Observe Game").
	bool observeMode = false;

	std::vector<CivUnitInstance> units;
	int nextUnitId = 1;

	// Last AI/status line for HUD.
	std::string lastLog;

	void Reset()
	{
		map.Clear();
		setup.Reset();
		gameTurn = 0;
		currentPlayerIndex = 0;
		started = false;
		observeMode = false;
		units.clear();
		nextUnitId = 1;
		lastLog.clear();
	}

	// After map + players + capitals exist (faction select done).
	void StartNewGame(RNG& rng);

	// Title "Observe Game": 8 random AI civs, default world, all computer-controlled.
	// Returns false if map generation failed.
	bool StartObserveGame(RNG& rng);

	CivPlayer* CurrentPlayer();
	const CivPlayer* CurrentPlayer() const;
	bool IsHumanTurn() const;
	bool IsObserveMode() const { return observeMode; }

	// Classic year string helper (approx Civ1 scale).
	static int TurnToYear(int turn);
	std::string YearString() const;

	// Process economy + production for one player (start-of-turn).
	void ProcessPlayerEconomy(CivPlayer& player, RNG& rng);

	// Complete research if science >= cost.
	void ProcessResearch(CivPlayer& player);

	// Apply shield income and complete builds for one city.
	void ProcessCityProduction(CivPlayer& player, CivCity& city, RNG& rng);

	// Spawn a unit at city (or x,y). Returns pointer into units vector (invalidated by later inserts).
	CivUnitInstance* SpawnUnit(int typeId, int owner, int x, int y, int homeCityId);

	// Human pressed End Turn: run remaining AI players, then start next human turn.
	// In observe mode (no human): run every AI once and advance the year.
	// Returns true if a full year/round advanced (gameTurn++).
	bool EndHumanTurn(RNG& rng);

	// Run a single AI player's full turn (economy already applied by caller).
	void RunAiPlayer(CivPlayer& player, RNG& rng);

	// Refresh fog + territory for everyone (after foundings).
	void RefreshMapState();
};

// Global session (set when leaving faction select / entering main).
inline CivGame g_Game;

#endif
