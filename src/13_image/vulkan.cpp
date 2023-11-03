#include <iostream>
#include <nlohmann/json.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/buffer.hpp>
#include <gct/image.hpp>
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
    "../shaders/invert.comp.spv"
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
                spec_t{ 32, 32, 1.0f }
              )
              .add_map< std::uint32_t >( 1, offsetof( spec_t, local_x_size ) )
              .add_map< std::uint32_t >( 2, offsetof( spec_t, local_y_size ) )
          )
      )
      .set_layout( pipeline_layout )
  );
  const auto allocator = device->get_allocator();

  const auto src_buffer = allocator->load_image(
    "../images/test.png",
    true
  );

  const auto dest_buffer = allocator->create_pixel_buffer(
    vk::BufferUsageFlagBits::eTransferDst,
    VMA_MEMORY_USAGE_GPU_TO_CPU,
    src_buffer->get_extent(),
    src_buffer->get_format()
  );

  // 入力イメージを作る
  auto src_image = allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
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
      ),
      // GPUだけから読める
      VMA_MEMORY_USAGE_GPU_ONLY
  );

  // 出力イメージを作る
  auto dest_image = allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          // 2次元で
          .setImageType( vk::ImageType::e2D )
          // RGBA各8bitで
          .setFormat( vk::Format::eR8G8B8A8Unorm )
          // 1024x1024で
          .setExtent( { 1024, 1024, 1 } )
          // ミップマップは無く
          .setMipLevels( 1 )
          // レイヤーは1枚だけの
          .setArrayLayers( 1 )
          // 1テクセルにつきサンプリング点を1つだけ持つ
          .setSamples( vk::SampleCountFlagBits::e1 )
          // GPUが読みやすいように配置された
          .setTiling( vk::ImageTiling::eOptimal )
          // 転送元とストレージイメージに使う
          .setUsage(
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eStorage
          )
          // 同時に複数のキューから操作しない
          .setSharingMode( vk::SharingMode::eExclusive )
          .setQueueFamilyIndexCount( 0 )
          .setPQueueFamilyIndices( nullptr )
          // 初期状態は不定な
          .setInitialLayout( vk::ImageLayout::eUndefined )
      ),
      // GPUだけから見える
      VMA_MEMORY_USAGE_GPU_ONLY
  );
  {
    const auto command_buffer = queue->get_command_pool()->allocate();
    {
      auto rec = command_buffer->begin();
      // イメージのレイアウトを不定から転送先に変更
      {
        VkImageMemoryBarrier image_memory_barrier;
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.pNext = nullptr;
        image_memory_barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
        image_memory_barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
        image_memory_barrier.newLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_memory_barrier.srcQueueFamilyIndex = 0;
        image_memory_barrier.dstQueueFamilyIndex = 0;
        image_memory_barrier.image = **src_image;
        image_memory_barrier.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        image_memory_barrier.subresourceRange.baseMipLevel = 0;
        image_memory_barrier.subresourceRange.levelCount = src_image->get_props().get_basic().mipLevels;
        image_memory_barrier.subresourceRange.baseArrayLayer = 0;
        image_memory_barrier.subresourceRange.layerCount = src_image->get_props().get_basic().arrayLayers;
        vkCmdPipelineBarrier(
          *rec,
          VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
          VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
          VkDependencyFlags( 0 ),
          0,
          nullptr,
          0,
          nullptr,
          1,
          &image_memory_barrier
        );
      }
      // 入力イメージの内容を書く
      {
        VkBufferImageCopy buffer_image_copy;
        buffer_image_copy.bufferOffset = 0;
        buffer_image_copy.bufferRowLength = src_image->get_props().get_basic().extent.width;
        buffer_image_copy.bufferImageHeight = src_image->get_props().get_basic().extent.height;
        buffer_image_copy.imageSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        buffer_image_copy.imageSubresource.mipLevel = 0;
        buffer_image_copy.imageSubresource.baseArrayLayer = 0;
        buffer_image_copy.imageSubresource.layerCount = 1;
        buffer_image_copy.imageOffset.x = 0;
        buffer_image_copy.imageOffset.y = 0;
        buffer_image_copy.imageOffset.z = 0;
        buffer_image_copy.imageExtent = static_cast< VkExtent3D >( src_image->get_props().get_basic().extent );
        vkCmdCopyBufferToImage(
          *rec,
          **src_buffer,
          **src_image,
          VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          1,
          &buffer_image_copy
        );
      }
      // イメージのレイアウトを不定から転送先に変更
      {
        VkImageMemoryBarrier image_memory_barrier;
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.pNext = nullptr;
        image_memory_barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;
        image_memory_barrier.oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_memory_barrier.newLayout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.srcQueueFamilyIndex = 0;
        image_memory_barrier.dstQueueFamilyIndex = 0;
        image_memory_barrier.image = **src_image;
        image_memory_barrier.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        image_memory_barrier.subresourceRange.baseMipLevel = 0;
        image_memory_barrier.subresourceRange.levelCount = src_image->get_props().get_basic().mipLevels;
        image_memory_barrier.subresourceRange.baseArrayLayer = 0;
        image_memory_barrier.subresourceRange.layerCount = src_image->get_props().get_basic().arrayLayers;
        vkCmdPipelineBarrier(
          *rec,
          VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
          VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VkDependencyFlags( 0 ),
          0,
          nullptr,
          0,
          nullptr,
          1,
          &image_memory_barrier
        );
      }
      src_image->get_layout().set_layout(
        0,
        src_image->get_props().get_basic().mipLevels,
        0,
        src_image->get_props().get_basic().arrayLayers,
        vk::ImageLayout::eGeneral
      );
      rec.convert_image( dest_image, vk::ImageLayout::eGeneral );
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }

  const auto &name_to_binding = descriptor_set->get_name_to_binding();

  // 入力イメージのイメージビューを作る
  VkImageView src_view;
  {
    VkImageViewCreateInfo image_view_create_info;
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.pNext = nullptr;
    image_view_create_info.flags = 0;
    // このイメージの
    image_view_create_info.image = **src_image;
    // イメージの色のうち
    image_view_create_info.subresourceRange.aspectMask =
      VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
    // 最大のミップマップから
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    // 1枚の範囲
    image_view_create_info.subresourceRange.levelCount = 1;
    // 最初のレイヤーから
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    // 1枚の範囲
    image_view_create_info.subresourceRange.layerCount = 1;
    // このフォーマットで
    image_view_create_info.format = VkFormat( int( src_image->get_props().get_basic().format ) );
    // 2次元のイメージを指している
    image_view_create_info.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
    // RGBAの値の順序は入れ替えない
    image_view_create_info.components.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_R;
    image_view_create_info.components.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_G;
    image_view_create_info.components.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_B;
    image_view_create_info.components.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_A;
    if( vkCreateImageView(
      **device,
      &image_view_create_info,
      nullptr,
      &src_view
    ) != VK_SUCCESS ) abort();
  }
  // 出力イメージのイメージビューを作る
  VkImageView dest_view;
  {
    VkImageViewCreateInfo image_view_create_info;
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.pNext = nullptr;
    image_view_create_info.flags = 0;
    // このイメージの
    image_view_create_info.image = **dest_image;
    // イメージの色のうち
    image_view_create_info.subresourceRange.aspectMask =
      VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
    // 最大のミップマップから
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    // 1枚の範囲
    image_view_create_info.subresourceRange.levelCount = 1;
    // 最初のレイヤーから
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    // 1枚の範囲
    image_view_create_info.subresourceRange.layerCount = 1;
    // このフォーマットで
    image_view_create_info.format = VkFormat( int( dest_image->get_props().get_basic().format ) );
    // 2次元のイメージを指している
    image_view_create_info.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
    // RGBAの値の順序は入れ替えない
    image_view_create_info.components.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_R;
    image_view_create_info.components.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_G;
    image_view_create_info.components.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_B;
    image_view_create_info.components.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_A;
    if( vkCreateImageView(
      **device,
      &image_view_create_info,
      nullptr,
      &dest_view
    ) != VK_SUCCESS ) abort();
  }

  // 更新するデスクリプタの情報
  VkDescriptorImageInfo src_descriptor_image_info;
  src_descriptor_image_info.sampler = VK_NULL_HANDLE;
  // デスクリプタをこのイメージビューにする
  src_descriptor_image_info.imageView = src_view;
  src_descriptor_image_info.imageLayout = VkImageLayout( int( src_image->get_props().get_basic().initialLayout ) );
  VkDescriptorImageInfo dest_descriptor_image_info;
  dest_descriptor_image_info.sampler = VK_NULL_HANDLE;
  // デスクリプタをこのイメージビューにする
  dest_descriptor_image_info.imageView = dest_view;
  dest_descriptor_image_info.imageLayout = VkImageLayout( int( dest_image->get_props().get_basic().initialLayout ) );

  // デスクリプタの内容を更新
  std::vector< VkWriteDescriptorSet > write_descriptor_set( 2 );
  write_descriptor_set[ 0 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write_descriptor_set[ 0 ].pNext = nullptr;
  // このデスクリプタセットの
  write_descriptor_set[ 0 ].dstSet = **descriptor_set;
  // binding=0の
  write_descriptor_set[ 0 ].dstBinding = 0;
  // 0要素目から
  write_descriptor_set[ 0 ].dstArrayElement = 0;
  // 1個の
  write_descriptor_set[ 0 ].descriptorCount = 1;
  // ストレージイメージのデスクリプタを
  write_descriptor_set[ 0 ].descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  // この内容にする
  write_descriptor_set[ 0 ].pImageInfo = &src_descriptor_image_info;
  write_descriptor_set[ 1 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write_descriptor_set[ 1 ].pNext = nullptr;
  // このデスクリプタセットの
  write_descriptor_set[ 1 ].dstSet = **descriptor_set;
  // binding=1の
  write_descriptor_set[ 1 ].dstBinding = 1;
  // 0要素目から
  write_descriptor_set[ 1 ].dstArrayElement = 0;
  // 1個の
  write_descriptor_set[ 1 ].descriptorCount = 1;
  // ストレージイメージのデスクリプタを
  write_descriptor_set[ 1 ].descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  // この内容にする
  write_descriptor_set[ 1 ].pImageInfo = &dest_descriptor_image_info;
  vkUpdateDescriptorSets(
    **device,
    write_descriptor_set.size(),
    write_descriptor_set.data(),
    0,
    nullptr
  );

  const auto command_buffer = queue->get_command_pool()->allocate();

  {
    auto rec = command_buffer->begin();
    
    rec.bind_descriptor_set(
      vk::PipelineBindPoint::eCompute,
      pipeline_layout,
      descriptor_set
    );
    
    rec.bind_pipeline(
      pipeline
    );
   
    // シェーダを実行
    rec->dispatch( 32, 32, 1 );

    // シェーダから出力イメージへの書き込みが完了した後で
    rec.barrier(
      vk::AccessFlagBits::eShaderWrite,
      vk::AccessFlagBits::eTransferRead,
      vk::PipelineStageFlagBits::eComputeShader,
      vk::PipelineStageFlagBits::eTransfer,
      vk::DependencyFlagBits( 0 ),
      {},
      { dest_image }
    );

    // CPUから見えるバッファに出力イメージの内容をコピー
    {
      VkBufferImageCopy buffer_image_copy;
      buffer_image_copy.bufferOffset = 0;
      buffer_image_copy.bufferRowLength = dest_image->get_props().get_basic().extent.width;
      buffer_image_copy.bufferImageHeight = dest_image->get_props().get_basic().extent.height;
      buffer_image_copy.imageSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
      buffer_image_copy.imageSubresource.mipLevel = 0;
      buffer_image_copy.imageSubresource.baseArrayLayer = 0;
      buffer_image_copy.imageSubresource.layerCount = 1;
      buffer_image_copy.imageOffset.x = 0;
      buffer_image_copy.imageOffset.y = 0;
      buffer_image_copy.imageOffset.z = 0;
      buffer_image_copy.imageExtent = static_cast< VkExtent3D >( dest_image->get_props().get_basic().extent );
      vkCmdCopyImageToBuffer(
        *rec,
        **dest_image,
        VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
        **dest_buffer,
        1,
        &buffer_image_copy
      );
    }
  }
  
  command_buffer->execute(
    gct::submit_info_t()
  );
  
  command_buffer->wait_for_executed();

  dest_buffer->dump_image( "out.png" );

}

