#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"

namespace MetaInit::RHI {

	class RHIShader {
	public:
		using Ptr = RHIShader*;
		using HashVal = Utils::Blake3Hash64;
	};

	class RHIComputeShader : public RHIShader {

	};

	class RHIVertexShader : public RHIShader {

	};

	class RHIPixelShader : public RHIShader {

	};

}
