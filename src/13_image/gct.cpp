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
#include <vulkan2json/ImageMemoryBarrier.hpp>
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
      std::cout << __FILE__ << " " << __LINE__ << std::endl;
      for(  const auto &v: src_image->get_layout().get_layout() )
        std::cout << nlohmann::json( v ) << std::endl;
      rec.convert_image( src_image, vk::ImageLayout::eTransferDstOptimal );
      // 入力イメージの内容を書く
      std::cout << __FILE__ << " " << __LINE__ << std::endl;
      for(  const auto &v: src_image->get_layout().get_layout() )
        std::cout << nlohmann::json( v ) << std::endl;
      rec.copy(
        src_buffer,
        src_image,
        vk::ImageLayout::eGeneral
      );
      std::cout << __FILE__ << " " << __LINE__ << std::endl;
      for(  const auto &v: src_image->get_layout().get_layout() )
        std::cout << nlohmann::json( v ) << std::endl;
      // イメージのレイアウトを不定から汎用に変更
      rec.convert_image( dest_image, vk::ImageLayout::eGeneral );
      for(  const auto &v: src_image->get_layout().get_layout() )
        std::cout << nlohmann::json( v ) << std::endl;
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }

  const auto &name_to_binding = descriptor_set->get_name_to_binding();

  // 入力イメージのイメージビューを作る
  auto src_view = 
    src_image->get_view(
      gct::image_view_create_info_t()
        .set_basic(
          vk::ImageViewCreateInfo()
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
            .setViewType( gct::to_image_view_type( src_image->get_props().get_basic().imageType, src_image->get_props().get_basic().arrayLayers ) )
        )
        .rebuild_chain()
    );
  // 出力イメージのイメージビューを作る
  auto dest_view = 
    dest_image->get_view(
      gct::image_view_create_info_t()
        .set_basic(
          vk::ImageViewCreateInfo()
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
            .setViewType( gct::to_image_view_type( dest_image->get_props().get_basic().imageType, dest_image->get_props().get_basic().arrayLayers ) )
        )
        .rebuild_chain()
    );

  // デスクリプタセットを更新
  descriptor_set->update(
    std::vector<gct::write_descriptor_set_t>{
      gct::write_descriptor_set_t()
        .set_basic(
          // このデスクリプタを
          (*descriptor_set)[ "src_image" ]
        )
        .add_image(
          gct::descriptor_image_info_t()
            .set_basic(
              vk::DescriptorImageInfo()
                .setImageLayout(
                  src_image->get_layout().get_uniform_layout()
                )
            )
            // このイメージビューにする
            .set_image_view( src_view )
        ),
      gct::write_descriptor_set_t()
        .set_basic(
          // このデスクリプタを
          (*descriptor_set)[ "dest_image" ]
        )
        .add_image(
          gct::descriptor_image_info_t()
            .set_basic(
              vk::DescriptorImageInfo()
                .setImageLayout(
                  dest_image->get_layout().get_uniform_layout()
                )
            )
            // このイメージビューにする
            .set_image_view( dest_view )
        )
    }
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

