#pragma once
#include "Utils/CommonUtils.h"

namespace MetaInit::Renderer
{
	class MINIT_API RtShaderCompileWorker
	{
	public:
		virtual ~RtShaderCompileWorker() {}
	};

	class MINIT_API RtShaderDXCCompileWorker final: public RtShaderCompileWorker
	{

	};

	class MINIT_API RtShaderCompiledRepo
	{
	public:
		using HashType = uint64_t;
	private:
		Map<HashType, >		shader_repo_;
	};
}
