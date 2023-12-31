#pragma once
#include "System/Resource/Loader/ResourceLoader.h"

namespace Shard::System::Resource
{
    class MINIT_API DDSTextureLoader : public ResourceLoaderInterface
    {
    public:
        bool Load(const ResourceRecord& record) override;
        virtual ~DDSTextureLoader();
        //todo convert to rhi texture
    };
}