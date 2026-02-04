uint32_t nChallengeSeriesCar = 0;
class ChallengeSeriesEvent {
public:
	int nTrackNumber;
	std::string sCarPreset;
	eCwoeeEventType nEventType = EVENT_RACE;
	bool bReversed = false;
	int nLapCountOverride = 0;

	ChallengeSeriesEvent(int trackId, const char* carPreset, eCwoeeEventType eventType, int lapCount = 0) : nTrackNumber(trackId), sCarPreset(carPreset), nEventType(eventType), nLapCountOverride(lapCount) {}

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

	void SetupEvent() const {
		DoConfigSave();

		bChallengeSeriesMode = true;
		nChallengeSeriesCar = bStringHash(sCarPreset.c_str());
		ForceAllAICarsToBeThisType = GetCarID();

		SkipFE = true;
		SkipFETrackNumber = nTrackNumber;
		SkipFETrackDirection = bReversed;
		SkipFETrafficDensity = eTRAFFICDENSITY_OFF;
		SkipFERaceType = RACE_TYPE_SINGLE_RACE;
		SkipFEDragRace = nEventType == EVENT_DRAG;
		SkipFEDriftRace = nEventType == EVENT_DRIFT;
		SkipFEBurnoutRace = nEventType == EVENT_BURNOUT;
		SkipFEShortTrackRace = nEventType == EVENT_SHORT_TRACK;
		SkipFEPoint2Point = TrackInfo::GetTrackInfo(nTrackNumber)->Point2Point;
		SkipFENumPlayerCars = 1;
		SkipFENumAICars = bChallengesOneGhostOnly ? 1 : 3;
		SkipFEPlayerCarUpgrades[0] = 3; // full upgrades

		int laps = nLapCountOverride;
		if (laps <= 0) {
			laps = SkipFEPoint2Point ? 1 : 2;
		}
		SkipFENumLaps = laps;

		OnlineEnabled = false;
		RaceStarter::StartSkipFERace();

		TheRaceParameters.DamageEnabled = false;
		TheRaceParameters.AIDifficultyModifier = eAI_DIFFICULTY_MODIFIER_NORMAL;
	}
};

void OnChallengeSeriesEventPB() {} // todo pb display

std::vector<ChallengeSeriesEvent> aNewChallengeSeries = {
		ChallengeSeriesEvent(4001, "DDAY_PLAYER_CAR", EVENT_RACE, 2),
		ChallengeSeriesEvent(4102, "DDAY_PLAYER_CAR_OLD_RX8", EVENT_RACE),
		ChallengeSeriesEvent(4014, "CAPONE", EVENT_RACE, 1),
};

// 4001 rachel circuit
// 4102 rachel sprint

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
	for (auto& event : aNewChallengeSeries) {
		auto car = FindFEPresetCar(FEHashUpper(event.sCarPreset.c_str()));
		std::string carName = car ? car->CarTypeName : event.sCarPreset;
		if (carName.starts_with("STOCK_")) {
			for (int i = 0; i < 6; i++) {
				carName.erase(carName.begin());
			}
		}
		carName = GetLocalizedString(FEngHashString(std::format("CAR_NAME_{}", carName).c_str()));
		auto trackName = GetLocalizedString(CalcTrackNameHash(event.nTrackNumber, event.bReversed));

		if (DrawMenuOption(std::format("{} - {}", trackName, carName))) {
			event.SetupEvent();
		}
	}
}

void __thiscall RideInfoInitHooked(RideInfo* pThis, int a1, int a2, int a3, int a4) {
	RideInfo::Init(pThis, a1, a2, a3, a4);
	RideInfo::FillWithPreset(pThis, nChallengeSeriesCar);
	RideInfo::SetCompositeNameHash(pThis, 1);
}

void ApplyChallengeSeriesPatches() {
	NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x5401B7, &RideInfoInitHooked);
	NyaHookLib::Patch<uint16_t>(0x5401E4, 0x9090);
	NyaHookLib::Patch(0x5401E8, &nChallengeSeriesCar);
}