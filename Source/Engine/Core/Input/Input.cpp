#include "Input.h"
#include "..\\CoreMinimal.h"

#include <timeapi.h>
#include <string.h>
#include <dinput.h>
#include <DirectXMath.h>
#include <vector>
#include <cmath>
#include <Xinput.h>
#include <Windows.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Xinput.lib")

using namespace DirectX;

namespace KaguyaEngine
{
    //パッドの入力範囲
    const int Input::PAD_RANGE = 1000;

    //解放マクロ
#define Release(X) { if((X) != nullptr) (X)->Release(); (X) = nullptr; }

//コンストラクタ
    Input::Input(HWND win)
        : m_hwnd(win)
        , result(S_OK)
        , input(nullptr)
        , key(nullptr)
        , m_beforeMouseClickedTime(0)
        , m_doubleClickInterval(0.4f)
        , m_mousePos(0, 0)
        , m_bCanResponseUnFocus(false)
        , m_bDoubleClicked(false)
        , m_connectedGamePad(false)
    {
        memset(&keys, 0, sizeof(keys));
        memset(&olds, 0, sizeof(olds));

        CreateInput();
        CreateKey();
        SetKeyFormat();
        SetKeyCooperative();
        CreateMouseState();

        InitializePad(win);
    }

    //デストラクタ
    Input::~Input()
    {
        Release(key);
        Release(input);
    }

    //インプットの生成
    HRESULT Input::CreateInput()
    {
        result = DirectInput8Create(GetModuleHandle(0), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)(&input), NULL);

