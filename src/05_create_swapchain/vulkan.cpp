#include <iostream>
#include <gct/glfw.hpp>
#include <gct/instance.hpp>
#include <gct/device_create_info.hpp>
#include <gct/device.hpp>
#include <vulkan/vulkan.h>

int main() {
  gct::glfw::get();
  uint32_t iext_count = 0u;
  auto exts = glfwGetRequiredInstanceExtensions( &iext_count );
  std::vector< const char* > iext{};
  for( uint32_t i = 0u; i != iext_count; ++i )
    iext.push_back( exts[ i ] );
  std::shared_ptr< gct::instance_t > instance(
    new gct::instance_t(
      gct::instance_create_info_t()
        .set_application_info(
          vk::ApplicationInfo()
            .setPApplicationName( "my_application" )
            .setApplicationVersion(  VK_MAKE_VERSION( 1, 0, 0 ) )
            .setApiVersion( VK_API_VERSION_1_2 )
        )
        .add_layer(
          "VK_LAYER_KHRONOS_validation"
        )
        .add_extension(
          iext.begin(), iext.end()
        )
    )
  );
  const auto physical_device =
    instance->get_physical_devices( {} )[ 0 ].devices[ 0 ];
  const unsigned int width = 640;
  const unsigned int height = 480;
  gct::glfw_window window(
    width,
    height,
    "my_window",
    false
  );
  auto surface = window.get_surface( *physical_device );
  auto &surface_capabilities = surface->get_caps().get_basic();


  // デバイスに備わっているキューを取得
  uint32_t queue_props_count = 0u;
  vkGetPhysicalDeviceQueueFamilyProperties( **physical_device, &queue_props_count, nullptr );
  std::vector< VkQueueFamilyProperties > queue_props( queue_props_count );
  vkGetPhysicalDeviceQueueFamilyProperties( **physical_device, &queue_props_count, queue_props.data() );
  
  // 描画コマンドを流せてsurfaceに書けるキューを探す
  uint32_t queue_family_index = 0u;
  for( uint32_t i = 0; i < queue_props.size(); ++i ) {
    if( queue_props[ i ].queueFlags & VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT ) {
      if( (*physical_device)->getSurfaceSupportKHR( i, **surface ) ) {
        queue_family_index = i;
        break;
      }
    }
  }

  const float priority = 0.0f;
  // 描画コマンドを流せてsurfaceに書けるキューを1つください
  VkDeviceQueueCreateInfo queue_create_info;
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.pNext = nullptr;
  queue_create_info.flags = 0;
  queue_create_info.queueFamilyIndex = queue_family_index;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &priority;

  // 拡張VK_EXT_swapchainを使う
  std::vector< const char* > extension{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

  // 論理デバイスを作る
  VkDeviceCreateInfo device_create_info;
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = nullptr;
  device_create_info.flags = 0;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledLayerCount = 0;
  device_create_info.ppEnabledLayerNames = nullptr;
  device_create_info.enabledExtensionCount = extension.size();
  device_create_info.ppEnabledExtensionNames = extension.data();
  device_create_info.pEnabledFeatures = nullptr;
  VkDevice device;
  if( vkCreateDevice(
    **physical_device,
    &device_create_info,
    nullptr,
    &device
  ) != VK_SUCCESS ) abort();
 
  // サーフェスのイメージとして使えるフォーマットを取得する
  std::uint32_t surface_formats_count = 0u;
  if( vkGetPhysicalDeviceSurfaceFormatsKHR(
    **physical_device,
    **surface,
    &surface_formats_count,
    nullptr
  ) != VK_SUCCESS ) abort();
  std::vector< VkSurfaceFormatKHR > surface_formats( surface_formats_count );
  if( vkGetPhysicalDeviceSurfaceFormatsKHR(
    **physical_device,
    **surface,
    &surface_formats_count,
    surface_formats.data()
  ) != VK_SUCCESS ) abort();
  const auto &surface_format = surface_formats[ 0 ];

  // サーフェスがサポートする範囲でスワップチェーンのイメージの数を決める
  const std::uint32_t image_count =
    std::max(
      std::min(
        2u,
        surface_capabilities.maxImageCount
      ),
      surface_capabilities.minImageCount
    );

  // スワップチェーンを作る
  VkSwapchainCreateInfoKHR swapchain_create_info;
  swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_create_info.pNext = nullptr;
  swapchain_create_info.flags = 0u;
  swapchain_create_info.surface = **surface;
  swapchain_create_info.minImageCount = image_count;
  swapchain_create_info.imageFormat = surface_format.format;
  swapchain_create_info.imageColorSpace = surface_format.colorSpace;
  swapchain_create_info.imageExtent = surface_capabilities.currentExtent;
  swapchain_create_info.imageArrayLayers = 1u;
  swapchain_create_info.imageUsage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_create_info.imageSharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
  swapchain_create_info.queueFamilyIndexCount = 0u;
  swapchain_create_info.pQueueFamilyIndices = nullptr;
  swapchain_create_info.preTransform = VkSurfaceTransformFlagBitsKHR::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  swapchain_create_info.compositeAlpha = VkCompositeAlphaFlagBitsKHR::VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_create_info.presentMode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
  swapchain_create_info.clipped = true;
  swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain;
  if( vkCreateSwapchainKHR(
    device,
    &swapchain_create_info,
    nullptr,
    &swapchain
  ) != VK_SUCCESS ) abort();

  // スワップチェーンを構成するイメージのvectorを取得
  std::uint32_t swapchain_image_count = 0u;
  if( vkGetSwapchainImagesKHR(
    device,
    swapchain,
    &swapchain_image_count,
    nullptr
  ) != VK_SUCCESS ) abort();
  std::vector< VkImage > swapchain_images( swapchain_image_count );
  if( vkGetSwapchainImagesKHR(
    device,
    swapchain,
    &swapchain_image_count,
    swapchain_images.data()
  ) != VK_SUCCESS ) abort();
  std::cout << "swapchain images : " << swapchain_images.size() << std::endl;
  
  vkDestroySwapchainKHR(
    device,
    swapchain,
    nullptr
  );
  
  // 論理デバイスを捨てる
  vkDestroyDevice(
    device,
    nullptr
  );

}

