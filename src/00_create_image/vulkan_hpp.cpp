#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <vulkan/vulkan.hpp>

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
  auto groups = instance->get_physical_devices( {} );
  auto selected = groups[ 0 ].with_extensions( {
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME
  } );
  auto physical_device = selected.devices[ 0 ];
 
  const auto device = selected.create_device(
    std::vector< gct::queue_requirement_t >{
      gct::queue_requirement_t{
        vk::QueueFlagBits::eCompute,
        0u,
        vk::Extent3D(),
#ifdef VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME
        vk::QueueGlobalPriorityEXT(),
#endif
        {},
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer
      }
    },
    gct::device_create_info_t()
  );
  const auto queue = device->get_queue( 0u );
  const auto command_buffer = queue->get_command_pool()->allocate();

  // アロケータを作る
  VmaAllocatorCreateInfo allocator_create_info;
  allocator_create_info.instance = **instance;
  allocator_create_info.physicalDevice = **physical_device;
  allocator_create_info.device = **device;
  VmaAllocator allocator;
  if( vmaCreateAllocator(
    &allocator_create_info, &allocator
  ) != VK_SUCCESS ) abort();
  
  // イメージを作る
  VmaAllocationCreateInfo image_alloc_info = {};
  // GPUのみが読めるイメージが欲しい
  image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  VkImageCreateInfo image_create_info = static_cast< VkImageCreateInfo >(
    vk::ImageCreateInfo()
      // 2次元で
      .setImageType( vk::ImageType::e2D )
      // RGBA各8bitで
      .setFormat( vk::Format::eR8G8B8A8Unorm )
      .setExtent( { 1024, 1024, 1 } )
      // ミップマップは無く
      .setMipLevels( 1 )
      // レイヤーは1枚だけの
      .setArrayLayers( 1 )
      // 1テクセルにつきサンプリング点を1つだけ持つ
      .setSamples( vk::SampleCountFlagBits::e1 )
      // GPUが読みやすいように配置された
      .setTiling( vk::ImageTiling::eOptimal )
      // 転送先とストレージイメージに使う
      .setUsage(
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eStorage
      )
      // 同時に複数のキューから操作しない
      .setSharingMode( vk::SharingMode::eExclusive )
      .setQueueFamilyIndexCount( 0 )
      .setPQueueFamilyIndices( nullptr )
      // 初期状態は不定な
      .setInitialLayout( vk::ImageLayout::eUndefined )
  );
  VkImage image;
  VmaAllocation image_allocation;
  if( vmaCreateImage(
    allocator,
    &image_create_info,
    &image_alloc_info,
    &image,
    &image_allocation,
    nullptr
  ) != VK_SUCCESS ) abort();
  
  // イメージを捨てる
  vmaDestroyImage( allocator, image, image_allocation );
  
  // アロケータを捨てる
  vmaDestroyAllocator( allocator );
}

