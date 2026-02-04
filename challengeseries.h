std::string GetCarNameForGhost(const std::string& carPreset) {
	auto car = FindFEPresetCar(bStringHash(carPreset.c_str()));
	std::string carName = car ? car->CarTypeName : carPreset;
	if (carName.starts_with("STOCK_")) {
		for (int i = 0; i < 6; i++) {
			carName.erase(carName.begin());
		}
	}
	return carName;
}

uint32_t nChallengeSeriesCar = 0;
class ChallengeSeriesEvent {
public:
	int nTrackNumber;
	std::string sCarPreset;
	eCwoeeEventType nEventType = EVENT_RACE;
	bool bReversed = false;
	int nLapCountOverride = 0;

	std::string sCarNameForGhost;
	tReplayGhost PBGhost;
	tReplayGhost aTargetGhosts[NUM_DIFFICULTY] = {};
	int nNumGhosts[NUM_DIFFICULTY] = {};

	PresetCar PresetCarData = {};

	ChallengeSeriesEvent(int trackId, const char* carPreset, eCwoeeEventType eventType, int lapCount = 0, bool reversed = false) : nTrackNumber(trackId), sCarPreset(carPreset), nEventType(eventType), nLapCountOverride(lapCount), bReversed(reversed) {}

	int GetCarID() const {
		std::string str = sCarPreset;
		if (str.starts_with("STOCK_")) {
			for (int i = 0; i < 6; i++) {
				str.erase(str.begin());
			}
		}
		else {
			if (auto preset = FindFEPresetCar(nChallengeSeriesCar)) {
				str = preset->CarTypeName;
			}
		}

		for (int i = 0; i < 45; i++) {
			if (CarTypeInfoArray[i].CarTypeName == str) return CarTypeInfoArray[i].Type;
		}
		return -1;
	}

	int GetNumLaps() const {
		int laps = nLapCountOverride;
		if (laps <= 0) {
			laps = TrackInfo::GetTrackInfo(nTrackNumber)->Point2Point ? 1 : 2;
		}
		return laps;
	}

	void ClearPBGhost() {
		PBGhost = {};
	}

	std::string GetCarModelName() const {
		if (!sCarNameForGhost.empty()) return sCarNameForGhost;
		return GetCarNameForGhost(sCarPreset);
	}

	tReplayGhost GetPBGhost() {
		if (PBGhost.nFinishTime != 0) return PBGhost;

		tReplayGhost temp;
		LoadPB(&temp, GetCarModelName(), nTrackNumber, bReversed, GetNumLaps(), 0);
		temp.aTicks.clear(); // just in case
		PBGhost = temp;
		return temp;
	}

	tReplayGhost GetTargetGhost() {
		if (aTargetGhosts[nDifficulty].nFinishTime != 0) return aTargetGhosts[nDifficulty];

		tReplayGhost targetTime;
		auto times = CollectReplayGhosts(GetCarModelName(), nTrackNumber, bReversed, GetNumLaps());
		if (!times.empty()) {
			times[0].aTicks.clear(); // just in case
			targetTime = aTargetGhosts[nDifficulty] = times[0];
		}
		nNumGhosts[nDifficulty] = times.size();
		return targetTime;
	}

	void SetupEvent() const {
		DoConfigSave();

		bChallengeSeriesMode = true;
		nChallengeSeriesCar = bStringHash(sCarPreset.c_str());
		ForceAllAICarsToBeThisType = GetCarID();

		// make preset car fully tuned
		if (auto car = FindFEPresetCar(nChallengeSeriesCar)) {
			car->PhysicsLevel = 3;
		}

		SkipFE = true;
		SkipFETrackNumber = nTrackNumber;
		SkipFETrackDirection = bReversed;
		SkipFETrafficDensity = nEventType == EVENT_DRAG ? eTRAFFICDENSITY_MEDIUM : eTRAFFICDENSITY_OFF;
		SkipFERaceType = RACE_TYPE_SINGLE_RACE;
		SkipFEDragRace = g_tweakIsDragRace = nEventType == EVENT_DRAG;
		SkipFEDriftRace = nEventType == EVENT_DRIFT;
		SkipFEBurnoutRace = nEventType == EVENT_BURNOUT;
		SkipFEShortTrackRace = nEventType == EVENT_SHORT_TRACK;
		SkipFEPoint2Point = TrackInfo::GetTrackInfo(nTrackNumber)->Point2Point;
		SkipFENumPlayerCars = 1;
		SkipFENumAICars = bChallengesOneGhostOnly ? 1 : 3;
		SkipFEPlayerCarUpgrades[0] = 3; // full upgrades
		SkipFENumLaps = GetNumLaps();

		OnlineEnabled = false;
		RaceStarter::StartSkipFERace();

		TheRaceParameters.DamageEnabled = false;
		TheRaceParameters.AIDifficultyModifier = eAI_DIFFICULTY_MODIFIER_NORMAL;
	}
};

