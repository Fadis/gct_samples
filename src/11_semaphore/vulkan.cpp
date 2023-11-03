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
      },
      gct::queue_requirement_t{
        vk::QueueFlagBits::eGraphics,
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
  const auto queue0 = device->get_queue( 0u );
  const auto queue1 = device->get_queue( 1u );
  const auto queue_family_index0 = queue0->get_available_queue_family_index();
  const auto queue_family_index1 = queue1->get_available_queue_family_index();

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
  
  VkCommandPoolCreateInfo command_pool_create_info0;
  command_pool_create_info0.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_create_info0.pNext = nullptr;
  command_pool_create_info0.flags = 0u;
  command_pool_create_info0.queueFamilyIndex = queue_family_index0;
  VkCommandPool command_pool0;
  if( vkCreateCommandPool(
    **device,
    &command_pool_create_info0,
    nullptr,
    &command_pool0
  ) != VK_SUCCESS ) abort();

  VkCommandPoolCreateInfo command_pool_create_info1;
  command_pool_create_info1.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_create_info1.pNext = nullptr;
  command_pool_create_info1.flags = 0u;
  command_pool_create_info1.queueFamilyIndex = queue_family_index1;
  VkCommandPool command_pool1;
  if( vkCreateCommandPool(
    **device,
    &command_pool_create_info1,
    nullptr,
    &command_pool1
  ) != VK_SUCCESS ) abort();

  // 1つめのコマンドバッファ
  VkCommandBufferAllocateInfo command_buffer_allocate_info0;
  command_buffer_allocate_info0.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_allocate_info0.pNext = nullptr;
  command_buffer_allocate_info0.commandPool = command_pool0;
  command_buffer_allocate_info0.level =
    VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_allocate_info0.commandBufferCount = 1u;
  VkCommandBuffer command_buffer0;
  if( vkAllocateCommandBuffers(
    **device,
    &command_buffer_allocate_info0,
    &command_buffer0
  ) != VK_SUCCESS ) abort();

  // 2つめのコマンドバッファ
  VkCommandBufferAllocateInfo command_buffer_allocate_info1;
  command_buffer_allocate_info1.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_allocate_info1.pNext = nullptr;
  command_buffer_allocate_info1.commandPool = command_pool1;
  command_buffer_allocate_info1.level =
    VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_allocate_info1.commandBufferCount = 1u;
  VkCommandBuffer command_buffer1;
  if( vkAllocateCommandBuffers(
    **device,
    &command_buffer_allocate_info1,
    &command_buffer1
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

  {
    VkCommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.pNext = nullptr;
    command_buffer_begin_info.flags = 0u;
    command_buffer_begin_info.pInheritanceInfo = nullptr;
    if( vkBeginCommandBuffer(
      command_buffer0,
      &command_buffer_begin_info
    ) != VK_SUCCESS ) abort();
  }

  {
    VkBufferCopy region;
    region.srcOffset = 0u;
    region.dstOffset = 0u;
    region.size = 1024u;
    vkCmdCopyBuffer(
      command_buffer0,
      **staging_buffer,
      **device_local_buffer,
      1u,
      &region
    );
  }

  {
    VkBufferMemoryBarrier buffer_memory_barrier;
    buffer_memory_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_memory_barrier.pNext = nullptr;
    buffer_memory_barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
    buffer_memory_barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;
    buffer_memory_barrier.srcQueueFamilyIndex = queue_family_index0;
    buffer_memory_barrier.dstQueueFamilyIndex = queue_family_index0;
    buffer_memory_barrier.buffer = **device_local_buffer;
    buffer_memory_barrier.offset = 0u;
    buffer_memory_barrier.size = 1024u;
    vkCmdPipelineBarrier(
      command_buffer0,
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
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
  VkDescriptorSet raw_descriptor_set = **descriptor_set;
  vkCmdBindDescriptorSets(
    command_buffer0,
    VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
    **pipeline_layout,
    0u,
    1u,
    &raw_descriptor_set,
    0u,
    nullptr
  );

  vkCmdBindPipeline(
    command_buffer0,
    VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
    **pipeline
  );

  // 1回目のdispatch    
  vkCmdDispatch(
    command_buffer0,
    4, 2, 1
  );

  {
    VkBufferMemoryBarrier buffer_memory_barrier;
    buffer_memory_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_memory_barrier.pNext = nullptr;
    buffer_memory_barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT;
    buffer_memory_barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT;
    buffer_memory_barrier.srcQueueFamilyIndex = queue_family_index0;
    buffer_memory_barrier.dstQueueFamilyIndex = queue_family_index1;
    buffer_memory_barrier.buffer = **device_local_buffer;
    buffer_memory_barrier.offset = 0u;
    buffer_memory_barrier.size = 1024u;
    vkCmdPipelineBarrier(
      command_buffer0,
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
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

  if( vkEndCommandBuffer(
    command_buffer0
  ) != VK_SUCCESS ) abort();

  {
    VkCommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.pNext = nullptr;
    command_buffer_begin_info.flags = 0u;
    command_buffer_begin_info.pInheritanceInfo = nullptr;
    if( vkBeginCommandBuffer(
      command_buffer1,
      &command_buffer_begin_info
    ) != VK_SUCCESS ) abort();
  }

  vkCmdBindDescriptorSets(
    command_buffer1,
    VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
    **pipeline_layout,
    0u,
    1u,
    &raw_descriptor_set,
    0u,
    nullptr
  );

  vkCmdBindPipeline(
    command_buffer1,
    VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
    **pipeline
  );

  // 2回目のdispatch    
  vkCmdDispatch(
    command_buffer1,
    4, 2, 1
  );

  {
    VkBufferMemoryBarrier buffer_memory_barrier;
    buffer_memory_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_memory_barrier.pNext = nullptr;
    buffer_memory_barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT;
    buffer_memory_barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
    buffer_memory_barrier.srcQueueFamilyIndex = queue_family_index1;
    buffer_memory_barrier.dstQueueFamilyIndex = queue_family_index1;
    buffer_memory_barrier.buffer = **device_local_buffer;
    buffer_memory_barrier.offset = 0u;
    buffer_memory_barrier.size = 1024u;
    vkCmdPipelineBarrier(
      command_buffer1,
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
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
    VkBufferCopy region;
    region.srcOffset = 0u;
    region.dstOffset = 0u;
    region.size = 1024u;
    vkCmdCopyBuffer(
      command_buffer1,
      **device_local_buffer,
      **staging_buffer,
      1u,
      &region
    );
  }

  if( vkEndCommandBuffer(
    command_buffer1
  ) != VK_SUCCESS ) abort();

  // セマフォをつくる
  VkSemaphoreCreateInfo semaphore_create_info;
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_create_info.pNext = nullptr;
  semaphore_create_info.flags = 0u;
  VkSemaphore semaphore;
  if( vkCreateSemaphore(
    **device,
    &semaphore_create_info,
    nullptr,
    &semaphore
  ) != VK_SUCCESS ) abort();

  // 1つめのコマンドバッファの実行が完了したらセマフォに通知
  {
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.waitSemaphoreCount = 0u;
    submit_info.pWaitSemaphores = nullptr;
    submit_info.pWaitDstStageMask = nullptr;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &command_buffer0;
    submit_info.signalSemaphoreCount = 1u;
    submit_info.pSignalSemaphores = &semaphore;
    if( vkQueueSubmit(
      **queue0,
      1u,
      &submit_info,
      VK_NULL_HANDLE
    ) != VK_SUCCESS ) abort();
  }

  // 2つめのコマンドバッファのコンピュートシェーダの実行は
  // セマフォの通知が来るまで実行してはいけない
  {
    VkPipelineStageFlags stage =
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.waitSemaphoreCount = 1u;
    submit_info.pWaitSemaphores = &semaphore;
    submit_info.pWaitDstStageMask = &stage;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &command_buffer1;
    submit_info.signalSemaphoreCount = 0u;
    submit_info.pSignalSemaphores = nullptr;
    if( vkQueueSubmit(
      **queue1,
      1u,
      &submit_info,
      fence
    ) != VK_SUCCESS ) abort();
  }

  // 2つめのコマンドバッファの完了を待機
  if( vkWaitForFences(
    **device,
    1u,
    &fence,
    true,
    1 * 1000 * 1000 * 1000
  ) != VK_SUCCESS ) abort();

  vkDestroyFence(
    **device,
    fence,
    nullptr
  );

  vkDestroySemaphore(
    **device,
    semaphore,
    nullptr
  );

  vkFreeCommandBuffers(
    **device,
    command_pool0,
    1u,
    &command_buffer0
  );
  
  vkFreeCommandBuffers(
    **device,
    command_pool1,
    1u,
    &command_buffer1
  );

  vkDestroyCommandPool(
    **device,
    command_pool0,
    nullptr
  );
  
  vkDestroyCommandPool(
    **device,
    command_pool1,
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