        return result;
    }
    //キーデバイスの生成

    HRESULT Input::CreateKey()
    {
        result = input->CreateDevice(GUID_SysKeyboard, &key, NULL);

        return result;
    }

    //キーフォーマットのセット
    HRESULT Input::SetKeyFormat()
    {
        result = key->SetDataFormat(&c_dfDIKeyboard);

        return result;
    }

    //キーの協調レベルのセット
    HRESULT Input::SetKeyCooperative()
    {
        result = key->SetCooperativeLevel(m_hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

        //入力デバイスへのアクセス権利を取得
        key->Acquire();

        return result;
    }

    bool Input::CreateMouseState()
    {
        //マウスデバイスの作成
        if (FAILED(input->CreateDevice(GUID_SysMouse, &m_pMouseDevice, nullptr))) {
            return false;
        }

        //データフォーマットの指定
        if (FAILED(m_pMouseDevice->SetDataFormat(&c_dfDIMouse))) {
            return false;
        }

        //協調レベルの設定（アクティブかつ非排他）
        if (FAILED(m_pMouseDevice->SetCooperativeLevel(m_hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE))) {
            return false;
        }

        //プロパティの設定
        DIPROPDWORD diProperty{};

        diProperty.diph.dwSize = sizeof(diProperty);
        diProperty.diph.dwHeaderSize = sizeof(diProperty.diph);
        diProperty.diph.dwObj = 0;
        diProperty.diph.dwHow = DIPH_DEVICE;
        diProperty.dwData = DIPROPAXISMODE_REL;

        if (FAILED(m_pMouseDevice->SetProperty(DIPROP_AXISMODE, &diProperty.diph))) {
            return false;
        }

        //アクセス権の取得
        m_pMouseDevice->Acquire();

        //チェック用領域のクリア
        memset(m_mouseCheck.rgbButtons, 0, sizeof(uint8) * (sizeof(m_mouseCheck.rgbButtons) / sizeof(m_mouseCheck.rgbButtons[0])));

        return true;
    }

    //キー入力
    bool Input::GetKeyState(uint32 index) const
    {
        //ウィンドウにフォーカスがない場合はキー操作を受け付けない
        if (!m_bCanResponseUnFocus && GetForegroundWindow() != m_hwnd) {
            return false;
        }

        return keys[index] & 0x80;
    }

    //トリガーの入力
    bool Input::GetKeyStateSync(uint32 index) const
    {
        //ウィンドウにフォーカスがない場合はキー操作を受け付けない
        if (!m_bCanResponseUnFocus && GetForegroundWindow() != m_hwnd) {
            return false;
        }

        return (keys[index] & 0x80) && !(olds[index] & 0x80);
    }

    bool Input::GetKeyStateRelease(uint32 index) const
    {
        //ウィンドウにフォーカスがない場合はキー操作を受け付けない
        if (!m_bCanResponseUnFocus && GetForegroundWindow() != m_hwnd) {
            return false;
        }

        return !(keys[index] & 0x80) && (olds[index] & 0x80);
    }

    void Input::Update()
    {
        //現在の状態を保存しておく
        m_prevMouseState = m_mouseState;

        if (FAILED(m_pMouseDevice->GetDeviceState(sizeof(DIMOUSESTATE), &m_mouseState))) {
            m_pMouseDevice->Acquire();
            memset(&m_mouseState, 0, sizeof(m_mouseState)); //エラー時はクリア
        }

        memcpy(olds, keys, sizeof(keys));

        //キー情報を取得
        key->GetDeviceState(sizeof(keys), &keys);

        //押した/離した 瞬間を検出する
        for (int i = 0; i < 4; ++i) {
            bool prev = (m_prevMouseState.rgbButtons[i] & 0x80) != 0;
            bool curr = (m_mouseState.rgbButtons[i] & 0x80) != 0;

            m_mouseDownFrame[i] = (!prev && curr);   //押した瞬間
            m_mouseUpFrame[i] = (prev && !curr);   //離した瞬間
        }

        //ダブルクリック判定
        m_bDoubleClicked = false;
        if (GetMouseStateSync(DIK_LBUTTON)) {
            uint32 time = timeGetTime();
            if (time - m_beforeMouseClickedTime <= static_cast<uint32>(m_doubleClickInterval * 1000.0f) && time > m_beforeMouseClickedTime) {
                m_bDoubleClicked = true;
            }
            m_beforeMouseClickedTime = time;
        }

        //マウス座標
        GetCursorPos(&m_mousePos);
        ScreenToClient(m_hwnd, &m_mousePos);

        //GamePadを更新
        UpdateGamePad();
    }

    POINT Input::GetMouseMove() const
    {
        POINT point{};

        point.x = m_mouseState.lX;
        point.y = m_mouseState.lY;

        return point;
    }

    POINT Input::GetMousePosition() const
    {
        return m_mousePos;
    }

    int Input::GetMouseWheelDelta() const
    {
        return m_mouseState.lZ;
    }

    uint8 Input::GetMouseState(const uint8 keyCode) const
    {
        return m_mouseState.rgbButtons[keyCode];
    }

    uint8 Input::GetMouseStateSync(const uint8 keyCode) const
    {
        return m_mouseDownFrame[keyCode];
    }

    uint8 Input::GetMouseStateRelease(const uint8 keyCode) const
    {
        return m_mouseUpFrame[keyCode];
    }

    bool Input::GetMouseStateDoubleClicked() const
    {
        return m_bDoubleClicked;
    }

    bool Input::InitializePad(const HWND& hAppWnd)
    {
        //デバイスの列挙
        input->EnumDevices(DI8DEVCLASS_GAMECTRL, (LPDIENUMDEVICESCALLBACK)EnumGamepadCallback, this, DIEDFL_ATTACHEDONLY);

        //ゲームパッドが無い場合は正常終了
        if (m_vecPad.empty()) {
            return true;
        }

        //ゲームパッドがあった場合は各種設定を行う
        for (unsigned int i = 0; i < m_vecPad.size(); i++) {

            //データフォーマットの指定
            if (FAILED(m_vecPad[i].m_pPadDevice->SetDataFormat(&c_dfDIJoystick))) {
                return false;
            }

            //協調レベルの設定（アクティブかつ非排他）
            if (FAILED(m_vecPad[i].m_pPadDevice->SetCooperativeLevel(hAppWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE))) {
                return false;
            }

            //チェック用データの初期化
            memset(&m_vecPad[i].m_padCheck, 0, sizeof(m_vecPad[i].m_padCheck));

            //パッドの軸範囲などの設定
            m_vecPad[i].m_pPadDevice->EnumObjects(EnumAxisCallback, &m_vecPad[i], DIDFT_AXIS);

            if (FAILED(m_vecPad[i].m_pPadDevice->Poll())) {

                while (m_vecPad[i].m_pPadDevice->Acquire() == DIERR_INPUTLOST);
            }

            for (int j = 0; j < 32; j++) {
                m_vecPad[i].m_triggeredGamePad2.push_back(false);
                m_vecPad[i].m_releasedGamePad2.push_back(false);
            }
        }

        return true;
    }

    void Input::UpdateGamePad()
    {
        m_prevStateGamePad = m_stateGamePad;

        //コントローラー0番をチェック
        ZeroMemory(&m_stateGamePad, sizeof(XINPUT_STATE));
        uint32 result = XInputGetState(0, &m_stateGamePad);

        m_connectedGamePad = (result == ERROR_SUCCESS);

        m_triggeredGamePad.clear();
        m_releasedGamePad.clear();
        if (m_connectedGamePad) {
            uint16 currButtons = m_stateGamePad.Gamepad.wButtons;
            uint16 prevButtons = m_prevStateGamePad.Gamepad.wButtons;

            for (int i = 0; i < 14; ++i) {
                uint16 mask = (1 << i);
                bool curr = (currButtons & mask) != 0;
                bool prev = (prevButtons & mask) != 0;

                if (curr && !prev) {
                    m_triggeredGamePad[mask] = true;
                }
                if (!curr && prev) {
                    m_releasedGamePad[mask] = true;
                }
            }

            //XBox対応のコントローラーが接続されていればm_vecPadの更新はしない
            return;
        }

        if (m_vecPad.empty()) {
            return;
        }

        for (unsigned int i = 0; i < m_vecPad.size(); i++) {

            m_vecPad[i].m_pPadDevice->Poll();

            m_vecPad[i].m_padPrevState = m_vecPad[i].m_padState;

            if (FAILED(m_vecPad[i].m_pPadDevice->GetDeviceState(sizeof(DIJOYSTATE), &m_vecPad[i].m_padState))) {
                m_vecPad[i].m_pPadDevice->Acquire();
            }

            for (int j = 0; j < 32; j++) {
                m_vecPad[i].m_triggeredGamePad2[j] = false;
                m_vecPad[i].m_releasedGamePad2[j] = false;

                uint8 currButton = m_vecPad[i].m_padState.rgbButtons[j];
                uint8 prevButton = m_vecPad[i].m_padPrevState.rgbButtons[j];

                if (currButton && !prevButton) {
                    m_vecPad[i].m_triggeredGamePad2[j] = true;
                }
                if (!currButton && prevButton) {
                    m_vecPad[i].m_releasedGamePad2[j] = true;
                }
            }
        }
    }

    //-----------------------------------------------------------------------------
    // ゲームパッドの列挙（コールバック関数）
    // 引　数：VOID* InputManageクラスのポインタ
    //-----------------------------------------------------------------------------
    int _stdcall EnumGamepadCallback(LPDIDEVICEINSTANCE pdidInstance, VOID* pContext)
    {
        std::vector<GamePad>& vecPad = static_cast<Input*>(pContext)->GetPadVector();

        GamePad gamePad{};

        gamePad.m_pPadDevice = nullptr;

        vecPad.push_back(gamePad);

        if (FAILED(((Input*)pContext)->GetDirectInput()->CreateDevice(pdidInstance->guidInstance, &(vecPad.back().m_pPadDevice), nullptr))) {
            return DIENUM_STOP;
        }

        return DIENUM_CONTINUE;
    }

    //-----------------------------------------------------------------------------
    // ゲームパッドの列挙（コールバック関数）
    // 引　数：VOID* GamePad構造体のポインタ
    //-----------------------------------------------------------------------------
    int _stdcall EnumAxisCallback(const DIDEVICEOBJECTINSTANCE* pdidObjectInstance, VOID* pContext)
    {
        GamePad* pGamePad = reinterpret_cast<GamePad*>(pContext);

        pGamePad->m_padRange.diph.dwSize = sizeof(DIPROPRANGE);
        pGamePad->m_padRange.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        pGamePad->m_padRange.diph.dwHow = DIPH_BYID;
        pGamePad->m_padRange.diph.dwObj = pdidObjectInstance->dwType;
        pGamePad->m_padRange.lMin = -Input::PAD_RANGE;
        pGamePad->m_padRange.lMax = +Input::PAD_RANGE;

        if (FAILED(pGamePad->m_pPadDevice->SetProperty(DIPROP_RANGE, &pGamePad->m_padRange.diph))) {
            return DIENUM_STOP;
        }

        return DIENUM_CONTINUE;
    }

    bool Input::IsConnectedGamePad() const
    {
        return m_connectedGamePad || !m_vecPad.empty();
    }

    XMFLOAT2 Input::GetPadLeftStick() const
    {
        if (!m_connectedGamePad && m_vecPad.size() <= 0) {
            return XMFLOAT2(0.0f, 0.0f);
        }

        if (m_connectedGamePad) {
            SHORT x = m_stateGamePad.Gamepad.sThumbLX;
            SHORT y = m_stateGamePad.Gamepad.sThumbLY;

            //デッドゾーン処理
            const SHORT deadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
            if (abs(x) < deadzone && abs(y) < deadzone) {
                return XMFLOAT2(0.0f, 0.0f);
            }

            float valueX = static_cast<float>(x) / 32767.0f;
            float valueY = static_cast<float>(y) / 32767.0f;

            return XMFLOAT2(valueX, valueY);
        }
        else {
            float valueX = m_vecPad[0].m_padState.lX / static_cast<float>(PAD_RANGE);
            float valueY = -m_vecPad[0].m_padState.lY / static_cast<float>(PAD_RANGE);

            return XMFLOAT2(valueX, valueY);
        }
    }

    XMFLOAT2 Input::GetPadRightStick() const
    {
        if (!m_connectedGamePad && m_vecPad.size() <= 0) {
            return XMFLOAT2(0.0f, 0.0f);
        }

        if (m_connectedGamePad) {
            SHORT x = m_stateGamePad.Gamepad.sThumbRX;
            SHORT y = m_stateGamePad.Gamepad.sThumbRY;

            //デッドゾーン処理
            const SHORT deadzone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
            if (abs(x) < deadzone && abs(y) < deadzone) {
                return XMFLOAT2(0.0f, 0.0f);
            }

            float valueX = static_cast<float>(x) / 32767.0f;
            float valueY = static_cast<float>(y) / 32767.0f;

            return XMFLOAT2(valueX, valueY);
        }
        else {
            float valueX = m_vecPad[0].m_padState.lZ / static_cast<float>(PAD_RANGE);
            float valueY = -m_vecPad[0].m_padState.lRz / static_cast<float>(PAD_RANGE);

            return XMFLOAT2(valueX, valueY);
        }
    }

    float Input::GetLeftTrigger() const
    {
        if (!m_connectedGamePad && m_vecPad.empty()) {
            return 0.0f;
        }

        if (m_connectedGamePad) {
            uint8 rt = m_stateGamePad.Gamepad.bLeftTrigger;
            const uint8 threshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

            if (rt < threshold) {
                return 0.0f; //デッドゾーン
            }

            return static_cast<float>(rt) / 255.0f;
        }
        else {
            return static_cast<float>(GetPadButtonState(6));
        }
    }

    float Input::GetRightTrigger() const
    {
        if (!m_connectedGamePad && m_vecPad.empty()) {
            return 0.0f;
        }

        if (m_connectedGamePad) {
            uint8 rt = m_stateGamePad.Gamepad.bRightTrigger;
            const uint8 threshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

            if (rt < threshold) {
                return 0.0f; //デッドゾーン
            }

            return static_cast<float>(rt) / 255.0f;
        }
        else {
            return static_cast<float>(GetPadButtonState(7));
        }
    }

    bool Input::GetPadButtonState(WORD button) const
    {
        if (!m_connectedGamePad && m_vecPad.empty()) {
            return false;
        }

        if (m_connectedGamePad) {
            return (m_stateGamePad.Gamepad.wButtons & button) != 0;
        }
        else {
            if (button < 0 || button >= sizeof(m_vecPad[0].m_padState.rgbButtons) / sizeof(m_vecPad[0].m_padState.rgbButtons[0])) {
                return 0;
            }

            return m_vecPad[0].m_padState.rgbButtons[button];
        }
    }

    bool Input::GetPadButtonStateSync(WORD button)
    {
        if (!m_connectedGamePad && m_vecPad.empty()) {
            return false;
        }
        if (m_connectedGamePad) {
            return m_triggeredGamePad[button];
        }
        else {
            return m_vecPad[0].m_triggeredGamePad2[button];
        }
    }

    bool Input::GetPadButtonStateRelease(WORD button)
    {
        if (!m_connectedGamePad && m_vecPad.empty()) {
            return false;
        }

        if (m_connectedGamePad) {
            return m_releasedGamePad[button];
        }
        else {
            return m_vecPad[0].m_triggeredGamePad2[button];
        }
    }
}
