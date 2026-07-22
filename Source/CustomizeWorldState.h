#ifndef _CUSTOMIZEWORLDSTATE_H_
#define _CUSTOMIZEWORLDSTATE_H_

#include "Geist/State.h"

// Classic Civ "Customize World" menus: Land Mass, Temperature, Climate, Age.
// On completion, generates a random map and opens the map viewer.
class CustomizeWorldState : public State
{
public:
	enum Step
	{
		StepLandMass = 0,
		StepTemperature,
		StepClimate,
		StepAge,
		StepCount
	};

	CustomizeWorldState() = default;
	~CustomizeWorldState() override = default;

	void Init(const std::string& configfile) override;
	void Shutdown() override;
	void Update() override;
	void Draw() override;
	void OnEnter() override;
	void OnExit() override;

private:
	void ActivateChoice(int choice);
	void FinishAndGenerate();

	int m_step = StepLandMass;
	int m_selected = 1; // default middle option
	int m_landMass = 1;
	int m_temperature = 1;
	int m_climate = 1;
	int m_age = 1;
};

#endif
