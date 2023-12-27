#include "RHI/VulkanWindowSystem.h"
#include "RHI/VulkanRHI.h"

namespace Shard
{
     static constexpr uint32_t g_swap_chain_image_count = 2;
     VkSwapchainCreateInfoKHR MakeSwapChainCreateInfo(VkSwapchainCreateFlagsKHR flags,
          VkSurfaceKHR surface,
          const VkSurfaceCapabilitiesKHR& surface_capabilities,
          VkSwapchainKHR old_swap)
     {
          VkSwapchainCreateInfoKHR swap_info{};
          memset(&swap_info, 0, sizeof(swap_info));
          swap_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
          swap_info.imageExtent.width = surface_capabilities.currentExtent.width;
          swap_info.imageExtent.height = surface_capabilities.currentExtent.height;
          swap_info.oldSwapchain = old_swap; //to further check this
          swap_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
          swap_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
          swap_info.imageArrayLayers = 1;
          //VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR specifies that image content is presented without being changed
          swap_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; //VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR
          swap_info.clipped = VK_FALSE;

          std::array<VkCompositeAlphaFlagBitsKHR, 4> composite_alpha_flags = {
               VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
               VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
               VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
               VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
          };
          for (auto i = 0; i < composite_alpha_flags.size(); ++i)
          {
               if (surface_capabilities.supportedCompositeAlpha & composite_alpha_flags[i])
               {
                    swap_info.compositeAlpha = composite_alpha_flags[i];
                    break;
               }
          }

          swap_info.minImageCount = std::min(surface_capabilities.maxImageCount, std::max(surface_capabilities.minImageCount, g_swap_chain_image_count));
          return swap_info;
     }

