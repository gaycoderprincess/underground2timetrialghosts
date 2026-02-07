const int nLocalReplayVersion = 4;
const int nMaxNumGhostsToCheck = 16;
const int nPlayerNameLength = 16;
const int nMinGhostTicks = 30;

enum eNitroType {
	NITRO_OFF,
	NITRO_ON,
	NITRO_INF,
	NUM_NITRO
};
int nNitroType = NITRO_ON;

bool bViewReplayMode = false; // todo read inputs
bool bViewReplayTargetTime = false;
enum eGhostVisuals {
	GHOST_HIDE,
	GHOST_SHOW,
	GHOST_HIDE_NEARBY,
	NUM_GHOST_VISUALS
};
int nGhostVisuals = GHOST_SHOW;
bool bShowInputsWhileDriving = false;
char sPlayerNameOverride[nPlayerNameLength] = "";
enum eDifficulty {
	DIFFICULTY_EASY, // slowest ghost only for every track
	DIFFICULTY_NORMAL, // first 3 ghosts in the folder
	DIFFICULTY_HARD, // quickest ghost only for every track
	NUM_DIFFICULTY,
};
int nDifficulty = DIFFICULTY_HARD;
bool bChallengesOneGhostOnly = false;
bool bChallengesPBGhost = false;
bool bCheckFileIntegrity = true;
bool bPracticeOpponentsOnly = false;

void DoConfigSave() {
	std::ofstream file("CwoeeGhosts/config.sav", std::iostream::out | std::iostream::binary);
	if (!file.is_open()) return;

	file.write((char*)&nGhostVisuals, sizeof(nGhostVisuals));
	file.write(sPlayerNameOverride, sizeof(sPlayerNameOverride));
	file.write((char*)&nDifficulty, sizeof(nDifficulty));
	file.write((char*)&bChallengesOneGhostOnly, sizeof(bChallengesOneGhostOnly));
	file.write((char*)&bChallengesPBGhost, sizeof(bChallengesPBGhost));
	file.write((char*)&bPracticeOpponentsOnly, sizeof(bPracticeOpponentsOnly));
	file.write((char*)&nNitroType, sizeof(nNitroType));
	file.write((char*)&bCheckFileIntegrity, sizeof(bCheckFileIntegrity));
	file.write((char*)&bShowInputsWhileDriving, sizeof(bShowInputsWhileDriving));
}

void DoConfigLoad() {
	std::ifstream file("CwoeeGhosts/config.sav", std::iostream::in | std::iostream::binary);
	if (!file.is_open()) return;

	file.read((char*)&nGhostVisuals, sizeof(nGhostVisuals));
	file.read(sPlayerNameOverride, sizeof(sPlayerNameOverride));
	sPlayerNameOverride[nPlayerNameLength-1] = 0;
	file.read((char*)&nDifficulty, sizeof(nDifficulty));
	file.read((char*)&bChallengesOneGhostOnly, sizeof(bChallengesOneGhostOnly));
	file.read((char*)&bChallengesPBGhost, sizeof(bChallengesPBGhost));
	file.read((char*)&bPracticeOpponentsOnly, sizeof(bPracticeOpponentsOnly));
	file.read((char*)&nNitroType, sizeof(nNitroType));
	file.read((char*)&bCheckFileIntegrity, sizeof(bCheckFileIntegrity));
	file.read((char*)&bShowInputsWhileDriving, sizeof(bShowInputsWhileDriving));
}

bool IsPracticeMode() {
	return !bChallengeSeriesMode;
}

double fGlobalReplayTimer = 0;
double fGlobalReplayTimerNoCountdown = 0;

struct tReplayTick {
	struct tTickVersion1 {
		RigidBodyState state;
		double timer;
	} v1;
	struct tTickVersion3 {
		int points;
	} v3;
	struct tTickVersion4 {
		float fNitroCharge;
		float fThrottle;
		float fEBrake;
		float fBrake;
		uint8_t bNOS;
		uint8_t pad;
		int16_t nSteering;
		float fPlayerSteering;
	} v4;

	void Collect(Car* pVehicle) {
		if (pVehicle->nMovementMode != PHYSICS_MOVEMENT) {
			memset(this, 0, sizeof(*this));
			return;
		}

		auto rb = pVehicle->pMover->GetRigidBody();
		if (!rb) {
			memset(this, 0, sizeof(*this));
			return;
		}
		v1.state = rb->State;
		v1.timer = fGlobalReplayTimer;

		v3.points = 0;
		if (TheRaceParameters.bDriftRaceFlag) {
			auto score = DriftManager::GetLeaderBoardScore(pVehicle->pDriverInfo->DriverNumber);
			v3.points = score ? score->fScore : 0;
		}

		auto controller = pVehicle->pController;
		if (controller && controller->pDriver) {
			v4.nSteering = controller->pDriver->nSteering;
			v4.fThrottle = controller->pDriver->fThrottle;
			v4.fBrake = controller->pDriver->fBrake;
			v4.fEBrake = controller->pDriver->fEBrake;
			if (pVehicle->nControlMode == REAL_CONTROLLER || pVehicle->nControlMode == DRAG_CONTROLLER) {
				v4.fPlayerSteering = ((RealDriver*)controller->pDriver)->fPlayerSteering;
			}
		}
		v4.bNOS = CarState::IsNitrousOn(&pVehicle->State);
		v4.fNitroCharge = pVehicle->State.fNitrous;
	}

