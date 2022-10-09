#pragma once
#include "Utils/CommonUtils.h"

namespace MetaInit::Renderer
{
	struct RtRendererShaderParamBinding
	{
		enum class Type : uint8_t
		{
			eScalarOrVec,
			eResource,
			eStructInclude,
		};
		Type		type_;
		uint32_t	index_{ 0 };
		uint32_t	byte_offset_{ 0 };
	};

	static constexpr uint32_t MAX_PARAM_BINDINGS = 8;
	using RtRendererShaderParamBindings = SmallVector<RtRendererShaderParamBinding, MAX_PARAM_BINDINGS>;

	class MINIT_API RtRenderShader
	{
	public:
		enum class Frequency :uint16_t
		{
			eVertex,
			eHull,//tessellation control
			eGeometry,
			eFrag,
			eCompute,
			eRayGen,
			eRayAnyHit,
			eRayCloseHit,
			eRayMiss,
			eNum,
		};
		struct ShaderParam
		{
			virtual ~ShaderParam() {}
		};
		virtual ~RtRenderShader() {}
		static const RtRendererShaderParamBindings& GetShaderBindings() {
			return bindings_;
		}
		virtual Frequency GetShaderFrequency()const=0;
	protected:
		static RtRendererShaderParamBindings bindings_;
	};

	template<class RHICommand, class RHIShader, class ShaderClass>
	static void SetShaderParameters(RHICommand& rhi_cmd, typename RHIShader::Ptr rhi_shader, const typename ShaderClass::ShaderParam& parameters)
	{

	}

	class MINIT_API RtRenderShaderParametersMeta
	{
	public:
		struct Element
		{
			uint16_t	buffer_index_{ 0 };
			uint16_t	base_index_{ 0 };
			uint16_t	byte_offset_{ 0 };
			//type
		};
		const Element& operator[](uint32_t index)const;
		FORCE_INLINE uint32_t ParameterCount()const {
			return members_.size();
		}
		void AddElement(Element&& element);
	private:
		SmallVector<Element>	members_;
	};
}