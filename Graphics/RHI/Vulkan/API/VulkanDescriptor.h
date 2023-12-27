#pragma once
#include "RHI/Vulkan/API/VulkanRHI.h"
#include "RHI/Vulkan/API/VulkanResources.h"

//https://stackoverflow.com/questions/48867995/how-to-correctly-use-decriptor-sets-for-multiple-interleaving-buffers
namespace Shard
{
     struct VulkanPoolCreateDesc
     {
          using ThisType = VulkanPoolCreateDesc;
          uint32_t     set_size_;
          VkDescriptorPoolCreateFlags     flags_{ 0x0 };
          SmallVector<VkDescriptorPoolSize> desc_count_;
          FORCE_INLINE ThisType& AddPoolSize(const VkDescriptorPoolSize& pool_size) { desc_count_.emplace_back(pool_size); }
          FORCE_INLINE VkDescriptorPoolCreateInfo ToVulkan() const {
               VkDescriptorPoolCreateInfo create_info{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                                 .pNext = nullptr,
                                                                 .flags = flags_,
                                                                 .maxSets = set_size_ };
               create_info.poolSizeCount = desc_count_.size();
               create_info.pPoolSizes = desc_count_.data();
               return create_info;
          }
     };

     struct VulkanDescriptorSetLayoutDesc
     {
          using ThisType = VulkanDescriptorSetLayoutDesc;
          String     name_;
          SmallVector<VkDescriptorSetLayoutBinding>     bindings_;
          SmallVector<String>     binding_names_;
          SmallVector<VkDescriptorBindingFlags>     binding_flags_;
          VkDescriptorSetLayoutCreateFlags     flags_{ 0x0 };
          mutable VkDescriptorSetLayoutBindingFlagsCreateInfo temp_binding_flags_create_info_;
          FORCE_INLINE static bool IsBindingSamplerIMmutable(const VkDescriptorSetLayoutBinding& binding) {
               return binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
          }
          FORCE_INLINE VkDescriptorSetLayoutBinding& operator[](uint32_t index) { return bindings_[index]; }
          FORCE_INLINE ThisType& AddBinding(const String& name, const VkDescriptorSetLayoutBinding& binding, VkDescriptorBindingFlags binding_flags) { 
               bindings_.emplace_back(binding);
               binding_names_.emplace_back(name);
               if (binding_flags != 0x0) {
                    binding_flags_.emplace_back(binding_flags);
               }
               return *this;
          }
          FORCE_INLINE uint32_t GetBindingSize() const { return bindings_.size(); }
          FORCE_INLINE VkDescriptorSetLayoutCreateInfo ToVulkan() const {
               PCHECK(binding_flags_.empty() || binding_flags_.size() == bindings_.size()) << "bind flags size wrong";
               VkDescriptorSetLayoutCreateInfo create_info;
               memset(&create_info, 0, sizeof(create_info));
               create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
               if (binding_flags_.size()) {
                    auto& flags_info = temp_binding_flags_create_info_;
                    flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                    flags_info.pNext = VK_NULL_HANDLE;
                    flags_info.bindingCount = binding_flags_.size();
                    flags_info.pBindingFlags = binding_flags_.data();
                    create_info.pNext = &flags_info;
               }
               create_info.flags = flags_;
               create_info.bindingCount = bindings_.size();
               create_info.pBindings = bindings_.data();
               return create_info;
          }
     };

     //like dxr RootSignature
     struct VulkanPipelineLayoutDesc
     {
          using ThisType = VulkanPipelineLayoutDesc;
          VkPipelineLayoutCreateFlags     flags_{ 0x0 };
          SmallVector<VkPushConstantRange>     push_consts_;
          SmallVector<VkDescriptorSetLayout>     set_layouts_;
          //fixme with this
          SmallVector<VulkanDescriptorSetLayoutDesc> set_layouts_desc_;
          FORCE_INLINE ThisType& AddSet(const VulkanDescriptorSetLayoutDesc& desc_set, const VkDescriptorSetLayout& desc_set_layout) {
               set_layouts_.emplace_back(desc_set_layout);
               set_layouts_desc_.emplace_back(desc_set);
               return *this;
          }
          FORCE_INLINE ThisType& AddConstRange(const VkPushConstantRange& const_range) { 
               PCHECK(IsPushConstantRangeValid(const_range)) << "push constant range is invalid";
               push_consts_.emplace_back(const_range);
               return *this;
          }
          FORCE_INLINE uint32_t GetNumConstants()const { return push_consts_.size(); }
          FORCE_INLINE uint32_t GetNumDescriptorSets()const { return set_layouts_.size(); }
          FORCE_INLINE const VkPushConstantRange& GetConstantDesc(uint32_t index)const {
               return push_consts_[index];
          }
          FORCE_INLINE const VkDescriptorSetLayout& GetDescriptorSetDesc(uint32_t index)const {
               return set_layouts_[index];
          }
          FORCE_INLINE static bool IsPushConstantRangeValid(const VkPushConstantRange& range) {
               return range.offset&0x3==0 && range.size&0x3==0;
          }
          FORCE_INLINE VkPipelineLayoutCreateInfo ToVulkan() const {
               VkPipelineLayoutCreateInfo create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
               create_info.pNext = VK_NULL_HANDLE;
               create_info.flags = flags_;
               create_info.setLayoutCount = set_layouts_.size();
               create_info.pSetLayouts = set_layouts_.data();
               create_info.pushConstantRangeCount = push_consts_.size();
               create_info.pPushConstantRanges = push_consts_.data();
               return create_info;
          }
     };