	void Apply(Car* pVehicle) {
		if (pVehicle->nMovementMode != PHYSICS_MOVEMENT) {
			Car::SetMovementMode(pVehicle, PHYSICS_MOVEMENT);
			return;
		}

		auto rb = pVehicle->pMover->GetRigidBody();
		if (!rb) return;
		rb->State = v1.state;

		if (TheRaceParameters.bDriftRaceFlag) {
			auto score = DriftManager::GetLeaderBoardScore(pVehicle->pDriverInfo->DriverNumber);
			if (score) score->fScore = v3.points;
		}

		auto controller = pVehicle->pController;
		if (controller && controller->pDriver) {
			controller->pDriver->nSteering = v4.nSteering;
			controller->pDriver->fThrottle = v4.fThrottle;
			controller->pDriver->fBrake = v4.fBrake;
			controller->pDriver->fEBrake = v4.fEBrake;
		}
		pVehicle->State.fNitrous = v4.fNitroCharge;

		if (CarState::IsNitrousOn(&pVehicle->State) != v4.bNOS) {
			if (v4.bNOS) {
				CarState::StartNitrous(&pVehicle->State);
			}
			else {
				CarState::StopNitrous(&pVehicle->State);
			}
		}
	}
};

uint32_t nLocalGameFilesHash = 0;
struct tReplayGhost {
public:
	std::vector<tReplayTick> aTicks;
	uint32_t nStartTime;
	uint32_t nFinishTime;
	uint32_t nFinishPoints;
	std::string sPlayerName;
	Car* pLastVehicle;
	uint32_t nGameFilesHash;
	bool bIsPersonalBest;

	tReplayGhost() {
		Invalidate();
	}

	bool IsValid() const {
		return nFinishTime != 0 && !aTicks.empty();
	}

	// the countdown is incredibly inconsistent, need to account for it
	double GetPlaybackTime() const {
		if (aTicks.empty()) return 0;

		auto startTime = (nStartTime / 1000.0);
		if (IsPlayerStaging()) {
			if (fGlobalReplayTimer > startTime) return startTime;
			return fGlobalReplayTimer;
		}
		return fGlobalReplayTimerNoCountdown + startTime;
	}

	void Invalidate() {
		aTicks.clear();
		nFinishTime = 0;
		nFinishPoints = 0;
		sPlayerName = "";
		pLastVehicle = nullptr;
		nGameFilesHash = 0;
		bIsPersonalBest = false;
	}

	tReplayTick* GetInterpolatedTick(double raceTime) {
		tReplayTick* pClosestWithoutGoingOver = &aTicks[0];
		tReplayTick* pNextTick = nullptr;
		for (auto& tick : aTicks) {
			if (tick.v1.timer <= raceTime) pClosestWithoutGoingOver = &tick;
		}

		if (pClosestWithoutGoingOver->v1.state.vPosition.x == 0.0) return nullptr;

		if (pClosestWithoutGoingOver->v1.timer == raceTime) return pClosestWithoutGoingOver;
		if (pClosestWithoutGoingOver == &aTicks[aTicks.size()-1]) return nullptr;
		pNextTick = pClosestWithoutGoingOver + 1;

		auto delta = (raceTime - pClosestWithoutGoingOver->v1.timer) / (pNextTick->v1.timer - pClosestWithoutGoingOver->v1.timer);

		static tReplayTick InterpolatedTick;
		InterpolatedTick = *pClosestWithoutGoingOver;
		for (int i = 0; i < sizeof(RigidBodyState) / sizeof(float); i++) {
			auto fLast = (float*)&pClosestWithoutGoingOver->v1.state;
			auto fNext = (float*)&pNextTick->v1.state;
			auto fOut = (float*)&InterpolatedTick.v1.state;
			fOut[i] = std::lerp(fLast[i], fNext[i], delta);
		}
		InterpolatedTick.v1.timer = raceTime;
		return &InterpolatedTick;
	}

	void ApplyFinalScore(Car* pVehicle) {
		if (TheRaceParameters.bDriftRaceFlag) {
			auto score = DriftManager::GetLeaderBoardScore(pVehicle->pDriverInfo->DriverNumber);
			if (score) score->fScore = nFinishPoints;
		}
	}
};
tReplayGhost PlayerPBGhost;
std::vector<tReplayGhost> OpponentGhosts;

int nGhostsLoaded = -1;
std::vector<tReplayTick> aRecordingTicks;

// if this fails the ghosts are paused
bool ShouldGhostRun() {
	if (TheGameFlowManager.CurrentGameFlowState != GAMEFLOW_STATE_RACING) return false;
	if (MovieIsStarted) return false;
	//if (CAnimManager::IsPlayingIntroNIS(&TheAnimManager)) return false;
	if (CAnimManager::IsPlayingEndNIS(&TheAnimManager)) return false;
	//if (Race::GetCountdownNumberToDisplay(pCurrentRace) == 4) return false;
	if (Player::pPlayersByIndex[0]->bFinishedRacing) return false;

	auto ply = GetLocalPlayerVehicle();
	if (!ply) return false;
	return true;
}

std::vector<tReplayGhost> aLeaderboardGhosts;
void InvalidateGhost(bool resetTimers = true) {
	nGhostsLoaded = -1;
	if (resetTimers) {
		fGlobalReplayTimer = 0;
		fGlobalReplayTimerNoCountdown = 0;
		aRecordingTicks.clear();
	}
	PlayerPBGhost.Invalidate();
	OpponentGhosts.clear();
	aLeaderboardGhosts.clear();
}

void InvalidateLocalGhost() {
	fGlobalReplayTimer = 0;
	fGlobalReplayTimerNoCountdown = 0;
	aRecordingTicks.clear();
}

void RunGhost(Car* veh, tReplayGhost* ghost) {
	if (!ghost) return;
	if (!veh) return;
	ghost->pLastVehicle = veh;
	Car::StartBlinking(veh, 0.5);

	if (auto driver = veh->pDriverInfo) {
		if (ghost->sPlayerName.empty()) SetRacerName(driver, ghost->bIsPersonalBest ? "PERSONAL BEST" : "RACER");
		else {
			SetRacerName(driver, ghost->sPlayerName.c_str());
		}
	}

	if (!ghost->IsValid()) return;

	if (veh->nControlMode == REAL_CONTROLLER || veh->nControlMode == DRAG_CONTROLLER) {
		Car::SetControlMode(veh, NO_CONTROLLER);
	}

	auto tick = ghost->GetInterpolatedTick(ghost->GetPlaybackTime());
	if (!tick) {
		ghost->ApplyFinalScore(veh);
		return;
	}
	tick->Apply(veh);
}

