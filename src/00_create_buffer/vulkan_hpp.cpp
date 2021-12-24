#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <vulkan/vulkan.hpp>

struct fb_resources_t {
  std::shared_ptr< gct::framebuffer_t > framebuffer;
  std::shared_ptr< gct::semaphore_t > image_acquired;
  std::shared_ptr< gct::semaphore_t > draw_complete;
  std::shared_ptr< gct::semaphore_t > image_ownership;
  std::shared_ptr< gct::fence_t > fence;
};

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
  
  // ステージングバッファを作る
  VmaAllocationCreateInfo staging_buffer_alloc_info = {};
  // CPUからGPUへの転送に適したバッファが欲しい
  staging_buffer_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
  VkBufferCreateInfo buffer_create_info =
    vk::BufferCreateInfo()
      // 1024バイトの
      .setSize( 1024 )
      // コピー元、コピー先にできる
      .setUsage(
        vk::BufferUsageFlagBits::eTransferSrc|
        vk::BufferUsageFlagBits::eTransferDst
      );
  VkBuffer staging_buffer;
  VmaAllocation staging_buffer_allocation;
  if( vmaCreateBuffer(
    allocator,
    &buffer_create_info,
    &staging_buffer_alloc_info,
    &staging_buffer,
    &staging_buffer_allocation,
    nullptr
  ) != VK_SUCCESS ) abort();

  // ステージングバッファをプロセスのアドレス空間にマップする
  void* mapped_memory = nullptr;
  if( vmaMapMemory(
    allocator, staging_buffer_allocation, &mapped_memory
  ) != VK_SUCCESS ) abort();
  const auto begin = reinterpret_cast< float* >( mapped_memory );
  const auto end = std::next( begin, 1024 / sizeof( float ) );
  std::fill( begin, end, 0.f );
  
  // ステージングバッファをプロセスのアドレス空間から外す
  vmaUnmapMemory( allocator, staging_buffer_allocation );

  // ステージングバッファを捨てる
  vmaDestroyBuffer( allocator, staging_buffer, staging_buffer_allocation );
  
  // アロケータを捨てる
  vmaDestroyAllocator( allocator );
}

