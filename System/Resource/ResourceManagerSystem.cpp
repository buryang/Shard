#include "System/Resource/ResourceManagerSystem.h"

namespace Shard::System::Resource
{

	ResourceID::ResourceID(const String& path) :path_(path), ext_fourcc_(0u)
	{
		if (const auto pos = path.find_last_of('.'); pos != String::npos && path.size()-pos <= 4) { //fourcc extension
            const auto* fourcc = path.substr(pos + 1).data();
            const auto fourcc_count = path.substr(pos + 1).size();
            for (auto n = 0u, shift = 24u; n < fourcc_count; ++n, shift -= 8u) {
                ext_fourcc_ |= (((uint32_t)fourcc[n]) << shift);
            }
		}
		else
		{
			LOG(ERROR) << "resource path with wrong format extension";
		}
        folly::hash::SpookyHashV2::Hash32(hash_.GetBytes(), hash_.GetHashSize(), ext_fourcc_); //ugly 
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
			//pending_requests_.emplace_back(load_request);
            auto insert_iter = eastl::find_if(pending_requests_.begin(), pending_requests_.end(), [&load_request](auto iter) { return iter->priority_ < load_request->priority_; });
            pending_requests_.insert(insert_iter, load_request);
		}
	}
	
	void ResourceManagerSystem::WaitAll()
	{
		while (IsBusy()) {
			Update();
		}
	}

}