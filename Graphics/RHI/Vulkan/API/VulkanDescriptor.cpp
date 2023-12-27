#include "RHI/Vulkan/API/VulkanDescriptor.h"
#include "RHI/Vulkan/API/VulkanCmdContext.h"
#include "RHI/Vulkan/API/VulkanRenderPass.h"

namespace Shard
{
     VkDescriptorSetAllocateInfo MakeDescriptorSetAllocateInfo(VkDescriptorPool pool, const Span<VkDescriptorSetLayout>& layout)
     {
          VkDescriptorSetAllocateInfo desc_info{};
          memset(&desc_info, 0, sizeof(desc_info));
          desc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
          desc_info.descriptorPool = pool;
          desc_info.descriptorSetCount = layout.size();
          desc_info.pSetLayouts = layout.data();
          return desc_info;
     }

     VkDescriptorSetLayoutCreateInfo MakeDescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutCreateFlags flags,
                                                                                               const SmallVector<VkDescriptorSetLayoutBinding>& bindings)
     {
          VkDescriptorSetLayoutCreateInfo desc_info{};
          memset(&desc_info, 0, sizeof(desc_info));
          desc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
          desc_info.flags = flags;
          if (!bindings.empty())
          {
               desc_info.bindingCount = bindings.size();
               desc_info.pBindings = bindings.data();
          }
          return desc_info;
     }

     VkDescriptorPoolCreateInfo MakeDescriptorPoolCreateInfo(VkDescriptorPoolCreateFlags flags, uint32_t max_sets,
                                                                                     const SmallVector<VkDescriptorPoolSize>& pool_size)
     {
          VkDescriptorPoolCreateInfo pool_info{};
          memset(&pool_info, 0, sizeof(pool_info));
          pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
          pool_info.flags = flags;
          pool_info.pNext = VK_NULL_HANDLE;
          pool_info.maxSets = max_sets;
          if (!pool_size.empty())
          {
               pool_info.poolSizeCount = pool_size.size();
               pool_info.pPoolSizes = pool_size.data();
          }
          return pool_info;
     }

     VulkanDescriptorPool::VulkanDescriptorPool(const VulkanPoolCreateDesc& pool_info):pool_desc_(pool_info)
     {
          UpdateCurrPool();
     }

     VkDescriptorSet VulkanDescriptorPool::CreateDescriptorSet(VkDescriptorSetLayout layout)
     {
          const auto desc_creater = [&](VkDescriptorPool pool) {
               VkDescriptorSet desc_set{ VK_NULL_HANDLE };
               VkDescriptorSetLayout layout_array[] = { layout };
               VkDescriptorSetAllocateInfo desc_info = MakeDescriptorSetAllocateInfo(pool, layout_array);
               auto ret = vkAllocateDescriptorSets(GetGlobalDevice(), &desc_info, &desc_set);
               PCHECK(ret == VK_SUCCESS) << "alloc desc set failed";
               return desc_set;
          };
          
          std::scoped_lock <std::mutex> lock_pool(pool_mutex_);

          if (curr_ == VK_NULL_HANDLE)
          {
               UpdateCurrPool();
          }

          try
          {
               auto desc_set = desc_creater(curr_);
               return desc_set;
          }
          catch (std::runtime_error& e)
          {
               UpdateCurrPool();
               auto desc_set = desc_creater(curr_);
               return desc_set;
          }

     }

     SmallVector<VkDescriptorSet> VulkanDescriptorPool::CreateDescriptorSets(const Span<VkDescriptorSetLayout>& layouts)
     {
          SmallVector<VkDescriptorSet> desc_sets(layouts.size());
          auto desc_creater = [&](VkDescriptorPool pool) {
               VkDescriptorSetAllocateInfo desc_info = MakeDescriptorSetAllocateInfo(pool, layouts);
               auto ret = vkAllocateDescriptorSets(GetGlobalDevice(), &desc_info, desc_sets.data());
               if (ret == VK_SUCCESS)
               {
                    return;
               }
               PLOG(FATAL) << "alloc desc set failed";
          };

          std::scoped_lock <std::mutex> lock_pool(pool_mutex_);
          if (curr_ == VK_NULL_HANDLE || !CanAllocate(layouts))
          {
               UpdateCurrPool();
          }

          try
          {
               desc_creater(curr_);
               return desc_sets;
          }
          catch (std::runtime_error& e)
          {
               PLOG(FATAL) << "descriptor set alloc failed";
          }

     }

     VulkanDescriptorPool::~VulkanDescriptorPool()
     {
          for (auto pool : available_)
          {
               vkDestroyDescriptorPool(GetGlobalDevice(), pool, g_host_alloc);
          }
          available_.clear();

          for (auto pool : used_)
          {
               vkDestroyDescriptorPool(GetGlobalDevice(), pool, g_host_alloc);
          }
          used_.clear();
     }

     bool VulkanDescriptorPool::CanAllocate(const Span<VkDescriptorSetLayout>& layout)const
     {
          if (alloc_set_ + layout.size() > pool_set_) {
               return false;
          }
          //whether check descriptor count;
          return true;
     }

     void VulkanDescriptorPool::Reset()
     {
          for (auto pool : used_)
          {
               //flags is reversed for future usage
               vkResetDescriptorPool(GetGlobalDevice(), pool, 0);
          }
          available_.insert(available_.end(), used_.begin(), used_.end());
          used_.clear();
          curr_ = VK_NULL_HANDLE;
     }


     void VulkanDescriptorPool::UpdateCurrPool()
     {
          if (available_.empty())
          {
               VkDescriptorPoolCreateInfo pool_info = pool_desc_.ToVulkan();
               VkDescriptorPool pool;
               auto ret = vkCreateDescriptorPool(GetGlobalDevice(), &pool_info, g_host_alloc, &pool);
               PCHECK(ret == VK_SUCCESS) << "create descriptor pool failed with error: " << ret;
               available_.push_back(pool);
          }
          curr_ = available_.front();
          available_.pop_front();
          used_.push_back(curr_);
     }

     VulkanDescriptorPoolManager& VulkanDescriptorPoolManager::Instance() {
          static VulkanDescriptorPoolManager manager;
          return manager;
     }

     VulkanDescriptorPool::SharedPtr VulkanDescriptorPoolManager::GetPool(const VulkanPoolCreateDesc& pool_desc)
     {
          if (pools_.find(pool_desc) == pools_.end())
          {
               VulkanDescriptorPool::SharedPtr pool_ptr(new VulkanDescriptorPool(pool_desc));
               pools_.insert(eastl::make_pair(pool_desc, pool_ptr));
          }
          return pools_[pool_desc];
     }

     VulkanDescriptorSet::PseudoDescriptor VulkanDescriptorSet::operator[](const String& desc_name)
     {
          return VulkanDescriptorSet::PseudoDescriptor(this, descLUT_[desc_name]);
     }

     void VulkanDescriptorSet::Init(const VulkanPipelineLayoutDesc& desc_set)
     {
     }

     VulkanDescriptorSet::PseudoDescriptor VulkanDescriptorSet::operator[](const uint32_t binding)
     {
          return VulkanDescriptorSet::PseudoDescriptor(this, binding);
     }

     void VulkanDescriptorSet::BeginUpdates()
     {
          write_cache_.clear();
     }

     void VulkanDescriptorSet::EndUpdates()
     {
          if (!write_cache_.empty())
          {
               vkUpdateDescriptorSets(GetGlobalDevice(), write_cache_.size(), write_cache_.data(), 0, VK_NULL_HANDLE);
          }
     }

     VulkanDescriptorSet::VulkanDescriptorSet(VulkanDescriptorPool::SharedPtr pool, const VulkanDescriptorSetLayoutDesc& set_desc):name_(set_desc.name_)
     {
          //vkCreateDescriptorSet();
          assert(set_desc.bindings_.size() == set_desc.binding_names_.size());
          descLUT_.clear();
          for (auto n = 0; n < set_desc.bindings_.size(); ++n)
          {
               descLUT_[set_desc.binding_names_[n]] = n;
          }
          desc_infos_.resize(set_desc.binding_names_.size());
          auto ret = vkCreateDescriptorSetLayout(GetGlobalDevice(), &set_desc.ToVulkan(), g_host_alloc, &layout_);
          assert(ret == VK_SUCCESS && layout_ != VK_NULL_HANDLE);
          handle_ = pool->CreateDescriptorSet(layout_);
     }

     VulkanDescriptorSet::~VulkanDescriptorSet() {
          if (layout_ != VK_NULL_HANDLE) {
               vkDestroyDescriptorSetLayout(GetGlobalDevice(), layout_, g_host_alloc);
               layout_ = VK_NULL_HANDLE;
               //descriptorset recyle by pool
          }
     }

     static inline VkDescriptorType SetDescriptorType(bool read_only, bool is_buffer, bool is_texel)
     {
          if (is_buffer) {
               if (is_texel)
               {
                    return read_only ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
               }
               else
               {
                    return read_only ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
               }
          }
          else
          {
               return read_only ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
          }
     }

     void VulkanDescriptorSet::UpdateImage(const  VulkanImage& image, uint32_t binding)
     {
          VkWriteDescriptorSet write_set{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
          write_set.pNext = VK_NULL_HANDLE;
          write_set.dstSet = handle_;
          write_set.dstBinding = binding;
          write_set.descriptorCount = 1;
          write_set.descriptorType = SetDescriptorType(read_only_, false, false);

          auto* image_info = new (&desc_infos_[binding])VkDescriptorImageInfo;
          image_info->imageLayout = read_only_ ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
          image_info->imageView = 0;
          image_info->sampler = nullptr;
          write_set.pImageInfo = image_info;
          write_cache_.emplace_back(write_cache_);
     }

     void VulkanDescriptorSet::UpdateSampler(VkSampler sampler, uint32_t binding)
     {
          VkWriteDescriptorSet write_set{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
          write_set.pNext = VK_NULL_HANDLE;
          write_set.dstSet = handle_;
          write_set.dstBinding = binding;
          write_set.descriptorCount = 1;
          write_set.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
          auto* image_info = new(&desc_infos_[binding])VkDescriptorImageInfo;
          image_info->sampler = sampler;
          write_set.pImageInfo = image_info;
          write_cache_.emplace_back(write_set);
     }

     void VulkanDescriptorSet::UpdateBuffer(const  VulkanBuffer& buffer, uint32_t binding, uint32_t offset)
     {
          VkWriteDescriptorSet write_set{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
          write_set.pNext = VK_NULL_HANDLE;
          write_set.dstSet = handle_;
          write_set.dstBinding = binding;
          write_set.descriptorCount = 1;
          write_set.descriptorType = SetDescriptorType(read_only_, true, false);
          auto* buffer_info = new(&desc_infos_[binding])VkDescriptorBufferInfo;
          buffer_info->buffer = buffer.Get();
          buffer_info->offset = offset;
          buffer_info->range = buffer.GetSize();
          write_set.pBufferInfo = buffer_info;
          write_cache_.emplace_back(write_set);
          //todo
     }

     void VulkanDescriptorSet::UpdateBufferView(const VulkanBufferView& buffer_view, uint32_t binding)
     {
          VkWriteDescriptorSet write_set{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
          write_set.pNext = VK_NULL_HANDLE;
          write_set.dstSet = handle_;
          write_set.dstBinding = binding;
          write_set.descriptorCount = 1;
          write_set.descriptorType = SetDescriptorType(read_only_, true, true);
          write_set.pTexelBufferView = buffer_view.Get();
          write_cache_.emplace_back(write_set);
     }

     void VulkanDescriptorSet::UpdateInAttachment(const VulkanAttachment& attach, uint32_t binding)
     {
          VkWriteDescriptorSet write_set{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
          write_set.pNext = VK_NULL_HANDLE;
          write_set.dstSet = handle_;
          write_set.dstBinding = binding;
          write_set.descriptorCount = 1;
          write_set.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
          auto* image_info = new(&desc_infos_[binding])VkDescriptorImageInfo;
          image_info->sampler = VK_NULL_HANDLE;
          //image_info->imageView = attach.
          image_info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          write_set.pImageInfo = image_info; 
          write_cache_.emplace_back(write_set);
     }

     void VulkanDescriptorSet::UpdateAccelerationStructure(void*, uint32_t binding)
     {
          /*If descriptorType is VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 
           *the pNext chain must include a VkWriteDescriptorSetAccelerationStructureKHR
           *structure whose accelerationStructureCount member equals descriptorCount */
          auto* acc_info = new(&desc_infos_[binding])VkWriteDescriptorSetAccelerationStructureKHR;
          memset(acc_info, 0, sizeof(*acc_info));
          acc_info->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
          acc_info->accelerationStructureCount = 1;
          acc_info->pAccelerationStructures = nullptr; //todo
          VkWriteDescriptorSet write_set;
          memset(&write_set, 0, sizeof(write_set));
          write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          write_set.dstBinding = binding;
          write_set.dstSet = handle_;
          write_set.pNext = acc_info;
          write_set.descriptorCount = acc_info.accelerationStructureCount;
          write_cache_.emplace_back(write_set); 
     }

     void VulkanDescriptorSet::PseudoDescriptor::operator=(const VulkanImage& image)
     {
          wrapper_->UpdateImage(image, desc_binding_);
     }
     void VulkanDescriptorSet::PseudoDescriptor::operator=(const VulkanSampler& sampler)
     {
          wrapper_->UpdateSampler(sampler, desc_binding_);
     }
     void VulkanDescriptorSet::PseudoDescriptor::operator=(const VulkanBuffer& buffer)
     {
          wrapper_->UpdateBuffer(buffer, desc_binding_, 0);
     }
     void VulkanDescriptorSet::PseudoDescriptor::operator=(const  VulkanBufferView& buffer_view)
     {
          wrapper_->UpdateBufferView(buffer_view, desc_binding_);
     }
     void VulkanDescriptorSet::PseudoDescriptor::operator=(const VulkanAttachment& attach)
     {
          wrapper_->UpdateInAttachment(attach, desc_binding_);
     }

}