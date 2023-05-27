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
#include <gct/device_address.hpp>
#include <gct/ray_tracing_pipeline.hpp>
#include <gct/command_buffer.hpp>
#include <gct/command_pool.hpp>
#include <gct/framebuffer.hpp>
#include <gct/render_pass.hpp>
#include <gct/descriptor_pool.hpp>

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
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME
  } );
  
  const auto device = selected.create_device(
    std::vector< gct::queue_requirement_t >{
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

  auto descriptor_pool = device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          .setMaxSets( 10000 )
      )
      .set_descriptor_pool_size( vk::DescriptorType::eUniformBuffer, 400 )
      .set_descriptor_pool_size( vk::DescriptorType::eStorageBuffer, 400 )
      .set_descriptor_pool_size( vk::DescriptorType::eStorageImage, 400 )
      .set_descriptor_pool_size( vk::DescriptorType::eCombinedImageSampler, 4000 )
      .set_descriptor_pool_size( vk::DescriptorType::eAccelerationStructureKHR, 400 )
      .rebuild_chain()
  );

  const auto rgen = device->get_shader_module(
    "../shaders/simple.rgen.spv"
  );
  const auto rchit = device->get_shader_module(
    "../shaders/simple.rchit.spv"
  );
  const auto rmiss = device->get_shader_module(
    "../shaders/simple.rmiss.spv"
  );
  const auto rmiss2 = device->get_shader_module(
    "../shaders/simple2.rmiss.spv"
  );
  
  const auto descriptor_set_layout = device->get_descriptor_set_layout(
    gct::descriptor_set_layout_create_info_t()
      .add_binding(
        rgen->get_props().get_reflection()
      )
      .add_binding(
        rmiss->get_props().get_reflection()
      )
      .add_binding(
        rmiss2->get_props().get_reflection()
      )
      .add_binding(
        rchit->get_props().get_reflection()
      )
      .rebuild_chain()
  );
 
  auto pipeline_cache = device->get_pipeline_cache();

  auto pipeline_layout = device->get_pipeline_layout(
    gct::pipeline_layout_create_info_t()
      .add_descriptor_set_layout( descriptor_set_layout )
  );

  // レイトレーシングパイプラインを作る
  auto ray_tracing_pipeline = pipeline_cache->get_pipeline(
    gct::ray_tracing_pipeline_create_info_t()
      .set_basic(
        vk::RayTracingPipelineCreateInfoKHR()
          // 再帰的な交差判定の最大回数
          .setMaxPipelineRayRecursionDepth( 5 )
      )
      .add_stage( rgen )
      .add_stage( rmiss )
      .add_stage( rmiss2 )
      .add_stage( rchit )
      .set_layout( pipeline_layout )
  );
}

