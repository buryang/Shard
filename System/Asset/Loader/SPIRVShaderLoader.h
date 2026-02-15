#pragma once
#include "System/Resource/Loader/ResourceLoader.h"

namespace Shard::System::Resource
{
    class MINIT_API SPIRVShaderLoader final : public ResourceLoaderInterface
    {
    public:
        bool Load(const ResourceRecord& record, ResourceID id) override;
        virtual ~SPIRVShaderLoader();
    };
}