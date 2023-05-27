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
  auto command_buffers0 = (*device)->allocateCommandBuffersUnique(
    vk::CommandBufferAllocateInfo()
      .setCommandPool( *command_pool )
      .setLevel( vk::CommandBufferLevel::ePrimary )
      .setCommandBufferCount( 1u )
  );
  const auto command_buffer0 = std::move( command_buffers0[ 0 ] );
  
  auto command_buffers1 = (*device)->allocateCommandBuffersUnique(
    vk::CommandBufferAllocateInfo()
      .setCommandPool( *command_pool )
      .setLevel( vk::CommandBufferLevel::ePrimary )
      .setCommandBufferCount( 1u )
  );
  const auto command_buffer1 = std::move( command_buffers1[ 0 ] );
  
  auto command_buffers2 = (*device)->allocateCommandBuffersUnique(
    vk::CommandBufferAllocateInfo()
      .setCommandPool( *command_pool )
      .setLevel( vk::CommandBufferLevel::ePrimary )
      .setCommandBufferCount( 1u )
  );
  const auto command_buffer2 = std::move( command_buffers2[ 0 ] );

  command_buffer0->begin(
    vk::CommandBufferBeginInfo()
  );

  command_buffer0->copyBuffer(
    **staging_buffer,
    **device_local_buffer,
    {
      vk::BufferCopy()
        .setSize( 1024u )
    }
  );

  command_buffer0->pipelineBarrier(
    vk::PipelineStageFlagBits::eTransfer,
    vk::PipelineStageFlagBits::eComputeShader,
    vk::DependencyFlagBits( 0 ),
    {},
    {
      vk::BufferMemoryBarrier()
        .setSrcAccessMask(
          vk::AccessFlagBits::eTransferWrite
        )
        .setDstAccessMask(
          vk::AccessFlagBits::eShaderRead
        )
        .setSrcQueueFamilyIndex( queue_family_index )
        .setDstQueueFamilyIndex( queue_family_index )
        .setBuffer( **device_local_buffer )
        .setOffset( 0 )
        .setSize( 1024u )
    },
    {}
  );
  command_buffer0->bindDescriptorSets(
    vk::PipelineBindPoint::eCompute,
    **pipeline_layout,
    0u,
    { **descriptor_set },
    {}
  );

  command_buffer0->bindPipeline(
    vk::PipelineBindPoint::eCompute,
    **pipeline
  );

  command_buffer0->dispatch( 4, 2, 1 );
  
  command_buffer0->pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader,
    vk::PipelineStageFlagBits::eComputeShader,
    vk::DependencyFlagBits( 0 ),
    {},
    {
      vk::BufferMemoryBarrier()
        .setSrcAccessMask(
          vk::AccessFlagBits::eShaderRead
        )
        .setDstAccessMask(
          vk::AccessFlagBits::eShaderRead
        )
        .setSrcQueueFamilyIndex( queue_family_index )
        .setDstQueueFamilyIndex( queue_family_index )
        .setBuffer( **device_local_buffer )
        .setOffset( 0 )
        .setSize( 1024u )
    },
    {}
  );
  
  command_buffer0->end();

  command_buffer1->begin(
    vk::CommandBufferBeginInfo()
  );
  
  command_buffer1->bindDescriptorSets(
    vk::PipelineBindPoint::eCompute,
    **pipeline_layout,
    0u,
    { **descriptor_set },
    {}
  );

  command_buffer1->bindPipeline(
    vk::PipelineBindPoint::eCompute,
    **pipeline
  );

  command_buffer1->dispatch( 4, 2, 1 );
 
  command_buffer1->end();

  command_buffer2->begin(
    vk::CommandBufferBeginInfo()
  );
  
  command_buffer2->bindDescriptorSets(
    vk::PipelineBindPoint::eCompute,
    **pipeline_layout,
    0u,
    { **descriptor_set },
    {}
  );

  command_buffer2->bindPipeline(
    vk::PipelineBindPoint::eCompute,
    **pipeline
  );

  command_buffer2->dispatch( 4, 2, 1 );
 
  command_buffer2->pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader,
    vk::PipelineStageFlagBits::eTransfer,
    vk::DependencyFlagBits( 0 ),
    {},
    {
      vk::BufferMemoryBarrier()
        .setSrcAccessMask(
          vk::AccessFlagBits::eShaderRead
        )
        .setDstAccessMask(
          vk::AccessFlagBits::eTransferRead
        )
        .setSrcQueueFamilyIndex( queue_family_index )
        .setDstQueueFamilyIndex( queue_family_index )
        .setBuffer( **device_local_buffer )
        .setOffset( 0 )
        .setSize( 1024u )
    },
    {}
  );

  command_buffer2->copyBuffer(
    **device_local_buffer,
    **staging_buffer,
    {
      vk::BufferCopy()
        .setSize( 1024u )
    }
  );

  command_buffer2->end();

  VkCommandBuffer raw_command_buffer0 = *command_buffer0;
  VkCommandBuffer raw_command_buffer1 = *command_buffer1;
  VkCommandBuffer raw_command_buffer2 = *command_buffer2;

  // タイムラインセマフォを作る
  VkSemaphoreTypeCreateInfo semaphore_type;
  semaphore_type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  semaphore_type.pNext = nullptr;
  semaphore_type.semaphoreType = VkSemaphoreType::VK_SEMAPHORE_TYPE_TIMELINE;
  VkSemaphoreCreateInfo semaphore_create_info;
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_create_info.pNext = &semaphore_type;
  semaphore_create_info.flags = 0u;
  VkSemaphore semaphore;
  if( vkCreateSemaphore(
    **device,
    &semaphore_create_info,
    nullptr,
    &semaphore
  ) != VK_SUCCESS ) abort();

  uint64_t current_semaphore_value = 0u;
  uint64_t next_semaphore_value = 1u;

  VkTimelineSemaphoreSubmitInfo timeline_semaphore_state;
  timeline_semaphore_state.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  timeline_semaphore_state.pNext = nullptr;
  timeline_semaphore_state.waitSemaphoreValueCount = 1;
  timeline_semaphore_state.pWaitSemaphoreValues = &current_semaphore_value;
  timeline_semaphore_state.signalSemaphoreValueCount = 1;
  timeline_semaphore_state.pSignalSemaphoreValues = &next_semaphore_value;

  const VkPipelineStageFlags stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; 
  
  // タイムラインセマフォの値が0になったら開始
  // 完了したらタイムラインセマフォの値を1にする
  {
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = &timeline_semaphore_state;
    submit_info.waitSemaphoreCount = 1u;
    submit_info.pWaitSemaphores = &semaphore;
    submit_info.pWaitDstStageMask = &stage;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &raw_command_buffer0;
    submit_info.signalSemaphoreCount = 1u;
    submit_info.pSignalSemaphores = &semaphore;
    if( vkQueueSubmit(
      **queue,
      1u,
      &submit_info,
      VK_NULL_HANDLE
    ) != VK_SUCCESS ) abort();
  }

  ++current_semaphore_value;
  ++next_semaphore_value;
  // タイムラインセマフォの値が1になったら開始
  // 完了したらタイムラインセマフォの値を2にする
  {
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = &timeline_semaphore_state;
    submit_info.waitSemaphoreCount = 1u;
    submit_info.pWaitSemaphores = &semaphore;
    submit_info.pWaitDstStageMask = &stage;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &raw_command_buffer1;
    submit_info.signalSemaphoreCount = 1u;
    submit_info.pSignalSemaphores = &semaphore;
    if( vkQueueSubmit(
      **queue,
      1u,
      &submit_info,
      VK_NULL_HANDLE
    ) != VK_SUCCESS ) abort();
  }
  
  ++current_semaphore_value;
  ++next_semaphore_value;
  // タイムラインセマフォの値が2になったら開始
  // 完了したらタイムラインセマフォの値を3にする
  {
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = &timeline_semaphore_state;
    submit_info.waitSemaphoreCount = 1u;
    submit_info.pWaitSemaphores = &semaphore;
    submit_info.pWaitDstStageMask = &stage;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &raw_command_buffer2;
    submit_info.signalSemaphoreCount = 1u;
    submit_info.pSignalSemaphores = &semaphore;
    if( vkQueueSubmit(
      **queue,
      1u,
      &submit_info,
      VK_NULL_HANDLE
    ) != VK_SUCCESS ) abort();
  }
  
  // タイムラインセマフォの値が3になるまで待機
  VkSemaphoreWaitInfo wait_info;
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  wait_info.pNext = nullptr;
  wait_info.flags = 0;
  wait_info.semaphoreCount = 1u;
  wait_info.pSemaphores = &semaphore;
  wait_info.pValues = &next_semaphore_value;
  if( vkWaitSemaphores(
    **device,
    &wait_info,
    UINT64_MAX
  ) != VK_SUCCESS ) abort();

  vkDestroySemaphore(
    **device,
    semaphore,
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

