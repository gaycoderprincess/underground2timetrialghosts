const int nLocalReplayVersion = 1;
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
char sPlayerNameOverride[nPlayerNameLength] = "";
enum eDifficulty {
	DIFFICULTY_EASY, // slowest ghost only for every track
	DIFFICULTY_NORMAL, // first 3 ghosts in the folder
	DIFFICULTY_HARD, // quickest ghost only for every track
	NUM_DIFFICULTY,
};
int nDifficulty = DIFFICULTY_HARD;
bool bChallengesPBGhost = false;
bool bChallengesOneGhostOnly = false;
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
}

bool IsPracticeMode() {
	return !bChallengeSeriesMode;
}

double fGlobalReplayTimer = 0;
double fGlobalReplayTimerNoCountdown = 0;

struct tReplayTick {
	struct tTickVersion1 {
		RigidBodyState state;
		double timer; // todo
	} v1;

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
	}

	void Apply(Car* pVehicle) {
		if (pVehicle->nMovementMode != PHYSICS_MOVEMENT) {
			Car::SetMovementMode(pVehicle, PHYSICS_MOVEMENT);
			return;
		}

		auto rb = pVehicle->pMover->GetRigidBody();
		if (!rb) return;
		rb->State = v1.state;

		Car::StartBlinking(pVehicle, 0.5);
	}
};

