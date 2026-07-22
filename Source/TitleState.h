#ifndef _TITLESTATE_H_
#define _TITLESTATE_H_

#include "Geist/State.h"

class TitleState : public State
{
public:
	TitleState() = default;
	~TitleState() override = default;

	void Init(const std::string& configfile) override;
	void Shutdown() override;
	void Update() override;
	void Draw() override;

	void OnEnter() override;
	void OnExit() override;

private:
	int m_selected = 0;
};

#endif
