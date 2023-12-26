#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Handle.h"
#include "RHI/RHIResources.h"
#include "Core/PixelInfo.h"
#include "Renderer/RtRenderResourceDefinitions.h"
#include "Renderer/RtRenderPass.h"

namespace Shard
{
	namespace Renderer
	{
		//resource access/usage flags
		enum class EAccessFlags : uint64_t
		{
			eNone = 0,
			eIndirectArgs = 1 << 0,
			eIndexBuffer = 1 << 1,
			eVertexBuffer = 1 << 2,
			eTransferSrc = 1 << 3,
			eTransferDst = 1 << 4,
			eDSVRead = 1 << 5,
			eDSVWrite = 1 << 6,
			eSRV = 1 << 8,
			eRTV = 1 << 9,
			eUAV = 1 << 10,
			eDSV = 1 << 11,
			ePresent = 1 << 12,
			eExternal = 1 << 13,
			eReadOnly = eVertexBuffer | eIndexBuffer | eIndirectArgs | eTransferSrc | eSRV | eDSVRead,
			eReadAble = eReadOnly | eUAV,
			eWriteOnly = eRTV | eTransferDst | eDSVWrite,
			eWriteAble = eWriteOnly | eUAV,
			eNonUAV = ~eUAV,
		};

		/*buffer use texturelayout width to represent size*/
		struct TextureLayout {
			uint32_t	width_{ 0 };
			uint32_t	height_{ 0 };
			uint32_t	depth_{ 0 };
			uint32_t	mip_slices_{ 1 };
			uint32_t	array_slices_{ 1 };
			uint32_t	plane_slices_{ 1 };
			FORCE_INLINE bool operator==(const TextureLayout& rhs)const {
				return !std::memcmp(this, &rhs, sizeof(*this));
			}
			FORCE_INLINE bool operator!=(const TextureLayout& rhs)const {
				return !(*this == rhs);
			}
		};
		struct TextureSubRangeIndex {
			uint32_t	mip_{ 0 };
			uint32_t	layer_{ 0 };
			uint32_t	plane_{ 0 };
			bool operator==(const TextureSubRangeIndex& rhs)const {
				return mip_ == rhs.mip_ && layer_ == rhs.layer_ && plane_ == rhs.plane_;
			}
			bool operator!=(const TextureSubRangeIndex& rhs)const {
				return !(*this == rhs);
			}
		};

		struct TextureSubRange {
			uint32_t	base_mip_{ 0 };
			uint32_t	mips_{ 1 };
			uint32_t	base_layer_{ 0 };
			uint32_t	layers_{ 1 };
			uint32_t	base_plane_{ 0 };
			uint32_t	planes_{ 1 };
			explicit TextureSubRange(const TextureLayout& layout): mips_(layout.mip_slices_),layers_(layout.array_slices_),
																   planes_(layout.plane_slices_){}
			bool IsWholeRange(const TextureLayout& layout)const{
				return base_mip_ == 0 && base_layer_ == 0
					&& base_plane_ == 0 && mips_ == layout.mip_slices_
					&& layers_ == layout.array_slices_ && planes_ == layout.plane_slices_;
			}
			uint32_t GetSubRangeIndexCount()const {
				return mips_ * layers_ * planes_;
			}
			bool IsSubRangeIndexIncluded(const TextureSubRangeIndex& index)const {
				if (index.mip_ < base_mip_ || index.mip_ > base_mip_ + mips_) {
					return false;
				}
				if (index.layer_ < base_layer_ || index.layer_ > base_layer_ + layers_) {
					return false;
				}
				if (index.plane_ < base_plane_ || index.plane_ > base_plane_ + planes_) {
					return false;
				}
				return true;
			}
			uint32_t GetSubRangeIndexLocation(const TextureSubRangeIndex& index)const {
				if (!IsSubRangeIndexIncluded(index)) {
					return -1;
				}
				return index.mip_ - base_mip_ + (index.layer_ - base_layer_) * mips_ +
					(index.plane_ - base_plane_) * mips_ * planes_;
			}
			template<typename LAMBDA>
			requires std::is_invocable_v<LAMBDA, const TextureSubRangeIndex&>
			void For(LAMBDA&& lambda)const {
				for (auto pl = base_plane_; pl < base_plane_ + planes_; ++pl) {
					for (auto lay = base_layer_; lay < base_layer_ + layers_; ++lay) {
						for (auto mip = base_mip_; mip < base_mip_ + mips_; ++mip) {
							lambda({mip, lay, pl});
						}
					}
				}
			}
		};

