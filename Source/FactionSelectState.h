#ifndef _FACTIONSELECTSTATE_H_
#define _FACTIONSELECTSTATE_H_

#include "Geist/State.h"
#include "CivPlayer.h"

// Free faction + color assignment. Colors (incl. red) are independent of tribe.
// Slot 0 is the human player; remaining slots are AI opponents.
class FactionSelectState : public State
{
public:
	FactionSelectState() = default;
	~FactionSelectState() override = default;

	void Init(const std::string& configfile) override;
	void Shutdown() override;
	void Update() override;
	void Draw() override;
	void OnEnter() override;
	void OnExit() override;

private:
	enum Phase
	{
		PhasePickColor = 0,   // choose color for current slot
		PhasePickFaction,     // choose tribe for current slot
	};

	void BeginSlot(int slot);
	void ConfirmColor(CivColor color);
	void ConfirmFaction(CivFactionId faction);
	void AutoFillRemaining();
	void FinishAndStart();
	bool ColorTaken(CivColor c, int exceptSlot = -1) const;
	bool FactionTaken(CivFactionId f, int exceptSlot = -1) const;
	CivColor NextFreeColor() const;
	CivFactionId NextFreeFaction() const;

	int m_slot = 0; // which player we're configuring
	Phase m_phase = PhasePickColor;
	int m_selected = 0;

	// Working assignment for slots 0..numCivs-1
	CivPlayerSlot m_slots[8]{};
	int m_numSlots = 7;
};

#endif
