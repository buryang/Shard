#pragma once
#include "Utils/CommonUtils.h"

namespace Shard::System
{
	namespace Input
	{
		enum class EInputDeviceType : uint8_t
		{
			eNone = 0 << 0,
			eController = 1 << 0,
			eKeyBoard = 1 << 1,
			eMouse = 1 << 2,
			eKeyBoardMouse = eMouse | eKeyBoard,
		};

		//#define GET_DEVICE_TYPE_FUN(PREFIX, VALUE, SUFFIX) PREFIX EInputDeviceType GetDeviceType() const SUFFIX { return VALUE;}

		enum class EButton : uint32_t
		{
			eNone,
			eMouseLeft,
			eMouseRight,
			eMouseMiddle,

			eKeyLeft,
			eKeyRight,
			eKeyUp,
			eKeyDown,
			eKeySpace,
			eKeyLShift,
			eKeyRShift,
			eKeyLCtrl,
			eKeyRCtrl,
			eKeyWin,
			eKeyLAlt,
			eKeyRAlt,
			aKeyTab,
			eKeyBackSpace,
			eKeyCapsLk,
			eKeyInsert,
			eKeyEscape,
			eKeyHome,
			eKeyEnd,
			eKeyPgUp,
			eKeyPgDn,
			eKeyF1,
			eKeyF2,
			eKeyF3,
			eKeyF4,
			eKeyF5,
			eKeyF6,
			eKeyF7,
			eKeyF8,
			eKeyF9,
			eKeyF10,
			eKeyF11,
			eKeyF12,
			eKeyMinus, //-
			eKeyPlus,  //+
			eKeyAsterisk, //*
			eKeyComma, //,
			eKeyPeriod, //.
			eKeySlash, //"/"

			eKeyCA = 65,
			eKeyCB,
			eKeyCC,
			eKeyCD,
			eKeyCE,
			eKeyCF,
			eKeyCG,
			eKeyCH,
			eKeyCI,
			eKeyCJ,
			eKeyCK,
			eKeyCL,
			eKeyCM,
			eKeyCN,
			eKeyCO,
			eKeyCP,
			eKeyCQ,
			eKeyCR,
			eKeyCS,
			eKeyCT,
			eKeyCU,
			eKeyCV,
			eKeyCW,
			eKeyCX,
			eKeyCY,
			eKeyCZ,

			eKeyN0,
			eKeyN1,
			eKeyN2,
			eKeyN3,
			eKeyN4,
			eKeyN5,
			eKeyN6,
			eKeyN7,
			eKeyN8,
			eKeyN9,

			eKeyN0Kp,
			eKeyN1Kp,
			eKeyN2Kp,
			eKeyN3Kp,
			eKeyN4Kp,
			eKeyN5Kp,
			eKeyN6Kp,
			eKeyN7Kp,
			eKeyN8Kp,
			eKeyN9Kp,

			eGPUP,
			eGPLT,
			eGPDWN,
			eGPRT,
			eGP1,
			eGP2,
			eGP3,
			eGP4,
			eGP5,
			eGP6,
			eGP7,
			eGP8,
			eGP9,
			eGP10,
			eGP11,
			//key numbers
			eMouseNum = eMouseMiddle,
			eKeyBoardNum = eKeyN9Kp,
			eGPNum = eGP11 - eGPUP + 2,

		};

		enum class EWheelType : uint8_t
		{
			eVertial,
			eHorizonal,
		};

		//common event warpper for diff platform
		struct ALIGN_CACHELINE InputMessage
		{
			uint8_t	data_[128]; //copy from SDL_Event struct
		};

		struct MouseState
		{
			vec2	mouse_pos_{ 0, 0 };
			vec2	mouse_pos_deta_{ 0, 0 };
			float	mouse_wheel_deta_{ 0.f };//wheel delta
			BitSet<Utils::EnumToInteger(EButton::eMouseNum)>	pressed_;
			BitSet<Utils::EnumToInteger(EButton::eMouseNum)>	flip_;
			Array<float, Utils::EnumToInteger(EButton::eMouseNum)>	held_time_;
		};

		struct KeyBoardState
		{
			BitSet<Utils::EnumToInteger(EButton::eKeyBoardNum)>	pressed_;
			BitSet<Utils::EnumToInteger(EButton::eKeyBoardNum)>	flip_;
			Array<float, Utils::EnumToInteger(EButton::eKeyBoardNum)>	held_time_;
			//Array<EButtonState, Utils::EnumToInteger(EButton::eKeyBoardNum)>	states_{ EButtonState::eNone };
		};

		struct ControllerState
		{
			vec2	thumbstick_left_{ 0, 0 };
			vec2	thumbstick_right_{ 0, 0 };
			float	trigger_left_{ 0 };
			float	trigger_right_{ 0 };
		};

		/*
		class InputDeviceKeyBoardMouse final : public InputDeviceInterface
		{
		public:
			friend class InputSystem;
			GET_DEVICE_TYPE_FUN(, EInputDeviceType::eKeyBoardMouse, override);
			InputDeviceKeyBoardMouse() = default;
			void ProcessMessage(const InputMessage& message) override;
			const MouseState& GetMouseState() const {
				return mouse_state_;
			}
			const KeyBoardState& GetKeyBoardState() const {
				return keyboard_state_;
			}
			EButtonState GetButtonState(EButton bt) const;
		private:
			MouseState	mouse_state_;
			KeyBoardState	keyboard_state_;
		};

		class InputDeviceController : public InputDeviceInterface
		{
		public:
			friend class InputSystem;
			GET_DEVICE_TYPE_FUN(, EInputDeviceType::eController, override);
			InputDeviceController(uint32_t device_id):device_id_(device_id){}
			void ProcessMessage(const InputMessage& message) override;
			uint32_t GetControllerID()const {
				return device_id_;
			}
			virtual ~InputDeviceController() = default;
		protected:
			uint32_t	device_id_{ -1 };
		};
		*/

		//todo maybe use monostate only 
		class MINIT_API	InputSystem
		{
		public:
			using Ptr = InputSystem*;
			static InputSystem& Instance();
			void Init();
			void UnInit();
			void ForwardMessage(const InputMessage& message);
			void Tick(float dt);
			void ClearFrameState();
			const MouseState& GetMouseState()const;
			const KeyBoardState& GetKeyBoardState()const;
			uint32_t GetControllerCount()const;
			const ControllerState& GetControllerState(uint32_t index)const;
			~InputSystem();
		private:
			InputSystem() = default;
			DISALLOW_COPY_AND_ASSIGN(InputSystem);
		};
	}
}