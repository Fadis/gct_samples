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
#include <gct/round_up.hpp>
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
  auto &rtprops = selected.devices[ 0 ]->get_props().get_ray_tracing_pipeline();

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

  auto ray_tracing_pipeline = pipeline_cache->get_pipeline(
    gct::ray_tracing_pipeline_create_info_t()
      .set_basic(
        vk::RayTracingPipelineCreateInfoKHR()
          .setMaxPipelineRayRecursionDepth( 5 )
      )
      .add_stage( rgen )
      .add_stage( rmiss )
      .add_stage( rmiss2 )
      .add_stage( rchit )
      .set_layout( pipeline_layout )
  );

  // Raygenグループの数
  const std::uint32_t raygen_shader_count = 1u;
  // Missグループの数
  const std::uint32_t miss_shader_count = 2u;
  // Hitグループの数
  const std::uint32_t hit_shader_count = 1u;
  // Raygenのハンドラのオフセットと間隔
  const std::uint32_t raygen_shader_binding_offset = 0;
  const std::uint32_t raygen_shader_binding_stride = rtprops.shaderGroupHandleSize;
  const std::uint32_t raygen_shader_table_size = raygen_shader_count * raygen_shader_binding_stride;
  // Missのハンドラのオフセットと間隔
  const std::uint32_t miss_shader_binding_offset =
    raygen_shader_binding_offset +
    gct::round_up( raygen_shader_table_size, rtprops.shaderGroupBaseAlignment );
  const std::uint32_t miss_shader_binding_stride = rtprops.shaderGroupHandleSize;
  const std::uint32_t miss_shader_table_size = miss_shader_count * miss_shader_binding_stride;
  // Hitのハンドラのオフセットと間隔
  const std::uint32_t hit_shader_binding_offset =
    miss_shader_binding_offset +
    gct::round_up( miss_shader_table_size, rtprops.shaderGroupBaseAlignment );
  const std::uint32_t hit_shader_binding_stride = rtprops.shaderGroupHandleSize;
  const std::uint32_t hit_shader_table_size = hit_shader_count * hit_shader_binding_stride;
  // シェーダバインディングテーブル全体のサイズ
  const std::uint32_t shader_binding_table_size = hit_shader_binding_offset + hit_shader_table_size;

  std::vector< std::uint8_t > shader_binding_table_data( shader_binding_table_size );
  // Raygenグループのハンドラを書く
  const auto get_raygen_handles_result = (*device)->getRayTracingShaderGroupHandlesKHR(
    **ray_tracing_pipeline,
    0u,
    raygen_shader_count,
    raygen_shader_table_size,
    std::next( shader_binding_table_data.data(), raygen_shader_binding_offset )
  );
  if( get_raygen_handles_result != vk::Result::eSuccess ) {
    vk::throwResultException( get_raygen_handles_result, "getRayTracingShaderGroupHandlesKHR failed" );
  }
  // Missグループのハンドラを書く
  const auto get_miss_handles_result = (*device)->getRayTracingShaderGroupHandlesKHR(
    **ray_tracing_pipeline,
    raygen_shader_count,
    miss_shader_count,
    miss_shader_table_size,
    std::next( shader_binding_table_data.data(), miss_shader_binding_offset )
  );
  if( get_miss_handles_result != vk::Result::eSuccess ) {
    vk::throwResultException( get_miss_handles_result, "getRayTracingShaderGroupHandlesKHR failed" );
  }
  // Hitグループのハンドラを書く
  const auto get_hit_handles_result = (*device)->getRayTracingShaderGroupHandlesKHR(
    **ray_tracing_pipeline,
    raygen_shader_count + miss_shader_count,
    hit_shader_count,
    hit_shader_table_size,
    std::next( shader_binding_table_data.data(), hit_shader_binding_offset )
  );
  if( get_hit_handles_result != vk::Result::eSuccess ) {
    vk::throwResultException( get_hit_handles_result, "getRayTracingShaderGroupHandlesKHR failed" );
  }
  std::cout << nlohmann::json( shader_binding_table_data ).dump() << std::endl;
}