void RecordGhost(Car* veh) {
	if (!bChallengeSeriesMode) {
		switch (nNitroType) {
			case NITRO_OFF:
				veh->State.fNitrous = 0;
				break;
			case NITRO_INF:
				veh->State.fNitrous = 9999;
				break;
		}
	}

	if (sPlayerNameOverride[0]) {
		SetRacerName(veh->pDriverInfo, sPlayerNameOverride);
	}

	tReplayTick state;
	state.Collect(veh);
	aRecordingTicks.push_back(state);
}

std::string GetGhostFilename(const std::string& car, int track, bool trackReversed, int lapCount, int opponentId, const char* folder = nullptr) {
	bool doNOSSpdbrkChecks = IsPracticeMode();
	bool doUpgradeChecks = IsPracticeMode(); // todo

	std::string path = gDLLPath.string();
	path += "/CwoeeGhosts/";
	if (bChallengeSeriesMode) {
		path += opponentId == 0 && !folder ? "ChallengePBs/" : "Challenges/";
	}
	else {
		path += "Practice/";
	}
	if (folder) {
		path += std::format("{}/", folder);
	}
	path += std::format("{}_{}", track, car);
	if (lapCount > 1) {
		path += std::format("_lap{}", lapCount);
	}

	if (doNOSSpdbrkChecks) {
		switch (nNitroType) {
			case NITRO_OFF:
				path += "_nos0";
				break;
			case NITRO_INF:
				path += "_nosinf";
				break;
			default:
				break;
		}
	}

	if (trackReversed) {
		path += "_rev";
	}

	if (opponentId) path += "_" + std::to_string(opponentId);
	else path += "_pb";

	path += ".ug2";
	path += "rep";
	return path;
}

void SavePB(tReplayGhost* ghost, const std::string& car, int track, int lapCount) {
	std::filesystem::create_directory("CwoeeGhosts");
	std::filesystem::create_directory("CwoeeGhosts/ChallengePBs");
	std::filesystem::create_directory("CwoeeGhosts/Practice");

	auto fileName = GetGhostFilename(car, track, TheRaceParameters.TrackDirection == eDIRECTION_BACKWARD, lapCount, 0);
	auto outFile = std::ofstream(fileName, std::ios::out | std::ios::binary);
	if (!outFile.is_open()) {
		WriteLog("Failed to save " + fileName + "!");
		return;
	}

	char signature[4] = "nya";
	int fileVersion = nLocalReplayVersion;

	int nitroType = bChallengeSeriesMode ? NITRO_ON : nNitroType;

	outFile.write(signature, sizeof(signature));
	outFile.write((char*)&fileVersion, sizeof(fileVersion));
	WriteStringToFile(outFile, car.c_str());
	outFile.write((char*)&track, sizeof(track));
	outFile.write((char*)&ghost->nStartTime, sizeof(ghost->nStartTime));
	outFile.write((char*)&ghost->nFinishTime, sizeof(ghost->nFinishTime));
	outFile.write((char*)&ghost->nFinishPoints, sizeof(ghost->nFinishPoints));
	outFile.write((char*)&nitroType, sizeof(nitroType));
	outFile.write((char*)&lapCount, sizeof(lapCount));
	auto name = GetLocalPlayerName();
	if (sPlayerNameOverride[0]) name = sPlayerNameOverride;
	outFile.write(name, nPlayerNameLength);
	outFile.write((char*)&nLocalGameFilesHash, sizeof(nLocalGameFilesHash));
	int count = ghost->aTicks.size();
	outFile.write((char*)&count, sizeof(count));
	outFile.write((char*)&ghost->aTicks[0], sizeof(ghost->aTicks[0]) * count);
}

void LoadPB(tReplayGhost* ghost, const std::string& car, int track, bool trackReversed, int lapCount, int opponentId, const char* folder = nullptr) {
	bool doNOSSpdbrkChecks = IsPracticeMode();
	bool doUpgradeChecks = IsPracticeMode();

	ghost->Invalidate();

	auto fileName = GetGhostFilename(car, track, trackReversed, lapCount, opponentId, folder);
	auto inFile = std::ifstream(fileName, std::ios::in | std::ios::binary);
	if (!std::filesystem::exists(fileName)) return;
	if (!inFile.is_open()) {
		return;
	}

	int fileVersion;

	uint32_t signature = 0;
	inFile.read((char*)&signature, sizeof(signature));
	if (memcmp(&signature, "nya", 4) == 0) {
		inFile.read((char*)&fileVersion, sizeof(fileVersion));
	}
	else {
		WriteLog("Invalid ghost " + fileName);
		return;
	}

	if (fileVersion > nLocalReplayVersion) {
		WriteLog(std::format("Too new ghost for {} (version {})", fileName, fileVersion));
		return;
	}

	int tmpstarttime, tmptime, tmppoints, tmpnitro, tmplaps, tmptrack;
	uint32_t fileHash = 0;
	char tmpplayername[nPlayerNameLength];
	strcpy_s(tmpplayername, opponentId ? "OPPONENT" : "PB GHOST");
	auto tmpcar = ReadStringFromFile(inFile);
	inFile.read((char*)&tmptrack, sizeof(tmptrack));
	inFile.read((char*)&tmpstarttime, sizeof(tmpstarttime));
	inFile.read((char*)&tmptime, sizeof(tmptime));
	inFile.read((char*)&tmppoints, sizeof(tmppoints));
	inFile.read((char*)&tmpnitro, sizeof(tmpnitro));
	inFile.read((char*)&tmplaps, sizeof(tmplaps));
	inFile.read(tmpplayername, sizeof(tmpplayername));
	if (fileVersion >= 2) {
		inFile.read((char*)&fileHash, sizeof(fileHash));
		if (!fileHash) fileHash = 0xFFFFFFFF;
	}
	if (tmpcar != car || tmptrack != track) {
		WriteLog("Mismatched ghost for " + fileName);
		return;
	}
	if (tmplaps != lapCount) {
		WriteLog("Mismatched ghost for " + fileName);
		return;
	}
	if (doNOSSpdbrkChecks && tmpnitro != nNitroType) {
		WriteLog("Mismatched ghost for " + fileName);
		return;
	}
	int count = 0;
	inFile.read((char*)&count, sizeof(count));
	if (count <= nMinGhostTicks) {
		WriteLog("Invalid ghost length for " + fileName);
		return;
	}
	// hack for my player name
	if (bChallengeSeriesMode && opponentId > 0) {
		if (!strcmp(tmpplayername, "woof")) strcpy_s(tmpplayername, "Chloe");
	}
	if (folder) {
		for (int i = 0; i < strlen(folder) && i < nPlayerNameLength; i++) {
			tmpplayername[i] = folder[i];
		}
	}
	tmpplayername[nPlayerNameLength-1] = 0;
	ghost->sPlayerName = tmpplayername;
	ghost->nStartTime = tmpstarttime;
	ghost->nFinishTime = tmptime;
	ghost->nFinishPoints = tmppoints;
	ghost->nGameFilesHash = fileHash;

	// don't needlessly load the full ghost data when previewing times in the menu
	if (TheGameFlowManager.CurrentGameFlowState <= GAMEFLOW_STATE_IN_FRONTEND) return;

	ghost->aTicks.reserve(count);
	for (int i = 0; i < count; i++) {
		tReplayTick state = {};
		inFile.read((char*)&state.v1, sizeof(state.v1));
		if (fileVersion >= 3) {
			inFile.read((char*)&state.v3, sizeof(state.v3));
		}
		if (fileVersion >= 4) {
			inFile.read((char*)&state.v4, sizeof(state.v4));
		}
		// padding added by the compiler, apparently
		if (fileVersion == 3 || fileVersion == 4) {
			int tmp = 0;
			inFile.read((char*)&tmp, sizeof(tmp));
		}
		ghost->aTicks.push_back(state);
	}
}

