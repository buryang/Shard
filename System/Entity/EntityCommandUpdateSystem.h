#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Algorithm.h"
#include "Utils/SimpleEntitySystem.h"

namespace Shard::System
{
    namespace Entity
    {
        class EntityUpdateSystem : public Utils::ECSSystem
        {
        public:
            void Init();
            void UnInit();
            void Enqueue();
            virtual void Tick(float dt);
            virtual ~EntityUpdateSystem();
        private:
            
        };
    }
}
