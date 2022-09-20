#pragma once
#include "Utils/CommonUtils.h"

namespace MetaInit
{
	namespace RHI
	{
		class RHIResource
		{
		public:
			using Ptr =	RHIResource*;
			virtual void SetRHI() = 0;
			virtual void Release() {}
			virtual size_t GetOccupySize() const = 0;
			virtual ~RHIResource() {}
			bool Dedicated() const { return dedicated_mem_; }
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
		private:
			//no multithread 
			uint32_t	 ref_count_{ 0 };
			bool		 dedicated_mem_{ false };
		};


		struct RHITextureDesc
		{
			enum class Type : uint8_t
			{
				eTexture2D,
				eTexture3D,
				eTextureArray,
				eCubeMaps,
			};
			vec3		dims_;
			uint32_t	plane_;
		};

		class RHITexture : public RHIResource
		{
		public:
			RHITexture(const RHITextureDesc& desc) :desc_(desc) {}
			void SetRHI() override;
			void Release() override;
			size_t GetOccupySize() const override;
			template<typename RHIHandle>
			RHIHandle GetRHI() {
				return static_cast<RHIHandle>(rhi_);
			}
		private:
			const RHITextureDesc& desc_;
			void* rhi_;
		};

		struct RHIBufferDesc
		{
			enum class Type : uint8_t
			{

			};
		};

		class RHIBuffer : public RHIResource
		{
		public:
			RHIBuffer(const RHIBufferDesc& desc) :desc_(desc) {}
			void SetRHI() override;
			void Release() override;
			size_t GetOccupySize() const override;
			template<typename RHIHandle>
			RHIHandle GetRHI() {
				return static_cast<RHIHandle>(rhi_);
			}
		private:
			const RHIBufferDesc& desc_;
			void* rhi_;
		};

		class RHIAccelerate : public RHIResource
		{
		public:
			enum class Type : uint8_t
			{
				eTopLevel,
				eBottomLevel,
				eNum,
			};
			void SetRHI() override;
			void Release() override;
			size_t GetOccupySize() const override;
			template<typename RHIHandle>
			RHIHandle GetRHI() {
				return static_cast<RHIHandle>(rhi_);
			}
		private:
			void* rhi_;
		};

		struct RHITextureSRVDesc
		{
			RHITexture* texture_;
			uint32_t	mip_start_ : 16;
			uint32_t	mip_levels_ : 16;
		};

		class RHITextureSRV : public RHIResource
		{
		public:
			RHITextureSRV(const RHITextureSRVDesc& desc);
		};

		struct RHITextureUAVDesc
		{
			RHITexture* texture_;
		};

		class RHITextureUAV : public RHIResource
		{
		public:
			RHITextureUAV(const RHITextureUAVDesc& desc);
		};
	}
}
