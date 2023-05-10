#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"

namespace MetaInit::RHI {

	class RHIShader {
	public:
		using Ptr = RHIShader*;
		using HashType = Utils::Blake3Hash64;
		virtual ~RHIShader() {}
	};

	class RHIComputeShader : public RHIShader {
	public:
		using Ptr = RHIComputeShader*;
	};

	class RHIVertexShader : public RHIShader {
	public:
		using Ptr = RHIVertexShader*;
	};

	class RHIHullShader : public RHIShader {
	public:
		using Ptr = RHIHullShader*;
	};

	class RHIDomainShader : public RHIShader {
	public:
		using Ptr = RHIDomainShader*;
	};

	class RHIGeometryShader : public RHIShader {
	public:
		using Ptr = RHIGeometryShader*;
	};

	class RHIPixelShader : public RHIShader {
	public:
		using Ptr = RHIPixelShader*;
	};



	class RHIPipelineStateObject {
	public:
		using Ptr = RHIPipelineStateObject*;
		using HashType = Utils::Blake3Hash64;
		virtual ~RHIPipelineStateObject() {}
	};

}
