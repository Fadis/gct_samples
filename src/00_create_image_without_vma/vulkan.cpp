#include <iostream>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/write_descriptor_set.hpp>
#include <gct/shader_module.hpp>
#include <vulkan/vulkan.h>

struct spec_t {
  std::uint32_t local_x_size = 0u;
  std::uint32_t local_y_size = 0u;
  float value = 0.f;
};

int main() {
  uint32_t iext_count = 0u;
  std::vector< const char* > iext{};
  const std::shared_ptr< gct::instance_t > instance(
    new gct::instance_t(
      gct::instance_create_info_t()
        .set_application_info(
          vk::ApplicationInfo()
            .setPApplicationName( "my_application" )
            .setApplicationVersion(  VK_MAKE_VERSION( 1, 0, 0 ) )
            .setApiVersion( VK_MAKE_VERSION( 1, 2, 0 ) )
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

  uint32_t queue_props_count = 0u;
  vkGetPhysicalDeviceQueueFamilyProperties( **physical_device, &queue_props_count, nullptr );
  std::vector< VkQueueFamilyProperties > queue_props( queue_props_count );
  vkGetPhysicalDeviceQueueFamilyProperties( **physical_device, &queue_props_count, queue_props.data() );
  uint32_t queue_family_index = 0u;
  for( uint32_t i = 0; i < queue_props.size(); ++i ) {
    if( queue_props[ i ].queueFlags & VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT ) {
      queue_family_index = i;
      break;
    }
  }

  const float priority = 0.0f;
  VkDeviceQueueCreateInfo queue_create_info;
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.pNext = nullptr;
  queue_create_info.flags = 0;
  queue_create_info.queueFamilyIndex = queue_family_index;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &priority;

  std::vector< const char* > extension{
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME
  };

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

  VkImageCreateInfo image_create_info;

  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.pNext = nullptr;
  image_create_info.flags = 0u;
  // 2次元で
  image_create_info.imageType = VkImageType::VK_IMAGE_TYPE_2D;
  // RGBA各8bitで
  image_create_info.format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
  // 1024x1024の
  image_create_info.extent.width = 1024;
  image_create_info.extent.height = 1024;
  image_create_info.extent.depth = 1;
  // ミップマップは無く
  image_create_info.mipLevels = 1u;
  // レイヤーは1枚だけの
  image_create_info.arrayLayers = 1u;
  // 1テクセルにつきサンプリング点を1つだけ持つ
  image_create_info.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
  // GPUが読みやすいように配置された
  image_create_info.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
  // 転送先とストレージイメージに使う
  image_create_info.usage =
    VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT |
    VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
  // 同時に複数のキューから操作しない
  image_create_info.sharingMode = 
  image_create_info.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.queueFamilyIndexCount = 0;
  image_create_info.pQueueFamilyIndices = nullptr;
  // 初期状態は不定な
  image_create_info.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
  VkImage image;

  if( vkCreateImage(
    device,
    &image_create_info,
    nullptr,
     &image
  ) != VK_SUCCESS ) abort();

  // イメージに必要なメモリの要件を調べる
  VkMemoryRequirements image_memory_reqs;
  vkGetImageMemoryRequirements(
    device,
    image,
    &image_memory_reqs
  );

  VkPhysicalDeviceMemoryProperties memory_props;
  vkGetPhysicalDeviceMemoryProperties( **physical_device, &memory_props );
  // GPUのみから読めるメモリを探す
  std::uint32_t device_heap_index = 0u;
  for( std::uint32_t i = 0u; i != memory_props.memoryHeapCount; ++i ) {
    if( memory_props.memoryHeaps[ i ].flags & VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
      device_heap_index = i;
      break;
    }
  }
  std::uint32_t device_memory_index = 0u;
  for( std::uint32_t i = 0u; i != memory_props.memoryTypeCount; ++i ) {
    if( ( image_memory_reqs.memoryTypeBits >> i ) & 0x1 ) {
      if( !( memory_props.memoryTypes[ i ].propertyFlags & VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) ) {
        if( memory_props.memoryTypes[ i ].heapIndex == device_heap_index ) {
          device_memory_index = i;
          break;
        }
      }
    }
  }
  
  VkMemoryAllocateInfo device_memory_allocate_info;
  device_memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  device_memory_allocate_info.pNext = nullptr;
  device_memory_allocate_info.allocationSize = image_memory_reqs.size;
  device_memory_allocate_info.memoryTypeIndex = device_memory_index;
  VkDeviceMemory device_memory;
  if( vkAllocateMemory(
    device,
    &device_memory_allocate_info,
    nullptr,
    &device_memory
  ) != VK_SUCCESS ) abort();

  if( vkBindImageMemory(
    device,
    image,
    device_memory,
    0u
  ) != VK_SUCCESS ) abort();

  vkDestroyImage(
    device,
    image,
    nullptr
  );

  vkFreeMemory(
    device,
    device_memory,
    nullptr
  );
}

