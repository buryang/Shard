#pragma once
#include "Utils/Hash.h"
#include "Utils/SimpleEntitySystem.h"
#include "Utils/SimpleJobSystem.h"
#include <shared_mutex>

namespace Shard::System::Resource
{

	class MINIT_API ResourceID
	{
	public:
		using HashType = Utils::SpookyV2Hash32;
		explicit ResourceID(const String& path);
		HashType Hash() const {
			return hash_;
		}
		const String& Extension() const {
			return ext_type_;
		}
	private:
		String	path_;
		HashType	hash_;
		String	ext_type_; //fourcc extension type
	};

	class MINIT_API ResourceRecord
	{
	public:
		using Ptr = ResourceRecord*;
		using Blob = Vector<uint8_t>;
	private:
		Blob	resource_blob_;
	};

	class MINIT_API ResourceProviderBase
	{
	public:
		using Ptr = ResourceProviderBase*;
		virtual ~ResourceProviderBase() = default;
		virtual void ResponseRequest(ResourceID resource_id);
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
		void SubmitLoadRequest(ResourceID resource_id, ResourceRecord::Ptr record);
		void SubmitUnLoadRequest(ResourceID resoource_id, ResourceRecord::Ptr record);
		void WaitAll();
		~ResourceManagerSystem();
	private:
		std::shared_mutex	access_mutex_;
		std::atomic<Utils::JobEntry*> async_load_worker_{ nullptr };
		struct ResourceRequest
		{
			enum class EType {
				eLoad,
				eUnload,
			};
			EType	type_{ EType::eLoad };
			ResourceRecord::Ptr	record_{ nullptr };
			ResourceID	resource_id_;
		};
		Vector<ResourceRequest> pending_requests_;
		Vector<ResourceRequest> active_requests_;
		ResourceProviderBase::Ptr	resource_provider_{ nullptr };
	};


}