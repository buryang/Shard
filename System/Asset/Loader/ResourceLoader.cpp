#include "ResourceLoader.h"

namespace Shard::System::Resource
{
    Utils::Coro<> ResourceLoaderInterface::AsyncLoad(const ResourceRecord& record, ResourceID id)
    {
        Load(record, id);
        co_return;
    }
}
