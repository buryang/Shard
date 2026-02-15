#pragma once
#include "System/Resource/Loader/ResourceLoader.h"

namespace Shard::System::Resource
{
    class MINIT_API DDSTextureLoader final: public ResourceLoaderInterface
    {
    public:
        bool Load(const ResourceRecord& record, ResourceID id) override;
        virtual ~DDSTextureLoader();
        //todo convert to rhi texture
    };
}