#if 1//def USE_RAWINPUT
#include "System/Input/InputSystem.h"
#include "Runtime/RuntimeEngineCore.h"
#include <Windows.h>
//#include "Application.h"

using namespace Shard;
namespace SI = namespace System::Input;

class EngineApplication 
{
public:
    static constexpr Tchar CLASS_NAME[] = STR_PREFIX("Shard Engine Example");
    explicit EngineApplication(HINSTANCE hinst) : hinst_(hinst) {
        WNDCLASSEX wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hinst;
        wc.lpszClassName = CLASS_NAME;
        RegisterClassEx(&wc);

        CreateEngineWindow();

        engine_ = new Runtime::ApplicationEngine;
        engine_->Init(static_cast<Utils::WindowHandle>(hwnd_));
    }
    ~EngineApplication() {
        UnregisterClass(CLASS_NAME, hinst_);
        DestroyWindow(hwnd_);
        if (engine_) {
            engine_->UnInit();
            delete engine_;
        }
    }
    int Run(int argc, char** argv) {
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
        ShowWindow(hwnd_, SW_SHOW);
        MSG message{};
        while (message.message != WM_QUIT) {
            if (PeekMessage(&message, hwnd_, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
            else
            {
                RunLoop();
            }
        }

        return (int)message.wParam;
    }
    LRESULT CALLBACK ProcMessage(HWND hwnd, UINT message, WPARAM paramw, LPARAM paraml) {
        assert(hwnd == hwnd_);
        switch (message) {
        case WM_MOUSEMOVE:
        case WM_INPUT:
        {
            SI::InputMessage input_message;
            auto* message_ptr = reinterpret_cast<uint64_t*>(&input_message);
            *message_ptr++ = message;
            *message_ptr++ = paramw;
            *message_ptr = paraml;
            engine_->GetInputSystem()->ForwardMessage(input_message);
            break;
        }
        case WM_QUIT:
            PostQuitMessage(0);
            break;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            //todo use hdc
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_NCCALCSIZE:
        {
            if (bool(paramw) == true) {
                auto* lpncsp = reinterpret_cast<NCCALCSIZE_PARAMS*>(paraml);
                lpncsp->rgrc[2] = lpncsp->rgrc[1];
                lpncsp->rgrc[1] = lpncsp->rgrc[0];
            }
            break;
        }
        case WM_NCHITTEST:
        {
            //when there's no border or title bar, deal with resizing and moving
            break;
        }
        default:
        }
        return DefWindowProc(hwnd, message, paramw, paraml);
    }
private:
    void RunLoop() {
        engine_->Run();
    }
    void CreateEngineWindow() {
        hwnd_ = CreateWindow(CLASS_NAME, CLASS_NAME, WS_OVERLAPPEDWINDOW | WS_THICKFRAME | WS_BORDER | WS_MAXIMIZE | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                                CW_USEDEFAULT, nullptr, nullptr, hinst_, this);
        if (nullptr == hwnd_) {
            throw;
        }
    }
private:
    double    delta_time_{ 0.f };
    Runtime::Engine* engine_{ nullptr };
    HWND    hwnd_{ nullptr };
    HINSTANCE    hinst_{ nullptr };
};

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM paramw, LPARAM paraml) {
    auto* app = reinterpret_cast<EngineApplication*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (nullptr != app) {
        return app->ProcMessage(hwnd, message, paramw, paraml);
    }
    return DefWindowProc(hwnd, message, paramw, paraml);
}

int APIENTRY _tWinMain(HINSTANCE hinst, HINSTANCE hinst_prev, PSTR cmdline, int cmdshow)
{
    EngineApplication application(hinst);
    return application.Run(__argc, __argv);
}


#endif