void OnChallengeSeriesEventPB();

void OnFinishRace() {
	uint32_t replayTime = fGlobalReplayTimerNoCountdown * 1000;
	uint32_t replayPoints = 0;
	bool isPointBased = TheRaceParameters.bDriftRaceFlag;
	if (isPointBased) {
		auto score = DriftManager::GetLeaderBoardScore(TheRaceParameters.PlayerDriverNumber[0]);
		replayPoints = score ? score->fScore : 0;
	}
	if (!bViewReplayMode && replayTime > 1000) {
		auto ghost = &PlayerPBGhost;
		bool isBetter = !ghost->nFinishTime || replayTime < ghost->nFinishTime;
		if (isPointBased) {
			isBetter = !ghost->nFinishTime || replayPoints > ghost->nFinishPoints;
		}
		if (isBetter) {
			WriteLog(std::format("Saving new lap PB of {}ms {}pts", replayTime, replayPoints));
			ghost->sPlayerName = sPlayerNameOverride[0] ? sPlayerNameOverride : GetLocalPlayerName();
			ghost->aTicks = aRecordingTicks;
			ghost->nStartTime = (fGlobalReplayTimer - fGlobalReplayTimerNoCountdown) * 1000;
			ghost->nFinishTime = replayTime;
			ghost->nFinishPoints = replayPoints;
			ghost->nGameFilesHash = nLocalGameFilesHash;
			SavePB(ghost, CarTypeInfoArray[PlayerCarType].CarTypeName, TheRaceParameters.TrackNumber, TheRaceParameters.NumLapsInRace);

			if (bChallengeSeriesMode) {
				OnChallengeSeriesEventPB();
			}
		}

		DoConfigSave();
	}
	aRecordingTicks.clear();
}

std::vector<tReplayGhost> CollectReplayGhosts(const std::string& car, int track, bool trackReversed, int laps, bool forFullLeaderboard = false) {
	std::vector<tReplayGhost> ghosts;

	auto difficulty = nDifficulty;
	if (forFullLeaderboard) difficulty = DIFFICULTY_HARD;

	if (difficulty != DIFFICULTY_NORMAL && std::filesystem::exists(gDLLPath.string() + "/CwoeeGhosts/Challenges")) {
		// check all subdirectories for community ghosts
		std::vector<std::string> folders;
		for (const auto& entry : std::filesystem::directory_iterator(gDLLPath.string() + "/CwoeeGhosts/Challenges")) {
			if (!entry.is_directory()) continue;

			folders.push_back(entry.path().filename().string());
		}
		for (auto& folder : folders) {
			tReplayGhost temp;
			LoadPB(&temp, car, track, trackReversed, laps, 0, folder.c_str());
			if (!temp.nFinishTime) continue;
			ghosts.push_back(temp);
		}
	}

	for (int i = 0; i < nMaxNumGhostsToCheck; i++) {
		tReplayGhost temp;
		LoadPB(&temp, car, track, trackReversed, laps, i+1);
		if (!temp.nFinishTime) continue;
		ghosts.push_back(temp);
	}

	if (difficulty == DIFFICULTY_EASY) {
		std::sort(ghosts.begin(), ghosts.end(), [](const tReplayGhost& a, const tReplayGhost& b) { if (a.nFinishPoints && b.nFinishPoints) { return a.nFinishPoints < b.nFinishPoints; } return a.nFinishTime > b.nFinishTime; });
	}
	else {
		std::sort(ghosts.begin(), ghosts.end(), [](const tReplayGhost& a, const tReplayGhost& b) { if (a.nFinishPoints && b.nFinishPoints) { return a.nFinishPoints > b.nFinishPoints; } return a.nFinishTime < b.nFinishTime; });
	}

	if (forFullLeaderboard) {
		for (auto& ghost: ghosts) {
			ghost.aTicks.clear();
		}
	}

	return ghosts;
}

tReplayGhost SelectTopGhost(const std::string& car, int track, bool trackReversed, int laps) {
	auto ghosts = CollectReplayGhosts(car, track, trackReversed, laps);
	if (ghosts.empty()) return {};
	return ghosts[0];
}