     VkWin32SurfaceCreateInfoKHR MakeSurfaceCreateInfo(VkWin32SurfaceCreateFlagsKHR flags, HINSTANCE hinstance, HWND hwnd)
     {
          VkWin32SurfaceCreateInfoKHR surface_info{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
          surface_info.pNext = VK_NULL_HANDLE;
          surface_info.flags = flags;
          surface_info.hinstance = hinstance;
          surface_info.hwnd = hwnd;
          return surface_info;
     }

     VkFramebufferCreateInfo MakeFramebufferCreateInfo(VkFramebufferCreateFlags flags, VkRenderPass pass, Span<VkImageView>& attachs, uint32_t width, uint32_t height, uint32_t layers)
     {
          VkFramebufferCreateInfo frame_info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
          memset(&frame_info, 0, sizeof(frame_info));
          frame_info.attachmentCount = attachs.size();
          frame_info.pAttachments = attachs.data();
          frame_info.renderPass = pass;
          frame_info.width = width;
          frame_info.height = height;
          frame_info.layers = layers;
          return frame_info;
     }

     VulkanWindowSystemImpl::VulkanWindowSystemImpl(VulkanInstance::Ptr instance, VulkanDevice::Ptr device, GLFWwindow* window) : instance_(instance), device_(device), window_(window)
     {
          //first create a surface
          auto ret = glfwCreateWindowSurface(instance_->Get(), window, g_host_alloc, &surface_);
          if (VK_SUCCESS != ret)
          {
               const char* error_message = NULL;
               glfwGetError(&error_message);
               throw std::runtime_error(error_message);
          }

          //todo check input swapchain info
          VkSurfaceCapabilitiesKHR surface_caplity{};
          vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_->GetPhy(), surface_, &surface_caplity);
          auto swap_info = MakeSwapChainCreateInfo(VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR, surface_, surface_caplity, nullptr);
          int32_t win_width = 0, win_height = 0;
          glfwGetWindowSize(window, &win_width, &win_height);
          /*
           *currentExtent is the current widthand height of the surface, or the special value(0xFFFFFFFF,
           *0xFFFFFFFF) indicating that the surface size will be determined by the extent of a swapchain
           *targeting the surface
           */
          swap_info.imageExtent.width = swap_info.imageExtent.width == 0xFFFFFFFF ? win_width : swap_info.imageExtent.width;
          swap_info.imageExtent.height = swap_info.imageExtent.height == 0xFFFFFFFF ? win_height : swap_info.imageExtent.height;
          if (swap_info.imageExtent.height * swap_info.imageExtent.width == 0)
          {
               vkDestroySurfaceKHR(instance_->Get(), surface_, g_host_alloc);
               throw std::invalid_argument("error swap chain width/height configure");
          }

          swap_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
#if 0 
          uint32_t present_mode_count = 0;
          VkPresentModeKHR no_vsync_present_mode = VK_PRESENT_MODE_FIFO_KHR;
          vkGetPhysicalDeviceSurfacePresentModesKHR(device_->GetPhy(), surface_, &present_mode_count, nullptr);
          if (present_mode_count != 0)
          {
               SmallVector<VkPresentModeKHR> present_modes(present_mode_count);
               vkGetPhysicalDeviceSurfacePresentModesKHR(device_->GetPhy(), surface_, &present_mode_count, present_modes.data());
               for (uint32_t i = 0; i < swapchain->present_mode_count; ++i) {
                    if (swapchain->present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
                         no_vsync_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                    if (swapchain->present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                         no_vsync_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                         break;
                    }
               }
          }
          swap_info.presentMode = no_vsync_present_mode;
#endif
          uint32_t surface_format_count = 0;
          swap_info.imageFormat = VK_FORMAT_UNDEFINED;
          vkGetPhysicalDeviceSurfaceFormatsKHR(device_->GetPhy(), surface_, &surface_format_count, nullptr);
          SmallVector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
          vkGetPhysicalDeviceSurfaceFormatsKHR(device_->GetPhy(), surface_, &surface_format_count, surface_formats.data());

          if (surface_formats.size() == 1 && surface_formats[0].format == VK_FORMAT_UNDEFINED)
               swap_info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
          for (auto sf : surface_formats)
          {
               if (sf.colorSpace != VK_COLORSPACE_SRGB_NONLINEAR_KHR)
               {
                    continue;
               }
               if (sf.format == VK_FORMAT_R8G8B8A8_UNORM || sf.format == VK_FORMAT_R8G8B8A8_SRGB || sf.format == VK_FORMAT_B8G8R8A8_UNORM
                    || sf.format == VK_FORMAT_B8G8R8A8_SRGB || sf.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || sf.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32)
               {
                    swap_info.imageFormat = sf.format;
               }
          }

          if (VK_FORMAT_UNDEFINED == swap_info.imageFormat) {
               vkDestroySurfaceKHR(instance_->Get(), surface_, g_host_alloc);
               throw std::invalid_argument("swap chain image format is invalid");
          }

          swap_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
          //queueFamilyIndex only need if imageSharingMode==VK_SHARING_MODE_CONCURRENT
          ret = vkCreateSwapchainKHR(device->Get(), &swap_info, g_host_alloc, &swap_chain_);
          if (VK_SUCCESS != ret)
          {
               vkDestroySurfaceKHR(instance_->Get(), surface_, g_host_alloc);
               throw std::runtime_error("failed to create swapchain");
          }

          //init present queue
          auto& query_surface_queue = [&](VkPhysicalDevice device, uint32_t family_index)->bool {
               VkBool32 present_able = false;
               vkGetPhysicalDeviceSurfaceSupportKHR(device, family_index, surface_, &present_able);
               return present_able == VK_TRUE;
          };
          present_queue_ = device_->QueryQueue(query_surface_queue);
          //FIXME to deal with old swapchain
          InitFrameWorkLoads();
     }

     VulkanWindowSystemImpl::VulkanWindowSystemImpl(VulkanWindowSystemImpl&& wsi)
     {
          surface_ = wsi.surface_;
          swap_chain_ = wsi.swap_chain_;
          instance_ = wsi.instance_;
          device_ = wsi.device_;
          present_queue_ = wsi.present_queue_;
          wsi.surface_ = VK_NULL_HANDLE;
          wsi.swap_chain_ = VK_NULL_HANDLE;
          InitFrameWorkLoads();
     }

     VulkanWindowSystemImpl::~VulkanWindowSystemImpl()
     {
          if (VK_NULL_HANDLE != surface_)
          {
               vkDestroySurfaceKHR(instance_->Get(), surface_, g_host_alloc);
          }

          if (VK_NULL_HANDLE != swap_chain_)
          {
               device_->WaitIdle();
               vkDestroySwapchainKHR(device_->Get(), swap_chain_, g_host_alloc);
          }

          ResetFrameWorkLoads();
          //release present queue
          device_->ReleaseQueue(present_queue_);
     }

     void VulkanWindowSystemImpl::OnWindowChange(GLFWwindow* window)
     {
          //first create a surface
          window_ = window;
          ResetFrameWorkLoads();
          if (VK_NULL_HANDLE != surface_)
          {
               vkDestroySurfaceKHR(instance_->Get(), surface_, g_host_alloc);
          }
          if (VK_NULL_HANDLE != swap_chain_)
          {
               vkDestroySwapchainKHR(device_->Get(), nullptr, g_host_alloc);
          }
          auto ret = glfwCreateWindowSurface(instance_->Get(), window, g_host_alloc, &surface_);
          if (VK_SUCCESS != ret)
          {
               const char* error_message = NULL;
               glfwGetError(&error_message);
               throw std::runtime_error(error_message);
          }
     }

     void VulkanWindowSystemImpl::OnWindowResize(uint32_t sizex, uint32_t sizey)
     {
          ResetFrameWorkLoads();
     }

     VulkanWindowSystemImpl::FrameWorkLoad VulkanWindowSystemImpl::GetFrameBufferAsync(VkSemaphore& semaphore)
     {
          uint32_t frame_index;
          acquire_index_ = (acquire_index_ + 1) % acquires_.size();
          semaphore = acquires_[acquire_index_];
          //semaphore and fence must not both be equal to VK_NULL_HANDLE
          vkAcquireNextImageKHR(device_->Get(), swap_chain_, std::numeric_limits<uint64_t>::max(),
                                             semaphore, nullptr, &frame_index);

          auto& work_load = frame_workloads_[frame_index];
          //check whether work_load is being writing
          if (work_load.using_)
          {
               auto ret = vkWaitForFences(device_->Get(), 1, &work_load.fence_, VK_TRUE, std::numeric_limits<uint64_t>::max());
               if (VK_SUCCESS == ret)
               {
                    throw std::runtime_error("wait frame workload too long");
               }
          }

          vkResetFences(device_->Get(), 1, &work_load.fence_);
          work_load.using_ = true;
          return work_load;
          
     }

     void VulkanWindowSystemImpl::SubmitFrameBufferAsync(VkSemaphore semaphore, const uint32_t fbo_index)
     {
          VkPresentInfoKHR present_info{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
          memset(&present_info, 0, sizeof(present_info));
          if (VK_NULL_HANDLE != semaphore)
          {
               present_info.waitSemaphoreCount = 1;
               present_info.pWaitSemaphores = &semaphore;
          }
          present_info.swapchainCount = 1;
          present_info.pSwapchains = &swap_chain_;
          present_info.pImageIndices = &fbo_index;
          present_queue_->Submit(present_info);
     }

     bool VulkanWindowSystemImpl::IsSwapChainImage(const VkImage image) const
     {
          for (auto& frame : frame_workloads_)
          {
               if (frame.image_ == image)
               {
                    return true;
               }
          }

          return false;
     }

     void VulkanWindowSystemImpl::CreateSwapChain()
     {
     }

     void VulkanWindowSystemImpl::DestroySwapchain()
     {
     }

     void VulkanWindowSystemImpl::InitFrameWorkLoads()
     {
          if (VK_NULL_HANDLE != swap_chain_)
          {
               uint32_t image_count = 0;
               vkGetSwapchainImagesKHR(device_->Get(), swap_chain_, &image_count, nullptr);
               assert(image_count > 0 && "swapchain should has images");
               SmallVector<VkImage> swap_images(image_count);
               vkGetSwapchainImagesKHR(device_->Get(), swap_chain_, &image_count, swap_images.data());

               //other post work
               frame_workloads_.resize(image_count);
               for(auto n = 0; n < image_count; ++n)
               { 
                    VkImageViewCreateInfo view_info{};
                    memset(&view_info, 0, sizeof(view_info));
                    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                    view_info.format = swap_format_;
                    view_info.subresourceRange.levelCount = 1;
                    view_info.subresourceRange.layerCount = 1;
                    view_info.image = swap_images[n];
                    frame_workloads_[n].image_ = swap_images[n];
                    vkCreateImageView(device_->Get(), &view_info, g_host_alloc, &(frame_workloads_[n].image_view_));
                    VkFenceCreateInfo fence_info{};
                    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                    fence_info.pNext = VK_NULL_HANDLE;
                    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                    vkCreateFence(device_->Get(), &fence_info, g_host_alloc, &(frame_workloads_[n].fence_));
               }

               acquires_.resize(image_count);
               for (auto& acquire : acquires_)
               {
                    VkSemaphoreCreateInfo semp_info{};
                    memset(&semp_info, 0, sizeof(semp_info));
                    //flags is reserved for future use.
                    semp_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                    vkCreateSemaphore(device_->Get(), &semp_info, g_host_alloc, &acquire);
               }
          }
     }
     void VulkanWindowSystemImpl::ResetFrameWorkLoads()
     {
          device_->WaitIdle();
          for (auto& frame : frame_workloads_)
          {
               if (frame.image_ != VK_NULL_HANDLE)
               {
                    vkDestroyImage(device_->Get(), frame.image_, g_host_alloc);
               }

               if (frame.image_view_ != VK_NULL_HANDLE)
               {
                    vkDestroyImageView(device_->Get(), frame.image_view_, g_host_alloc);
               }

               if (frame.fence_ != VK_NULL_HANDLE)
               {
                    vkDestroyFence(device_->Get(), frame.fence_, g_host_alloc);
               }
          }

          frame_workloads_.clear();

          for (auto& acquire : acquires_)
          {
               if (acquire != VK_NULL_HANDLE)
               {
                    vkDestroySemaphore(device_->Get(), acquire, g_host_alloc);
               }
          }
          acquires_.clear();
     }
}