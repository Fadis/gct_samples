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

  const auto physical_device =
    instance->get_physical_devices( {} )[ 0 ].devices[ 0 ];

  const auto queue_props = (*physical_device)->getQueueFamilyProperties();
  uint32_t queue_family_index = 0u;
  for( uint32_t i = 0; i < queue_props.size(); ++i ) {
    if( queue_props[ i ].queueFlags & vk::QueueFlagBits::eCompute ) {
      queue_family_index = i;
      break;
    }
  }

  const float priority = 0.0f;
  std::vector< vk::DeviceQueueCreateInfo > queues{
    vk::DeviceQueueCreateInfo()
      .setQueueFamilyIndex( queue_family_index )
      .setQueueCount( 1 )
      .setPQueuePriorities( &priority )
  };

  std::vector< const char* > extension{
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME
  };
 
  auto device = (*physical_device)->createDeviceUnique(
    vk::DeviceCreateInfo()
      .setQueueCreateInfoCount( queues.size() )
      .setPQueueCreateInfos( queues.data() )
      .setEnabledExtensionCount( extension.size() )
      .setPpEnabledExtensionNames( extension.data() )
  );

  // イメージを作る
  auto image = device->createImageUnique(
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

  // イメージに必要なメモリの要件を調べる
  auto image_memory_reqs = device->getImageMemoryRequirements( *image );

  // GPUのみから読めるメモリを探す
  const auto memory_props = (*physical_device)->getMemoryProperties();
  std::uint32_t device_heap_index = 0u;
  for( std::uint32_t i = 0u; i != memory_props.memoryHeapCount; ++i ) {
    if( memory_props.memoryHeaps[ i ].flags & vk::MemoryHeapFlagBits::eDeviceLocal ) {
      device_heap_index = i;
      break;
    }
  }
  std::uint32_t device_memory_index = 0u;
  for( std::uint32_t i = 0u; i != memory_props.memoryTypeCount; ++i ) {
    if( !( memory_props.memoryTypes[ i ].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible ) ) {
      if( ( image_memory_reqs.memoryTypeBits >> i ) & 0x1 ) {
        if( memory_props.memoryTypes[ i ].heapIndex == device_heap_index ) {
          device_memory_index = i;
          break;
        }
      }
    }
  }
  
  // イメージ用のメモリを確保する
  auto device_memory = device->allocateMemoryUnique(
    vk::MemoryAllocateInfo()
      .setAllocationSize( image_memory_reqs.size )
      .setMemoryTypeIndex( device_memory_index )
  );

  // メモリをイメージに結びつける
  device->bindImageMemory(
    *image,
    *device_memory,
    0u
  );
  
  
}

