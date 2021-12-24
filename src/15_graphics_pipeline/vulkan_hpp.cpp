#include <cmath>
#include <iostream>
#include <nlohmann/json.hpp>
#include <glm/mat2x2.hpp>
#include <gct/get_extensions.hpp>
#include <gct/instance.hpp>
#include <gct/queue.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/image_create_info.hpp>
#include <gct/swapchain.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/pipeline_cache.hpp>
#include <gct/pipeline_layout_create_info.hpp>
#include <gct/buffer_view_create_info.hpp>
#include <gct/submit_info.hpp>
#include <gct/shader_module_create_info.hpp>
#include <gct/shader_module.hpp>
#include <gct/graphics_pipeline_create_info.hpp>
#include <gct/graphics_pipeline.hpp>
#include <gct/sampler_create_info.hpp>
#include <gct/pipeline_layout.hpp>
#include <gct/gltf.hpp>

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

  // 頂点シェーダをロード
  const auto vs = device->get_shader_module(
    "../shaders/simple.vert.spv"
  );

  // フラグメントシェーダをロード
  const auto fs = device->get_shader_module(
    "../shaders/simple.frag.spv"
  );
  
  // デスクリプタセットレイアウトを作る
  const auto descriptor_set_layout = device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding(
        // このシェーダにあるデスクリプタを追加
        vs->get_props().get_reflection()
      )
      .add_binding(
        // このシェーダにあるデスクリプタを追加
        fs->get_props().get_reflection()
      )
  );

  // パイプラインレイアウトを作る
  const auto pipeline_layout = device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      // このデスクリプタセットレイアウトを使う
      .add_descriptor_set_layout( descriptor_set_layout )
      // push_constants_tを置けるサイズのプッシュコンスタントを使う
      .add_push_constant_range(
        vk::PushConstantRange()
          // 頂点シェーダとフラグメンとシェーダから見える
          .setStageFlags(
            vk::ShaderStageFlagBits::eVertex |
            vk::ShaderStageFlagBits::eFragment
          )
          // 先頭から
          .setOffset( 0 )
          // このサイズ
          .setSize( sizeof( gct::gltf::push_constants_t ) )
      )
  );

  const std::vector< vk::AttachmentDescription > attachments{
    // 色を出力するアタッチメントを追加
    vk::AttachmentDescription()
      // RGBA各8bitの
      .setFormat( vk::Format::eR8G8B8A8Unorm )
      // サンプリング点が1テクセルあたり1つで
      .setSamples( vk::SampleCountFlagBits::e1 )
      // レンダーパスに入る前の値は読めなくてもよくて
      .setLoadOp( vk::AttachmentLoadOp::eClear )
      // 書いた内容はレンダーパスの後で読める必要があり
      .setStoreOp( vk::AttachmentStoreOp::eStore )
      // ステンシルとして読める必要がなくて
      .setStencilLoadOp( vk::AttachmentLoadOp::eDontCare )
      // ステンシルとして書ける必要もない
      .setStencilStoreOp( vk::AttachmentStoreOp::eDontCare )
      // 初期レイアウトは何でも良くて
      .setInitialLayout( vk::ImageLayout::eUndefined )
      // このレンダーパスが終わったら表示に適したレイアウトになっている
      .setFinalLayout( vk::ImageLayout::ePresentSrcKHR ),
    // 深度を出力するアタッチメントを追加
    vk::AttachmentDescription()
      // D16bitの
      .setFormat( vk::Format::eD16Unorm )
      // サンプリング点が1テクセルあたり1つで
      .setSamples( vk::SampleCountFlagBits::e1 )
      // レンダーパスに入る前の値は読めなくてもよくて
      .setLoadOp( vk::AttachmentLoadOp::eClear )
      // 書いた内容はレンダーパスの後で読める必要があり
      .setStoreOp( vk::AttachmentStoreOp::eStore )
      // ステンシルとして読める必要がなくて
      .setStencilLoadOp( vk::AttachmentLoadOp::eDontCare )
      // ステンシルとして書ける必要もない
      .setStencilStoreOp( vk::AttachmentStoreOp::eDontCare )
      // 初期レイアウトは何でも良くて
      .setInitialLayout( vk::ImageLayout::eUndefined )
      // このレンダーパスが終わったら深度とステンシルを保持するのに適したレイアウトになっている
      .setFinalLayout( vk::ImageLayout::eDepthStencilAttachmentOptimal )
  };

  const auto color_attachment =
    vk::AttachmentReference()
      // 0番目のアタッチメントを
      .setAttachment( 0 )
      // 色を吐くのに丁度良いレイアウトにして使う
      .setLayout( vk::ImageLayout::eColorAttachmentOptimal );

  const auto depth_attachment =
    vk::AttachmentReference()
      // 1番目のアタッチメントを
      .setAttachment( 1 )
      // 深度とステンシルを吐くのに丁度良いレイアウトにして使う
      .setLayout( vk::ImageLayout::eDepthStencilAttachmentOptimal );

  // サブパスを定義する
  const std::vector< vk::SubpassDescription > subpass{
    vk::SubpassDescription()
      // グラフィクスパイプラインに対応するサブパス
      .setPipelineBindPoint( vk::PipelineBindPoint::eGraphics )
      // Input Attachmentは使わない
      .setInputAttachmentCount( 0 )
      .setPInputAttachments( nullptr )
      // Color Attachmentを1つ使う
      .setColorAttachmentCount( 1 )
      .setPColorAttachments( &color_attachment )
      // Depth Stencil Attachmentを使う
      .setPDepthStencilAttachment( &depth_attachment )
      // Resolve Attachmentを使わない
      .setPResolveAttachments( nullptr )
      // 他に依存するアタッチメントは無い
      .setPreserveAttachmentCount( 0 )
      .setPPreserveAttachments( nullptr )
  };

  // レンダーパスを作る
  const auto render_pass = (*device)->createRenderPassUnique(
    vk::RenderPassCreateInfo()
      // このアタッチメントがあり
      .setAttachmentCount( attachments.size() )
      .setPAttachments( attachments.data() )
      // このサブパスがある
      .setSubpassCount( subpass.size() )
      .setPSubpasses( subpass.data() )
      // サブパス間に依存関係はない
      .setDependencyCount( 0 )
      .setPDependencies( nullptr )
  );

  const auto pipeline_cache = device->get_pipeline_cache();

  // 使用するシェーダモジュール
  const std::vector< vk::PipelineShaderStageCreateInfo > shader{
    vk::PipelineShaderStageCreateInfo()
      .setStage( vk::ShaderStageFlagBits::eVertex )
      .setModule( **vs )
      .setPName( "main" ),
    vk::PipelineShaderStageCreateInfo()
      .setStage( vk::ShaderStageFlagBits::eFragment )
      .setModule( **fs )
      .setPName( "main" )
  };

  // 頂点配列の読み方
  const std::vector< vk::VertexInputBindingDescription > vib{
    vk::VertexInputBindingDescription()
      // 頂点配列binding 0番は
      .setBinding( 0 )
      // 頂点1個毎に
      .setInputRate( vk::VertexInputRate::eVertex )
      // 12バイト移動しながら読む
      .setStride( sizeof( float ) * 3 )
  };

  const std::vector< vk::VertexInputAttributeDescription > via{
    vk::VertexInputAttributeDescription()
      // 頂点の座標は
      .setLocation( 0 )
      // floatが3つで1要素になっていて
      .setFormat( vk::Format::eR32G32B32Sfloat )
      // 頂点配列binding 0番の読み方で
      .setBinding( 0 )
      // 先頭から読む
      .setOffset( 0 )
  };

  auto vistat =
    vk::PipelineVertexInputStateCreateInfo()
      .setVertexBindingDescriptionCount( vib.size() )
      .setPVertexBindingDescriptions( vib.data() )
      .setVertexAttributeDescriptionCount( via.size() )
      .setPVertexAttributeDescriptions( via.data() );

  // プリミティブの組み立て方
  const auto input_assembly =
    vk::PipelineInputAssemblyStateCreateInfo()
      // 頂点配列の要素3個毎に1つの三角形
      .setTopology( vk::PrimitiveTopology::eTriangleList );

  // ビューポートとシザーの設定
  const auto viewport =
    vk::PipelineViewportStateCreateInfo()
      // 1個のビューポートと
      .setViewportCount( 1 )
      // 1個のシザーを使う
      .setScissorCount( 1 );
      // 具体的な値は後でDynamicStateを使って設定する

  // ラスタライズの設定
  const auto rasterization =
    vk::PipelineRasterizationStateCreateInfo()
      // 範囲外の深度を丸めない
      .setDepthClampEnable( false )
      // ラスタライズを行う
      .setRasterizerDiscardEnable( false )
      // 三角形の中を塗る
      .setPolygonMode( vk::PolygonMode::eFill )
      // 背面カリングを行わない
      .setCullMode( vk::CullModeFlagBits::eNone )
      // 表面は時計回り
      .setFrontFace( vk::FrontFace::eClockwise )
      // 深度バイアスを使わない
      .setDepthBiasEnable( false )
      // 線を描く時は太さ1.0で
      .setLineWidth( 1.0f );

  // マルチサンプルの設定
  const auto multisample =
    // 全部デフォルト(マルチサンプルを使わない)
    vk::PipelineMultisampleStateCreateInfo();

  // 深度とステンシルの設定
  const auto depth_stencil =
    vk::PipelineDepthStencilStateCreateInfo()
      // 深度テストをする
      .setDepthTestEnable( true )
      // 深度値を深度バッファに書く
      .setDepthWriteEnable( true )
      // 深度値がより小さい場合手前と見做す
      .setDepthCompareOp( vk::CompareOp::eLessOrEqual )
      // 深度の範囲を制限しない
      .setDepthBoundsTestEnable( false )
      // ステンシルテストをしない
      .setStencilTestEnable( false );

  // カラーブレンドの設定
  const auto color_blend_attachment =
    vk::PipelineColorBlendAttachmentState()
      // フレームバッファに既にある色と新しい色を混ぜない
      // (新しい色で上書きする)
      .setBlendEnable( false )
      // RGBA全ての要素を書く
      .setColorWriteMask(
        vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA
      );
  const auto color_blend =
    vk::PipelineColorBlendStateCreateInfo()
      // 論理演算をしない
      .setLogicOpEnable( false )
      // 論理演算をする場合clearを使う
      .setLogicOp( vk::LogicOp::eClear )
      // カラーアタッチメント1つ分の設定がある
      .setAttachmentCount( 1 )
      // このブレンドの設定を0番目のカラーアタッチメントで使う
      .setPAttachments( &color_blend_attachment )
      // カラーブレンドに使う定数
      .setBlendConstants( { 0.f, 0.f, 0.f, 0.f } );


  // 後から変更できるパラメータの設定
  const std::vector< vk::DynamicState > dynamic_state{
    vk::DynamicState::eViewport,
    vk::DynamicState::eScissor
  };
  const auto dynamic =
    vk::PipelineDynamicStateCreateInfo()
      .setDynamicStateCount( dynamic_state.size() )
      .setPDynamicStates( dynamic_state.data() );

  // テッセレーションの設定
  const auto tessellation =
    // 全部デフォルト(テッセレーションを使わない)
    vk::PipelineTessellationStateCreateInfo();

  const auto create_info = 
    vk::GraphicsPipelineCreateInfo()
      .setStageCount( shader.size() )
      .setPStages( shader.data() )
      .setPVertexInputState( &vistat )
      .setPInputAssemblyState( &input_assembly )
      .setPTessellationState( &tessellation )
      .setPViewportState( &viewport )
      .setPRasterizationState( &rasterization )
      .setPMultisampleState( &multisample )
      .setPDepthStencilState( &depth_stencil )
      .setPColorBlendState( &color_blend )
      .setPDynamicState( &dynamic )
      .setLayout( **pipeline_layout )
      // このパイプラインレイアウトで
      .setRenderPass( *render_pass )
      // このレンダーパスの0番目のサブパスとして使う
      .setSubpass( 0 );
  
  // グラフィクスパイプラインを作る
  auto pipeline = (*device)->createGraphicsPipelinesUnique(
    **pipeline_cache,
    { create_info },
    nullptr
  );

}