void OnRaceRestart() {
	InvalidateLocalGhost();
}

tReplayGhost* GetViewReplayGhost() {
	if (bChallengeSeriesMode && bViewReplayTargetTime && !OpponentGhosts.empty()) return &OpponentGhosts[0];
	return &PlayerPBGhost;
}

tReplayGhost* GetGhostForOpponent(int id) {
	bool showPB = false;
	if (bChallengeSeriesMode && bChallengesPBGhost) showPB = true;
	if (IsPracticeMode() && !bPracticeOpponentsOnly) showPB = true;

	if (showPB && !PlayerPBGhost.IsValid()) showPB = false;

	// todo
	//if (showPB && VEHICLE_LIST::GetList(VEHICLE_AIRACERS).size() == 1 && !OpponentGhosts.empty() && OpponentGhosts[0].IsValid()) {
	//	if (PlayerPBGhost.nFinishPoints && OpponentGhosts[0].nFinishPoints) {
	//		return PlayerPBGhost.nFinishPoints > OpponentGhosts[0].nFinishPoints ? &PlayerPBGhost : &OpponentGhosts[0];
	//	}
	//
	//	return PlayerPBGhost.nFinishTime < OpponentGhosts[0].nFinishTime ? &PlayerPBGhost : &OpponentGhosts[0];
	//}

	if (showPB && id == 0) return &PlayerPBGhost;
	if (showPB) id -= 1;
	if (id >= OpponentGhosts.size()) return nullptr;
	return &OpponentGhosts[id];
}

void RunGhosts() {
	if (bViewReplayMode) {
		RunGhost(GetLocalPlayerVehicle(), GetViewReplayGhost());
	}
	else {
		// set fixed start points, super ultra hack
		//if (!bCareerMode && GetLocalPlayerVehicle()->IsStaging() && !OpponentGhosts.empty() && OpponentGhosts[0].IsValid()) {
		//	InvalidatePlayerPos();
		//	OpponentGhosts[0].aTicks[0].ApplyPhysics(GetLocalPlayerVehicle());
		//}

		for (int i = 0; i < GetNumOpponentsInRace(); i++) {
			RunGhost(GetCarByRacerId(i+1), GetGhostForOpponent(i));
		}
	}
}

void TimeTrialLoop(double delta) {
	if (TheGameFlowManager.CurrentGameFlowState != GAMEFLOW_STATE_RACING) {
		InvalidateGhost();
		return;
	}

	bool isInRace = TheRaceParameters.RaceType != RACE_TYPE_NONE && pCurrentRace;
	if (!isInRace) {
		InvalidateGhost();
		return;
	}

	// force manual transmission for drag races - fixes skipfe
	if (TheRaceParameters.bDragRaceFlag && Player::pPlayersByIndex[0]) {
		Player::pPlayersByIndex[0]->bManualTransmission = true;
	}

	if (IsInLoadingScreen()) {
		InvalidateLocalGhost();
		return;
	}

	if (GetIsGamePaused()) return;

	auto ply = GetLocalPlayerVehicle();
	if (nGhostsLoaded != TheRaceParameters.TrackNumber) {
		auto car = CarTypeInfoArray[PlayerCarType].CarTypeName;
		auto track = TheRaceParameters.TrackNumber;
		auto trackReversed = TheRaceParameters.TrackDirection == eDIRECTION_BACKWARD;
		auto laps = TheRaceParameters.NumLapsInRace;
		LoadPB(&PlayerPBGhost, car, track, trackReversed, laps, 0);
		PlayerPBGhost.bIsPersonalBest = true;

		OpponentGhosts.clear();

		if (bChallengeSeriesMode) {
			aLeaderboardGhosts = CollectReplayGhosts(car, track, trackReversed, laps, true);

			if (bChallengesOneGhostOnly || bViewReplayMode || nDifficulty == DIFFICULTY_EASY) {
				auto opponent = SelectTopGhost(car, track, trackReversed, laps);
				if (opponent.nFinishTime != 0) {
					OpponentGhosts.push_back(opponent);
				}
			}
			else {
				auto ghosts = CollectReplayGhosts(car, track, trackReversed, laps);

				int opponentCount = GetNumOpponentsInRace();
				for (int i = 0; i < opponentCount && i < ghosts.size(); i++) {
					OpponentGhosts.push_back(ghosts[i]);
				}
			}
		}
		else {
			int opponentCount = GetNumOpponentsInRace();
			for (int i = 0; i < opponentCount; i++) {
				OpponentGhosts.push_back({});
				LoadPB(&OpponentGhosts[i], car, track, trackReversed, laps, i+1);
			}
		}

		nGhostsLoaded = TheRaceParameters.TrackNumber;
	}

	if (Player::pPlayersByIndex[0]->bFinishedRacing && (!TheRaceParameters.bDragRaceFlag || (ply && !Car::IsEngineBlown(ply) && !ply->bIsTotaled))) {
		OnFinishRace();
	}

	if (!ShouldGhostRun()) return;

	if (IsPracticeMode()) {
		TheRaceParameters.TrafficDensity = TheRaceParameters.bDragRaceFlag ? eTRAFFICDENSITY_MEDIUM : eTRAFFICDENSITY_OFF;
	}

	//auto opponents = VEHICLE_LIST::GetList(VEHICLE_AIRACERS);
	//for (int i = 0; i < opponents.size(); i++) {
	//	opponents[i]->mCOMObject->Find<IRBVehicle>()->EnableObjectCollisions(false);
	//}

	if (IsPlayerStaging()) {
		fGlobalReplayTimerNoCountdown = 0;
	}

	RunGhosts();

	RecordGhost(ply);
	if (!IsPlayerStaging()) {
		fGlobalReplayTimerNoCountdown += delta;
	}
	fGlobalReplayTimer += delta;
}

auto gInputRGBBackground = NyaDrawing::CNyaRGBA32(215,215,215,255);
auto gInputRGBHighlight = NyaDrawing::CNyaRGBA32(0,255,0,255);
float fInputBaseXPosition = 0.45;
float fInputBaseYPosition = 0.85;

