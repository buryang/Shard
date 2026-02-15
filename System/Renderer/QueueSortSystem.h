#pragma once

#include "Utils/SimpleEntitySystem.h"
#include "Utils/SimpleEntitySystemPrimitive.h"

namespace Shard::Renderer
{

    using ECSNoAutoBatchingTag = Utils::ECSTagComponent<struct NoAutoBatching_>;

}
