#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/SimpleCoro.h"
#include "System/Resource/ResourceManagerSystem.h"

namespace Shard::System::Resource
{
    class MINIT_API ResourceLoaderInterface
    {
    public:
        virtual bool Load(const ResourceRecord& record, ResourceID id) = 0;
        Utils::Coro<> AsyncLoad(const ResourceRecord& record, ResourceID id);
        virtual ~ResourceLoaderInterface() = default;
    };
}