void DrawInputTriangle(float posX, float posY, float sizeX, float sizeY, float inputValue, bool invertValue) {
	float minX = std::min(posX - sizeX, posX);
	float maxX = std::max(posX - sizeX, posX);

	DrawTriangle(posX, posY - sizeY, posX - sizeX, std::lerp(posY - sizeY, posY + sizeY, 0.5), posX,
				 posY + sizeY, invertValue ? gInputRGBBackground : gInputRGBHighlight);

	DrawTriangle(posX, posY - sizeY, posX - sizeX, std::lerp(posY - sizeY, posY + sizeY, 0.5), posX,
				 posY + sizeY, invertValue ? gInputRGBHighlight : gInputRGBBackground, std::lerp(minX, maxX, inputValue), 0, 1, 1);
}

void DrawInputTriangleY(float posX, float posY, float sizeX, float sizeY, float inputValue, bool invertValue) {
	float minY = std::min(posY - sizeY, posY + sizeY);
	float maxY = std::max(posY - sizeY, posY + sizeY);

	DrawTriangle(std::lerp(posX - sizeX, posX + sizeX, 0.5), posY - sizeY, posX - sizeX, posY + sizeY, posX + sizeX,
				 posY + sizeY, invertValue ? gInputRGBBackground : gInputRGBHighlight);

	DrawTriangle(std::lerp(posX - sizeX, posX + sizeX, 0.5), posY - sizeY, posX - sizeX, posY + sizeY, posX + sizeX,
				 posY + sizeY, invertValue ? gInputRGBHighlight : gInputRGBBackground, 0, std::lerp(minY, maxY + 0.001, inputValue), 1, 1);
}

void DrawInputRectangle(float posX, float posY, float scaleX, float scaleY, float inputValue) {
	DrawRectangle(posX - scaleX, posX + scaleX, posY - scaleY, posY + scaleY, gInputRGBBackground);
	if (inputValue == 0.0) return;
	DrawRectangle(posX - scaleX, posX + scaleX, std::lerp(posY + scaleY, posY - scaleY, inputValue), posY + scaleY, gInputRGBHighlight);
}

void DisplayInputs(tReplayTick* tick) {
	auto inputs = &tick->v4;
	//DrawInputTriangle((fInputBaseXPosition - 0.005) * GetAspectRatioInv(), fInputBaseYPosition, 0.08 * GetAspectRatioInv(), 0.07, 1 - (inputs->nSteering / 8192.0), true);
	//DrawInputTriangle((fInputBaseXPosition + 0.08) * GetAspectRatioInv(), fInputBaseYPosition, -0.08 * GetAspectRatioInv(), 0.07, inputs->nSteering / -8192.0, false);
	DrawInputTriangle((fInputBaseXPosition - 0.005) * GetAspectRatioInv(), fInputBaseYPosition, 0.08 * GetAspectRatioInv(), 0.07, 1 - (inputs->fPlayerSteering), true);
	DrawInputTriangle((fInputBaseXPosition + 0.08) * GetAspectRatioInv(), fInputBaseYPosition, -0.08 * GetAspectRatioInv(), 0.07, -inputs->fPlayerSteering, false);
	DrawInputTriangleY((fInputBaseXPosition + 0.0375) * GetAspectRatioInv(), fInputBaseYPosition - 0.05, 0.035 * GetAspectRatioInv(), 0.045, 1 - inputs->fThrottle, true);
	DrawInputTriangleY((fInputBaseXPosition + 0.0375) * GetAspectRatioInv(), fInputBaseYPosition + 0.05, 0.035 * GetAspectRatioInv(), -0.045, inputs->fBrake, false);

	//DrawInputTriangleY((fInputBaseXPosition + 0.225) * GetAspectRatioInv(), fInputBaseYPosition - 0.04, 0.035 * GetAspectRatioInv(), 0.035, 1 - (inputs->keys[INPUT_GEAR_UP] / 128.0), true);
	//DrawInputTriangleY((fInputBaseXPosition + 0.225) * GetAspectRatioInv(), fInputBaseYPosition + 0.04, 0.035 * GetAspectRatioInv(), -0.035, inputs->keys[INPUT_GEAR_DOWN] / 128.0, false);

	DrawInputRectangle((fInputBaseXPosition + 0.325) * GetAspectRatioInv(), fInputBaseYPosition + 0.05, 0.03 * GetAspectRatioInv(), 0.03, inputs->bNOS);
	DrawInputRectangle((fInputBaseXPosition + 0.425) * GetAspectRatioInv(), fInputBaseYPosition + 0.05, 0.03 * GetAspectRatioInv(), 0.03, inputs->fEBrake);
}

// special cases for some player names that are above 16 chars
std::string GetRealPlayerName(const std::string& ghostName) {
	if (ghostName == "Chloe") return "gaycoderprincess";
	if (ghostName == "ProfileInProces") return "ProfileInProcess";
	return ghostName;
}

std::string GetGameDataHashName(uint32_t hash) {
	if (hash == 0xCC654017) return "1.2 Vanilla";
	return std::format("{:X}", hash);
}