		struct BufferSubRange {
			uint32_t	offset_{ 0 };
			uint32_t	size_{ 0 };
			bool operator==(const BufferSubRange& rhs)const {
				return offset_ == rhs.offset_ &&
					size_ == rhs.size_;
			}
			bool operator!=(const BufferSubRange& rhs)const {
				return !(*this != rhs);
			}
		};

		class RtField
		{
		public:
			enum class EType : uint8_t
			{
				eBuffer,
				eTexture,
				eNum,
			};

			enum class EUsage :uint8_t
			{
				eUnkown	= 0x0,
				eInput	= 0x1,
				eOutput = 0x2,
				eExtracted	= eOutput | 0x10,
				eExternal = 0x100, //external resource
				eCulled	= 0x200, //field deleted
			};

			struct TextureSubField
			{
				EAccessFlags	access_;
			};

			static bool IsSubFieldMergeAllowed(const TextureSubField& lhs, const TextureSubField& rhs) {
				if (lhs.access_ == rhs.access_) {
					return true;
				}
				auto union_access = Utils::LogicOrFlags(lhs.access_, rhs.access_);
				if (Utils::HasAnyFlags(lhs.access_, EAccessFlags::eReadOnly)
					&& Utils::HasAnyFlags(rhs.access_, EAccessFlags::eWriteAble)) {
					return false;
				}
				if (Utils::HasAnyFlags(lhs.access_, EAccessFlags::eWriteOnly)
					&& Utils::HasAnyFlags(rhs.access_, EAccessFlags::eReadAble)) {
					return false;
				}
				//FIXME
				if (Utils::HasAnyFlags(union_access, EAccessFlags::eUAV) &&
					Utils::HasAnyFlags(union_access, EAccessFlags::eNonUAV)) {
					return false;
				}
				return true;
			}

			//fixme check pipeline outside function
			static bool IsSubFieldTransitionNeeded(const TextureSubField& prev, const TextureSubField& next) {
				if (prev.access_ != next.access_||IsSubFieldMergeAllowed(prev, next)) {
					return true;
				}
				if (Utils::HasAnyFlags(next.access_, EAccessFlags::eUAV)) {
					return true;
				}
				return false;
			}

			FORCE_INLINE bool IsWholeResource()const { return type_ == EType::eBuffer || texture_sub_range_.IsWholeRange(layout_); }
			FORCE_INLINE bool IsConnectAble(const RtField& other)const;
			FORCE_INLINE bool IsExternal()const { return sub_resources_.access_ == EAccessFlags::eExternal; }
			FORCE_INLINE bool IsOutput()const { return Utils::HasAnyFlags(usage_, EUsage::eOutput); }
			FORCE_INLINE bool IsInput()const { return !IsOutput(); }
			FORCE_INLINE bool IsExtract()const { return usage_ == EUsage::eExtracted; }
			FORCE_INLINE bool IsReferenced()const { return ref_count_ > 1; }
			FORCE_INLINE bool IsCrossQueue()const { return is_cross_queue_; }
			FORCE_INLINE bool IsDedicated()const {}
			FORCE_INLINE bool IsTransiant()const{}
			RtField& Name(const String& name);
			RtField& ParentName(const String& name);
			RtField& Width(const uint32_t width);
			RtField& Height(const uint32_t height);
			RtField& Depth(const uint32_t depth);
			RtField& MipSlices(const uint32_t mip_slices);
			RtField& ArraySlices(const uint32_t array_slices);
			RtField& PlaneSlices(const uint32_t plane_slices);
			RtField& StageFlags(EPipelineStageFlags stage);
			//use with asyc compute and gfx queue
			RtField& CrossQueue(bool value);
			RtField& ForceTransiant();
			EType GetType()const;
			const String& GetName()const;
			const String& GetParentName()const;
			const TextureLayout& GetLayout()const;
			const TextureSubField& GetSubField()const;
			const TextureSubRange& GetSubRange()const;
			EPixFormat GetPixFormat()const;
			uint32_t InCrRef(uint32_t count=1) { 
				ref_count_ += count;
				return ref_count_;
			}
			uint32_t DecrRef(uint32_t count=1) {
				ref_count_ -= count;
				return ref_count_;
			}
			friend bool operator==(const RtField& lhs, const RtField& rhs) {
				return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
			}
			friend bool operator!=(const RtField& lhs, const RtField& rhs) { return !(lhs == rhs); }
		private:
			friend class RtRendererPass;
			String	name_;
			//name for producer
			String	parent_name_;
			EType	type_;
			EUsage	usage_{ EUsage::eUnkown };
			EPixFormat	pix_fmt_{ EPixFormat::eUnkown };
			mutable uint32_t	ref_count_{ 1 };
			//which stage of pass use this rtfield
			EPipelineStageFlags	stage_{ EPipelineStageFlags::eUnkown };
			//user force it tobe transiant
			bool	is_dedicated_{ false };
			bool	is_transiant_{ false };
			bool	is_cross_queue_{ false };
			uint32_t	sample_count_{ 1 };
			TextureLayout	layout_;
			union {
				/*texture range current field used*/
				TextureSubRange	texture_sub_range_;
				BufferSubRange buffer_sub_range_;
			};
			TextureSubField	sub_resources_;
		};