std::vector<ChallengeSeriesEvent> aNewChallengeSeries = {
		ChallengeSeriesEvent(4001, "DDAY_PLAYER_CAR", EVENT_RACE, 2), // rachel circuit
		ChallengeSeriesEvent(4102, "DDAY_PLAYER_CAR_OLD_RX8", EVENT_RACE), // rachel sprint
		ChallengeSeriesEvent(4014, "CAPONE", EVENT_RACE, 1),
		ChallengeSeriesEvent(4022, "D3", EVENT_RACE, 2),
		ChallengeSeriesEvent(4063, "DEMO_AI_300GT_ORANGE", EVENT_RACE, 1),
		ChallengeSeriesEvent(4042, "DEMO_PRESET_4", EVENT_RACE, 2),
		ChallengeSeriesEvent(4086, "DEMO_AI_IMPREZAWRX_BLUE", EVENT_RACE, 2),
		ChallengeSeriesEvent(4608, "NIKKI_MUSTANGGT", EVENT_SHORT_TRACK, 3),
		ChallengeSeriesEvent(4141, "SCOTT_TT", EVENT_RACE),
		ChallengeSeriesEvent(4703, "SHINESTREET", EVENT_RACE, 2, true),
		ChallengeSeriesEvent(4144, "MARCUS_CELICA", EVENT_RACE),
		ChallengeSeriesEvent(4716, "STOCK_CIVIC", EVENT_RACE, 1),
		ChallengeSeriesEvent(4201, "LANCEREVO8_AI_PRESET_1", EVENT_DRAG),
		ChallengeSeriesEvent(4212, "AL_RX8", EVENT_DRAG),
		ChallengeSeriesEvent(4121, "JAPANTUNING", EVENT_RACE),
		ChallengeSeriesEvent(4164, "DEMO_AI_350Z_BROWN", EVENT_RACE),
		ChallengeSeriesEvent(4601, "THE_DOORS", EVENT_SHORT_TRACK, 3),
		ChallengeSeriesEvent(4716, "PresetCar/RX7_CUSTOM", EVENT_RACE, 2, true),
		ChallengeSeriesEvent(4701, "NIGEL_3000GT", EVENT_RACE, 2),
		ChallengeSeriesEvent(4221, "DAVIDCHOE", EVENT_DRAG),
		ChallengeSeriesEvent(4713, "TOM_G35", EVENT_RACE, 2),
		ChallengeSeriesEvent(4088, "CALEB_GTO", EVENT_RACE, 2),
		ChallengeSeriesEvent(4107, "G35_AI_PRESET_1", EVENT_RACE, 1), // marathon
};

ChallengeSeriesEvent* GetChallengeEvent(int id) {
	for (auto& event : aNewChallengeSeries) {
		if (event.nTrackNumber == id) return &event;
	}
	return nullptr;
}

void OnChallengeSeriesEventPB() {
	auto event = GetChallengeEvent(TheRaceParameters.TrackNumber);
	if (!event) return;
	event->ClearPBGhost();
}

