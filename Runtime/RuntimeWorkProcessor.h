#pragma once
#include "Runtime/RuntimeWorkIR.h"

namespace MetaInit
{
	namespace Runtime
	{

		//mesh draw usually used passes
		enum class EMeshPass : uint8_t
		{
			eDepth,
			eBase,
			eSky,
			eVelocity,
			eVolumetricFog,
			eRayTracing,
			eNum,
		};

		class MINIT_API DrawPassProcessor
		{
		public:
			virtual void operator()(DrawWorkCommandIRBatch& cmds) = 0;
			virtual EMeshPass TargetType() const = 0;
		};

		class MINIT_API DepthPassProcessor final : public DrawPassProcessor
		{
		public:
			void operator()(DrawWorkCommandIRBatch& cmds) override;
			EMeshPass TargetType() const override { return EMeshPass::eDepth; }
		};

		class MINIT_API VolumetricFogPassProcessor final : public DrawPassProcessor
		{
		public:
			void operator()(DrawWorkCommandIRBatch& cmds) override;
			EMeshPass TargetType() const override { return EMeshPass::eVolumetricFog; }
		};

		class MINIT_API RayTracingPassProcessor final : public DrawPassProcessor
		{
		public:
			void operator()(DrawWorkCommandIRBatch& cmds)override;
			EMeshPass TargetType() const override { return EMeshPass::eRayTracing; }

		};
	}
}