		/*parent resource class for texture and buffer*/
		class RtRenderResource
		{
		public:
			using Ptr = RtRenderResource*;
			using ThisType = RtRenderResource;

			RtRenderResource() = default;
			RtRenderResource(const RtField& field) { Init(field); }
			virtual void Init(const RtField& field);
			virtual ~RtRenderResource() {}
			bool IsExternal()const {
				return is_external_;
			}
			bool IsTransient()const {
				return is_transient_;
			}
			bool IsOutput()const {
				return is_output_;
			}
			bool IsValid(float time) const {
				return time >= life_time_.first && time <= life_time_.second;
			}
			void IncrRef(uint32_t ref) const{
				ref_count_.fetch_add(ref);
			}
			void DecrRef(uint32_t ref) const{
				ref_count_.fetch_sub(ref);
				assert(ref_count_ >= 0);
			}
		private:
			std::pair<uint32_t, uint32_t>	life_time_;
			uint8_t	is_external_:1{ 0 };
			uint8_t	is_transient_:1{ 1 };
			uint8_t	is_output_ : 1{ 0 };
			mutable std::atomic_uint	ref_count_{ 0u };
		};

		/*warper for rhi resource*/
		template <class RHIHandle>
		//requires std::is_same_v<std::decltype(std::declval<RHIHandle>().Release()), void>//FIXME
		class TRtRenderResource : public RtRenderResource
		{
		public:
			using HandleType = RHIHandle;
			HandleType Handle() const { return handle_; }
			ThisType& SetRHI(RHIHandle handle) {
				assert(IsExternal());
				handle_ = handle;
				return *this;
			}
			template<class RHIContext>
			ThisType& SetRHI(RHIContext::Ptr context) {
				//todo
			}
			void EndRHI() {
				handle_.Release();
			}
		private:
			RHIHandle	handle_{ nullptr };
		};

		using RtRenderTexture = TRtRenderResource<RHI::RHITexture>;
		using RtRenderBuffer = TRtRenderResource<RHI::RHIBuffer>;
		template<class Allocator=Utils::Allocator>
		using TextureRepo = Utils::ResourceManager<Allocator, RtRenderTexture>;
		template<class Allocator=Utils::Allocator>
		using BufferRepo = Utils::ResourceManager<Allocator, RtRenderBuffer>;
		static constexpr uint32_t MAX_RENDER_TARGET_ATTACHMENTS = 8;
		struct ALIGN_AS(128) RtRenderFrameBuffer
		{
			RtRenderTexture	depth_stencil_;
			SmallVector<RtRenderTexture, MAX_RENDER_TARGET_ATTACHMENTS>	color_attachments_;
		};

	}
}
