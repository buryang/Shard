#pragma once
#include "Utils/SimpleEntitySystem.h"
#include "Utils/SimpleEntitySystemPrimitive.h"

namespace Shard::PhyX
{

	class MINIT PhyXDynamicUpdateSystem : public Utils::ECSSystem
	{
	public:
		void Init() override;
		void UnInit() override;
		void Update(Utils::ECSSystemUpdateContext& ctx) override;
	public:
	};
}