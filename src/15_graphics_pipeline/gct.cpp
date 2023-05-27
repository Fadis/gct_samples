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
#include <gct/command_buffer.hpp>
#include <gct/command_pool.hpp>
#include <gct/framebuffer.hpp>
#include <gct/render_pass.hpp>

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

  // レンダーパスを作る
  const auto render_pass = device->get_render_pass(
    gct::render_pass_create_info_t()
      // 色を出力するアタッチメントを追加
      .add_attachment(
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
          .setFinalLayout( vk::ImageLayout::ePresentSrcKHR )
      )
      // 深度を出力するアタッチメントを追加
      .add_attachment(
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
      )
      // サブパスを追加
      .add_subpass(
        gct::subpass_description_t()
          // 0番目のアタッチメントに色を出力
          .add_color_attachment( 0, vk::ImageLayout::eColorAttachmentOptimal )
          // 1番目のアタッチメントに深度を出力
          .set_depth_stencil_attachment( 1, vk::ImageLayout::eDepthStencilAttachmentOptimal )
      )
    );

  const auto pipeline_cache = device->get_pipeline_cache();

  // 何もしないステンシルの設定
  const auto stencil_op = vk::StencilOpState()
    // 常にtrue
    .setCompareOp( vk::CompareOp::eAlways )
    // 比較結果がtrueなら通過
    .setFailOp( vk::StencilOp::eKeep )
    // 比較結果がfalseなら通過
    .setPassOp( vk::StencilOp::eKeep );

  // 頂点配列の読み方
 
  auto vistat = gct::pipeline_vertex_input_state_create_info_t()
    .add_vertex_input_binding_description(
      vk::VertexInputBindingDescription()
        // 頂点配列binding 0番は
        .setBinding( 0 )
        // 頂点1個毎に
        .setInputRate( vk::VertexInputRate::eVertex )
        // 12バイト移動しながら読む
        .setStride( sizeof( float ) * 3 )
    )
    .add_vertex_input_attribute_description(
      vk::VertexInputAttributeDescription()
        // 頂点の座標は
        .setLocation( 0 )
        // floatが3つで1要素になっていて
        .setFormat( vk::Format::eR32G32B32Sfloat )
        // 頂点配列binding 0番の読み方で
        .setBinding( 0 )
        // 先頭から読む
        .setOffset( 0 )
    );

  // プリミティブの組み立て方
  const auto input_assembly =
    gct::pipeline_input_assembly_state_create_info_t()
      .set_basic(
        vk::PipelineInputAssemblyStateCreateInfo()
          // 頂点配列の要素3個毎に1つの三角形
          .setTopology( vk::PrimitiveTopology::eTriangleList )
      );

  // ビューポートとシザーの設定
  const auto viewport =
    gct::pipeline_viewport_state_create_info_t()
      .set_basic(
        vk::PipelineViewportStateCreateInfo()
          // 1個のビューポートと
          .setViewportCount( 1 )
          // 1個のシザーを使う
          .setScissorCount( 1 )
          // 具体的な値は後でDynamicStateを使って設定する
      );

  // ラスタライズの設定
  const auto rasterization =
    gct::pipeline_rasterization_state_create_info_t()
      .set_basic(
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
          .setLineWidth( 1.0f )
      );

  // マルチサンプルの設定
  const auto multisample =
    gct::pipeline_multisample_state_create_info_t()
      .set_basic(
        // 全部デフォルト(マルチサンプルを使わない)
        vk::PipelineMultisampleStateCreateInfo()
      );

  // 深度とステンシルの設定
  const auto depth_stencil =
    gct::pipeline_depth_stencil_state_create_info_t()
      .set_basic(
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
          .setStencilTestEnable( false )
          // ステンシルテストの演算を指定
          .setFront( stencil_op )
          .setBack( stencil_op )
      );

  // カラーブレンドの設定
  const auto color_blend =
    gct::pipeline_color_blend_state_create_info_t()
      .add_attachment(
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
          )
      );

  // 後から変更できるパラメータの設定
  const auto dynamic =
    gct::pipeline_dynamic_state_create_info_t()
      // ビューポートと
      .add_dynamic_state( vk::DynamicState::eViewport )
      // シザーの値はあとから変更できるようにする
      .add_dynamic_state( vk::DynamicState::eScissor );

  // グラフィクスパイプラインを作る
  auto pipeline = pipeline_cache->get_pipeline(
    gct::graphics_pipeline_create_info_t()
      .add_stage( vs )
      .add_stage( fs )
      .set_vertex_input( vistat )
      .set_input_assembly( input_assembly )
      .set_viewport( viewport )
      .set_rasterization( rasterization )
      .set_multisample( multisample )
      .set_depth_stencil( depth_stencil )
      .set_color_blend( color_blend )
      .set_dynamic( dynamic )
      // このパイプラインレイアウトで
      .set_layout( pipeline_layout )
      // このレンダーパスの0番目のサブパスとして使う
      .set_render_pass( render_pass, 0 )
  );

}

