#pragma once
/*for some usually usaged ecs component and system*/
#include "Utils/SimpleEntitySystem.h"
namespace Shard
{
	namespace Utils {
		
		struct ECSRelationShipComponent
		{
			std::size_t	children_{ 0u };
			//parent
			Entity	parent_{ Entity::Null };
			//prev sibling 
			Entity	prev_{ Entity::Null };
			//next sibling
			Entity	next_{ Entity::Null };
			//first child
			Entity	first_child_{ Entity::Null };
		};

		struct ECSInputComponent
		{

		};
	}
}
