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
        constexpr auto resource_batch_size = 64u;
		if (async_load_worker_) {
            async_load_worker_.Wait(); //wait previous async work done
            //return;
		}

		{
			assert(active_requests_.empty()&&"async work should execute all activate request");
			std::unique_lock<std::shared_mutex> lock(access_mutex_);
			eastl::swap(pending_requests_, active_requests_);
		}

		Utils::Coro<> LoadResourceBatch(auto iter_begin, auto iter_end) {
            for(auto iter = iter_begin; iter < iter_end; ++iter) {
                resource_provider_->ResponseRequest(iter->resource_id);
            }
			co_return;
		}

		async_load_worker_ = Utils::Schedule([&, this]() {
            auto request_counter{0u};
            auto iter_begin = active_requests_.begin();
			for (auto iter = active_requests_.begin(); iter != active_requests_.end(); ++iter, ++request_counter) {
                if( (iter == active_requests_.end() || request_counter >= resource_batch_size) && iter_begin != iter) {
                    //co_await LoadResourceBatch(iter_begin, iter);
                    Utils::Schedule(LoadResourceBatch(iter_begin, iter));
                    iter_begin = iter;
                    request_counter = 0u;
                }
			}
		});
	}

	void ResourceManagerSystem::UnInit()
	{
		WaitAll();
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