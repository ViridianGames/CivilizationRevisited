#ifndef _DIFFICULTYSTATE_H_
#define _DIFFICULTYSTATE_H_

#include "Geist/State.h"

// Classic-style new game setup: difficulty, then number of civilizations.
// Continues to Customize World (map options).
class DifficultyState : public State
{
public:
	enum Step
	{
		StepDifficulty = 0,
		StepCompetition,
		StepCount
	};

	DifficultyState() = default;
	~DifficultyState() override = default;

	void Init(const std::string& configfile) override;
	void Shutdown() override;
	void Update() override;
	void Draw() override;
	void OnEnter() override;
	void OnExit() override;

private:
	void ActivateChoice(int choice);
	void Finish();

	int m_step = StepDifficulty;
	int m_selected = 2; // Prince / 7 civs defaults
	int m_difficulty = 2;
	int m_numCivs = 7; // 3..8
};

#endif
