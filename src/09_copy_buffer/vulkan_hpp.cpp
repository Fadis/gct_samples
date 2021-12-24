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
  const auto command_pool = (*device)->createCommandPoolUnique(
    vk::CommandPoolCreateInfo()
      // このキューファミリのキューにコマンドを流すためのプールを作る
      .setQueueFamilyIndex( queue_family_index )
  );

  // コマンドバッファを作る
  auto command_buffers = (*device)->allocateCommandBuffersUnique(
    vk::CommandBufferAllocateInfo()
      // このコマンドプールから
      .setCommandPool( *command_pool )
      // 直接キューに流すためのコマンドバッファを
      .setLevel( vk::CommandBufferLevel::ePrimary )
      // 1個
      .setCommandBufferCount( 1u )
  );
  // 返ってきたコマンドバッファのうち先頭の1個を使う
  const auto command_buffer = std::move( command_buffers[ 0 ] );

  // フェンスを作る
  const auto fence = (*device)->createFenceUnique(
    vk::FenceCreateInfo()
  );

  // コマンドバッファにコマンドの記録を開始する
  command_buffer->begin(
    vk::CommandBufferBeginInfo()
  );

  // ステージングバッファからデバイスのバッファへコピー
  command_buffer->copyBuffer(
    // このバッファから
    **staging_buffer,
    // このバッファへ
    **device_local_buffer,
    {
      vk::BufferCopy()
        // 先頭からこのサイズの範囲を
        .setSize( 1024u )
    }
  );

  // コマンドバッファにコマンドの記録を終了する
  command_buffer->end();

  // コマンドバッファの内容をキューに流す
  (*queue)->submit(
    {
      vk::SubmitInfo()
        .setCommandBufferCount( 1u )
        .setPCommandBuffers( &*command_buffer )
    },
    // 実行し終わったらこのフェンスに通知
    *fence
  );

  // フェンスが完了通知を受けるのを待つ
  if( (*device)->waitForFences(
    {
      // このフェンスを待つ
      *fence
    },
    // 全部のフェンスに完了通知が来るまで待つ
    true,
    // 1秒でタイムアウト
    1*1000*1000*1000
  ) != vk::Result::eSuccess ) abort();

}

