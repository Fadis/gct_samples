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
  const auto command_pool = (*device)->createCommandPoolUnique(
    vk::CommandPoolCreateInfo()
      .setQueueFamilyIndex( queue_family_index )
  );
  auto command_buffers = (*device)->allocateCommandBuffersUnique(
    vk::CommandBufferAllocateInfo()
      .setCommandPool( *command_pool )
      .setLevel( vk::CommandBufferLevel::ePrimary )
      .setCommandBufferCount( 1u )
  );
  const auto command_buffer = std::move( command_buffers[ 0 ] );
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

  // デバイスのバッファに対する以下の条件を満たすコマンドが完了するまで
  // 以降に現れる以下の条件を満たすコマンドを開始してはならない
  command_buffer->pipelineBarrier(
    // 下のバッファを操作するコマンドの転送が完了するまで
    vk::PipelineStageFlagBits::eTransfer,
    // 下のバッファを操作するコマンドのコンピュートシェーダを開始してはならない
    vk::PipelineStageFlagBits::eComputeShader,
    vk::DependencyFlagBits( 0 ),
    {},
    {
      vk::BufferMemoryBarrier()
        // 下のバッファに転送で書くコマンドが完了するまで
        .setSrcAccessMask(
          vk::AccessFlagBits::eTransferWrite
        )
        // 下のバッファからシェーダで読むコマンドを開始してはならない
        .setDstAccessMask(
          vk::AccessFlagBits::eShaderRead
        )
        // 現在このキューが所有権を握っている以下のバッファを
        .setSrcQueueFamilyIndex( queue_family_index )
        // 以下のキューに渡す(同じ値なので所有権は移動しない)
        .setDstQueueFamilyIndex( queue_family_index )
        // このバッファの
        .setBuffer( **device_local_buffer )
        // 先頭から
        .setOffset( 0 )
        // 1024バイトの範囲
        .setSize( 1024u )
    },
    {}
  );
  // 以降のパイプラインの実行ではこのデスクリプタセットを使う
  command_buffer->bindDescriptorSets(
    // コンピュートパイプラインの実行に使うデスクリプタセットを
    vk::PipelineBindPoint::eCompute,
    **pipeline_layout,
    0u,
    // これにする
    { **descriptor_set },
    {}
  );

  // 以降のパイプラインの実行ではこのパイプラインを使う
  command_buffer->bindPipeline(
    // コンピュートパイプラインを
    vk::PipelineBindPoint::eCompute,
    // これにする
    **pipeline
  );

  // コンピュートパイプラインを実行する
  command_buffer->dispatch( 4, 2, 1 );

  // デバイスのバッファに対する以下の条件を満たすコマンドが完了するまで
  // 以降に現れる以下の条件を満たすコマンドを開始してはならない
  command_buffer->pipelineBarrier(
    // 下のバッファを操作するコマンドのコンピュートシェーダが完了するまで
    vk::PipelineStageFlagBits::eComputeShader,
    // 下のバッファから転送で読み出すコマンドを開始してはならない
    vk::PipelineStageFlagBits::eTransfer,
    vk::DependencyFlagBits( 0 ),
    {},
    {
      vk::BufferMemoryBarrier()
        // 下のバッファにシェーダで書くコマンドが完了するまで
        .setSrcAccessMask(
          vk::AccessFlagBits::eShaderWrite
        )
        // 下のバッファから転送で読むコマンドを開始してはならない
        .setDstAccessMask(
          vk::AccessFlagBits::eTransferRead
        )
        // 現在このキューが所有権を握っている以下のバッファを
        .setSrcQueueFamilyIndex( queue_family_index )
        // 以下のキューに渡す(同じ値なので所有権は移動しない)
        .setDstQueueFamilyIndex( queue_family_index )
        // このバッファの
        .setBuffer( **device_local_buffer )
        // 先頭から
        .setOffset( 0 )
        // 1024バイトの範囲
        .setSize( 1024u )
    },
    {}
  );

  // デバイスのバッファからステージングバッファにコピー
  command_buffer->copyBuffer(
    // このバッファから
    **device_local_buffer,
    // このバッファへ
    **staging_buffer,
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

