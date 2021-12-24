#include <iostream>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/write_descriptor_set.hpp>
#include <gct/shader_module.hpp>
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
  const auto shader = device->get_shader_module(
    "../shaders/add.comp.spv"
  );
  const auto descriptor_set_layout = device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding( shader->get_props().get_reflection() )
      .rebuild_chain()
  );

  // コンピュートシェーダでプッシュコンスタントを32バイト使う
  const auto push_constant_range =
    vk::PushConstantRange()
      .setStageFlags( vk::ShaderStageFlagBits::eCompute )
      .setOffset( 0 )
      .setSize( 32u );

  // パイプラインレイアウトを作る
  auto pipeline_layout = (*device)->createPipelineLayoutUnique(
    vk::PipelineLayoutCreateInfo()
      .setSetLayoutCount( 1u )
      // このレイアウトのデスクリプタセットとくっつく
      .setPSetLayouts( &**descriptor_set_layout )
      .setPushConstantRangeCount( 1u )
      .setPPushConstantRanges( &push_constant_range )
  );

  // パイプラインキャッシュを作る
  auto pipeline_cache = (*device)->createPipelineCacheUnique(
    vk::PipelineCacheCreateInfo()
  );

  // シェーダの特殊化パラメータの配置
  const std::vector< vk::SpecializationMapEntry > specialization_map{
    vk::SpecializationMapEntry()
      .setConstantID( 1 )
      .setOffset( offsetof( spec_t, local_x_size ) )
      .setSize( sizeof( std::uint32_t ) ),
    vk::SpecializationMapEntry()
      .setConstantID( 2 )
      .setOffset( offsetof( spec_t, local_y_size ) )
      .setSize( sizeof( std::uint32_t ) ),
    vk::SpecializationMapEntry()
      .setConstantID( 3 )
      .setOffset( offsetof( spec_t, value ) )
      .setSize( sizeof( std::uint32_t ) ),
  };
  
  // シェーダの特殊化パラメータの値
  const spec_t specialization_values{ 8, 4, 1.0f };
  
  const auto specialization_info = vk::SpecializationInfo()
    .setMapEntryCount( specialization_map.size() )
    .setPMapEntries( specialization_map.data() )
    .setDataSize( sizeof( spec_t ) )
    .setPData( &specialization_values );

  // パイプラインの設定
  const std::vector< vk::ComputePipelineCreateInfo > pipeline_create_info{
    vk::ComputePipelineCreateInfo()
    // シェーダの設定
    .setStage(
      vk::PipelineShaderStageCreateInfo()
        // コンピュートシェーダに
        .setStage( vk::ShaderStageFlagBits::eCompute )
        // このシェーダを使う
        .setModule( **shader )
        // main関数から実行する
        .setPName( "main" )
        // シェーダの特殊化パラメータを設定
        .setPSpecializationInfo(
          &specialization_info
        )
    )
    // このレイアウトのパイプラインを作る
    .setLayout( *pipeline_layout )
  };

  // パイプラインを作る
  auto wrapped = (*device)->createComputePipelinesUnique(
    // このパイプラインキャッシュを使って
    *pipeline_cache,
    // この設定で
    pipeline_create_info
  );

  // パイプラインは一度に複数作れるのでvectorで返ってくる
  // 1つめの設定のパイプラインは返り値の1要素目に入っている
  if( wrapped.result != vk::Result::eSuccess )
    vk::throwResultException( wrapped.result, "createComputePipeline failed" );
  auto pipeline = std::move( wrapped.value[ 0 ] );

  // パイプラインキャッシュをシリアライズする
  auto serialized = (*device)->getPipelineCacheData( *pipeline_cache );
}

