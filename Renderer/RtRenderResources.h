#pragma once
#include <unordered_map>
#include "Utils/CommonUtils.h"
#include "Utils/Handle.h"
#include "RHI/RHIResource.h"
#include "Renderer/RtRenderResourceDefinitions.h"

namespace MetaInit
{
	namespace Renderer
	{

		class RtField
		{
		public:
			enum class EType : uint8_t
			{
				eBuffer,
				eTexture,
				eNum,
			};

			enum class EFlags : uint32_t
			{
				eCommon = 0x0,
				eVertex = 0x1,
				eIndexBuffer = 0x2,
				eIndirectArgs = 0x3,
				eCopySrc = 0x4,
				eCopyDst = 0x5,
				eDepthStencil = 0x6,
				eResolveDst = 0x7,
				//support views
				eCBV = 0x9,
				eSRV = 0x10,
				eRTV = 0x11,
				eUAV = 0x12,
				eDSV = 0x13,
				ePresent = 0x14,
				eRenderTarget = 0x15,
				eExternal = 0x100,
			};

			enum class EUsage
			{
				eInput,
				eOutput,
				eInternal,
				eCulled, //field deleted
				eNum,
			};

			bool IsWholeResource()const { return sub_resources_.size() == 1; }
			bool IsMergeAble(const RtField& other)const;
			bool IsTransitionNeeded(const RtField& dst)const;
			bool IsExternal()const { return flags_ == EFlags::eExternal; }
			bool IsOutput()const { return usage_ == EUsage::eOutput; }
			bool IsTransiant()const;
			RtField& Name(const std::string& name);
			RtField& Width(const uint32_t width);
			RtField& Height(const uint32_t height);
			RtField& Depth(const uint32_t depth);
			RtField& MipSlices(const uint32_t mip_slices);
			RtField& ArraySlices(const uint32_t array_slices);
			RtField& PlaneSlices(const uint32_t plane_slices);
			RtField& StageFlags(PipelineStageFlags stage);
			RtField& IncrRef(uint32_t reference = 1);
			RtField& DecrRef(uint32_t reference = 1);
			RtField& ForceTransiant();
			EType GetType()const;
			uint32_t GetFlags()const;
			uint32_t GetSubFieldCount()const;
			friend bool operator==(const RtField& lhs, const RtField& rhs);
			friend bool operator!=(const RtField& lhs, const RtField& rhs) { return !(lhs == rhs); }
		private:
			friend class RtRendererPass;
			std::string					name_;
			EType						type_;
			EUsage						usage_;
			EFlags						flags_;
			//id for attachment
			uint8_t						id_{ 0 };
			//control resource lifetime
			mutable uint32_t			reference_{ 0 };
			//which stage of pass use this rtfield
			PipelineStageFlags			stage_{ PipelineStageFlags::eStageUnkown };
			//user force it tobe transiant
			bool						force_transiant_{ false };
			uint32_t					sample_count_{ 1 };
			uint32_t					width_{ 0 };
			uint32_t					height_{ 0 };
			uint32_t					depth_{ 0 };
			uint32_t					mip_slices_{ 1 };
			uint32_t					array_slices_{ 1 };
			uint32_t					plane_slices_{ 1 };
			struct SubTextureFieldRange
			{
				uint32_t	base_mip_{ 0 };
				uint32_t	mip_count_{ 1 };
				uint32_t	base_layer_{ 0 };
				uint32_t	layer_count_{ 1 };
				uint32_t	base_plane_{ 0 };
				uint32_t	plane_count_{ 1 };
			};
			SmallVector<SubTextureFieldRange>	sub_resources_;
		};

		using FieldHandle = Utils::Handle<RtField>;
		using FieldManager = Utils::ResourceManager<RtField>;

		/*warper for rhi resource*/
		template <class RHIHandle>
		requires std::is_same_v<std::decltype(std::declval<RHIHandle>().Release()), void>//FIXME
		class RtRenderResource
		{
		public:
			using HandleType = RHIHandle;
			using Ptr = RtRenderResource<HandleType>*;
			enum class _InitialState
			{
				eUnkown,
				eClear,
				eLoad,
				eNum,
			}InitState;

			RtRenderResource() = default;
			RtRenderResource(const RtField& field) { Init(field); }
			void Init(const RtField& field);
			~RtRenderResource() { handle_.Release(); }
			bool IsExternal()const;
			bool IsTransient()const;
			bool IsOutput()const;
			bool IsValid(float time) const { return time >= life_time_.first && time <= life_time_.second; }
			HandleType Handle() const { return handle_; }
		private:
			RHIHandle							handle_{ nullptr };
			std::pair<uint32_t, uint32_t>		life_time_;
			uint8_t								is_external_:1{ 0 };
			uint8_t								is_transient_:1{ 1 };
			uint8_t								is_output_ : 1{ 0 };
			InitState							init_state_{ InitState::eUnkown };
		};

		using RtRenderTexture = RtRenderResource<RHI::RHITexture>;
		using RtRenderBuffer = RtRenderResource<RHI::RHIBuffer>;

		static constexpr uint32_t MAX_RENDER_TARGET_ATTACHMENTS = 8;
		ALIGN_AS(128) struct RtRenderTargets
		{
			RtRenderTexture	depth_stencil_;
			SmallVector<RtRenderTexture, MAX_RENDER_TARGET_ATTACHMENTS>	color_attachments_;
		};

	}
}

#include "Renderer/RTRenderResources.inl"
