void UpdateD3DProperties() {
	g_pd3dDevice = GameD3DDevice;
	ghWnd = GameWindow;
	GetRacingResolution(&nResX, &nResY);
}

bool bDeviceJustReset = false;
void D3DHookMain() {
	DLLDirSetter _setdir;

	if (!g_pd3dDevice) {
		UpdateD3DProperties();
		InitHookBase();
	}

	if (bDeviceJustReset) {
		ImGui_ImplDX9_CreateDeviceObjects();
		bDeviceJustReset = false;
	}
	HookBaseLoop();
}

void OnD3DReset() {
	DLLDirSetter _setdir;

	if (g_pd3dDevice) {
		UpdateD3DProperties();
		ImGui_ImplDX9_InvalidateDeviceObjects();
		bDeviceJustReset = true;
	}
}

void RenderLoop();
void HookLoop() {
	RenderLoop();
	CommonMain();
}