float fLeaderboardX = 0.03;
float fLeaderboardY = 0.6;
float fLeaderboardYSpacing = 0.03;
float fLeaderboardSize = 0.03;
float fLeaderboardOutlineSize = 0.02;
void DisplayLeaderboard() {
	if (IsPracticeMode()) return;
	if (MovieIsStarted) return;

	if (IsPlayerStaging() || GetIsGamePaused()) {
		std::vector<std::string> uniquePlayers;

		auto leaderboard = aLeaderboardGhosts;
		if (PlayerPBGhost.IsValid()) {
			tReplayGhost dummy;
			dummy.sPlayerName = sPlayerNameOverride[0] ? sPlayerNameOverride : GetLocalPlayerName();
			dummy.nFinishTime = PlayerPBGhost.nFinishTime;
			dummy.nFinishPoints = PlayerPBGhost.nFinishPoints;
			dummy.nGameFilesHash = PlayerPBGhost.nGameFilesHash;
			dummy.bIsPersonalBest = true;
			leaderboard.push_back(dummy);

			std::sort(leaderboard.begin(), leaderboard.end(), [](const tReplayGhost& a, const tReplayGhost& b) { if (a.nFinishPoints && b.nFinishPoints) { return a.nFinishPoints > b.nFinishPoints; } return a.nFinishTime < b.nFinishTime; });
		}
		
		int numOnLeaderboard = 0;
		for (auto& ghost : leaderboard) {
			auto name = ghost.bIsPersonalBest ? ghost.sPlayerName : GetRealPlayerName(ghost.sPlayerName);
			if (std::find(uniquePlayers.begin(), uniquePlayers.end(), name) != uniquePlayers.end()) continue;
			uniquePlayers.push_back(name);
			numOnLeaderboard++;
		}
		uniquePlayers.clear();

		int ranking = 1;

		tNyaStringData data;
		data.x = fLeaderboardX * GetAspectRatioInv();
		data.y = fLeaderboardY - (numOnLeaderboard * fLeaderboardYSpacing);
		data.size = fLeaderboardSize;
		data.outlinea = 255;
		data.outlinedist = fLeaderboardOutlineSize;
		for (auto& ghost : leaderboard) {
			auto name = ghost.bIsPersonalBest ? ghost.sPlayerName : GetRealPlayerName(ghost.sPlayerName);
			if (std::find(uniquePlayers.begin(), uniquePlayers.end(), name) != uniquePlayers.end()) continue;
			uniquePlayers.push_back(name);

			if (ghost.bIsPersonalBest) {
				data.SetColor(126, 246, 240, 255);
			}
			else {
				data.SetColor(255, 255, 255, 255);
			}

			bool isPointBased = TheRaceParameters.bDriftRaceFlag;
			std::string str;
			if (isPointBased) {
				str = std::format("{}. {} - {}", ranking++, name, FormatScore(ghost.nFinishPoints));
			}
			else {
				auto time = GetTimeFromMilliseconds(ghost.nFinishTime);
				time.pop_back();
				str = std::format("{}. {} - {}", ranking++, name, time);
			}
			if (bCheckFileIntegrity) {
				if (!ghost.nGameFilesHash) {
					str += " (Old ghost, no game data info)";
				}
				else if (ghost.nGameFilesHash != nLocalGameFilesHash) {
					str += std::format(" (Game data mismatch, {})", GetGameDataHashName(ghost.nGameFilesHash));
				}
			}
			DrawString(data, str);
			data.y += fLeaderboardYSpacing;
		}
	}
}

void DisplayPlayerNames() {
	if (bChallengeSeriesMode && nGhostVisuals != GHOST_HIDE && !GetIsGamePaused()) {
		const float fPlayerNameOffset = 0.031;
		const float fPlayerNameSize = 0.022;
		const float fPlayerNameFadeStart = 50.0;
		const float fPlayerNameFadeEnd = 150.0;
		const float fPlayerNameAlpha = 200.0;

		auto count = GetNumOpponentsInRace();
		for (int i = 0; i < count; i++) {
			auto ghost = GetGhostForOpponent(i);
			if (!ghost) continue;

			auto car = ghost->pLastVehicle;
			if (!IsVehicleValidAndActive(car)) continue;

			auto name = ghost->bIsPersonalBest ? ghost->sPlayerName : GetRealPlayerName(ghost->sPlayerName);

			float carHeight = 1.0;
			//UMath::Vector3 dim;
			//car->mCOMObject->Find<IRigidBody>()->GetDimension(&dim);

			auto pos = GetCarPosition(car);
			pos.z += carHeight;

			//auto cam = PrepareCameraMatrix(GetLocalPlayerCamera());
			//auto camFwd = RenderToWorldCoords(cam.z);
			//auto camPos = RenderToWorldCoords(cam.p);
			//auto playerDir = camPos - pos;
			//auto cameraDist = playerDir.length();
			//playerDir.Normalize();
			//if (playerDir.Dot(camFwd) > 0) continue;
			//if (cameraDist > fPlayerNameFadeEnd) continue;

			bVector3 screenPos;
			eViewPlatInterface::GetScreenPosition(&eViews[EVIEW_PLAYER1], &screenPos, &pos);

			screenPos.x /= (double)nResX;
			screenPos.y /= (double)nResY;
			if (screenPos.z >= 1.0) continue;

			tNyaStringData data;
			data.x = screenPos.x;
			data.y = screenPos.y - fPlayerNameOffset;
			data.size = fPlayerNameSize;
			data.XCenterAlign = true;
			//if (cameraDist > fPlayerNameFadeStart) {
			//	data.a = std::lerp(fPlayerNameAlpha, 0, (cameraDist - fPlayerNameFadeStart) / (fPlayerNameFadeEnd - fPlayerNameFadeStart));
			//}
			//else {
				data.a = fPlayerNameAlpha;
			//}
			DrawString(data, name);
		}
	}
}

void TimeTrialRenderLoop() {
	if (TheGameFlowManager.CurrentGameFlowState != GAMEFLOW_STATE_RACING) return;
	if (IsInLoadingScreen()) return;

	DisplayLeaderboard();

	if (!ShouldGhostRun()) return;

	if (!GetIsGamePaused()) {
		if (bViewReplayMode) {
			auto ghost = GetViewReplayGhost();

			if (auto tick = ghost->GetInterpolatedTick(ghost->GetPlaybackTime())) {
				DisplayInputs(tick);
			}
		}
		else if (bShowInputsWhileDriving) {
			tReplayTick tmp;
			tmp.Collect(GetLocalPlayerVehicle());
			DisplayInputs(&tmp);
		}
	}

	DisplayPlayerNames();
}

void ChallengeSeriesMenu();