struct tReplayGhost {
public:
	std::vector<tReplayTick> aTicks;
	uint32_t nStartTime;
	uint32_t nFinishTime;
	uint32_t nFinishPoints;
	std::string sPlayerName;
	Car* pLastVehicle;
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
		bIsPersonalBest = false;
	}

	tReplayTick* GetInterpolatedTick(double raceTime) {
		tReplayTick* pClosestWithoutGoingOver = &aTicks[0];
		tReplayTick* pNextTick = nullptr;
		for (auto& tick : aTicks) {
			if (tick.v1.timer <= raceTime) pClosestWithoutGoingOver = &tick;
		}

		if (pClosestWithoutGoingOver->v1.timer == raceTime) return pClosestWithoutGoingOver;
		if (pClosestWithoutGoingOver == &aTicks[aTicks.size()-1]) return nullptr;
		pNextTick = pClosestWithoutGoingOver + 1;

		auto delta = (raceTime - pClosestWithoutGoingOver->v1.timer) / (pNextTick->v1.timer - pClosestWithoutGoingOver->v1.timer);

		static tReplayTick InterpolatedTick;
		for (int i = 0; i < sizeof(RigidBodyState) / sizeof(float); i++) {
			auto fLast = (float*)&pClosestWithoutGoingOver->v1.state;
			auto fNext = (float*)&pNextTick->v1.state;
			auto fOut = (float*)&InterpolatedTick.v1.state;
			fOut[i] = std::lerp(fLast[i], fNext[i], delta);
		}
		InterpolatedTick.v1.timer = raceTime;
		return &InterpolatedTick;
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

void RunGhost(Car* veh, DriverInfo* driver, tReplayGhost* ghost) {
	if (!ghost) return;
	ghost->pLastVehicle = veh;

	if (ghost->sPlayerName.empty()) SetRacerName(driver, ghost->bIsPersonalBest ? "PERSONAL BEST" : "RACER");
	else {
		SetRacerName(driver, ghost->sPlayerName.c_str());
	}

	if (!ghost->IsValid()) {
		Car::StartBlinking(veh, 0.5);
		return;
	}

	auto tick = ghost->GetInterpolatedTick(ghost->GetPlaybackTime());
	if (!tick) return;
	tick->Apply(veh);
}

void RecordGhost(Car* veh) {
	// todo
	//if (!bChallengeSeriesMode) {
	//	switch (nNitroType) {
	//		case NITRO_OFF:
	//			veh->mCOMObject->Find<IEngine>()->ChargeNOS(-1);
	//			break;
	//		case NITRO_INF:
	//			veh->mCOMObject->Find<IEngine>()->ChargeNOS(1);
	//			break;
	//	}
	//}

	if (sPlayerNameOverride[0]) {
		SetRacerName(veh->pDriverInfo, sPlayerNameOverride);
	}

	tReplayTick state;
	state.Collect(veh);
	aRecordingTicks.push_back(state);
}

std::string GetGhostFilename(const std::string& car, int track, int lapCount, int opponentId, const char* folder = nullptr) {
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

	if (TheRaceParameters.TrackDirection == eDIRECTION_BACKWARD) {
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

	auto fileName = GetGhostFilename(car, track, lapCount, 0);
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
	int count = ghost->aTicks.size();
	outFile.write((char*)&count, sizeof(count));
	outFile.write((char*)&ghost->aTicks[0], sizeof(ghost->aTicks[0]) * count);
}

void LoadPB(tReplayGhost* ghost, const std::string& car, int track, int lapCount, int opponentId, const char* folder = nullptr) {
	bool doNOSSpdbrkChecks = IsPracticeMode();
	bool doUpgradeChecks = IsPracticeMode();

	ghost->aTicks.clear();
	ghost->nFinishTime = 0;
	ghost->nFinishPoints = 0;

	auto fileName = GetGhostFilename(car, track, lapCount, opponentId, folder);
	auto inFile = std::ifstream(fileName, std::ios::in | std::ios::binary);

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
		strcpy_s(tmpplayername, folder);
	}
	tmpplayername[nPlayerNameLength-1] = 0;
	ghost->sPlayerName = tmpplayername;
	ghost->nStartTime = tmpstarttime;
	ghost->nFinishTime = tmptime;
	ghost->nFinishPoints = tmppoints;

	// don't needlessly load the full ghost data when previewing times in the menu
	if (TheGameFlowManager.CurrentGameFlowState <= GAMEFLOW_STATE_IN_FRONTEND) return;

	ghost->aTicks.reserve(count);
	for (int i = 0; i < count; i++) {
		tReplayTick state = {};
		inFile.read((char*)&state.v1, sizeof(state.v1));
		ghost->aTicks.push_back(state);
	}
}

void OnChallengeSeriesEventPB();

void OnFinishRace() {
	uint32_t replayTime = fGlobalReplayTimerNoCountdown * 1000;
	uint32_t replayPoints = 0;
	bool isPointBased = false; // todo drifts
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
			SavePB(ghost, CarTypeInfoArray[PlayerCarType].CarTypeName, TheRaceParameters.TrackNumber, TheRaceParameters.NumLapsInRace);

			if (bChallengeSeriesMode) {
				OnChallengeSeriesEventPB();
			}
		}

		DoConfigSave();
	}
	aRecordingTicks.clear();
}

