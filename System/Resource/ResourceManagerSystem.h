#pragma once
#include "Utils/Hash.h"
#include "Utils/SimpleEntitySystem.h"
#include "Utils/SimpleCoro.h"
#include <shared_mutex>

namespace Shard::System::Resource
{
    //assets/media path as ID; Every resource in a game must have some
    //kind of globally unique identifier(GUID).The most common choice of
    //GUID is the resource¡¯s fi le system path(stored either as a string
    //or a 32 - bit hash).This kind of GUID is intuitive,
	class MINIT_API ResourceID
	{
	public:
		using HashType = Utils::SpookyV2Hash32;
		explicit ResourceID(const String& path);
		HashType Hash() const {
			return hash_;
		}
		uint32_t Extension() const {
			return ext_fourcc_;
		}
        const String& Path() const {
            return path_;
        }
	private:
		const String    path_;
		HashType	hash_;
        uint32_t	ext_fourcc_{ 0u }; //fourcc extension type
	};

	struct ResourceRecord
	{
		using Blob = Vector<uint8_t, Utils::ScalablePoolAllocator<uint8_t>>; 
		Blob	resource_blob_;
	};

	//todo sync/async resource response
	class MINIT_API ResourceProviderBase
	{
	public:
		virtual ~ResourceProviderBase() = default;
		virtual void ResponseRequest(ResourceID resource_id) {}
		virtual void AsyncResponseRequest(ResourceID resource_id) {}
	};

	class MINIT_API LocalHostResourceProvider : public ResourceProviderBase
	{
	public:
		virtual void ResponseRequest(ResourceID reource_id);
	};

	class MINIT_API ResourceManagerSystem : public Utils::ECSSystem
	{
	public:
		void Init();
		void Update(float dt);
		void UnInit();
		bool IsBusy() const;
		void SubmitLoadRequest(ResourceID resource_id, ResourceRecord* record);
		void SubmitUnLoadRequest(ResourceID resoource_id, ResourceRecord* record);
		void WaitAll();
		~ResourceManagerSystem();
	private:
		std::shared_mutex	access_mutex_;
		std::atomic<Utils::JobEntry*> async_load_worker_{ nullptr };
		struct ResourceRequest
		{
			enum class EType : uint8_t{
				eLoad,
				eUnload,
			};
            enum class EPriority : uint8_t {
                eLow,
                eMedium,
                eHigh,
                eUltra,
            };
			EType	type_{ EType::eLoad };
            EPriority   piroity_{ EPriority::eMedium };
			ResourceRecord*	record_{ nullptr };
			ResourceID	resource_id_;
		};
		Vector<ResourceRequest> pending_requests_;
		Vector<ResourceRequest> active_requests_;
		ResourceProviderBase*	resource_provider_{ nullptr };
	};


}