void DebugMenu() {
	ChloeMenuLib::BeginMenu();

	if (DrawMenuOption("Challenge Series")) {
		ChloeMenuLib::BeginMenu();
		if (TheGameFlowManager.CurrentGameFlowState == GAMEFLOW_STATE_IN_FRONTEND && !TheGameFlowManager.pSingleFunction) {
			ChallengeSeriesMenu();
		}
		else if (TheGameFlowManager.CurrentGameFlowState == GAMEFLOW_STATE_RACING) {
			if (DrawMenuOption("Quit Event")) {
				SkipFE = false;
				CAnimManager::EndNIS_SafeRemove(&TheAnimManager);
				cFrontendDatabase::NotifyExitRaceToFrontend(&FEDatabase, false);
				GameFlowManager::UnloadTrack(&TheGameFlowManager);
			}
		}
		ChloeMenuLib::EndMenu();
	}

	if (DrawMenuOption("Options")) {
		ChloeMenuLib::BeginMenu();

		QuickValueEditor("Show Inputs While Driving", bShowInputsWhileDriving);
		QuickValueEditor("Player Name Override", sPlayerNameOverride, sizeof(sPlayerNameOverride));

		if (TheGameFlowManager.CurrentGameFlowState == GAMEFLOW_STATE_IN_FRONTEND) {
			QuickValueEditor("Replay Viewer", bViewReplayMode);
			if (bViewReplayMode) {
				if (DrawMenuOption(std::format("Replay Viewer Ghost - {}", bViewReplayTargetTime ? "Target Time" : "Personal Best"))) {
					bViewReplayTargetTime = !bViewReplayTargetTime;
				}
			}
			if (bChallengeSeriesMode) {
				const char* difficultyNames[] = {
						"Easy",
						"Normal",
						"Hard",
						"Very Hard",
				};
				const char* difficultyDescs[] = {
						"Easier ghosts",
						"Average ghosts",
						"Faster community ghosts",
						"Speedrunners' community ghosts",
				};
				if (DrawMenuOption(std::format("Difficulty - {}", difficultyNames[nDifficulty]))) {
					ChloeMenuLib::BeginMenu();
					for (int i = 0; i < NUM_DIFFICULTY; i++) {
						if (DrawMenuOption(difficultyNames[i], difficultyDescs[i])) {
							nDifficulty = (eDifficulty)i;
						}
					}
					ChloeMenuLib::EndMenu();
				}
				if (nDifficulty != DIFFICULTY_EASY) {
					QuickValueEditor("Show Target Ghost Only", bChallengesOneGhostOnly);
					if (DrawMenuOption(std::format("Show Personal Ghost - {}", bChallengesPBGhost))) {
						bChallengesPBGhost = !bChallengesPBGhost;
					}
				}
			}
			else {
				QuickValueEditor("Opponent Ghosts Only", bPracticeOpponentsOnly);

				if (bViewReplayMode) bPracticeOpponentsOnly = false;

				if (DrawMenuOption("NOS")) {
					ChloeMenuLib::BeginMenu();
					if (DrawMenuOption("Off")) {
						nNitroType = NITRO_OFF;
					}
					if (DrawMenuOption("On")) {
						nNitroType = NITRO_ON;
					}
					if (DrawMenuOption("Infinite")) {
						nNitroType = NITRO_INF;
					}
					ChloeMenuLib::EndMenu();
				}
			}
		}
		else {
			if (bChallengeSeriesMode) {
				if (DrawMenuOption(std::format("Show Personal Ghost - {}", bChallengesPBGhost))) {
					bChallengesPBGhost = !bChallengesPBGhost;
				}
			}
		}

		if (DrawMenuOption("Ghost Visuals")) {
			ChloeMenuLib::BeginMenu();
			if (DrawMenuOption("Hidden")) {
				nGhostVisuals = GHOST_HIDE;
			}
			if (DrawMenuOption("Shown")) {
				nGhostVisuals = GHOST_SHOW;
			}
			if (DrawMenuOption("Hide Too Close")) {
				nGhostVisuals = GHOST_HIDE_NEARBY;
			}
			ChloeMenuLib::EndMenu();
		}

		QuickValueEditor("Verify Game Data Integrity", bCheckFileIntegrity);
		DrawMenuOption(std::format("Game Data Hash: {:X}", nLocalGameFilesHash));

		ChloeMenuLib::EndMenu();
	}

	if (TheGameFlowManager.CurrentGameFlowState == GAMEFLOW_STATE_RACING) {
		if (DrawMenuOption("Debug")) {
			ChloeMenuLib::BeginMenu();
			DrawMenuOption(std::format("NoCountdown - {}", fGlobalReplayTimerNoCountdown));
			DrawMenuOption(std::format("Timer - {}", fGlobalReplayTimer));
			DrawMenuOption(std::format("MovieIsStarted - {}", MovieIsStarted));
			DrawMenuOption(std::format("IsPlayingIntroNIS - {}", CAnimManager::IsPlayingIntroNIS(&TheAnimManager)));
			DrawMenuOption(std::format("IsPlayingEndNIS - {}", CAnimManager::IsPlayingEndNIS(&TheAnimManager)));
			DrawMenuOption(std::format("GetCountdownNumberToDisplay - {}", Race::GetCountdownNumberToDisplay(pCurrentRace)));
			DrawMenuOption(std::format("bFinishedRacing - {}", Player::pPlayersByIndex[0]->bFinishedRacing));
			DrawMenuOption(std::format("GetLocalPlayerVehicle() - {:X}", (uintptr_t)GetLocalPlayerVehicle()));
			DrawMenuOption(std::format("GetLocalPlayerDriver() - {:X}", (uintptr_t)GetLocalPlayerDriver()));
			DrawMenuOption(std::format("GetNumOpponentsInRace() - {}", GetNumOpponentsInRace()));
			DrawMenuOption(std::format("GetPlaybackTime() - {}", PlayerPBGhost.GetPlaybackTime()));
			ChloeMenuLib::EndMenu();
		}
	}

	DrawMenuOption(std::format("Race - {}", TheRaceParameters.TrackNumber));

	ChloeMenuLib::EndMenu();
}