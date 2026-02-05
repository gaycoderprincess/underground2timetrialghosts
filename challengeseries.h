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
			if (auto preset = FindFEPresetCar(bStringHash(sCarPreset.c_str()))) {
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
		ForceAllAICarsToBeThisType = CarTypeInfoArray[GetCarID()].Type;

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
		ChallengeSeriesEvent(4012, "PresetCar/MIATA_CINGULAR", EVENT_RACE, 2),
		ChallengeSeriesEvent(4141, "SCOTT_TT", EVENT_RACE),
		ChallengeSeriesEvent(4703, "SHINESTREET", EVENT_RACE, 2, true),
		ChallengeSeriesEvent(4144, "MARCUS_CELICA", EVENT_RACE),
		ChallengeSeriesEvent(4304, "PresetCar/SUPRA_CUSTOM", EVENT_DRIFT, 1),
		ChallengeSeriesEvent(4716, "STOCK_CIVIC", EVENT_RACE, 1),
		ChallengeSeriesEvent(4201, "LANCEREVO8_AI_PRESET_1", EVENT_DRAG),
		ChallengeSeriesEvent(4212, "AL_RX8", EVENT_DRAG),
		ChallengeSeriesEvent(4121, "JAPANTUNING", EVENT_RACE),
		ChallengeSeriesEvent(4315, "PresetCar/DDAY_PLAYER_CAR_OLD_FIX", EVENT_DRIFT, 2),
		ChallengeSeriesEvent(4164, "DEMO_AI_350Z_BROWN", EVENT_RACE),
		ChallengeSeriesEvent(4175, "PresetCar/MUSTANGGT_RAZOR", EVENT_DRIFT),
		ChallengeSeriesEvent(4601, "THE_DOORS", EVENT_SHORT_TRACK, 3),
		ChallengeSeriesEvent(4716, "PresetCar/RX7_CUSTOM", EVENT_RACE, 2, true),
		ChallengeSeriesEvent(4701, "PresetCar/3000GT_CUSTOM", EVENT_RACE, 2),
		ChallengeSeriesEvent(4221, "DAVIDCHOE", EVENT_DRAG),
		ChallengeSeriesEvent(4604, "PresetCar/SKYLINE_CUSTOM", EVENT_DRIFT, 2),
		ChallengeSeriesEvent(4713, "TOM_G35", EVENT_RACE, 2),
		ChallengeSeriesEvent(4024, "PresetCar/240SX_CUSTOM", EVENT_RACE, 2),
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

void ChallengeSeriesMenu() {
	static std::string sLastSelectedCar;

	bChallengeSeriesMode = true;
	for (auto& event : aNewChallengeSeries) {
		if (event.nEventType == EVENT_DRAG) continue; // drag races start with automatic transmission????

		auto trackName = (std::string)GetLocalizedString(CalcTrackNameHash(event.nTrackNumber, false));
		if (event.bReversed) trackName += " Reversed";
		if (trackName.starts_with("Bayview Speedway Track ")) {
			trackName.erase(trackName.begin(), trackName.begin() + 23);
			trackName = "Bayview Speedway " + trackName;
		}

		auto pb = event.GetPBGhost();
		auto target = event.GetTargetGhost();
		auto targetName = GetRealPlayerName(target.sPlayerName);
		//auto optionName = std::format("{} - {}", trackName, carName);
		auto optionName = trackName;
		auto targetTime = std::format("Target Time - {} ({})", FormatTime(target.nFinishTime), targetName);
		if (event.nEventType == EVENT_DRIFT) {
			targetTime = std::format("Target - {} ({})", FormatScore(target.nFinishPoints), targetName);
		}
		if (pb.nFinishTime != 0) {
			bool won = pb.nFinishTime <= target.nFinishTime;
			if (pb.nFinishPoints && target.nFinishPoints) {
				won = pb.nFinishPoints >= target.nFinishPoints;
			}
			if (won) {
				optionName += std::format(" - Completed");
			}
		}
		if (DrawMenuOption(optionName, targetTime)) {
			// unstable
			/*if (sLastSelectedCar != event.sCarPreset && FEngFindScreen("GarageMain.fng")) {
				if (event.sCarPreset.starts_with("STOCK_")) {
					RideInfo::Init(&TopOrFullScreenRide, event.GetCarID(), 1, 0, 0);
					RideInfo::SetCompositeNameHash(&TopOrFullScreenRide, 1);
					RideInfo::SetStockParts(&TopOrFullScreenRide, 0);
					GarageMainScreen::SetRideInfo((GarageMainScreen*)FEngFindScreen("GarageMain.fng"), &TopOrFullScreenRide, SET_RIDE_INFO_REASON_CATCHALL);
				}
				else {
					RideInfo::FillWithPreset(&TopOrFullScreenRide, bStringHash(event.sCarPreset.c_str()));
					GarageMainScreen::SetRideInfo((GarageMainScreen*)FEngFindScreen("GarageMain.fng"), &TopOrFullScreenRide, SET_RIDE_INFO_REASON_CATCHALL);
				}
			}*/
			sLastSelectedCar = event.sCarPreset;

			std::string carName = CarTypeInfoArray[event.GetCarID()].ManufacturerName;
			std::transform(carName.begin(), carName.end(), carName.begin(), [](char c){ return std::tolower(c); });
			carName[0] = std::toupper(carName[0]);

			carName += " ";
			if (!strcmp(CarTypeInfoArray[event.GetCarID()].CarTypeName, "HUMMER")) carName.clear(); // doubled brand name
			carName += GetLocalizedString(FEngHashString(std::format("CAR_NAME_{}", event.GetCarModelName()).c_str()));

			ChloeMenuLib::BeginMenu();
			DrawMenuOption(std::format("Track - {}", trackName));
			DrawMenuOption(std::format("Car - {}", carName));
			DrawMenuOption(targetTime);
			if (pb.nFinishTime != 0) {
				DrawMenuOption(std::format("Personal Best - {}", event.nEventType == EVENT_DRIFT ? FormatScore(pb.nFinishPoints) : FormatTime(pb.nFinishTime)));
			}
			if (DrawMenuOption("Launch Event")) {
				event.SetupEvent();
				return;
			}
			ChloeMenuLib::EndMenu();
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