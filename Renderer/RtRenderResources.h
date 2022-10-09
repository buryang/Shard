#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Handle.h"
#include "RHI/RHIResource.h"
#include "Renderer/RtRenderResourceDefinitions.h"
#include "Renderer/RtRenderPass.h"

namespace MetaInit
{
	namespace Renderer
	{

		struct RtField
		{
		public:
			enum class EType : uint8_t
			{
				eBuffer,
				eTexture,
				eNum,
			};

			enum class EFlags : uint16_t
			{
				eNone = 0x0,
				eVertex = 0x1,
				eIndexBuffer = 0x2,
				eIndirectArgs = 0x4,
				eCopySrc = 0x8,
				eCopyDst = 0x10,
				eDepthStencil = 0x11,
				eResolveDst = 0x12,
				//support views
				eCBV = 0x14,
				eSRV = 0x18,
				eRTV = 0x20,
				eUAV = 0x21,
				eDSV = 0x22,
				ePresent = 0x24,
				eRenderTarget = 0x28,
				eExternal = 0x30,
			};

			enum class EUsage:uint8_t
			{
				eInternal	= 0x0,
				eInput		= 0x1,
				eOutput		= 0x2,
				eCulled		= 0x10, //field deleted
			};

			struct SubTextureField
			{
				EUsage						usage_{};
				EFlags						access_;
			};

			static bool IsSubFieldMergeAllowed(const SubTextureField& lhs, const SubTextureField& rhs) {

			}

			//fixme check pipeline outside function
			static bool IsSubFieldTransitionNeeded(const SubTextureField& prev, const SubTextureField& next) {
				if (prev.access_ != next.access_ || prev.usage_ != next.usage_) {
					return true;
				}
				if (Utils::HasAnyFlags(next.access_, EFlags::eUAV)) {
					return true;
				}
				return false;
			}
			struct SubRangeIndex {
				uint32_t mip_{ 0 };
				uint32_t layer_{ 0 };
				uint32_t plane_{ 0 };
			};

			bool IsWholeResource()const { return type_ == EType::eBuffer || sub_resources_.size() == 1; }
			bool IsMergeAble(const RtField& other)const;
			bool IsConnectAble(const RtField& other)const;
			bool IsExternal()const { return GetWholeResource().access_ == EFlags::eExternal; }
			bool IsOutput()const { return GetWholeResource().usage_ == EUsage::eOutput; }
			bool IsTransiant()const;
			RtField& Name(const String& name);
			RtField& ParentName(const String& name);
			RtField& Width(const uint32_t width);
			RtField& Height(const uint32_t height);
			RtField& Depth(const uint32_t depth);
			RtField& MipSlices(const uint32_t mip_slices);
			RtField& ArraySlices(const uint32_t array_slices);
			RtField& PlaneSlices(const uint32_t plane_slices);
			RtField& StageFlags(PipelineStageFlags stage);
			RtField& ForceTransiant();
			RtField& Merge(const RtField& other);
			RtField& InitWholeResource(const SubTextureField& init_state);
			RtField& InitAllSubResources(const SubTextureField& init_state);
			SubTextureField& operator[](uint32_t index);
			const SubTextureField& operator[](uint32_t index)const;
			SubTextureField& operator[](const SubRangeIndex& index);
			const SubTextureField& operator[](const SubRangeIndex& index)const;
			EType GetType()const;
			uint32_t GetSubFieldCount()const;
			const String& GetName()const;
			const String& GetParentName()const;
			const SubTextureField& GetWholeResource()const;
			friend bool operator==(const RtField& lhs, const RtField& rhs);
			friend bool operator!=(const RtField& lhs, const RtField& rhs) { return !(lhs == rhs); }
		public:
			friend class RtRendererPass;
			String						name_;
			//name for producer
			String						parent_name_;
			EType						type_;
			//which stage of pass use this rtfield
			PipelineStageFlags			stage_{ PipelineStageFlags::eUnkown };
			//user force it tobe transiant
			bool						force_transiant_{ false };
			uint32_t					sample_count_{ 1 };
			uint32_t					width_{ 0 };
			uint32_t					height_{ 0 };
			uint32_t					depth_{ 0 };
			uint32_t					mip_slices_{ 1 };
			uint32_t					array_slices_{ 1 };
			uint32_t					plane_slices_{ 1 };
			SmallVector<SubTextureField, 1>	sub_resources_;
		};


		/*warper for rhi resource*/
		template <class RHIHandle>
		requires std::is_same_v<std::decltype(std::declval<RHIHandle>().Release()), void>//FIXME
		class RtRenderResource
		{
		public:
			using HandleType = RHIHandle;
			using Ptr = RtRenderResource<HandleType>*;
			using ThisType = RtRenderResource<HandleType>;
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
			ThisType& IncrRef(uint32_t ref) const {
				ref_count_ += ref;
				return *this;
			}
			ThisType& DecrRef(uint32_t ref) const {
				ref_count_ -= ref;
				assert(ref_count_ >= 0);
				return *this;
			}
			ThisType& SetRHI(RHIHandle handle) {
				assert(IsExternal());
				handle_ = handle;
				return *this;
			}
			template<class RHIContext>
			ThisType& SetRHI(RHIContext::Ptr context) {
				//todo
			}
		private:
			RHIHandle							handle_{ nullptr };
			std::pair<uint32_t, uint32_t>		life_time_;
			uint8_t								is_external_:1{ 0 };
			uint8_t								is_transient_:1{ 1 };
			uint8_t								is_output_ : 1{ 0 };
			mutable uint32_t					ref_count_{ 0 };
			InitState							init_state_{ InitState::eUnkown };
		};

		using RtRenderTexture = RtRenderResource<RHI::RHITexture>;
		using RtRenderBuffer = RtRenderResource<RHI::RHIBuffer>;
		using TextureHandle = Utils::Handle<RtRenderTexture>;
		using BufferHandle = Utils::Handle<RtRenderBuffer>;

		static constexpr uint32_t MAX_RENDER_TARGET_ATTACHMENTS = 8;
		ALIGN_AS(128) struct RtRenderTargets
		{
			RtRenderTexture	depth_stencil_;
			SmallVector<RtRenderTexture, MAX_RENDER_TARGET_ATTACHMENTS>	color_attachments_;
		};

	}
}

#include "Renderer/RTRenderResources.inl"
