#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/buffer.hpp>
#include <gct/device_create_info.hpp>
#include <gct/image_create_info.hpp>
#include <gct/swapchain.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/pipeline_cache.hpp>
#include <gct/pipeline_layout_create_info.hpp>
#include <gct/buffer_view_create_info.hpp>
#include <gct/semaphore_create_info.hpp>
#include <gct/semaphore.hpp>
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
  
  const auto command_buffer0 = queue->get_command_pool()->allocate();

  {
    auto rec = command_buffer0->begin();
    
    rec.copy( staging_buffer, device_local_buffer );
    
    rec.barrier(
      vk::AccessFlagBits::eTransferWrite,
      vk::AccessFlagBits::eShaderRead,
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eComputeShader,
      vk::DependencyFlagBits( 0 ),
      { device_local_buffer },
      {}
    );
    
    rec.bind_descriptor_set(
      vk::PipelineBindPoint::eCompute,
      pipeline_layout,
      descriptor_set
    );
    
    rec.bind_pipeline(
      pipeline
    );
    
    rec->dispatch( 4, 2, 1 );
  }

  const auto command_buffer1 = queue->get_command_pool()->allocate();

  {
    auto rec = command_buffer1->begin();
    
    rec.bind_descriptor_set(
      vk::PipelineBindPoint::eCompute,
      pipeline_layout,
      descriptor_set
    );
    
    rec.bind_pipeline(
      pipeline
    );
    
    rec->dispatch( 4, 2, 1 );
  }
 
  const auto command_buffer2 = queue->get_command_pool()->allocate();

  {
    auto rec = command_buffer2->begin();
    
    rec.bind_descriptor_set(
      vk::PipelineBindPoint::eCompute,
      pipeline_layout,
      descriptor_set
    );
    
    rec.bind_pipeline(
      pipeline
    );
    
    rec->dispatch( 4, 2, 1 );

    rec.barrier(
      vk::AccessFlagBits::eShaderWrite,
      vk::AccessFlagBits::eTransferRead,
      vk::PipelineStageFlagBits::eComputeShader,
      vk::PipelineStageFlagBits::eTransfer,
      vk::DependencyFlagBits( 0 ),
      { device_local_buffer },
      {}
    );
    
    rec.copy( device_local_buffer, staging_buffer );
  }


  // タイムラインセマフォを作る
  const auto semaphore = device->get_semaphore(
    gct::semaphore_create_info_t()
      .set_type(
        vk::SemaphoreTypeCreateInfo()
          .setSemaphoreType( vk::SemaphoreType::eTimeline )
      )
  );

  uint64_t current_semaphore_value = 0u;
  uint64_t next_semaphore_value = 1u;
  // タイムラインセマフォの値が0になったら開始
  // 完了したらタイムラインセマフォの値を1にする
  command_buffer0->execute(
    gct::submit_info_t()
      .add_wait_for( semaphore, vk::PipelineStageFlagBits::eComputeShader )
      .add_signal_to( semaphore )
      .set_timeline_semaphore(
        vk::TimelineSemaphoreSubmitInfo()
          .setWaitSemaphoreValueCount( 1 )
          .setPWaitSemaphoreValues( &current_semaphore_value )
          .setSignalSemaphoreValueCount( 1 )
          .setPSignalSemaphoreValues( &next_semaphore_value )
      )
  );

  ++current_semaphore_value;
  ++next_semaphore_value;
  // タイムラインセマフォの値が1になったら開始
  // 完了したらタイムラインセマフォの値を2にする
  command_buffer1->execute(
    gct::submit_info_t()
      .add_wait_for( semaphore, vk::PipelineStageFlagBits::eComputeShader )
      .add_signal_to( semaphore )
      .set_timeline_semaphore(
        vk::TimelineSemaphoreSubmitInfo()
          .setWaitSemaphoreValueCount( 1 )
          .setPWaitSemaphoreValues( &current_semaphore_value )
          .setSignalSemaphoreValueCount( 1 )
          .setPSignalSemaphoreValues( &next_semaphore_value )
      )
  );
  
  ++current_semaphore_value;
  ++next_semaphore_value;
  // タイムラインセマフォの値が2になったら開始
  // 完了したらタイムラインセマフォの値を3にする
  command_buffer2->execute(
    gct::submit_info_t()
      .add_wait_for( semaphore, vk::PipelineStageFlagBits::eComputeShader )
      .add_signal_to( semaphore )
      .set_timeline_semaphore(
        vk::TimelineSemaphoreSubmitInfo()
          .setWaitSemaphoreValueCount( 1 )
          .setPWaitSemaphoreValues( &current_semaphore_value )
          .setSignalSemaphoreValueCount( 1 )
          .setPSignalSemaphoreValues( &next_semaphore_value )
      )
  );

  // タイムラインセマフォの値が3になるまで待機
  auto result = (*device)->waitSemaphores(
    vk::SemaphoreWaitInfo()
      .setSemaphoreCount( 1 )
      .setPSemaphores( &**semaphore )
      .setPValues( &next_semaphore_value ),
    UINT64_MAX
  );
  if( result != vk::Result::eSuccess ) abort();

  command_buffer0->wait_for_executed();
  command_buffer1->wait_for_executed();
  command_buffer2->wait_for_executed();
  

  std::vector< float > host;
  host.reserve( 1024 );
  {
    auto mapped = staging_buffer->map< float >();
    std::copy( mapped.begin(), mapped.end(), std::back_inserter( host ) );
  }
  
  nlohmann::json json = host;
  std::cout << json.dump( 2 ) << std::endl;
}

