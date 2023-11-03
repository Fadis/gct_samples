#include <cmath>
#include <iostream>
#include <nlohmann/json.hpp>
#include <glm/mat2x2.hpp>
#include <gct/glfw.hpp>
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
#include <gct/wait_for_sync.hpp>
#include <gct/present_info.hpp>
#include <gct/fence.hpp>
#include <gct/semaphore.hpp>
#include <gct/command_buffer.hpp>
#include <gct/command_pool.hpp>
#include <gct/framebuffer.hpp>
#include <gct/render_pass.hpp>
#include <gct/descriptor_pool.hpp>

struct fb_resources_t {
  std::shared_ptr< gct::image_t > color_image;
  std::shared_ptr< gct::image_t > depth_image;
  std::shared_ptr< gct::framebuffer_t > framebuffer;
  std::shared_ptr< gct::semaphore_t > image_acquired;
  std::shared_ptr< gct::semaphore_t > draw_complete;
  std::shared_ptr< gct::semaphore_t > image_ownership;
  std::shared_ptr< gct::bound_command_buffer_t > command_buffer;
  std::shared_ptr< gct::fence_t > fence;
  bool initial = true;
};

int main() {
  gct::glfw::get();
  uint32_t iext_count = 0u;
  auto exts = glfwGetRequiredInstanceExtensions( &iext_count );
  std::vector< const char* > iext{};
  for( uint32_t i = 0u; i != iext_count; ++i )
    iext.push_back( exts[ i ] );
  
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
  unsigned int width = 1024; 
  unsigned int height = 1024; 
  gct::glfw_window window(
    width,
    height,
    "my_window",
    false
  );

  auto groups = instance->get_physical_devices( {} );
  auto selected = groups[ 0 ].with_extensions( {
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  } );
  
  auto surface = window.get_surface( *groups[ 0 ].devices[ 0 ] );

  const auto device = selected.create_device(
    std::vector< gct::queue_requirement_t >{
      gct::queue_requirement_t{
        vk::QueueFlagBits::eGraphics,
        0u,
        vk::Extent3D(),
#ifdef VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME
        vk::QueueGlobalPriorityEXT(),
#endif
        { **surface },
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer
      }
    },
    gct::device_create_info_t()
  );
  
  // スワップチェーンを作る
  auto swapchain = device->get_swapchain( surface );
  // スワップチェーンからイメージを取得する
  auto swapchain_images = swapchain->get_images();
  
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

  const auto vs = device->get_shader_module(
    "../shaders/simple.vert.spv"
  );

  const auto fs = device->get_shader_module(
    "../shaders/simple.frag.spv"
  );
  
  const auto descriptor_set_layout = device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding(
        vs->get_props().get_reflection()
      )
      .add_binding(
        fs->get_props().get_reflection()
      )
  );
  
  const auto descriptor_set = descriptor_pool->allocate( descriptor_set_layout );

  const auto pipeline_layout = device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      .add_descriptor_set_layout( descriptor_set_layout )
  );

  const auto render_pass = device->get_render_pass(
    gct::render_pass_create_info_t()
      .add_attachment(
        vk::AttachmentDescription()
          .setFormat( surface->get_caps().get_formats()[ 0 ].basic.format )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setLoadOp( vk::AttachmentLoadOp::eClear )
          .setStoreOp( vk::AttachmentStoreOp::eStore )
          .setStencilLoadOp( vk::AttachmentLoadOp::eDontCare )
          .setStencilStoreOp( vk::AttachmentStoreOp::eDontCare )
          .setInitialLayout( vk::ImageLayout::eUndefined )
          .setFinalLayout( vk::ImageLayout::ePresentSrcKHR )
      )
      .add_attachment(
        vk::AttachmentDescription()
          .setFormat( vk::Format::eD16Unorm )
          .setSamples( vk::SampleCountFlagBits::e1 )
          .setLoadOp( vk::AttachmentLoadOp::eClear )
          .setStoreOp( vk::AttachmentStoreOp::eStore )
          .setStencilLoadOp( vk::AttachmentLoadOp::eDontCare )
          .setStencilStoreOp( vk::AttachmentStoreOp::eDontCare )
          .setInitialLayout( vk::ImageLayout::eUndefined )
          .setFinalLayout( vk::ImageLayout::eDepthStencilAttachmentOptimal )
      )
      .add_subpass(
        gct::subpass_description_t()
          .add_color_attachment( 0, vk::ImageLayout::eColorAttachmentOptimal )
          .set_depth_stencil_attachment( 1, vk::ImageLayout::eDepthStencilAttachmentOptimal )
          .rebuild_chain()
      )
    );

  const auto pipeline_cache = device->get_pipeline_cache();

  const auto stencil_op = vk::StencilOpState()
    .setCompareOp( vk::CompareOp::eAlways )
    .setFailOp( vk::StencilOp::eKeep )
    .setPassOp( vk::StencilOp::eKeep );

  auto vistat = gct::pipeline_vertex_input_state_create_info_t()
    .add_vertex_input_binding_description(
      vk::VertexInputBindingDescription()
        .setBinding( 0 )
        .setInputRate( vk::VertexInputRate::eVertex )
        .setStride( sizeof( float ) * 3 )
    )
    .add_vertex_input_attribute_description(
      vk::VertexInputAttributeDescription()
        .setLocation( 0 )
        .setFormat( vk::Format::eR32G32B32Sfloat )
        .setBinding( 0 )
        .setOffset( 0 )
    );

  const auto input_assembly =
    gct::pipeline_input_assembly_state_create_info_t()
      .set_basic(
        vk::PipelineInputAssemblyStateCreateInfo()
          .setTopology( vk::PrimitiveTopology::eTriangleList )
      );

  const auto viewport =
    gct::pipeline_viewport_state_create_info_t()
      .set_basic(
        vk::PipelineViewportStateCreateInfo()
          .setViewportCount( 1 )
          .setScissorCount( 1 )
      );

  const auto rasterization =
    gct::pipeline_rasterization_state_create_info_t()
      .set_basic(
        vk::PipelineRasterizationStateCreateInfo()
          .setDepthClampEnable( false )
          .setRasterizerDiscardEnable( false )
          .setPolygonMode( vk::PolygonMode::eFill )
          .setCullMode( vk::CullModeFlagBits::eNone )
          .setFrontFace( vk::FrontFace::eClockwise )
          .setDepthBiasEnable( false )
          .setLineWidth( 1.0f )
      );

  const auto multisample =
    gct::pipeline_multisample_state_create_info_t()
      .set_basic(
        vk::PipelineMultisampleStateCreateInfo()
      );

  const auto depth_stencil =
    gct::pipeline_depth_stencil_state_create_info_t()
      .set_basic(
        vk::PipelineDepthStencilStateCreateInfo()
          .setDepthTestEnable( true )
          .setDepthWriteEnable( true )
          .setDepthCompareOp( vk::CompareOp::eLessOrEqual )
          .setDepthBoundsTestEnable( false )
          .setStencilTestEnable( false )
          .setFront( stencil_op )
          .setBack( stencil_op )
      );

  const auto color_blend =
    gct::pipeline_color_blend_state_create_info_t()
      .add_attachment(
        vk::PipelineColorBlendAttachmentState()
          .setBlendEnable( false )
          .setColorWriteMask(
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA
          )
      );

  const auto dynamic =
    gct::pipeline_dynamic_state_create_info_t()
      .add_dynamic_state( vk::DynamicState::eViewport )
      .add_dynamic_state( vk::DynamicState::eScissor );

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
      .set_layout( pipeline_layout )
      .set_render_pass( render_pass, 0 )
  );
  
  VmaAllocatorCreateInfo allocator_create_info{};
  allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  auto allocator = device->get_allocator(
    allocator_create_info
  );

  std::vector< fb_resources_t > fbs;

  for( unsigned int i = 0u; i != swapchain_images.size(); ++i ) {
    auto depth = allocator->create_image(
      gct::image_create_info_t()
        .set_basic(
          vk::ImageCreateInfo()
            .setImageType( vk::ImageType::e2D )
            .setFormat( vk::Format::eD16Unorm )
            .setExtent( swapchain_images[ i ]->get_props().get_basic().extent )
            .setUsage( vk::ImageUsageFlagBits::eDepthStencilAttachment )
        ),
      VMA_MEMORY_USAGE_GPU_ONLY
    );
    auto depth_view = depth->get_view( vk::ImageAspectFlagBits::eDepth );
    auto color_view = swapchain_images[ i ]->get_view( vk::ImageAspectFlagBits::eColor );
    auto framebuffer = render_pass->get_framebuffer(
      gct::framebuffer_create_info_t()
        .add_attachment( color_view )
        .add_attachment( depth_view )
    );
    fbs.push_back( {
      swapchain_images[ i ],
      depth,
      framebuffer,
      device->get_semaphore(),
      device->get_semaphore(),
      device->get_semaphore(),
      queue->get_command_pool()->allocate(),
      device->get_fence()
    } );
  }

  std::shared_ptr< gct::buffer_t > vertex_buffer;
  {
    const auto command_buffer = queue->get_command_pool()->allocate();
    {
      auto rec = command_buffer->begin();
      for( unsigned i = 0; i != fbs.size(); ++i )
        rec.convert_image(
          swapchain_images[ i ],
          vk::ImageLayout::eColorAttachmentOptimal
        );
 
      const std::vector< float > vertex{
        0.f, 0.f, 0.f,
        0.f, 1.f, 0.f,
        1.f, 0.f, 0.f
      };
      vertex_buffer = rec.load_buffer(
        allocator,
        vertex.data(),
        sizeof( float ) * vertex.size(),
        vk::BufferUsageFlagBits::eVertexBuffer
      );
      rec.barrier(
        vk::AccessFlagBits::eTransferWrite,
        vk::AccessFlagBits::eVertexAttributeRead,
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eVertexInput,
        vk::DependencyFlagBits( 0 ),
        { vertex_buffer },
        {}
      );
    }
    command_buffer->execute(
      gct::submit_info_t()
    );
    command_buffer->wait_for_executed();
  }

  const std::array< vk::ClearValue, 2 > clear_values{
    vk::ClearColorValue(
      std::array< float, 4u >{ 1.0f, 1.0f, 1.0f, 1.0f }
    ),
    vk::ClearDepthStencilValue( 1.f, 0 )
  };

  unsigned int current_frame = 0u;
  while( 1 ) {
    const auto begin_time = std::chrono::high_resolution_clock::now();
    auto &sync = fbs[ current_frame ];
    // コマンドバッファの以前のコマンドが完了するのを待つ
    if( !sync.initial ) {
      if( (*device)->waitForFences(
        1,
        &**sync.fence,
        true,
        UINT64_MAX
      ) != vk::Result::eSuccess ) abort();
      if( (*device)->resetFences(
        1,
        &**sync.fence
      ) != vk::Result::eSuccess ) abort();
    }
    else sync.initial = false;
    // スワップチェーンから描画に使えるイメージを貰う
    std::uint32_t image_index = (*device)->acquireNextImageKHR(
        // このスワップチェーンから貰う
        **swapchain,
        // 今すぐイメージを得られない時は貰えるまで待つ
        UINT64_MAX,
        // もらったイメージが描ける状態になったらこのセマフォに通知
        **sync.image_acquired,
        // フェンスへの通知はしない
        vk::Fence()
      ).value;
    auto &fb = fbs[ image_index ];
    {
      auto rec = fb.command_buffer->begin();
 
      rec->beginRenderPass(
        vk::RenderPassBeginInfo()
          .setRenderPass( **render_pass )
          .setFramebuffer( **fb.framebuffer )
          .setRenderArea(
            vk::Rect2D()
              .setOffset( { 0, 0 } )
              .setExtent( { width, height } )
          )
          .setClearValueCount( clear_values.size() )
          .setPClearValues( clear_values.data() ),
        vk::SubpassContents::eInline
      );
      rec->setViewport(
        0,
        {
          vk::Viewport()
            .setWidth( width )
            .setHeight( height )
            .setMinDepth( 0.0f )
            .setMaxDepth( 1.0f )
        }
      );
      rec->setScissor(
        0,
        {
          vk::Rect2D()
            .setOffset( { 0, 0 } )
            .setExtent( { width, height } )
        }
      );
      rec->bindPipeline(
        vk::PipelineBindPoint::eGraphics,
        **pipeline
      );
      rec->bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        **pipeline_layout,
        0,
        **descriptor_set,
        {}
      );
      rec->bindVertexBuffers( 0, { **vertex_buffer }, { 0 } );
      rec->draw( 3, 1, 0, 0 );
      rec->endRenderPass();
      
    }

    // コマンドバッファを実行
    vk::PipelineStageFlags dest_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    (*queue)->submit(
      vk::SubmitInfo()
        // このコマンドバッファの内容をキューに流す
        .setCommandBufferCount( 1 )
        .setPCommandBuffers( &**fb.command_buffer )
        // このセマフォを待ってから
        .setWaitSemaphoreCount( 1 )
        .setPWaitSemaphores( &**sync.image_acquired )
        .setPWaitDstStageMask( &dest_stage_mask )
        // 終わったらこのセマフォに通知
        .setSignalSemaphoreCount( 1 )
        .setPSignalSemaphores( &**sync.draw_complete ),
      **fb.fence
    );
    
    // スワップチェーンのイメージを表示にまわす
    if( (*queue)->presentKHR(
      vk::PresentInfoKHR()
        // このセマフォを待ってから
        .setWaitSemaphoreCount( 1 )
        .setPWaitSemaphores( &**sync.draw_complete )
        // このスワップチェーンのこのイメージを表示にまわす
        .setSwapchainCount( 1 )
        .setPSwapchains( &**swapchain )
        .setPImageIndices( &image_index )
    ) != vk::Result::eSuccess ) abort();
    glfwPollEvents();
    ++current_frame;
    current_frame %= fbs.size();
    gct::wait_for_sync( begin_time );
  }
}

