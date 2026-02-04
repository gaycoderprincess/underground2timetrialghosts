#include <windows.h>
#include <mutex>
#include <filesystem>
#include <format>
#include <toml++/toml.hpp>

#include "nya_dx9_hookbase.h"
#include "nya_commonhooklib.h"
#include "nya_commonmath.h"
#include "nfsug2.h"
#include "chloemenulib.h"

bool bChallengeSeriesMode = false;

#include "util.h"
#include "timetrials.h"
#include "challengeseries.h"

void HookLoop() {}

uint32_t nCarBlinkCounter = 0;
void MainLoop(float delta) {
	if (TheGameFlowManager.CurrentGameFlowState == GAMEFLOW_STATE_IN_FRONTEND && !TheGameFlowManager.pSingleFunction) bChallengeSeriesMode = false;

	DLLDirSetter _setdir;

	static double time = 0;
	time += delta;
	if (time > 1.0 / 15.0) {
		nCarBlinkCounter = !nCarBlinkCounter;
		time -= 1.0 / 15.0;
	}

	TimeTrialLoop(delta);
}

void RenderLoop() {
	UnlockAllThings = true;

	if (TheGameFlowManager.CurrentGameFlowState == GAMEFLOW_STATE_IN_FRONTEND && !TheGameFlowManager.pSingleFunction) bChallengeSeriesMode = false;

	if (TheGameFlowManager.CurrentGameFlowState != GAMEFLOW_STATE_RACING) {
		InvalidateGhost();
		return;
	}

	bool isInRace = TheRaceParameters.RaceType != RACE_TYPE_NONE && pCurrentRace;
	if (!isInRace) {
		InvalidateGhost();
		return;
	}
}

// force all MellowMover sets to be PhysicsMover
void __thiscall SetMovementModeHooked(Car* pThis, MovementMode mode) {
	if (mode == MELLOW_MOVEMENT) mode = PHYSICS_MOVEMENT;
	Car::SetMovementMode(pThis, mode);
}

// force all RaceCarController sets to be NullController
void __thiscall SetControlModeHooked(Car* pThis, ControlMode mode) {
	if (mode == RACECAR_CONTROLLER) mode = NO_CONTROLLER;
	Car::SetControlMode(pThis, mode);
}

void __thiscall TheRaceHasFinishedHooked(RaceCoordinator* pThis) {
	RaceCoordinator::TheRaceHasFinished(pThis);
	OnFinishRace();
}

class eView;
int IsCarBlinkingOrPoofedHooked(Car* pCar, eView* view) {
	auto ply = GetLocalPlayerVehicle();
	if (pCar == ply) {
		return Car::IsBlinking(pCar) ? nCarBlinkCounter & 1 : 0;
	}

	if (nGhostVisuals == GHOST_SHOW) return false;

	bool hide = nGhostVisuals == GHOST_HIDE;
	if (!hide) {
		auto dist = (GetCarPosition(ply) - GetCarPosition(pCar)).length();
		if (dist < 8) hide = true;
	}
	return hide;
}

class IconOption;
class IconScrollerMenu;
static inline auto AddOption_orig = (void(__thiscall*)(IconScrollerMenu*, IconOption*))nullptr;
void __thiscall AddOptionHooked(IconScrollerMenu* pThis, IconOption* a2) {
	if (!a2) return;
	return AddOption_orig(pThis, a2);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
	switch( fdwReason ) {
		case DLL_PROCESS_ATTACH: {
			if (NyaHookLib::GetEntryPoint() != 0x35BCC7) {
				MessageBoxA(nullptr, "Unsupported game version! Make sure you're using v1.2 (.exe size of 4800512 bytes)", "nya?!~", MB_ICONERROR);
				return TRUE;
			}

			gDLLPath = std::filesystem::current_path();
			GetCurrentDirectoryW(MAX_PATH, gDLLDir);

			ChloeMenuLib::RegisterMenu("Time Trial Ghosts", &DebugMenu);

			DoConfigLoad();

			UnlockAllThings = true;
			//aNewChallengeSeries[0].SetupEventSkipFE();

			ApplyChallengeSeriesPatches();

			NyaHooks::PlaceD3DHooks();
			NyaHooks::D3DEndSceneHook::aFunctions.push_back(RenderLoop);
			NyaHooks::WorldTimestepHook::Init();
			NyaHooks::WorldTimestepHook::aFunctions.push_back(MainLoop);
			NyaHookLib::Patch(0x60D55D, &nCarBlinkCounter);

			NyaHookLib::Patch<uint8_t>(0x5EBD9B, 0xEB); // disable drafting bonus
			NyaHookLib::Patch<uint8_t>(0x56F7D2, 0xEB); // disable gained position bonus
			NyaHookLib::Patch<uint8_t>(0x56EFD3, 0xEB); // disable lead lap bonus

			// skip other menu options
			NyaHookLib::Patch<uint8_t>(0x4AEE64, 0xEB);
			NyaHookLib::Patch<uint8_t>(0x4AEEDF, 0xEB);
			NyaHookLib::Patch<uint8_t>(0x4AEF40, 0xEB);
			AddOption_orig = (void(__thiscall*)(IconScrollerMenu*, IconOption*))(*(uintptr_t*)0x797528);
			NyaHookLib::Patch(0x797528, &AddOptionHooked);

			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x60D4F0, &IsCarBlinkingOrPoofedHooked);

			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x603E1B, &OnRaceRestart);
			//NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x60B5BD, &TheRaceHasFinishedHooked);
			//NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x60B5DE, &TheRaceHasFinishedHooked);

			uintptr_t addresses[] = {
					0x42C7D6,
					0x430A91,
					0x435F74,
					0x4B5C1F,
					0x4B5E7F,
					0x56D348,
					0x5F3023,
					0x5F3093,
					0x5F6D79,
					0x5F6DB8,
					0x5F6E5F,
					0x5FAE3B,
					0x5FAF23,
					0x5FB8DF,
					0x60653A,
					0x607101,
					0x607648,
			};
			for (auto& addr : addresses) {
				NyaHookLib::PatchRelative(NyaHookLib::CALL, addr, &SetControlModeHooked);
			}

			uintptr_t addresses2[] = {
					0x4020ED,
					0x40214B,
					0x4022EF,
					0x4024DA,
					0x4048B1,
					0x4048BB,
					0x408C62,
					0x408CBF,
					0x408EDF,
					0x4097F3,
					0x409982,
					0x4099F3,
					0x409A92,
					0x411263,
					0x42A028,
					0x42AC3A,
					0x42B280,
					0x42B2A2,
					0x42C2DE,
					0x42C7CD,
					0x42C933,
					0x42CA90,
					0x430AAA,
					0x435F87,
					0x443790,
					0x455D13,
					0x4562A8,
					0x56D351,
					0x56D3E7,
					0x56E68F,
					0x5F52D7,
					0x5F53E4,
					0x5F6DC9,
					0x5F6E3A,
					0x5F8C77,
					0x5FAE6E,
					0x5FAF2D,
					0x5FB8D3,
					0x5FB8EB,
					0x60128F,
					0x601297,
					0x606550,
					0x606573,
			};
			for (auto& addr : addresses2) {
				NyaHookLib::PatchRelative(NyaHookLib::CALL, addr, &SetMovementModeHooked);
			}
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x40A23B, &SetMovementModeHooked);
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x40A274, &SetMovementModeHooked);
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x5F499C, &SetMovementModeHooked);
		} break;
		default:
			break;
	}
	return TRUE;
}