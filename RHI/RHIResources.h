#pragma once
#include "Utils/CommonUtils.h"
#include "Core/RenderGlobalParams.h"
#include "Renderer/RtRenderResources.h"
#include "RHI/RHIGlobalEntity.h"

namespace MetaInit
{
	namespace RHI
	{
		//resource global parameter configure
		//whether resource shade mode be concurrent 
		REGIST_PARAM_TYPE(BOOL, RHI_CONCURRENT_SHARE, false);

		class RHIResource
		{
		public:
			using Ptr =	RHIResource*;
			virtual void SetRHI(RHIGlobalEntity::Ptr rhi_entity) = 0;
			virtual void Release(RHIGlobalEntity::Ptr rhi_entity) {}
			virtual size_t GetOccupySize() const = 0;
			virtual ~RHIResource() {}
			bool IsDedicated() const { 
				return dedicated_mem_;
			}
			template<typename RHIHandle>
			RHIHandle GetRHI() {
				return static_cast<RHIHandle>(rhi_);
			}
		protected:
			uint32_t RefInCr() {
				return ++ref_count_;
			}
			uint32_t RefDeCr(uint32_t count) {
				assert(ref_count_ > count);
				ref_count_ -= count;
				return ref_count_;
			}
			uint32_t RefCount() const {
				return ref_count_;
			}
		protected:
			//no multithread 
			mutable uint32_t	ref_count_{ 0 };
			bool	dedicated_mem_{ false };
			void*	rhi_;
		};


		struct RHITextureDesc
		{
			enum class EType : uint8_t
			{
				eTexture1D = 0x0,
				eTexture2D = 0x1,
				eTexture3D = 0x2,
				eTextureCube = 0x3,
				eTextureArray = 0x4,
			};
			explicit RHITextureDesc(const Renderer::RtField& field):layout_(field.GetLayout()), 
																	access_(field.GetSubField().access_),
																	format_(field.GetPixFormat()){
				is_transiant_ = field.IsTransiant();
				//FIXME deal with pipeline
			}
			FORCE_INLINE EType GetType() const {
				assert(layout_.width_ != 0 && "at least one dimension nonzero");
				if (layout_.array_slices_ != 0) {
					return EType::eTextureArray;
				}
				if (layout_.depth_ != 0) {
					return EType::eTexture3D;
				}
				if (layout_.width_ != 0 && layout_.height_ != 0) {
					return EType::eTexture2D;
				}
				return EType::eTexture1D;
			}
			Renderer::TextureLayout	layout_;
			Renderer::EAccessFlags access_{ Renderer::EAccessFlags::eNone };
			Renderer::EPipeLine	pipeline_{ Renderer::EPipeLine::eGraphics };
			EPixFormat	format_;
			bool	is_transiant_{ true };
		};

		class RHITexture final: public RHIResource
		{
		public:
			using Ptr = RHITexture*;
			RHITexture(const RHITextureDesc& desc) :desc_(desc) {}
			void SetRHI(RHIGlobalEntity::Ptr rhi_entity) override;
			void Release(RHIGlobalEntity::Ptr rhi_entity) override;
			size_t GetOccupySize() const override;
		private:
			const RHITextureDesc& desc_;
		};

		struct RHIBufferDesc
		{
			enum class Type : uint8_t
			{
				eUniform = 0x1,
				eRawBuffer = 0x2,
				eStructedBuffer = 0x4,
				eUniformStructed = eUniform | eStructedBuffer,
			};
			explicit RHIBufferDesc(const Renderer::RtField& field) {

			}
			Type	type_;
			uint32_t	size_;
			Renderer::EPipeLine	pipeline_{ Renderer::EPipeLine::eGraphics };
			Renderer::EAccessFlags access_{ Renderer::EAccessFlags::eNone };
		};

		class RHIBuffer final : public RHIResource
		{
		public:
			using Ptr = RHIBuffer*;
			RHIBuffer(const RHIBufferDesc& desc) :desc_(desc) {}
			void SetRHI(RHIGlobalEntity::Ptr rhi_entity) override;
			void Release(RHIGlobalEntity::Ptr rhi_entity) override;
			size_t GetOccupySize() const override;
		private:
			const RHIBufferDesc& desc_;
		};

		struct RHIAccelerateDesc
		{

		};

		class RHIAccelerate final : public RHIResource
		{
		public:
			enum class Type : uint8_t
			{
				eTopLevel,
				eBottomLevel,
				eNum,
			};
			void SetRHI(RHIGlobalEntity::Ptr rhi_entity) override;
			void Release(RHIGlobalEntity::Ptr rhi_entity) override;
			size_t GetOccupySize() const override;
		private:
			const RHIAccelerateDesc& desc_;
		};

		struct RHITextureSRVDesc
		{
			RHITexture::Ptr	texture_{ nullptr };
			EPixFormat	format_{ EPixFormat::eUnkown };
			Renderer::TextureSubRange	range_;
		};

		class RHITextureSRV final : public RHIResource
		{
		public:
			RHITextureSRV(const RHITextureSRVDesc& desc);
			void SetRHI(RHIGlobalEntity::Ptr rhi_entity) override;
			void Release(RHIGlobalEntity::Ptr rhi_entity) override;
			size_t GetOccupySize() const override;
		};

		struct RHITextureUAVDesc
		{
			RHITexture::Ptr	texture_;
			EPixFormat	format_{ EPixFormat::eUnkown };
			uint32_t mip_{ 0 };
		};

		class RHITextureUAV final : public RHIResource
		{
		public:
			RHITextureUAV(const RHITextureUAVDesc& desc);
			void SetRHI(RHIGlobalEntity::Ptr rhi_entity) override;
			void Release(RHIGlobalEntity::Ptr rhi_entity) override;
			size_t GetOccupySize() const override;
		};
	}
}
