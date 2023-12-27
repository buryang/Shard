#if defined(_WIN32) && !defined(USE_SDL)
#define USE_RAWINPUT
#endif
#ifdef USE_SDL
#include "SDL3/SDL.h"
#endif
#ifdef USE_IMGUI
#include "imgui.h"
#endif
#ifdef USE_RAWINPUT
#include <hidsdi.h>
#include <winuser.h>
#endif
#include "glog/logging.h"
#include "Utils/Memory.h"
#include "InputSystem.h"

namespace Shard::System
{

        namespace Input 
        {

                namespace State
                {
                        MouseState        mouse_state;
                        KeyBoardState        keyboard_state;
                        constexpr uint32_t MAX_CONTROLLER_COUNT{ 4u }; //xbox controller upto 4
                        Array<ControllerState, MAX_CONTROLLER_COUNT> controller_state;
                };
#ifdef USE_SDL
                class InputSystemImplSDL
                {
                public:
                        static void Process(const InputMessage& message) {
                                const auto* event = reinterpret_cast<const SDL_Event*>(&message);
                                switch (event->type) {
                                        //keyboard
                                case SDL_EVENT_KEY_DOWN:
                                        const auto bt = ConvertScancodeToEButton(event->key.keysym.scancode, event->key.keysym.sym);
                                        State::keyboard_state.pressed_[Utils::EnumToInteger(bt)] = true;
                                        break;
                                case SDL_EVENT_KEY_UP:
                                        const auto bt = ConvertScancodeToEButton(event->key.keysym.scancode, event->key.keysym.sym);
                                        State::keyboard_state.pressed_[Utils::EnumToInteger(bt)] = false;
                                        break;
                                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                                case SDL_EVENT_MOUSE_BUTTON_UP:
                                        const EButton button_type[4] = { EButton::eNone, EButton::eMouseLeft, EButton::eMouseRight, EButton::eMouseMiddle };
                                        const auto bt = button_type[event->button.button];
                                        const auto old_state = State::mouse_state.pressed_[bt];
                                        State::mouse_state.pressed_[bt] = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                                        State::mouse_state.flip_[bt] = old_state != State::mouse_state.pressed_[bt];
                                        break;
                                case SDL_EVENT_MOUSE_MOTION:
                                        State::mouse_state.mouse_pos_ = { event->motion.x, event->motion.y };
                                        State::mouse_state.mouse_pos_deta_ = { event->motion.xrel, event->motion.yrel };
                                        break;
                                case SDL_EVENT_MOUSE_WHEEL:
                                        float deta = event->wheel.y;
                                        if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                                                deta = -deta;
                                        }
                                        State::mouse_state.mouse_wheel_deta_ += deta;
                                        break;
                                default:
                                        LOG(ERROR) << "sdl get error message";
                                }
                        }
                        static void Init() {
                                const auto* platform = SDL_GetPlatform();
                                LOG(INFO) << fmt::format("current SDL platform:{}", platform);
                                if (!)
                        }
                        static void UnInit();
                        static void Tick(float dt);
                private:
                        static EButton ConvertScancodeToEButton(SDL_Scancode scan_code, SDL_Keycode vk_code) {
                                if (scan_code >= SDL_SCANCODE_A && scan_code <= SDL_SCANCODE_Z) {
                                        return EButton(scan_code + Utils::EnumToInteger(EButton::eKeyCA) - SDL_SCANCODE_A);
                                }
                                if (scan_code >= SDL_SCANCODE_0 && scan_code <= SDL_SCANCODE_9) {
                                        return EButton(scan_code + Utils::EnumToInteger(EButton::eKeyN0) - SDL_SCANCODE_0);
                                }
                                if (scan_code >= SDL_SCANCODE_F1 && scan_code <= SDL_SCANCODE_F12) {
                                        return EButton(scan_code + Utils::EnumToInteger(EButton::eKeyF1) - SDL_SCANCODE_F1);
                                }

                                switch (scan_code) {
                                case SDL_SCANCODE_SPACE:
                                        return EButton::eKeyEscape;
                                case SDL_SCANCODE_LSHIFT:
                                        return EButton::eKeyLShift;
                                case SDL_SCANCODE_RSHIFT:
                                        return EButton::eKeyRShift;
                                case SDL_SCANCODE_LALT:
                                        return EButton::eKeyLAlt;
                                case SDL_SCANCODE_RALT:
                                        return EButton::eKeyRAlt;
                                case SDL_SCANCODE_LCTRL:
                                        return EButton::eKeyLCtrl;
                                case SDL_SCANCODE_RCTRL:
                                        return EButton::eKeyRCtrl;
                                case SDL_SCANCODE_BACKSPACE:
                                        return EButton::eKeyBackSpace;
                                }

                                if (vk_code >= SDLK_a && vk_code <= SDLK_z) {
                                        //todo
                                }
                                return EButton::eNone;
                        }
                private:
                        SmallVector<SDL_Event>        events_;
                };
#endif
#ifdef USE_IMGUI
                class InputSystemImplImGUI
                {
                public:
                        static void Process(const InputMessage& message) {
                        }
                        static void Init() {

                        }
                        static void GetMouseState() {
                        }
                        static void GetKeyBoardState() {
                        }
                        static uint32_t GetControllerCount() {
                                return 0u;
                        }
                        static void GetControllerState() {
                        }
                };
#endif
#ifdef USE_XInput //xbox controller
                enum {
                        constexpr float MAX_THUMB_STICK_RANGE{ 32767.0f };
                constexpr float MAX_TRIGGER_RANGE{ 255.f };
                };
                class InputSystemImplXInput
                {
                public:
                        static void Process(const InputMessage& message) {
                        }
                        static void Init() {

                        }
                        static void Tick(float dt) {
                        }
                        static constexpr float DeadZone(float val) {
                                if (val < 0.24f || val > -0.24f) {
                                        val = 0.f;
                                }
                                return std::clamp(val, -1.f, 1.f);
                        }
                };
#endif
#ifdef USE_RAWINPUT
                class InputSystemImplRawInput
                {
                public:
                        static void Process(const InputMessage& message) {
                                const auto* message_ptr = reinterpret_cast<const uint64_t*>(&message);
                                uint64_t message_id = *message_ptr++;
                                uintptr_t paramW = *message_ptr++;
                                uintptr_t paramL = *message_ptr;

                                if (message_id == WM_INPUT) {
                                        uint32_t cb_size{ 0u };
                                        GetRawInputData((HRAWINPUT)paramL, RID_INPUT, nullptr, &cb_size, sizeof(RAWINPUTHEADER));
                                        auto* raw_input = rawinput_allocator.Alloc();//todo
                                        GetRawInputData((HRAWINPUT)paramL, RID_INPUT, &raw_input, &cb_size, sizeof(RAWINPUTHEADER));
                                        if (raw_input->header.dwType == RIM_TYPEMOUSE) {
                                                const auto& raw_mouse = raw_input->data.mouse;

                                                //deal with press message
                                                if (raw_mouse.usButtonFlags != 0u) {
                                                        if (raw_mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) {
                                                                State::mouse_state.flip_[Utils::EnumToInteger(EButton::eMouseLeft)] = !State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseLeft)];
                                                                State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseLeft)] = true;
                                                        }
                                                        if (raw_mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
                                                                State::mouse_state.flip_[Utils::EnumToInteger(EButton::eMouseLeft)] = State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseLeft)];
                                                                State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseLeft)] = false;
                                                        }
                                                        if (raw_mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
                                                                State::mouse_state.flip_[Utils::EnumToInteger(EButton::eMouseRight)] = !State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseRight)];
                                                                State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseRight)] = true;
                                                        }
                                                        if (raw_mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
                                                                State::mouse_state.flip_[Utils::EnumToInteger(EButton::eMouseRight)] = State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseRight)];
                                                                State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseRight)] = false;
                                                        }
                                                        if (raw_mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
                                                                State::mouse_state.flip_[Utils::EnumToInteger(EButton::eMouseMiddle)] = !State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseMiddle)];
                                                                State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseMiddle)] = true;
                                                        }
                                                        if (raw_mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) {
                                                                State::mouse_state.flip_[Utils::EnumToInteger(EButton::eMouseMiddle)] = State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseMiddle)];
                                                                State::mouse_state.pressed_[Utils::EnumToInteger(EButton::eMouseMiddle)] = false;
                                                        }
                                                }

                                                //deal with movement
                                                if (raw_mouse.usFlags == MOUSE_MOVE_RELATIVE) {
                                                        if (std::abs(raw_mouse.lLastX) < 30000) {
                                                                State::mouse_state.mouse_pos_deta_.x += raw_mouse.lLastX;
                                                        }
                                                        if (std::abs(raw_mouse.lLastY) < 30000) {
                                                                State::mouse_state.mouse_pos_deta_.y += raw_mouse.lLastY;
                                                        }
                                                        if (raw_mouse.usButtonFlags & RI_MOUSE_WHEEL) {
                                                                State::mouse_state.mouse_wheel_deta_ = float((SHORT)raw_mouse.usButtonData) / float(WHEEL_DELTA);
                                                        }
                                                }
                                                else if (raw_mouse.usFlags == MOUSE_MOVE_ABSOLUTE) {
                                                        //as wickengine said, never appear
                                                }

                                        }
                                        else if (raw_input->header.dwType == RIM_TYPEKEYBOARD) {
                                                const auto& raw_keyboard = raw_input->data.keyboard;
                                                const auto bt = ConvertVkToEButton(raw_keyboard.MakeCode, raw_keyboard.VKey); //fixme e0 prefix
                                                //set keyboard state
                                                const auto index = Utils::EnumToInteger(bt);
                                                if (raw_keyboard.Flags == RI_KEY_MAKE) {//todo
                                                        State::keyboard_state.flip_[index] = !State::keyboard_state.pressed_[index];
                                                        State::keyboard_state.pressed_[index] = true;
                                                }
                                                if (raw_keyboard.Flags & RI_KEY_BREAK == RI_KEY_BREAK) {
                                                        State::keyboard_state.flip_[index] = State::keyboard_state.pressed_[index];
                                                        State::keyboard_state.pressed_[index] = false;
                                                }


                                        }
                                        else if (raw_input->header.dwType == RIM_TYPEHID) {

                                        }
                                        rawinput_allocator.DeAlloc(raw_input, 1u);
                                }
                                else if (message_id == WM_MOUSEMOVE)
                                {
                                        //for rawinput doesn't contain absolute mouse position; so get position with this api
                                        POINTS point{ MAKEPOINTS(paramL) };
                                        State::mouse_state.mouse_pos_ = { point.x, point.y };
                                }
                        }
                        static void Init() {
                                RAWINPUTDEVICE input_device[4];

                                //mouse
                                input_device[0].usUsagePage = 0x01;
                                input_device[0].usUsage = HID_USAGE_GENERIC_MOUSE;
                                input_device[0].hwndTarget = 0;
                                input_device[0].dwFlags = 0;

                                //keyboard
                                input_device[1].usUsagePage = 0x01;
                                input_device[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
                                input_device[1].hwndTarget = 0;
                                input_device[1].dwFlags = 0;

                                //joystick

                                if (!RegisterRawInputDevices(input_device, 2, sizeof(RAWINPUTDEVICE))) {
                                        LOG(ERROR) << "rawinput initial device failed";
                                }

                                BuildVkMap();

                                //init allocator
                                rawinput_allocator.Reset();
                                rawinputs_ = { rawinput_allocator };//todo error
                        }
                        static void UnInit() {
                                vk2bt_map_.clear();
                        }
                        static void Tick(float dt) {

                        }
                private:
                        static void BuildVkMap() {
                                vk2bt_map_[VK_HOME] = EButton::eKeyHome;
                                vk2bt_map_[VK_INSERT] = EButton::eKeyInsert;
                                vk2bt_map_[VK_END] = EButton::eKeyEnd;
                                vk2bt_map_[VK_ESCAPE] = EButton::eKeyEscape;
                                vk2bt_map_[VK_LCONTROL] = EButton::eKeyLCtrl;
                                vk2bt_map_[VK_RCONTROL] = EButton::eKeyRCtrl;
                                vk2bt_map_[VK_LSHIFT] = EButton::eKeyLShift;
                                vk2bt_map_[VK_RSHIFT] = EButton::eKeyRShift;
                                vk2bt_map_[VK_LMENU] = EButton::eKeyLAlt;
                                vk2bt_map_[VK_RMENU] = EButton::eKeyRAlt;
                                vk2bt_map_[VK_PRIOR] = EButton::eKeyPgUp;
                                vk2bt_map_[VK_NEXT] = EButton::eKeyPgDn;
                                vk2bt_map_[VK_SPACE] = EButton::eKeySpace;
                        }
                        static EButton ConvertVkToEButton(LPARAM lparam, WPARAM vk_code) {
                                const uint32_t scan_code = (lparam & 0x00ff0000) >> 16; //16-23bit
                                bool is_extend_bitset = (lparam & 0x01000000) != 0;
                                switch (vk_code) {
                                case VK_SHIFT:
                                        vk_code = is_extend_bitset ? VK_RSHIFT : VK_LSHIFT;
                                        break;
                                case VK_CONTROL:
                                        vk_code = is_extend_bitset ? VK_RCONTROL : VK_LCONTROL;
                                        break;
                                case VK_MENU:
                                        vk_code = is_extend_bitset ? VK_RMENU : VK_LMENU;
                                        break;
                                case VK_HOME:
                                        vk_code = is_extend_bitset ? vk_code : VK_NUMPAD7;
                                        break;
                                case VK_END:
                                        vk_code = is_extend_bitset ? vk_code : VK_NUMPAD1;
                                        break;
                                case VK_PRIOR:
                                        vk_code = is_extend_bitset ? vk_code : VK_NUMPAD9;
                                        break;
                                case VK_NEXT:
                                        vk_code = is_extend_bitset ? vk_code : VK_NUMPAD3;
                                        break;
                                case VK_CLEAR:
                                        vk_code = is_extend_bitset ? vk_code : VK_NUMPAD5;
                                        break;
                                case VK_LEFT:
                                        vk_code = is_extend_bitset ? vk_code : VK_NUMPAD4;
                                        break;
                                case VK_RIGHT:
                                        vk_code = is_extend_bitset ? vk_code : VK_NUMPAD6;
                                        break;
                                case VK_UP:
                                        vk_code = is_extend_bitset ? vk_code : VK_NUMPAD8;
                                        break;
                                case VK_DOWN:
                                        vk_code = is_extend_bitset ? vk_code : VK_NUMPAD2;
                                        break;
                                }
                                if (vk_code >= 'A' && vk_code <= 'Z') {
                                        return EButton(vk_code + Utils::EnumToInteger(EButton::eKeyCA) - 'A');
                                }
                                if (vk_code >= '0' && vk_code <= '9') {
                                        return EButton(vk_code + Utils::EnumToInteger(EButton::eKeyN0) - '0');
                                }
                                if (vk_code >= VK_NUMPAD0 && vk_code <= VK_NUMPAD9) {
                                        return EButton(vk_code + Utils::EnumToInteger(EButton::eKeyN0Kp) - VK_NUMPAD0);
                                }
                                if (vk_code >= VK_F1 && vk_code <= VK_F12) {
                                        return EButton(vk_code + Utils::EnumToInteger(EButton::eKeyF1) - VK_F1);
                                }
                                if (auto iter = vk2bt_map_.find(vk_code); iter != vk2bt_map_.end()) {
                                        return iter->second;
                                }
                                return EButton::eNone;
                        }
                private:
                        using InnerAllocator = Utils::LinearAllocator<RAWINPUT>;
                        static Map<uint16_t, EButton>        vk2bt_map_;
                        static InnerAllocator        rawinput_allocator_;
                        static Vector<RAWINPUT, InnerAllocator>        rawinputs_;
                }
