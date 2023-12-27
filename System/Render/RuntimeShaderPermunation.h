#pragma once
#include "Utils/CommonUtils.h"
//shader compile permunation logic

namespace Shard
{
        namespace Runtime
        {
                
                struct ShaderPermunationKey
                {

                };

                class MINIT_API RuntimeShaderMapSingleton
                {
                public:
                        RuntimeShaderMapSingleton& Instance();
                        void Regist();
                        void Query();
                private:
                        DISALLOW_COPY_AND_ASSIGN(RuntimeShaderMapSingleton);
                        RuntimeShaderMapSingleton() = default;
                private:
                        Map<>        shader_map_;
                };
        }
}