std::vector<tReplayGhost> CollectReplayGhosts(const std::string& car, int track, int laps, bool forFullLeaderboard = false) {
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
			LoadPB(&temp, car, track, laps, 0, folder.c_str());
			if (!temp.nFinishTime) continue;
			ghosts.push_back(temp);
		}
	}

	for (int i = 0; i < nMaxNumGhostsToCheck; i++) {
		tReplayGhost temp;
		LoadPB(&temp, car, track, laps, i+1);
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

tReplayGhost SelectTopGhost(const std::string& car, int track, int laps) {
	auto ghosts = CollectReplayGhosts(car, track, laps);
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

void InvalidatePlayerPos();

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

	if (IsInLoadingScreen()) {
		InvalidateLocalGhost();
		return;
	}

	if (GetIsGamePaused()) return;

	auto ply = GetLocalPlayerVehicle();
	if (nGhostsLoaded != TheRaceParameters.TrackNumber) {
		auto car = CarTypeInfoArray[PlayerCarType].CarTypeName;
		auto track = TheRaceParameters.TrackNumber;
		auto laps = TheRaceParameters.NumLapsInRace;
		LoadPB(&PlayerPBGhost, car, track, laps, 0);
		PlayerPBGhost.bIsPersonalBest = true;

		OpponentGhosts.clear();

		if (bChallengeSeriesMode) {
			aLeaderboardGhosts = CollectReplayGhosts(car, track, laps, true);

			if (bChallengesOneGhostOnly || bViewReplayMode || nDifficulty == DIFFICULTY_EASY) {
				auto opponent = SelectTopGhost(car, track, laps);
				if (opponent.nFinishTime != 0) {
					OpponentGhosts.push_back(opponent);
				}
			}
			else {
				auto ghosts = CollectReplayGhosts(car, track, laps);

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
				LoadPB(&OpponentGhosts[i], car, track, laps, i+1);
			}
		}

		nGhostsLoaded = TheRaceParameters.TrackNumber;
	}

	if (Player::pPlayersByIndex[0]->bFinishedRacing) {
		OnFinishRace();
	}

	if (!ShouldGhostRun()) return;

	//auto opponents = VEHICLE_LIST::GetList(VEHICLE_AIRACERS);
	//for (int i = 0; i < opponents.size(); i++) {
	//	opponents[i]->mCOMObject->Find<IRBVehicle>()->EnableObjectCollisions(false);
	//}

	if (IsPlayerStaging()) {
		fGlobalReplayTimerNoCountdown = 0;
	}

	if (bViewReplayMode) {
		RunGhost(ply, &TheRaceParameters.DriverInfos[0], GetViewReplayGhost());
	}
	else {
		// set fixed start points, super ultra hack
		//if (!bCareerMode && GetLocalPlayerVehicle()->IsStaging() && !OpponentGhosts.empty() && OpponentGhosts[0].IsValid()) {
		//	InvalidatePlayerPos();
		//	OpponentGhosts[0].aTicks[0].ApplyPhysics(GetLocalPlayerVehicle());
		//}

		for (int i = 0; i < GetNumOpponentsInRace(); i++) {
			RunGhost(GetCarByRacerId(i+1), &TheRaceParameters.DriverInfos[i+1], GetGhostForOpponent(i));
		}
	}

	RecordGhost(ply);
	if (!IsPlayerStaging()) {
		fGlobalReplayTimerNoCountdown += delta;
	}
	fGlobalReplayTimer += delta;
}

// special cases for some player names that are above 16 chars
std::string GetRealPlayerName(const std::string& ghostName) {
	if (ghostName == "Chloe") return "gaycoderprincess";
	if (ghostName == "ProfileInProces") return "ProfileInProcess";
	return ghostName;
}

float fLeaderboardX = 0.03;
float fLeaderboardY = 0.6;
float fLeaderboardYSpacing = 0.03;
float fLeaderboardSize = 0.03;
float fLeaderboardOutlineSize = 0.02;
void DisplayLeaderboard() {
	if (IsPracticeMode()) return;
	if (MovieIsStarted) return;

	if (IsPlayerStaging() || World::IsWorldPaused(pCurrentWorld)) {
		std::vector<std::string> uniquePlayers;

		auto leaderboard = aLeaderboardGhosts;
		if (PlayerPBGhost.IsValid()) {
			tReplayGhost dummy;
			dummy.sPlayerName = sPlayerNameOverride[0] ? sPlayerNameOverride : GetLocalPlayerName();
			dummy.nFinishTime = PlayerPBGhost.nFinishTime;
			dummy.nFinishPoints = PlayerPBGhost.nFinishPoints;
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
#ifdef TIMETRIALS_CARBON
				data.SetColor(126, 246, 240, 255);
#else
				data.SetColor(245, 185, 110, 255);
#endif
			}
			else {
				data.SetColor(255, 255, 255, 255);
			}

			bool isPointBased = false;
			std::string str;
			if (isPointBased) {
				str = std::format("{}. {} - {}", ranking++, name, ghost.nFinishPoints);
			}
			else {
				auto time = GetTimeFromMilliseconds(ghost.nFinishTime);
				time.pop_back();
				str = std::format("{}. {} - {}", ranking++, name, time);
			}
			DrawString(data, str);
			data.y += fLeaderboardYSpacing;
		}
	}
}

