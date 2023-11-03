#include <cmath>
#include <iostream>
#include <nlohmann/json.hpp>
#include <glm/mat2x2.hpp>
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
#include <gct/sampler_create_info.hpp>
#include <gct/pipeline_layout.hpp>
#include <gct/command_buffer.hpp>
#include <gct/command_pool.hpp>

struct push_constant_t {
  glm::mat2x2 tex_mat;
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
    "../shaders/sampler.comp.spv"
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
          .setSize( sizeof( push_constant_t ) )
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

  auto src_image = allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          .setImageType( vk::ImageType::e2D )
          .setFormat( vk::Format::eR8G8B8A8Unorm )
          .setExtent( { 1024, 1024, 1 } )
          .setMipLevels( 10 )
          .setArrayLayers( 1 )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setTiling( vk::ImageTiling::eOptimal )
          .setUsage(
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eSampled
          )
          .setSharingMode( vk::SharingMode::eExclusive )
          .setQueueFamilyIndexCount( 0 )
          .setPQueueFamilyIndices( nullptr )
          .setInitialLayout( vk::ImageLayout::eUndefined )
      ),
      VMA_MEMORY_USAGE_GPU_ONLY
  );

  auto dest_image = allocator->create_image(
    gct::image_create_info_t()
      .set_basic(
        vk::ImageCreateInfo()
          .setImageType( vk::ImageType::e2D )
          .setFormat( vk::Format::eR8G8B8A8Unorm )
          .setExtent( { 1024, 1024, 1 } )
          .setMipLevels( 1 )
          .setArrayLayers( 1 )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setTiling( vk::ImageTiling::eOptimal )
          .setUsage(
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eStorage
          )
          .setSharingMode( vk::SharingMode::eExclusive )
          .setQueueFamilyIndexCount( 0 )
          .setPQueueFamilyIndices( nullptr )
          .setInitialLayout( vk::ImageLayout::eUndefined )
      ),
      VMA_MEMORY_USAGE_GPU_ONLY
  );
  {
    const auto command_buffer = queue->get_command_pool()->allocate();
    {
      auto rec = command_buffer->begin();
      rec.copy(
        src_buffer,
        src_image,
        vk::ImageLayout::eGeneral
      );
      rec.create_mipmap( src_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal );
      rec.convert_image( dest_image, vk::ImageLayout::eGeneral );
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }

  const auto &name_to_binding = descriptor_set->get_name_to_binding();

  auto src_view = 
    src_image->get_view(
      gct::image_view_create_info_t()
        .set_basic(
          vk::ImageViewCreateInfo()
            .setSubresourceRange(
              vk::ImageSubresourceRange()
                .setAspectMask( vk::ImageAspectFlagBits::eColor )
                .setBaseMipLevel( 0 )
                .setLevelCount( 10 )
                .setBaseArrayLayer( 0 )
                .setLayerCount( 1 )
            )
            .setViewType( gct::to_image_view_type( src_image->get_props().get_basic().imageType, src_image->get_props().get_basic().arrayLayers ) )
        )
        .rebuild_chain()
    );
  auto dest_view = 
    dest_image->get_view(
      gct::image_view_create_info_t()
        .set_basic(
          vk::ImageViewCreateInfo()
            .setSubresourceRange(
              vk::ImageSubresourceRange()
                .setAspectMask( vk::ImageAspectFlagBits::eColor )
                .setBaseMipLevel( 0 )
                .setLevelCount( 1 )
                .setBaseArrayLayer( 0 )
                .setLayerCount( 1 )
            )
            .setViewType( gct::to_image_view_type( dest_image->get_props().get_basic().imageType, dest_image->get_props().get_basic().arrayLayers ) )
        )
        .rebuild_chain()
    );
  // サンプラーを作る
  VkSamplerCreateInfo sampler_create_info;
  sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_create_info.pNext = nullptr;
  sampler_create_info.flags = 0;
  // 拡大するときは線形補間
  sampler_create_info.magFilter = VkFilter::VK_FILTER_LINEAR;
  // 縮小する時も線形補間
  sampler_create_info.minFilter = VkFilter::VK_FILTER_LINEAR;
  // ミップマップレベル間での合成も線形補間
  sampler_create_info.mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
  // 横方向に範囲外を読んだら境界色を返す
  sampler_create_info.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  // 縦方向に範囲外を読んだら境界色を返す
  sampler_create_info.addressModeV = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  // 奥行き方向に範囲外を読んだら境界色を返す
  sampler_create_info.addressModeW = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  // 異方性フィルタリングを使わない
  sampler_create_info.anisotropyEnable = false;
  sampler_create_info.maxAnisotropy = 0.f;
  // 比較演算を使わない
  sampler_create_info.compareEnable = false;
  sampler_create_info.compareOp = VkCompareOp::VK_COMPARE_OP_NEVER;
  // ミップマップの選択にバイアスをかけない
  sampler_create_info.mipLodBias = 0.f;
  // 最小のミップマップレベル(最も大きい画像)は0枚目
  sampler_create_info.minLod = 0.f;
  // 最大のミップマップレベル(最も小さい画像)は5枚目
  sampler_create_info.maxLod = 5.f;
  // 境界色は不透明の白
  sampler_create_info.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  // 画像の端が(1.0,1.0)になるようなテクスチャ座標を使う
  sampler_create_info.unnormalizedCoordinates = false;
  VkSampler sampler;
  if( vkCreateSampler(
    **device,
    &sampler_create_info,
    nullptr,
    &sampler
  ) != VK_SUCCESS ) abort();

  // 更新するデスクリプタの情報
  VkDescriptorImageInfo src_descriptor_image_info;
  src_descriptor_image_info.sampler = sampler;
  // デスクリプタをこのイメージビューにする
  src_descriptor_image_info.imageView = **src_view;
  src_descriptor_image_info.imageLayout = VkImageLayout( int( src_image->get_props().get_basic().initialLayout ) );
  VkDescriptorImageInfo dest_descriptor_image_info;
  dest_descriptor_image_info.sampler = VK_NULL_HANDLE;
  // デスクリプタをこのイメージビューにする
  dest_descriptor_image_info.imageView = **dest_view;
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
  write_descriptor_set[ 0 ].descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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

  push_constant_t push_constant;
  push_constant.tex_mat = glm::mat2x2(
    std::cos( M_PI / 4 ), std::sin( M_PI / 4 ),
    -std::sin( M_PI / 4 ), std::cos( M_PI / 4 )
  );


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

    // プッシュコンスタントを更新
    vkCmdPushConstants(
      *rec,
      **pipeline_layout,
      // コンピュートシェーダから見えるプッシュコンスタントの
      VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT,
      // 0バイト目から
      0u,
      // push_constant_tのサイズ分の範囲を
      sizeof( push_constant_t ),
      // この内容にする
      reinterpret_cast< void* >( &push_constant )
    );
    
    rec->dispatch( 32, 32, 1 );

    rec.barrier(
      vk::AccessFlagBits::eShaderWrite,
      vk::AccessFlagBits::eTransferRead,
      vk::PipelineStageFlagBits::eComputeShader,
      vk::PipelineStageFlagBits::eTransfer,
      vk::DependencyFlagBits( 0 ),
      {},
      { dest_image }
    );

    rec.copy(
      dest_image,
      dest_buffer
    );
  }
  
  command_buffer->execute(
    gct::submit_info_t()
  );
  
  command_buffer->wait_for_executed();

  dest_buffer->dump_image( "out.png" );

}