/*
350Z_AI_PRESET_1 - 350Z
AL_RX8 - RX8
CALEB_GTO - GTO
CAPONE - HUMMER
CHINGY - NAVIGATOR
D3 - ECLIPSE
DAVIDCHOE - COROLLA
DDAY_PLAYER_CAR - 350Z
DDAY_PLAYER_CAR_OLD - LANCEREVO8
DDAY_PLAYER_CAR_OLD_RX8 - RX8
DEMO_AI_300GT_BLUE - 3000GT
DEMO_AI_300GT_ORANGE - 3000GT
DEMO_AI_350Z_BROWN - 350Z
DEMO_AI_350Z_SILVER - 350Z
DEMO_AI_IMPREZAWRX_BLUE - IMPREZAWRX
DEMO_AI_IMPREZAWRX_WHITE - IMPREZAWRX
DEMO_PRESET_1 - 350Z
DEMO_PRESET_2 - 350Z
DEMO_PRESET_3 - 350Z
DEMO_PRESET_4 - 350Z
G35_AI_PRESET_1 - G35
JAPANTUNING - G35
LANCEREVO8_AI_PRESET_1 - LANCEREVO8
MARCUS_CELICA - CELICA
NIGEL_3000GT - 3000GT
NIKKI_MUSTANGGT - MUSTANGGT
SCOTT_TT - TT
SHINESTREET - IS300
SNOOP_DOGG - ESCALADE
THE_DOORS - GTO
TOM_G35 - G35
TT_AI_PRESET_1 - TT
*/

void ChallengeSeriesMenu() {
	bChallengeSeriesMode = true;
	for (auto& event : aNewChallengeSeries) {
		if (event.nEventType == EVENT_DRAG) continue; // drag races start with automatic transmission????

		auto carName = GetLocalizedString(FEngHashString(std::format("CAR_NAME_{}", event.GetCarModelName()).c_str()));
		auto trackName = (std::string)GetLocalizedString(CalcTrackNameHash(event.nTrackNumber, false));
		if (event.bReversed) trackName += " Reversed";
		if (trackName.starts_with("Bayview Speedway Track ")) {
			trackName.erase(trackName.begin(), trackName.begin() + 23);
			trackName = "Bayview Speedway " + trackName;
		}

		auto pb = event.GetPBGhost();
		auto target = event.GetTargetGhost();
		auto pbTime = GetTimeFromMilliseconds(pb.nFinishTime);
		pbTime.pop_back();
		auto targetTime = GetTimeFromMilliseconds(target.nFinishTime);
		targetTime.pop_back();
		//auto optionName = std::format("{} - {}", trackName, carName);
		auto optionName = trackName;
		auto description = std::format("Target Time - {} ({})", targetTime, target.sPlayerName);
		if (pb.nFinishTime != 0) optionName += std::format(" - {}", pbTime);
		if (DrawMenuOption(optionName, description)) {
			event.SetupEvent();
			return;
		}
	}
	bChallengeSeriesMode = false;
}

void __thiscall RideInfoInitHooked(RideInfo* pThis, int a1, int a2, int a3, int a4) {
	RideInfo::Init(pThis, a1, a2, a3, a4);
	RideInfo::FillWithPreset(pThis, nChallengeSeriesCar);
	RideInfo::SetCompositeNameHash(pThis, 1);
}

PresetCar* FindFEPresetCarHooked(uint32_t hash) {
	for (auto& event : aNewChallengeSeries) {
		if (event.sCarPreset.find('/') == std::string::npos) continue;
		if (hash == bStringHash(event.sCarPreset.c_str())) {
			if (event.PresetCarData.PresetName[0]) return &event.PresetCarData;

			std::ifstream file(std::format("{}/CwoeeGhosts/{}", gDLLPath.string(), event.sCarPreset), std::ios::in | std::ios::binary);
			if (!file.is_open()) {
				MessageBoxA(nullptr, std::format("Failed to find preset car {}", event.sCarPreset).c_str(), "nya?!~", MB_ICONERROR);
				exit(0);
			}

			auto preset = &event.PresetCarData;
			memset(preset, 0, sizeof(*preset));
			file.read((char*)preset, sizeof(*preset));
			preset->PhysicsLevel = 3;
			return preset;
		}
	}

	auto node = PresetCarList.HeadNode.Next;
	while (node != &PresetCarList.HeadNode) {
		if (hash == FEHashUpper(node->PresetName)) {
			return node;
		}
		node = node->Next;
	}
	return nullptr;
}

void ApplyChallengeSeriesPatches() {
	NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x5401B7, &RideInfoInitHooked);
	NyaHookLib::Patch<uint16_t>(0x5401E4, 0x9090);
	NyaHookLib::Patch(0x5401E8, &nChallengeSeriesCar);

	NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x61C460, &FindFEPresetCarHooked);
}