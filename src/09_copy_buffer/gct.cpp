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
#include <gct/command_buffer.hpp>
#include <gct/command_pool.hpp>

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

  // コマンドバッファを作る
  const auto command_buffer = queue->get_command_pool()->allocate();

  {
    // コマンドバッファにコマンドの記録を開始する
    auto rec = command_buffer->begin();
    // ステージングバッファからデバイスのバッファへコピー
    rec.copy( staging_buffer, device_local_buffer );    
  }

  // コマンドバッファの内容をキューに流す
  command_buffer->execute(
    gct::submit_info_t()
  );

  // コマンドバッファの内容が完了するのを待つ
  command_buffer->wait_for_executed();

}

