#pragma once
#include "Utils/CommonUtils.h"
#include "Renderer/RtRenderBarrier.h"

namespace MetaInit
{
	namespace RHI
	{
		class RHIBarrier
		{
		public:
			enum class EType : uint8_t
			{
				eTransition,
				eAliasing,
				//not know what vulkan uav barrier is
				//d3d between two acess both read or write
				eUAV,
			};

			enum class EFlags : uint32_t
			{

			};
		private:


		};

		class RHICommandContext
		{
		public:
			enum class Flags {
				eUnkown,
				eGraphics,
				eCompute,
				eAsyncCompute,
				eTransfer, //fixme whether we need a transfer context
				eNum,
			};
			using Ptr = RHICommandContext*;
			RHICommandContext() = default;
			virtual void SetRHI() = 0;
			//build optimize command
			virtual void Build() = 0;
			template<class RHIHandle>
			RHIHandle GetRHI()const;
			//submit command
			virtual void Submit() = 0;
			virtual void Begin() = 0;
			virtual void End() = 0;
			virtual void SetPiplineState() = 0;
			virtual Flags GetFlags() const { return Flags::eUnkown; }
			//common command context operations
			void SetShaderRHI();
			void SetBarrierBatch(Renderer::RtBarrierBatch& barriers);
			template<typename ParamStruct>
			void SetShaderParameters(uint32_t buffer_index, uint32_t base_index, ParamStruct& param) {
				SetShaderParameters(buffer_index, base_index, sizeof(param), &param);
			}
			void SetShaderParameters(uint32_t buffer_index, uint32_t base_index, uint32_t bytes, const void* param_ptr);
		};

		class RHIGraphicsCommandContext : public RHICommandContext
		{
		public:
			void SetRHI() override;
			void Build() override;
			void Submit() override;
			Flags GetFlags() const override {
				return Flags::eGraphics;
			}
			bool MergeAble(const RHIGraphicsCommandContext& rhs) const;
		private:
		};

		class RHIComputeComandContext : public RHICommandContext
		{
		public:
			void SetRHI() override;
			void Build() override;
			void Submit() override;
			Flags GetFlags() const override {
				return is_async_ ? Flags::eAsyncCompute : Flags::eCompute;
			}
			bool IsAsync() const {
				return GetFlags() == Flags::eAsyncCompute;
			}
			void Dispatch(vec3 dimensions);
			void SetAsyncComputeBudget();
			bool MergeAble(const RHIComputeComandContext& rhs) const;
		private:
			void SetComputeShader();
		private:
			bool is_async_{ false };
		};

	}
}
