#include "RHI/RHIResourceBinding.h"
#include "Shaders/BindlessResource.hlsli"

namespace Shard::RHI
{
     REGIST_PARAM_TYPE(BOOL, BINDLESS_TABLE_ENABLE, true);

     const RHIBindLessTableInitializer& RHIBindLessTableInitializer::GetBindlessTableInitializer()
     {
          static RHIBindLessTableInitializer initializer;
          if (auto enable_num = initializer.num_member_; enable_num == 0u) {

               if (GET_PARAM_TYPE_VAL(BOOL, BINDLESS_TABLE_ENABLE))
               {
                    initializer.members_[enable_num].tag_ = EBindLessTag::eTextureSRVTag;
                    initializer.members_[enable_num].space_ = SHARD_BINDLESS_TEXTURE_SPACE;
                    initializer.members_[enable_num++].index_ = SHARD_BINDLESS_TEXTURE_INDEX;

                    initializer.members_[enable_num].tag_ = EBindLessTag::eTextureUAVTag;
                    initializer.members_[enable_num].space_ = SHARD_BINDLESS_TEXTURE_SPACE;
                    initializer.members_[enable_num++].index_ = SHARD_BINDLESS_TEXTURE_INDEX;

                    initializer.members_[enable_num].tag_ = EBindLessTag::eBufferSRVTag;
                    initializer.members_[enable_num].space_ = SHARD_BINDLESS_BUFFER_SPACE;
                    initializer.members_[enable_num++].index_ = SHARD_BINDLESS_BUFFER_INDEX;

                    initializer.members_[enable_num].tag_ = EBindLessTag::eBufferUAVTag;
                    initializer.members_[enable_num].space_ = SHARD_BINDLESS_BUFFER_SPACE;
                    initializer.members_[enable_num++].index_ = SHARD_BINDLESS_BUFFER_INDEX;
#if 1
                    initializer.members_[enable_num].tag_ = EBindLessTag::eAccStructTag;

#endif
          }
               initializer.num_member_ = enable_num;
          }
          return initializer;
     }
}