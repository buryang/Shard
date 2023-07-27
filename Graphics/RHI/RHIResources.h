#pragma once
#include "eastl/shared_ptr.h"
#include "Utils/CommonUtils.h"
#include "Core/RenderGlobalParams.h"
#include "Renderer/RtRenderResources.h"
#include "RHI/RHIGlobalEntity.h"

namespace Shard
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
			using SharedPtr = eastl::shared_ptr<RHIResource>;
			RHIResource(RHIGlobalEntity::Ptr parent):parent_(parent){}
			DISALLOW_COPY_AND_ASSIGN(RHIResource);
			virtual void SetUp() = 0;
			virtual void Release() = 0;
			//FIXME whether map a cpu pointer
			virtual void* MapBackMem() {
				PLOG(ERROR) << "resource not support map backend memory";
			}
			virtual void UnMapBackMem() {
				PLOG(ERROR) << "resource not support unmap backend memory";
			}
			virtual size_t GetOccupySize() const = 0;
			virtual ~RHIResource() {}
			FORCE_INLINE bool IsDedicated() const { 
				return is_dedicated_;
			}
			FORCE_INLINE bool IsTransient() const {
				return is_transient_;
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
			RHIGlobalEntity::Ptr parent_{ nullptr };
			uint32_t is_dedicated_ : 1;
			uint32_t is_transient_ : 1;
		};


		struct RHITextureInitializer
		{
			enum class EType : uint8_t
			{
				eTexture1D = 0x0,
				eTexture2D = 0x1,
				eTexture3D = 0x2,
				eTextureCube = 0x3,
				eTextureArray = 0x4,
			};
			enum class EAspect : uint8_t
			{

			};
			static RHITextureInitializer Create(const Renderer::RtField& field) {
				RHITextureInitializer desc{
					.layout_ = field.GetLayout(),
					.access_ = field.GetSubField().access_,
					.is_dedicated_ = field.IsDedicated(),
					.is_transiant_ = field.IsTransiant(),
					.format_ = field.GetPixFormat(),
				};
				return desc;
			}
			Renderer::TextureLayout	layout_;
			Renderer::EAccessFlags access_{ Renderer::EAccessFlags::eNone };
			Renderer::EPipeLine	pipeline_{ Renderer::EPipeLine::eGraphics };
			uint32_t	is_dedicated_ : 1;
			uint32_t	is_transiant_: 1;
			EPixFormat	format_{ EPixFormat::eUnkown };
			eastl::pair<uint32_t, uint32_t> life_time_;
		};

		FORCE_INLINE RHITextureInitializer::EType GetTextureType(const RHITextureInitializer& desc) {
			assert(desc.layout_.width_ != 0 && "at least one dimension nonzero");
			if (desc.layout_.array_slices_ != 0) {
				return  RHITextureInitializer::EType::eTextureArray;
			}
			if (desc.layout_.depth_ != 0) {
				return RHITextureInitializer::EType::eTexture3D;
			}
			if (desc.layout_.width_ != 0 && desc.layout_.height_ != 0) {
				return  RHITextureInitializer::EType::eTexture2D;
			}
			return  RHITextureInitializer::EType::eTexture1D;
		}
		//FIXME
		FORCE_INLINE bool operator==(const RHITextureInitializer& lhs, const RHITextureInitializer& rhs) {
			return lhs.layout_ == rhs.layout_ && lhs.format_ == rhs.format_
				&& lhs.access_ == rhs.access_;
		}

		FORCE_INLINE bool operator!=(const RHITextureInitializer& lhs, const RHITextureInitializer& rhs) {
			return !(lhs == rhs);
		}

		class RHITexture : public RHIResource
		{
		public:
			using Ptr = RHITexture*;
			using SharedPtr = eastl::shared_ptr<RHITexture>;
			explicit RHITexture(RHIGlobalEntity::Ptr parent, const RHITextureInitializer& desc) :RHIResource(parent), desc_(desc) {}
			virtual ~RHITexture() {}
			FORCE_INLINE const RHITextureInitializer& GetTextureDesc() const {
				return desc_;
			}
		protected:
			const RHITextureInitializer& desc_;
		};

		struct RHIBufferInitializer
		{
			enum class Type : uint8_t
			{
				eUniform = 0x1,
				eRawBuffer = 0x2,
				eStructedBuffer = 0x4,
				eUniformStructed = eUniform | eStructedBuffer,
			};
			static RHIBufferInitializer Create(const Renderer::RtField& field) {
				assert(field.GetType() == Renderer::RtField::EType::eBuffer);
				RHIBufferInitializer desc{ .size_ = field.GetLayout().width_, .is_dedicated_ = field.IsDedicated(), .is_transiant_ = field.IsTransiant() };
				return desc;
			}
			Type	type_;
			uint32_t	size_;
			Renderer::EPipeLine	pipeline_{ Renderer::EPipeLine::eGraphics };
			Renderer::EAccessFlags access_{ Renderer::EAccessFlags::eNone };
			uint32_t	is_dedicated_ : 1;
			uint32_t	is_transiant_ : 1;
			eastl::pair<uint32_t, uint32_t> life_time_;
		};

		FORCE_INLINE bool operator==(const RHIBufferInitializer& lhs, const RHIBufferInitializer& rhs) {
			return lhs.type_ == rhs.type_ && lhs.size_ == rhs.size_
				&& lhs.pipeline_ == rhs.pipeline_
				&& lhs.access_ == rhs.access_;
		}
		FORCE_INLINE bool operator!=(const RHIBufferInitializer& lhs, const RHIBufferInitializer& rhs) {
			return !(lhs == rhs);
		}

		class RHIBuffer : public RHIResource
		{
		public:
			using Ptr = RHIBuffer*;
			explicit RHIBuffer(RHIGlobalEntity::Ptr parent, const RHIBufferInitializer& desc) :RHIResource(parent), desc_(desc) {}
			virtual ~RHIBuffer() {};
			FORCE_INLINE const RHIBufferInitializer& GetBufferDesc() const {
				return desc_;
			}
			size_t GetOccupySize() const override;
		protected:
			const RHIBufferInitializer& desc_;
		};

		class RHIVertexBuffer : public RHIBuffer
		{
		public:
			using Ptr = RHIVertexBuffer*;
		};

		struct RHIAccelerateDesc
		{

		};

		class RHIAccelerate : public RHIResource
		{
		public:
			using Ptr = RHIAccelerate*;
			using SharedPtr = eastl::shared_ptr<RHIAccelerate>;
			enum class Type : uint8_t
			{
				eTopLevel,
				eBottomLevel,
				eNum,
			};
			virtual ~RHIAccelerate() {}
		private:
			const RHIAccelerateDesc& desc_;
		};

		struct RHITextureSRVInitializer
		{
			RHITexture::Ptr	texture_{ nullptr };
			EPixFormat	format_{ EPixFormat::eUnkown };
			Renderer::TextureSubRange	range_;
		};

		class RHITextureSRV : public RHIResource
		{
		public:
			RHITextureSRV(const RHITextureSRVInitializer& desc);
			virtual ~RHITextureSRV() {}
		private:
			const RHITextureSRVInitializer& desc_;
		};

		struct RHITextureUAVInitializer
		{
			RHITexture::Ptr	texture_;
			EPixFormat	format_{ EPixFormat::eUnkown };
			uint32_t mip_{ 0 };
		};

		class RHITextureUAV : public RHIResource
		{
		public:
			RHITextureUAV(const RHITextureUAVInitializer& desc);
			virtual ~RHITextureUAV() {}
		};

		class RHISampler : public RHIResource
		{
		public:
			using Ptr = RHISampler*;
			virtual ~RHISampler() {}
		};
	}
}
