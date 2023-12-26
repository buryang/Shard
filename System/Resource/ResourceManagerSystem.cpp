#include "System/Resource/ResourceManagerSystem.h"

namespace Shard::System::Resource
{

	ResourceID::ResourceID(const String& path) :path_(path)
	{
		folly::hash::SpookyHashV2::Hash32(hash_.GetBytes(), hash_.GetHashSize(), 0u); //ugly 
		if (const auto pos = path.find_last_of('.'); pos != String::npos && path.size()-pos <= 4) { //fourcc extension
			ext_type_ = path.substr(pos+1);
		}
		else
		{
			LOG(ERROR) << "resource path with wrong format extension";
		}
	}

	void ResourceManagerSystem::Update(float dt)
	{
		if (async_load_worker_ != nullptr&& !async_load_worker_.load()->IsFinished()) {
			return;
		}

		{
			assert(active_requests_.empty()&&"async work should execute all activate request");
			std::unique_lock<std::shared_mutex> lock(access_mutex_);
			eastl::swap(pending_requests_, active_requests_);
		}

		/*
		Utils::Coro<void> LoadResource(ResourceID resource_id) {
			resource_provider_->ResponseRequest(resource_id, ); //todo sync conflict
			co_return;
		}

		async_load_worker_ = Utils::Schedule([&, this]() {
			for (auto iter = active_requests_.begin(); iter != active_requests_.end(); ++iter) {
				//deal with load request here
				Utils::Schedule(LoadResource(iter->resource_id));
			}
		});
		*/
	}

	void ResourceManagerSystem::UnInit()
	{
		WaitAll();
		async_load_worker_ = nullptr;
	}
	
	bool ResourceManagerSystem::IsBusy() const
	{
		if (async_load_worker_ && !async_load_worker_->IsFinished()) {
			return true;
		}

		{
			std::shared_lock<std::shared_mutex> lock(access_mutex_);
			if (!pending_requests_.empty()) {
				return true;
			}
		}
		return false;
	}

	void ResourceManagerSystem::SubmitLoadRequest(ResourceID resource_id, ResourceRecord::Ptr record)
	{
		ResourceRequest load_request{ .type_ = ResourceRequest::EType::eLoad,
									  .resource_id_ = resource_id,
									  .record_ = record };
		{
			std::shared_lock<std::shared_mutex> lock(access_mutex_);
			pending_requests_.emplace_back(load_request);
		}
	}
	
	void ResourceManagerSystem::WaitAll()
	{
		while (IsBusy()) {
			Update();
		}
	}

}