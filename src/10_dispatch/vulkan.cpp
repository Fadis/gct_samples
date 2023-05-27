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
#include <gct/pipeline_layout.hpp>
#include <gct/buffer_view_create_info.hpp>
#include <gct/submit_info.hpp>
#include <gct/shader_module_create_info.hpp>
#include <gct/shader_module.hpp>
#include <gct/compute_pipeline_create_info.hpp>
#include <gct/compute_pipeline.hpp>
#include <gct/write_descriptor_set.hpp>
#include <vulkan/vulkan.hpp>

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

  const auto descriptor_pool = device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          .setMaxSets( 10 )
      )
      .set_descriptor_pool_size( vk::DescriptorType::eStorageBuffer, 5 )
      .rebuild_chain()
  );
  
  const auto shader = device->get_shader_module(
    "../shaders/add.comp.spv"
  );
  
  const auto descriptor_set_layout = device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding(
        shader->get_props().get_reflection()
      )
      .rebuild_chain()
  );

  const auto descriptor_set = descriptor_pool->allocate( descriptor_set_layout );

  const auto pipeline_layout = device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      .add_descriptor_set_layout( descriptor_set_layout )
      .add_push_constant_range(
        vk::PushConstantRange()
          .setStageFlags( vk::ShaderStageFlagBits::eCompute )
          .setOffset( 0 )
          .setSize( 32u )
      )
  );

  const auto pipeline_cache = device->get_pipeline_cache();

  const auto pipeline = pipeline_cache->get_pipeline(
    gct::compute_pipeline_create_info_t()
      .set_stage(
        gct::pipeline_shader_stage_create_info_t()
          .set_shader_module( shader )
          .set_specialization_info(
            gct::specialization_info_t< spec_t >()
              .set_data(
                spec_t{ 8, 4, 1.0f }
              )
              .add_map< std::uint32_t >( 1, offsetof( spec_t, local_x_size ) )
              .add_map< std::uint32_t >( 2, offsetof( spec_t, local_y_size ) )
              .add_map< std::uint32_t >( 3, offsetof( spec_t, value ) )
          )
      )
      .set_layout( pipeline_layout )
  );
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
 
  const auto &name_to_binding = descriptor_set->get_name_to_binding();


  descriptor_set->update(
    {
      gct::write_descriptor_set_t()
        .set_basic(
          (*descriptor_set)[ "layout1" ]
        )
        .add_buffer(
          gct::descriptor_buffer_info_t()
            .set_buffer( device_local_buffer )
            .set_basic(
              vk::DescriptorBufferInfo()
                .setOffset( 0 )
                .setRange( 1024 )
            )
        )
    }
  );

  {
    auto mapped = staging_buffer->map< float >();
    std::fill( mapped.begin(), mapped.end(), 0.f );
  }
  
  VkCommandPoolCreateInfo command_pool_create_info;
  command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_create_info.pNext = nullptr;
  command_pool_create_info.flags = 0u;
  command_pool_create_info.queueFamilyIndex = queue_family_index;
  VkCommandPool command_pool;
  if( vkCreateCommandPool(
    **device,
    &command_pool_create_info,
    nullptr,
    &command_pool
  ) != VK_SUCCESS ) abort();

  VkCommandBufferAllocateInfo command_buffer_allocate_info;
  command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_allocate_info.pNext = nullptr;
  command_buffer_allocate_info.commandPool = command_pool;
  command_buffer_allocate_info.level =
    VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_allocate_info.commandBufferCount = 1u;
  VkCommandBuffer command_buffer;
  if( vkAllocateCommandBuffers(
    **device,
    &command_buffer_allocate_info,
    &command_buffer
  ) != VK_SUCCESS ) abort();
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

  {
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
  }

  {
    // デバイスのバッファに対する以下の条件を満たすコマンドが完了するまで
    // 以降に現れる以下の条件を満たすコマンドを開始してはならない
    VkBufferMemoryBarrier buffer_memory_barrier;
    buffer_memory_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_memory_barrier.pNext = nullptr;
    // 下のバッファに転送で書くコマンドが完了するまで
    buffer_memory_barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
    // 下のバッファからシェーダで読むコマンドを開始してはならない
    buffer_memory_barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;
    // 現在このキューが所有権を握っている以下のバッファを
    buffer_memory_barrier.srcQueueFamilyIndex = queue_family_index;
    // 以下のキューに渡す(同じ値なので所有権は移動しない)
    buffer_memory_barrier.dstQueueFamilyIndex = queue_family_index;
    // このバッファの
    buffer_memory_barrier.buffer = **device_local_buffer;
    // 先頭から
    buffer_memory_barrier.offset = 0u;
    // 1024バイトの範囲
    buffer_memory_barrier.size = 1024u;
    vkCmdPipelineBarrier(
      command_buffer,
      // 上のバッファを操作するコマンドの転送が完了するまで
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
      // 上のバッファを操作するコマンドのコンピュートシェーダを開始してはならない
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      0u,
      0u,
      nullptr,
      1u,
      &buffer_memory_barrier,
      0u,
      nullptr
    );
  }
  // 以降のパイプラインの実行ではこのデスクリプタセットを使う
  VkDescriptorSet raw_descriptor_set = **descriptor_set;
  vkCmdBindDescriptorSets(
    command_buffer,
    // コンピュートパイプラインの実行に使うデスクリプタセットを
    VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
    **pipeline_layout,
    0u,
    1u,
    // これにする
    &raw_descriptor_set,
    0u,
    nullptr
  );

  // 以降のパイプラインの実行ではこのパイプラインを使う
  vkCmdBindPipeline(
    command_buffer,
    // コンピュートパイプラインを
    VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
    // これにする
    **pipeline
  );

  // コンピュートパイプラインを実行する
  vkCmdDispatch(
    command_buffer,
    4, 2, 1
  );

  {
    // デバイスのバッファに対する以下の条件を満たすコマンドが完了するまで
    // 以降に現れる以下の条件を満たすコマンドを開始してはならない
    VkBufferMemoryBarrier buffer_memory_barrier;
    buffer_memory_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_memory_barrier.pNext = nullptr;
    // 下のバッファにシェーダで書くコマンドが完了するまで
    buffer_memory_barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT;
    // 下のバッファから転送で読むコマンドを開始してはならない
    buffer_memory_barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
    // 現在このキューが所有権を握っている以下のバッファを
    buffer_memory_barrier.srcQueueFamilyIndex = queue_family_index;
    // 以下のキューに渡す(同じ値なので所有権は移動しない)
    buffer_memory_barrier.dstQueueFamilyIndex = queue_family_index;
    // このバッファの
    buffer_memory_barrier.buffer = **device_local_buffer;
    // 先頭から
    buffer_memory_barrier.offset = 0u;
    // 1024バイトの範囲
    buffer_memory_barrier.size = 1024u;
    vkCmdPipelineBarrier(
      command_buffer,
      // 上のバッファを操作するコマンドのコンピュートシェーダが完了するまで
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      // 上のバッファを操作するコマンドの転送を開始してはならない
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
      0u,
      0u,
      nullptr,
      1u,
      &buffer_memory_barrier,
      0u,
      nullptr
    );
  }

  {
    // デバイスのバッファからステージングバッファへコピー
    VkBufferCopy region;
    // 先頭から
    region.srcOffset = 0u;
    region.dstOffset = 0u;
    // このサイズの範囲を
    region.size = 1024u;
    vkCmdCopyBuffer(
      command_buffer,
      **device_local_buffer,
      **staging_buffer,
      1u,
      &region
    );
  }

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

  std::vector< float > host;
  host.reserve( 1024 );
  {
    auto mapped = staging_buffer->map< float >();
    std::copy( mapped.begin(), mapped.end(), std::back_inserter( host ) );
  }
  unsigned int count;
  nlohmann::json json = host;
  std::cout << json.dump( 2 ) << std::endl;
}

