#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Reflection.h"
#include "Renderer/RtRenderShaderUtils.inl"

namespace MetaInit::Renderer {

	struct ShaderParametersReflection
	{
		String name_;
		uint32_t base_index_{ 0u };
		uint32_t buffer_index_{ 0u };
	};

	using RtShaderParameterInfosMap = Map<const String&, ShaderParametersReflection>;

	//parameters for shader def
	enum class EShaderResourceType {
		eTrivalData,
		eBuffer,
		eTexture,
		eSampler,
		eAS,
		eNum,
	};

	struct ShaderResourceParameter
	{
		String	name_;
		EShaderResourceType	type_;
		uint32_t	base_index_{ 0u };
		uint32_t	buffer_index_{ 0u };
		uint32_t	buffer_size_{ 0u };
	};

	class RtShaderParameterInfosMapUtils
	{
	public:
		using HashType = Utils::Blake3Hash64;
		explicit RtShaderParameterInfosMapUtils(RtShaderParameterInfosMap& map);
		bool FindShaderParameters(const String& name, ShaderParametersReflection& allocation)const;
		void AddShaderParameters(const ShaderParametersReflection& allocation);
		void RemoveShaderParameters(const String& name);
		const HashType& ComputeHash()const;
	private:
		RtShaderParameterInfosMap& map_;
	};

	void GetSPIRVShaderParameterInfosReflection(const Span<uint8_t>& spriv_code, RtShaderParameterInfosMap& map);
}