void DisplayPlayerNames() {
	// todo
	/*if (bChallengeSeriesMode && nGhostVisuals != GHOST_HIDE && !GetIsGamePaused()) {
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

			float carHeight = 1.5;
			//UMath::Vector3 dim;
			//car->mCOMObject->Find<IRigidBody>()->GetDimension(&dim);

			auto pos = GetCarPosition(car);
			pos.y += carHeight;

			auto cam = PrepareCameraMatrix(GetLocalPlayerCamera());
			auto camFwd = RenderToWorldCoords(cam.z);
			auto camPos = RenderToWorldCoords(cam.p);
			auto playerDir = camPos - pos;
			auto cameraDist = playerDir.length();
			playerDir.Normalize();
			if (playerDir.Dot(camFwd) > 0) continue;
			if (cameraDist > fPlayerNameFadeEnd) continue;

			bVector3 screenPos;
			auto worldPos = WorldToRenderCoords(pos);
			eViewPlatInterface::GetScreenPosition(&eViews[EVIEW_PLAYER1], &screenPos, (bVector3*)&worldPos);

			screenPos.x /= (double)nResX;
			screenPos.y /= (double)nResY;
			//if (screenPos.z <= 0) continue;

			tNyaStringData data;
			data.x = screenPos.x;
			data.y = screenPos.y - fPlayerNameOffset;
			data.size = fPlayerNameSize;
			data.XCenterAlign = true;
			if (cameraDist > fPlayerNameFadeStart) {
				data.a = std::lerp(fPlayerNameAlpha, 0, (cameraDist - fPlayerNameFadeStart) / (fPlayerNameFadeEnd - fPlayerNameFadeStart));
			}
			else {
				data.a = fPlayerNameAlpha;
			}
			DrawString(data, name);
		}
	}*/
}

void TimeTrialRenderLoop() {
	if (TheGameFlowManager.CurrentGameFlowState != GAMEFLOW_STATE_RACING) return;
	if (IsInLoadingScreen()) return;

	DisplayLeaderboard();

	if (!ShouldGhostRun()) return;

	// todo
	/*if (!GetIsGamePaused()) {
		if (bViewReplayMode) {
			auto ghost = GetViewReplayGhost();

			auto tick = ghost->GetCurrentTick();
			if (ghost->aTicks.size() > tick) {
				DisplayInputs(&ghost->aTicks[tick].v1.inputs);
			}
		}
		else if (bShowInputsWhileDriving) {
			DisplayInputs(GetLocalPlayerInterface<IInput>()->GetControls());
		}
	}*/

	DisplayPlayerNames();
}

void ChallengeSeriesMenu();

void DebugMenu() {
	ChloeMenuLib::BeginMenu();

	if (DrawMenuOption("Challenge Series")) {
		ChloeMenuLib::BeginMenu();
		if (TheGameFlowManager.CurrentGameFlowState == GAMEFLOW_STATE_RACING) {
			if (DrawMenuOption("Quit Event")) {
				SkipFE = false;
				CAnimManager::EndNIS_SafeRemove(&TheAnimManager);
				cFrontendDatabase::NotifyExitRaceToFrontend(&FEDatabase, false);
				GameFlowManager::UnloadTrack(&TheGameFlowManager);
			}
		}
		else {
			ChallengeSeriesMenu();
		}
		ChloeMenuLib::EndMenu();
	}

	if (DrawMenuOption("Options")) {
		ChloeMenuLib::BeginMenu();

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
			ChloeMenuLib::EndMenu();
		}
	}

	DrawMenuOption(std::format("Race - {}", TheRaceParameters.TrackNumber));

	ChloeMenuLib::EndMenu();
}