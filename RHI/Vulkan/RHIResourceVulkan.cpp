#include "RHI/RHIResource.h"

namespace MetaInit::RHI {

	constexpr size_t RHI_PER_HEAP0_ALLOC_SIZE = 256;
	constexpr size_t RHI_PER_HEAP1_ALLOC_SIZE = 64; //total size limited on windows to 256MB
	constexpr size_t RHI_PER_HEAP2_ALLOC_SIZE = RHI_PER_HEAP0_ALLOC_SIZE;

	void RHITexture::SetRHI() {

	}

	void RHIBuffer::SetRHI() {

	}
}