#endif

                InputSystem& InputSystem::Instance()
                {
                        static InputSystem system;
                        return system;
                }
                void InputSystem::Init()
                {
#ifdef USE_SDL
                        InputSystemImplSDL::Init();
#endif 
#ifdef USE_XInput
                        InputSystemImplXInput::Init();
#endif
                }
                void InputSystem::UnInit()
                {
#ifdef USE_SDL
                        InputSystemImplSDL::UnInit();
#endif 
#ifdef USE_RAWINPUT
                        InputSystemImplRawInput::UnInit();
#endif
                }
                void InputSystem::ForwardMessage(const InputMessage& message)
                {
#ifdef USE_RAWINPUT
                        InputSystemImplRawInput::Process(message);
#elif defined(USE_SDL)
                        InputSystemImplSDL::Process(message);
#endif
                }

                void InputSystem::Tick(float dt)
                {
#ifdef USE_RAWINPUT
                        InputSystemImplRawInput::Tick(dt);
#elif defined(USE_SDL)
                        InputSystemImplSDL::Tick(dt);
#endif
                        //mouse state
                        for (auto n = 0; n < Utils::EnumToInteger(EButton::eMouseNum); ++n) {
                                if (State::mouse_state.pressed_[n]) {
                                        if (State::mouse_state.flip_[n]) {
                                                State::mouse_state.held_time_[n] = dt;
                                                State::mouse_state.flip_[n] = false; //clear flip state todo 
                                        }
                                        else {
                                                State::mouse_state.held_time_[n] += dt;
                                        }
                                }
                                else
                                {
                                        State::mouse_state.held_time_[n] = 0.f;
                                }
                        }

                        //keyboard state
                        for (auto n = 0; n < Utils::EnumToInteger(EButton::eKeyBoardNum); ++n) {
                                if (State::keyboard_state.pressed_[n]) {
                                        if (State::keyboard_state.flip_[n]) {
                                                State::keyboard_state.held_time_[n] = dt;
                                                State::keyboard_state.flip_[n] = false; //clear flip state
                                        }
                                        else
                                        {
                                                State::keyboard_state.held_time_[n] += dt;
                                        }
                                }
                                else
                                {
                                        State::keyboard_state.held_time_[n] = 0.f;
                                }
                        }
                }

                void InputSystem::ClearFrameState()
                {
                        memset(&State::mouse_state, 0, sizeof(State::mouse_state));
                        memset(&State::keyboard_state, 0, sizeof(State::keyboard_state));
                        memset(State::controller_state.data(), 0, sizeof(State::controller_state[0]) * State::controller_state.size());
                }

                InputSystem::~InputSystem()
                {
                        UnInit();
                }

                const MouseState& InputSystem::GetMouseState() const
                {
                        return State::mouse_state;
                }
                const KeyBoardState& InputSystem::GetKeyBoardState() const
                {
                        return State::keyboard_state;
                }
                uint32_t InputSystem::GetControllerCount() const
                {
                        return State::controller_state.size();
                }
                const ControllerState& InputSystem::GetControllerState(uint32_t index) const
                {
                        assert(index < GetControllerCount());
                        return State::controller_state[index];
                }
        }

}