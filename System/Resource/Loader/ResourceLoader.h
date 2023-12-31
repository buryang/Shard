#pragma once
#include "Utils/CommonUtils.h"
#include "System/Resource/ResourceManagerSystem.h"

namespace Shard::System::Resource
{
    class MINIT_API ResourceLoaderInterface
    {
    public:
        virtual bool Load(const ResourceRecord& record);
        virtual ~ResourceLoaderInterface() = default;
    };
}