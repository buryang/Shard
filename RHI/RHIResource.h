#pragma once
#include "Utils/CommonUtils.h"

namespace MetaInit
{
	namespace RHI
	{
	
		class RHIBarrier
		{
		public:
			enum class EType :	uint8_t
			{

			};

			enum class EFlags : uint32_t
			{

			};
		private:


		};


		class RHIResource
		{
		public:
			using Ptr = std::shared_ptr<RHIResource>;
			virtual void SetRHI() = 0;
			virtual Ptr GetRHI() = 0;
			virtual ~RHIResource() {}
		};


		class RHITexture : public RHIResource
		{
		public:
			void SetRHI() override;
			RHIResource::Ptr GetRHI() override;

		};

		class RHIBuffer : public RHIResource
		{
		public:
			void SetRHI() override;
			RHIResource::Ptr GetRHI() override;

		};
	}
}
