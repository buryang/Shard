#pragma once
#include "Utils/CommonUtils.h"

namespace MetaInit
{
	struct MINIT_API GlobalRenderConfig
	{
	public:
		using BOOL = bool;
		using UINT = uint32_t;
		using INT = int32_t;
		using FLOAT = float;
		static bool SetBOOLVal(const String& key, BOOL val);
		static BOOL GetBOOLVal(const String& key);
		static bool SetINTVal(const String& key, INT val);
		static INT GetINTVal(const String& key);
		static bool SetUINTVal(const String& key, UINT val);
		static UINT GetUINTVal(const String& key);
		static bool SetFLOATVal(const String& key, FLOAT val);
		static FLOAT GetFLOATVal(const String& key);
	private:
		static Map<String, BOOL>	bool_params_;
		static Map<String, UINT>	uint_params_;
		static Map<String, INT>	int_params_;
		static Map<String, FLOAT>	float_params_;
	};

#define SET_PARAM_TYPE_VAL(Type, Key, Value) \
	GlobalRenderConfig::Set##Type##Val(#Key, static_cast<GlobalRenderConfig::##Type>(Value))
#define REGIST_PARAM_TYPE(Type, Key, DefaultVal) \
	static const bool g_internal_##Key = SET_PARAM_TYPE_VAL(Type, Key, DefaultVal);
#define GET_PARAM_TYPE_VAL(Type, Key) \
	GlobalRenderConfig::Get##Type##Val(#Key)
}