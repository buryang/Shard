#pragma once
#include "System/Resource/Loader/ResourceLoader.h"

namespace Shard::System::Resource
{
    class MINIT_API SPIRVShaderLoader : public ResourceLoaderInterface
    {
    public:
        bool Load(const ResourceRecord& record) override;
        virtual ~SPIRVShaderLoader();
    };
}