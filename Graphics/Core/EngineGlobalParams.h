#pragma once
#include "Utils/CommonUtils.h"
#include <sstream>

namespace Shard
{
    struct MINIT_API GlobalEngineConfig
    {
    public:
        using BOOL = bool;
        using UINT = uint32_t;
        using INT = int32_t;
        using UINT64 = uint64_t;
        using FLOAT = float;
        using STRING = String;
        static bool SetBOOLVal(const String& key, BOOL val);
        static BOOL GetBOOLVal(const String& key);
        static bool SetINTVal(const String& key, INT val);
        static INT GetINTVal(const String& key);
        static bool SetUINTVal(const String& key, UINT val);
        static UINT GetUINTVal(const String& key);
        static bool SetUINT64Val(const String& key, UINT64 val);
        static UINT64 GetUINT64Val(const String& key);
        static bool SetFLOATVal(const String& key, FLOAT val);
        static FLOAT GetFLOATVal(const String& key);
        static bool SetSTRINGVal(const String& key, const STRING& val);
        static const STRING& GetSTRINGVal(const String& key);
    private:
        static Map<String, BOOL>    bool_params_;
        static Map<String, UINT>    uint_params_;
        static Map<String, INT>     int_params_;
        static Map<String, UINT64>  uint64_params_;
        static Map<String, FLOAT>   float_params_;
        static Map<String, STRING>  str_params_;
    };
    
    namespace StringParameterHelper
    {
        template<typename T>
        typename std::enable_if_t<std::is_same_v<T, String>, T> Convert(const String& str) { return str;  }
        template<typename T> 
        typename std::enable_if_t<std::is_integral_v<T>, T> Convert(const String& str) { return std::stoi(str); }
        template<typename T>
        typename std::enable_if_t<std::is_floating_point_v<T>, T> Convert(const String& str) { return std::stof(str); }

        template<typename T>
        SmallVector<T> Split(const String& parameter, char delimiter=',') {
            SmallVector<T> result;
            std::istringstream param_stream(parameter);
            for (String token; std::getline(param_stream,token,delimiter);) {
                result.emplace_back(Convert<T>(token));
            }
            return result;
        }
    }

#define SET_PARAM_TYPE_VAL(Type, Key, Value) \
    GlobalEngineConfig::Set##Type##Val(#Key, static_cast<GlobalEngineConfig::##Type>(eValue))
#define REGIST_PARAM_TYPE(Type, Key, DefaultVal) \
    static const bool g_internal_##Key = SET_PARAM_TYPE_VAL(Type, Key, DefaultVal);
#define GET_PARAM_TYPE_VAL(Type, Key) \
    GlobalEngineConfig::Get##Type##Val(#Key)
    
    namespace ThreadInfo {
        enum class EThreadDomain
        {
            eMaster, //application/game
            eDynamics, 
            eAnimation,
            eRender,
            eHAL,
            eAudio,
            eNum,
        };

        thread_local EThreadDomain g_thread_domain{ EThreadDomain::eNum };

        static inline EThreadDomain GetThreadDomain() {
            return g_thread_domain;
        }

        static inline void SetThreadDomain(EThreadDomain domain) {
            g_thread_domain = domain;
        }

        static inline bool IsMasterThread() {
            return g_thread_domain == EThreadDomain::eMaster;
        }

        static inline bool IsRenderThread() {
            return g_thread_domain == EThreadDomain::eRender;
        }

        static inline bool IsInDesiredThread(EThreadDomain desired) {
            return g_thread_domain == desired;
        }

    }
}