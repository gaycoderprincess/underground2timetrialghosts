void WriteLog(const std::string& str) {
	static auto file = std::ofstream("NFSUG2TimeTrialGhosts_gcp.log");

	file << str;
	file << "\n";
	file.flush();
}

bool IsInLoadingScreen() {
	return FEngIsPackagePushed("PC_Loading.fng");
}

enum eCwoeeEventType {
	EVENT_RACE,
	EVENT_DRAG,
	EVENT_DRIFT,
	EVENT_URL,
	EVENT_SHORT_TRACK,
	EVENT_OUTRUN,
	EVENT_FREE_RUN,
	EVENT_BURNOUT,
};

Car* GetNthCar(int n) {
	if (!pCurrentWorld) return nullptr;
	auto car = pCurrentWorld->pCarTable;
	for (int i = 0; i < n; i++) {
		car = car->pNext;
		if (!car) return nullptr;
	}
	return car;
}

int GetNumCarsInWorld() {
	return pCurrentWorld->nNumCars;
}

Car* GetCarByDriverInfo(DriverInfo* driver) {
	for (int i = 0; i < GetNumCarsInWorld(); i++) {
		auto car = GetNthCar(i);
		if (car->pDriverInfo == driver) return car;
	}
	return nullptr;
}

Car* GetCarByRacerId(int i) {
	if (i >= TheRaceParameters.NumDriverInfo) return nullptr;
	return GetCarByDriverInfo(&TheRaceParameters.DriverInfos[i]);
}

int GetNumOpponentsInRace() {
	return TheRaceParameters.NumDriverInfo-1;
}

Car* GetLocalPlayerVehicle() {
	return GetCarByRacerId(0);
}

void SetRacerName(DriverInfo* racer, const char* name) {
	strcpy_s(racer->sPlayerName, 16, name);
}

bool GetIsGamePaused() {
	return pRaceCoordinator->RaceState == 1 || pRaceCoordinator->RaceState == 6 || World::IsWorldPaused(pCurrentWorld);
}

const char* GetLocalPlayerName() {
	return TheRaceParameters.DriverInfos[0].sPlayerName;
}

bool IsPlayerStaging() {
	auto number = Race::GetCountdownNumberToDisplay(pCurrentRace);
	return number != 0 && number != 4;
}

bool IsVehicleValidAndActive(Car* car) {
	for (int i = 0; i < GetNumCarsInWorld(); i++) {
		if (GetNthCar(i) == car) return true;
	}
	return false;
}

bVector3 GetCarPosition(Car* car) {
	if (!car) return {};
	if (car->nMovementMode != PHYSICS_MOVEMENT) return {};
	auto mover = car->pMover;
	if (!mover) return {};
	auto rb = mover->GetRigidBody();
	if (!rb) return {};
	return rb->State.vPosition;
}

void ValueEditorMenu(float& value) {
	ChloeMenuLib::BeginMenu();

	static char inputString[1024] = {};
	ChloeMenuLib::AddTextInputToString(inputString, 1024, true);
	ChloeMenuLib::SetEnterHint("Apply");

	if (DrawMenuOption(inputString + (std::string)"...", "", false, false) && inputString[0]) {
		value = std::stof(inputString);
		memset(inputString,0,sizeof(inputString));
		ChloeMenuLib::BackOut();
	}

	ChloeMenuLib::EndMenu();
}

void ValueEditorMenu(int& value) {
	ChloeMenuLib::BeginMenu();

	static char inputString[1024] = {};
	ChloeMenuLib::AddTextInputToString(inputString, 1024, true);
	ChloeMenuLib::SetEnterHint("Apply");

	if (DrawMenuOption(inputString + (std::string)"...", "", false, false) && inputString[0]) {
		value = std::stoi(inputString);
		memset(inputString,0,sizeof(inputString));
		ChloeMenuLib::BackOut();
	}

	ChloeMenuLib::EndMenu();
}

void ValueEditorMenu(char* value, int len) {
	ChloeMenuLib::BeginMenu();

	static char inputString[1024] = {};
	ChloeMenuLib::AddTextInputToString(inputString, 1024, false);
	ChloeMenuLib::SetEnterHint("Apply");

	if (DrawMenuOption(inputString + (std::string)"...", "", false, false) && inputString[0]) {
		strcpy_s(value, len, inputString);
		memset(inputString,0,sizeof(inputString));
		ChloeMenuLib::BackOut();
	}

	ChloeMenuLib::EndMenu();
}

void QuickValueEditor(const char* name, float& value) {
	if (DrawMenuOption(std::format("{} - {}", name, value))) { ValueEditorMenu(value); }
}

void QuickValueEditor(const char* name, int& value) {
	if (DrawMenuOption(std::format("{} - {}", name, value))) { ValueEditorMenu(value); }
}

void QuickValueEditor(const char* name, bool& value) {
	if (DrawMenuOption(std::format("{} - {}", name, value))) { value = !value; }
}

void QuickValueEditor(const char* name, char* value, int len) {
	if (DrawMenuOption(std::format("{} - {}", name, value))) { ValueEditorMenu(value, len); }
}

std::filesystem::path gDLLPath;
wchar_t gDLLDir[MAX_PATH];
class DLLDirSetter {
public:
	wchar_t backup[MAX_PATH];

	DLLDirSetter() {
		GetCurrentDirectoryW(MAX_PATH, backup);
		SetCurrentDirectoryW(gDLLDir);
	}
	~DLLDirSetter() {
		SetCurrentDirectoryW(backup);
	}
};