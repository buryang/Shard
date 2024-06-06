#include "Core/EngineGlobalParams.h"

namespace Shard {

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
        PLOG(ERROR) << "GetMapVal:not supported global params(" << key << ")";
    }
    
    bool GlobalEngineConfig::SetBOOLVal(const String& key, BOOL val)
    {
        return SetMapVal(GlobalEngineConfig::bool_params_, key, val);
    }

    GlobalEngineConfig::BOOL GlobalEngineConfig::GetBOOLVal(const String& key)
    {
        return GetMapVal(GlobalEngineConfig::bool_params_, key);
    }

    bool GlobalEngineConfig::SetINTVal(const String& key, INT val)
    {
        return SetMapVal(GlobalEngineConfig::int_params_, key, val);
    }

    GlobalEngineConfig::INT GlobalEngineConfig::GetINTVal(const String& key)
    {
        return GetMapVal(GlobalEngineConfig::int_params_, key);
    }

    bool GlobalEngineConfig::SetUINTVal(const String& key, UINT val)
    {
        return SetMapVal(GlobalEngineConfig::uint_params_, key, val);
    }

    GlobalEngineConfig::UINT GlobalEngineConfig::GetUINTVal(const String& key)
    {
        return GetMapVal(GlobalEngineConfig::uint_params_, key);
    }

    bool GlobalEngineConfig::SetUINT64Val(const String& key, UINT64 val)
    {
        return SetMapVal(GlobalEngineConfig::uint64_params_, key, val);;
    }

    UINT64 GlobalEngineConfig::GetUINT64Val(const String& key)
    {
        return GetMapVal(GlobalEngineConfig::uint64_params_, key);
    }

    bool GlobalEngineConfig::SetFLOATVal(const String& key, FLOAT val)
    {
        return SetMapVal(GlobalEngineConfig::float_params_, key, val);
    }

    GlobalEngineConfig::FLOAT GlobalEngineConfig::GetFLOATVal(const String& key)
    {
        return GetMapVal(GlobalEngineConfig::float_params_, key);
    }
    bool GlobalEngineConfig::SetSTRINGVal(const String& key, const STRING& val)
    {
        return SetMapVal(GlobalEngineConfig::str_params_, key, val);
    }
    const GlobalEngineConfig::STRING& GlobalEngineConfig::GetSTRINGVal(const String& key)
    {
        return GetMapVal(GlobalEngineConfig::str_params_, key);
    }
}
