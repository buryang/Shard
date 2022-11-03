#include "Core/RenderGlobalParams.h"

namespace MetaInit {

	template<typename T>
	FORCE_INLINE bool SetMapVal(Map<String, T>& map, const String& key, T val) {
		map[key] = val;
		return true;
	}

	template<typename T>
	FORCE_INLINE auto GetMapVal(const Map<String, T>& map, const String& key) {
		if (auto iter = map.find(key); iter != map.end()) {
			return iter->second;
		}
		LOG(ERROR) << __FUNCTION__ << ":not supported global params(" << key << ")" << std::endl;
	}

	bool GlobalRenderConfig::SetBOOLVal(const String& key, BOOL val)
	{
		return SetMapVal(GlobalRenderConfig::bool_params_, key, val);
	}

	GlobalRenderConfig::BOOL GlobalRenderConfig::GetBOOLVal(const String& key)
	{
		return GetMapVal(GlobalRenderConfig::bool_params_, key);
	}

	bool GlobalRenderConfig::SetINTVal(const String& key, INT val)
	{
		return SetMapVal(GlobalRenderConfig::int_params_, key, val);
	}

	GlobalRenderConfig::INT GlobalRenderConfig::GetINTVal(const String& key)
	{
		return GetMapVal(GlobalRenderConfig::int_params_, key);
	}

	bool GlobalRenderConfig::SetUINTVal(const String& key, UINT val)
	{
		return SetMapVal(GlobalRenderConfig::uint_params_, key, val);
	}

	GlobalRenderConfig::UINT GlobalRenderConfig::GetUINTVal(const String& key)
	{
		return GetMapVal(GlobalRenderConfig::uint_params_, key);
	}

	bool GlobalRenderConfig::SetFLOATVal(const String& key, FLOAT val)
	{
		return SetMapVal(GlobalRenderConfig::float_params_, key, val);
	}

	GlobalRenderConfig::FLOAT GlobalRenderConfig::GetFLOATVal(const String& key)
	{
		return GetMapVal(GlobalRenderConfig::float_params_, key);
	}
}
