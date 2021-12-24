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
      rec->pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlagBits( 0 ),
        {},
        {},
        {
          vk::ImageMemoryBarrier()
            .setOldLayout( vk::ImageLayout::eUndefined )
            .setNewLayout( vk::ImageLayout::eTransferDstOptimal )
            .setSrcAccessMask( vk::AccessFlagBits::eTransferRead )
            .setDstAccessMask( vk::AccessFlagBits::eTransferWrite )
            .setImage( **src_image )
            .setSubresourceRange(
              vk::ImageSubresourceRange()
                .setAspectMask( vk::ImageAspectFlagBits::eColor )
                .setBaseMipLevel( 0 )
                .setLevelCount( src_image->get_props().get_basic().mipLevels )
                .setBaseArrayLayer( 0 )
                .setLayerCount( src_image->get_props().get_basic().arrayLayers )
            )
        }
      );
      // 入力イメージの内容を書く
      rec->copyBufferToImage(
        **src_buffer,
        **src_image,
        vk::ImageLayout::eTransferDstOptimal,
        std::vector< vk::BufferImageCopy >{
          vk::BufferImageCopy()
            .setBufferOffset( 0 )
            .setBufferRowLength( src_image->get_props().get_basic().extent.width )
            .setBufferImageHeight( src_image->get_props().get_basic().extent.height )
            .setImageSubresource(
              vk::ImageSubresourceLayers()
                .setAspectMask( vk::ImageAspectFlagBits::eColor )
                .setMipLevel( 0 )
                .setBaseArrayLayer( 0 )
                .setLayerCount( 1 )
            )
            .setImageOffset(
              vk::Offset3D()
                .setX( 0 )
                .setY( 0 )
                .setZ( 0 )
            )
            .setImageExtent(
              src_image->get_props().get_basic().extent
            )
        }
      );
      // イメージのレイアウトを不定から転送先に変更
      rec->pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::DependencyFlagBits( 0 ),
        {},
        {},
        {
          vk::ImageMemoryBarrier()
            .setOldLayout( vk::ImageLayout::eTransferDstOptimal )
            .setNewLayout( vk::ImageLayout::eGeneral )
            .setSrcAccessMask( vk::AccessFlagBits::eTransferWrite )
            .setDstAccessMask( vk::AccessFlagBits::eShaderRead )
            .setImage( **src_image )
            .setSubresourceRange(
              vk::ImageSubresourceRange()
                .setAspectMask( vk::ImageAspectFlagBits::eColor )
                .setBaseMipLevel( 0 )
                .setLevelCount( src_image->get_props().get_basic().mipLevels )
                .setBaseArrayLayer( 0 )
                .setLayerCount( src_image->get_props().get_basic().arrayLayers )
            )
        }
      );
      src_image->set_layout( vk::ImageLayout::eGeneral );
      rec.convert_image( dest_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral );
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }

  const auto &name_to_binding = descriptor_set->get_name_to_binding();

  // 入力イメージのイメージビューを作る
  auto src_view =
    (*device)->createImageViewUnique(
      vk::ImageViewCreateInfo()
        //このイメージの
        .setImage( **src_image )
        .setSubresourceRange(
          vk::ImageSubresourceRange()
            // イメージの色のうち
            .setAspectMask( vk::ImageAspectFlagBits::eColor )
            // 最大のミップマップから
            .setBaseMipLevel( 0 )
            // 1枚の範囲
            .setLevelCount( 1 )
            // 最初のレイヤーから
            .setBaseArrayLayer( 0 )
            // 1枚の範囲
            .setLayerCount( 1 )
        )
        // このフォーマットで
        .setFormat( src_image->get_props().get_basic().format )
        // 2次元のイメージを指している
        .setViewType( vk::ImageViewType::e2D )
        // RGBAの値の順序は入れ替えない
        .setComponents(
          vk::ComponentMapping()
            .setR( vk::ComponentSwizzle::eR )
            .setG( vk::ComponentSwizzle::eG )
            .setB( vk::ComponentSwizzle::eB )
            .setA( vk::ComponentSwizzle::eA )
        )
    );
  // 出力イメージのイメージビューを作る
  auto dest_view =
    (*device)->createImageViewUnique(
      vk::ImageViewCreateInfo()
        // このイメージの
        .setImage( **dest_image )
        .setSubresourceRange(
          vk::ImageSubresourceRange()
            // イメージの色のうち
            .setAspectMask( vk::ImageAspectFlagBits::eColor )
            // 最大のミップマップから
            .setBaseMipLevel( 0 )
            // 1枚の範囲
            .setLevelCount( 1 )
            // 最初のレイヤーから
            .setBaseArrayLayer( 0 )
            // 1枚の範囲
            .setLayerCount( 1 )
        )
        // このフォーマットで
        .setFormat( src_image->get_props().get_basic().format )
        // 2次元のイメージを指している
        .setViewType( vk::ImageViewType::e2D )
        // RGBAの値の順序は入れ替えない
        .setComponents(
          vk::ComponentMapping()
            .setR( vk::ComponentSwizzle::eR )
            .setG( vk::ComponentSwizzle::eG )
            .setB( vk::ComponentSwizzle::eB )
            .setA( vk::ComponentSwizzle::eA )
        )
    );

  // 更新するデスクリプタの情報
  const auto src_descriptor_image_info =
    vk::DescriptorImageInfo()
      // デスクリプタをこのイメージビューにする
      .setImageView( *src_view )
      .setImageLayout(
        src_image->get_props().get_basic().initialLayout
      );
  const auto dest_descriptor_image_info =
    vk::DescriptorImageInfo()
      // デスクリプタをこのイメージビューにする
      .setImageView( *dest_view )
      .setImageLayout(
        dest_image->get_props().get_basic().initialLayout
      );

  // デスクリプタの内容を更新
  (*device)->updateDescriptorSets(
    {
      vk::WriteDescriptorSet()
        // このデスクリプタセットの
        .setDstSet( **descriptor_set )
        // binding=0の
        .setDstBinding( 0 )
        // 0要素目から
        .setDstArrayElement( 0 )
        // 1個の
        .setDescriptorCount( 1u )
        // ストレージバッファのデスクリプタを
        .setDescriptorType( vk::DescriptorType::eStorageImage )
        // この内容にする
        .setPImageInfo( &src_descriptor_image_info ),
      vk::WriteDescriptorSet()
        // このデスクリプタセットの
        .setDstSet( **descriptor_set )
        // binding=0の
        .setDstBinding( 1 )
        // 0要素目から
        .setDstArrayElement( 0 )
        // 1個の
        .setDescriptorCount( 1u )
        // ストレージバッファのデスクリプタを
        .setDescriptorType( vk::DescriptorType::eStorageImage )
        // この内容にする
        .setPImageInfo( &dest_descriptor_image_info )
    },
    {}
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
      vk::PipelineBindPoint::eCompute,
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
    rec->copyImageToBuffer(
      **dest_image,
      vk::ImageLayout::eGeneral,
      **dest_buffer,
      std::vector< vk::BufferImageCopy >{
        vk::BufferImageCopy()
          .setBufferOffset( 0 )
          .setBufferRowLength( dest_image->get_props().get_basic().extent.width )
          .setBufferImageHeight( dest_image->get_props().get_basic().extent.height )
          .setImageSubresource(
            vk::ImageSubresourceLayers()
              .setAspectMask( vk::ImageAspectFlagBits::eColor )
              .setMipLevel( 0 )
              .setBaseArrayLayer( 0 )
              .setLayerCount( 1 )
          )
          .setImageOffset(
            vk::Offset3D()
              .setX( 0 )
              .setY( 0 )
              .setZ( 0 )
          )
          .setImageExtent(
            dest_image->get_props().get_basic().extent
          )
      }
    );
  }
  
  command_buffer->execute(
    gct::submit_info_t()
  );
  
  command_buffer->wait_for_executed();

  dest_buffer->dump_image( "out.png" );

}

