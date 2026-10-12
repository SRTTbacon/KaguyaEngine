#include "Main.h"
#include "WindowSetting.h"

#include "..\\..\\Engine.h"
#include "..\\..\\..\\GameLoop.h"

#include <minwindef.h>
#ifdef _DEBUG
#include <dxgidebug.h>
#endif
#include <cstdio>
#include <memory>
#include <Windows.h>

using namespace KaguyaEngine;

HINSTANCE g_hInst;
HWND g_hWnd = NULL;

//FPS計算用の変数
static int frameCount = 0;
static float elapsedTime = 0.0f;

//extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);                // Use ImGui::GetCurrentContext()

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	//if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wp, lp))
		//return true;

	switch (msg)
	{
	case WM_ACTIVATE:
		// ウィンドウがアクティブかどうかを判定()
		//g_WindowActive = !(LOWORD(wp) == WA_INACTIVE);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:

		break;
	}

	return DefWindowProc(hWnd, msg, wp, lp);
}

static void InitWindow(const TCHAR* appName)
{
	g_hInst = GetModuleHandle(nullptr);
	if (g_hInst == nullptr)
	{
		return;
	}

	//ウィンドウの設定
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;

	//wc.hIcon = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);

	wc.hCursor = LoadCursor(g_hInst, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = appName;
	wc.hIconSm = LoadIcon(g_hInst, IDI_APPLICATION);

	//ウィンドウクラスの登録。
	RegisterClassEx(&wc);

	//ウィンドウサイズの設定
	RECT rect = {};
	rect.right = static_cast<LONG>(DEFAULT_WINDOW_SIZE.x);
	rect.bottom = static_cast<LONG>(DEFAULT_WINDOW_SIZE.y);

	//ウィンドウサイズを調整
	auto style = WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU;
	AdjustWindowRect(&rect, style, FALSE);

	//モニターの解像度を取得
	HMONITOR hMonitor = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTONEAREST);
	MONITORINFO monitorInfo = {};
	monitorInfo.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(hMonitor, &monitorInfo);

	//ウィンドウの生成
	g_hWnd = CreateWindowEx(
		0,
		appName,
		appName,
		style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		g_hInst,
		nullptr
	);

	//ウィンドウを表示
	ShowWindow(g_hWnd, SW_SHOW);

	//ウィンドウにフォーカスする
	SetFocus(g_hWnd);
}

static int MainLoop()
{
	MSG msg = {};

	LARGE_INTEGER frequency;	//パフォーマンス周波数
	LARGE_INTEGER prevTime;		//前フレームの時間
	LARGE_INTEGER currentTime;	//現在の時間

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&prevTime);

	//ゲームクラス
	std::unique_ptr<Engine> pEngine = std::make_unique<Engine>(g_hWnd);
	pEngine->Initialize();

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			//終了処理(QUITﾒｯｾｰｼﾞで終了)
			if (msg.message == WM_QUIT) {
				//clsGame.Release();
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			//現在の時間を取得
			QueryPerformanceCounter(&currentTime);
			float deltaTime = static_cast<float>(currentTime.QuadPart - prevTime.QuadPart) / frequency.QuadPart;
			prevTime = currentTime;

			//経過時間を加算
			elapsedTime += deltaTime;
			frameCount++;

			//1秒を超えたらFPSを計算
			if (elapsedTime >= 1.0f) {
				int fps = frameCount;
				printf("%d[FPS] - CPU: %.2f ms, GPU: %.2f ms\n", fps, g_Engine->GetCPUTime(), g_Engine->GetGPUTime());
				frameCount = 0;
				elapsedTime = 0.0f;
			}

			Game* pGame = Game::GetInstance();

			if (pGame && !pGame->Run())
			{
				DestroyWindow(g_hWnd);
				Game::ClearInstance();	// エンジンよりも前に解放させる
				return 0;
			}

			pEngine->Update();

			if (pGame) {
				pGame->UpdateScene();
			}

			pEngine->LateUpdate();
			pEngine->Begin3DRender();
			pEngine->Begin2DRender();

			if (pGame) {
				pGame->Draw2D();
			}

			pEngine->EndRender();
		}

	}

	Game::ClearInstance();	// エンジンよりも前に解放させる

	return 0;
}

int StartApp()
{
	//dllの参照場所を指定
	if (!SetDllDirectory(TEXT("Resource\\dll"))) {
		printf("dllディレクトリの指定に失敗しました。");
		return 1;
	}

	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	//LoadLibrary(L"WinPixGpuCapturer.dll");

	//ウィンドウ生成
	InitWindow(DEFAULT_WINDOW_NAME);

	//メイン処理ループ
	int r = MainLoop();
#ifdef _DEBUG
	/*
	ComPtr<IDXGIDebug1> dxgiDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
	}
	*/
#endif

	return r;
}