     class VulkanDescriptorPool
     {
     public:
          using SharedPtr = std::shared_ptr<VulkanDescriptorPool>;
          using Handle = VkDescriptorPool;
          explicit VulkanDescriptorPool(const VulkanPoolCreateDesc& pool_info);
          ~VulkanDescriptorPool();
          VkDescriptorSet CreateDescriptorSet(VkDescriptorSetLayout layout);
          SmallVector<VkDescriptorSet> CreateDescriptorSets(const Span<VkDescriptorSetLayout>& layouts);
          void Reset();
     private:
          DISALLOW_COPY_AND_ASSIGN(VulkanDescriptorPool);
          bool CanAllocate(const Span<VkDescriptorSetLayout>& layout)const;
          void UpdateCurrPool();
     private:
          friend class DescriptorPoolManager;
          using List = List<VkDescriptorPool>;
          List available_;
          List used_;
          VkDescriptorPool curr_{ VK_NULL_HANDLE };
          uint32_t     alloc_set_{ 0 };
          uint32_t     pool_set_;
          const VulkanPoolCreateDesc pool_desc_;
          std::mutex     pool_mutex_;
     };

     class VulkanDescriptorPoolManager
     {
     public:
          using ThisType = VulkanDescriptorPoolManager;
          static ThisType& Instance();
          VulkanDescriptorPool::SharedPtr GetPool(const VulkanPoolCreateDesc& pool_desc);
     private:
          VulkanDescriptorPoolManager() = default;
          DISALLOW_COPY_AND_ASSIGN(VulkanDescriptorPoolManager);
     private:
          Map<VulkanPoolCreateDesc, VulkanDescriptorPool::SharedPtr>     pools_;
     };

     class VulkanCmdBuffer;
     class VulkanAttachment;
     class VulkanDescriptorSet : public std::enable_shared_from_this<VulkanDescriptorSet>
     {
     public:
          using Handle = VkDescriptorSet;
          using Ptr = VulkanDescriptorSet*;
          using SharedPtr = eastl::shared_ptr<VulkanDescriptorSet>;
          struct PseudoDescriptor
          {
               PseudoDescriptor(Ptr wrapper, uint32_t binding) : wrapper_(wrapper), desc_binding_(binding) {}
               void operator=(const VulkanImage& image);
               void operator=(VkSampler sampler);
               void operator=(const VulkanBuffer& buffer);
               void operator=(const VulkanBufferView& buffer_view);
               void operator=(const VulkanAttachment& attach);
               Ptr wrapper_{ nullptr };
               uint32_t desc_binding_{ 0 };
          };
          PseudoDescriptor operator[](const String& desc_name);
          //interface for bindless access
          PseudoDescriptor operator[](const uint32_t binding);
          void BeginUpdates();
          void EndUpdates();
          FORCE_INLINE const String& GetName()const { return name_; }
          FORCE_INLINE Handle Get()const { return handle_; }
          FORCE_INLINE VkDescriptorSetLayout GetLayout()const { return layout_; }
          explicit VulkanDescriptorSet(VulkanDescriptorPool::SharedPtr pool, const VulkanDescriptorSetLayoutDesc& set_desc);
          DISALLOW_COPY_AND_ASSIGN(VulkanDescriptorSet);
          //descriptor pool to manage resource release
          ~VulkanDescriptorSet();
     private:
          //configure descriptor 
          void UpdateImage(const VulkanImage& image, uint32_t binding);
          void UpdateSampler(VkSampler sampler, uint32_t binding);
          void UpdateBuffer(const VulkanBuffer& buffer, uint32_t binding, uint32_t offset);
          void UpdateBufferView(const VulkanBufferView& buffer_view, uint32_t binding);
          void UpdateInAttachment(const VulkanAttachment& attach, uint32_t binding);
          void UpdateAccelerationStructure(void*, uint32_t binding);
     private:
          Handle handle_{ VK_NULL_HANDLE };
          //descriptor set layout 
          VkDescriptorSetLayout     layout_{ VK_NULL_HANDLE };
          Map<String, uint32_t>     descLUT_;
          SmallVector<VkWriteDescriptorSet>     write_cache_;
          bool     read_only_{ false };
          String     name_;
          //descriptor set write data container
          struct VulkanDescriptorInfoDetail
          {
               union
               {
                    VkDescriptorBufferInfo     buffer_info_;
                    VkDescriptorImageInfo     image_info_;
                    VkBufferView buffer_view_;
               };
          };
          SmallVector<VulkanDescriptorInfoDetail>     desc_infos_;
     };




}
