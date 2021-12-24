#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/image_create_info.hpp>
#include <gct/swapchain.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/pipeline_cache.hpp>
#include <gct/pipeline_layout_create_info.hpp>
#include <gct/buffer_view_create_info.hpp>
#include <gct/submit_info.hpp>
#include <gct/shader_module_create_info.hpp>
#include <gct/shader_module.hpp>
#include <gct/compute_pipeline_create_info.hpp>
#include <gct/compute_pipeline.hpp>
#include <gct/write_descriptor_set.hpp>

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
  const auto queue_family_index = queue->get_available_queue_family_index();
  const auto allocator = device->get_allocator();
  const auto staging_buffer = allocator->create_buffer(
    gct::buffer_create_info_t()
      .set_basic(
        vk::BufferCreateInfo()
          .setSize( 1024 )
          .setUsage(
            vk::BufferUsageFlagBits::eTransferSrc|
            vk::BufferUsageFlagBits::eTransferDst
          )
      ),
    VMA_MEMORY_USAGE_CPU_TO_GPU
  );
  const auto device_local_buffer = allocator->create_buffer(
    gct::buffer_create_info_t()
      .set_basic(
        vk::BufferCreateInfo()
          .setSize( 1024 )
          .setUsage(
            vk::BufferUsageFlagBits::eStorageBuffer|
            vk::BufferUsageFlagBits::eTransferSrc|
            vk::BufferUsageFlagBits::eTransferDst
          )
      ),
    VMA_MEMORY_USAGE_GPU_ONLY
  );
  {
    auto mapped = staging_buffer->map< float >();
    std::fill( mapped.begin(), mapped.end(), 0.f );
  }

  // コマンドプールを作る
  VkCommandPoolCreateInfo command_pool_create_info;
  command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_create_info.pNext = nullptr;
  command_pool_create_info.flags = 0u;
  // このキューファミリのキューにコマンドを流すためのプールを作る
  command_pool_create_info.queueFamilyIndex = queue_family_index;
  VkCommandPool command_pool;
  if( vkCreateCommandPool(
    **device,
    &command_pool_create_info,
    nullptr,
    &command_pool
  ) != VK_SUCCESS ) abort();

  // コマンドバッファを作る
  VkCommandBufferAllocateInfo command_buffer_allocate_info;
  command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_allocate_info.pNext = nullptr;
  // このコマンドプールから
  command_buffer_allocate_info.commandPool = command_pool;
  // 直接キューに流すためのコマンドバッファを
  command_buffer_allocate_info.level =
    VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  // 1個
  command_buffer_allocate_info.commandBufferCount = 1u;
  VkCommandBuffer command_buffer;
  if( vkAllocateCommandBuffers(
    **device,
    &command_buffer_allocate_info,
    &command_buffer
  ) != VK_SUCCESS ) abort();

  // フェンスを作る
  VkFenceCreateInfo fence_create_info;
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.pNext = nullptr;
  fence_create_info.flags = 0u;
  VkFence fence;
  if( vkCreateFence(
    **device,
    &fence_create_info,
    nullptr,
    &fence
  ) != VK_SUCCESS ) abort();

  // コマンドバッファにコマンドの記録を開始する
  VkCommandBufferBeginInfo command_buffer_begin_info;
  command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_begin_info.pNext = nullptr;
  command_buffer_begin_info.flags = 0u;
  command_buffer_begin_info.pInheritanceInfo = nullptr;
  if( vkBeginCommandBuffer(
    command_buffer,
    &command_buffer_begin_info
  ) != VK_SUCCESS ) abort();

  // ステージングバッファからデバイスのバッファへコピー
  VkBufferCopy region;
  // 先頭から
  region.srcOffset = 0u;
  region.dstOffset = 0u;
  // このサイズの範囲を
  region.size = 1024u;
  vkCmdCopyBuffer(
    command_buffer,
    **staging_buffer,
    **device_local_buffer,
    1u,
    &region
  );

  // コマンドバッファにコマンドの記録を終了する
  if( vkEndCommandBuffer(
    command_buffer
  ) != VK_SUCCESS ) abort();

  // コマンドバッファの内容をキューに流す
  VkSubmitInfo submit_info;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = nullptr;
  submit_info.waitSemaphoreCount = 0u;
  submit_info.pWaitSemaphores = nullptr;
  submit_info.pWaitDstStageMask = nullptr;
  submit_info.commandBufferCount = 1u;
  // このコマンドバッファの内容を流す
  submit_info.pCommandBuffers = &command_buffer;
  submit_info.signalSemaphoreCount = 0u;
  submit_info.pSignalSemaphores = nullptr;
  if( vkQueueSubmit(
    **queue,
    1u,
    &submit_info,
    // 実行し終わったらこのフェンスに通知
    fence
  ) != VK_SUCCESS ) abort();

  // フェンスが完了通知を受けるのを待つ
  if( vkWaitForFences(
    **device,
    1u,
    // このフェンスを待つ
    &fence,
    // 全部のフェンスに完了通知が来るまで待つ
    true,
    // 1秒でタイムアウト
    1 * 1000 * 1000 * 1000
  ) != VK_SUCCESS ) abort();

  // フェンスを捨てる
  vkDestroyFence(
    **device,
    fence,
    nullptr
  );

  // コマンドバッファを捨てる
  vkFreeCommandBuffers(
    **device,
    command_pool,
    1u,
    &command_buffer
  );

  // コマンドプールを捨てる
  vkDestroyCommandPool(
    **device,
    command_pool,
    nullptr
  );

}

