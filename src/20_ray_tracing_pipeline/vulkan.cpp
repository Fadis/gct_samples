#include <cmath>
#include <future>
#include <iostream>
#include <functional>
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
#include <gct/io_context.hpp>

std::future< VkResult > async(
  const VkDevice &device,
  const VkDeferredOperationKHR &deferred_operation
) {
  uint32_t(*vkGetDeferredOperationMaxConcurrencyKHR)( VkDevice, VkDeferredOperationKHR ) =
    reinterpret_cast< uint32_t(*)( VkDevice, VkDeferredOperationKHR ) >(
      vkGetDeviceProcAddr( device, "vkGetDeferredOperationMaxConcurrencyKHR" )
    );
  const auto max = vkGetDeferredOperationMaxConcurrencyKHR(
    device,
    deferred_operation
  );
  std::shared_ptr< std::promise< VkResult > > p( new std::promise< VkResult > () );
  auto f = p->get_future();
  gct::async(
    max,
    [p,deferred_operation,device]() {
      VkResult(*vkDeferredOperationJoinKHR)( VkDevice, VkDeferredOperationKHR ) =
        reinterpret_cast< VkResult(*)( VkDevice, VkDeferredOperationKHR ) >(
          vkGetDeviceProcAddr( device, "vkDeferredOperationJoinKHR" )
        );
      VkResult(*vkGetDeferredOperationResultKHR)( VkDevice, VkDeferredOperationKHR ) =
        reinterpret_cast< VkResult(*)( VkDevice, VkDeferredOperationKHR ) >(
          vkGetDeviceProcAddr( device, "vkGetDeferredOperationResultKHR" )
        );
      auto result = vkDeferredOperationJoinKHR(
        device,
        deferred_operation
      );
      if( result == VK_SUCCESS ) {
        p->set_value(
          vkGetDeferredOperationResultKHR(
            device,
            deferred_operation
          )
        );
      }
    }
  );
  return f;
}

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

  // ????????????????????????
  std::vector< VkPipelineShaderStageCreateInfo > shaders( 4 );
  shaders[ 0 ].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaders[ 0 ].pNext = nullptr;
  shaders[ 0 ].flags = 0;
  // Raygen?????????????????????
  shaders[ 0 ].stage = VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  // ??????????????????????????????????????????
  shaders[ 0 ].module = **rgen;
  // ???????????????????????????????????????main
  shaders[ 0 ].pName = "main";
  shaders[ 0 ].pSpecializationInfo = nullptr;
  shaders[ 1 ].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaders[ 1 ].pNext = nullptr;
  shaders[ 1 ].flags = 0;
  // Miss?????????????????????
  shaders[ 1 ].stage = VkShaderStageFlagBits::VK_SHADER_STAGE_MISS_BIT_KHR;
  // ??????????????????????????????????????????
  shaders[ 1 ].module = **rmiss;
  // ???????????????????????????????????????main
  shaders[ 1 ].pName = "main";
  shaders[ 1 ].pSpecializationInfo = nullptr;
  shaders[ 2 ].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaders[ 2 ].pNext = nullptr;
  shaders[ 2 ].flags = 0;
  // Miss?????????????????????
  shaders[ 2 ].stage = VkShaderStageFlagBits::VK_SHADER_STAGE_MISS_BIT_KHR;
  // ??????????????????????????????????????????
  shaders[ 2 ].module = **rmiss;
  // ???????????????????????????????????????main
  shaders[ 2 ].pName = "main";
  shaders[ 2 ].pSpecializationInfo = nullptr;
  shaders[ 3 ].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaders[ 3 ].pNext = nullptr;
  shaders[ 3 ].flags = 0;
  // Closest HIt?????????????????????
  shaders[ 3 ].stage = VkShaderStageFlagBits::VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  // ??????????????????????????????????????????
  shaders[ 3 ].module = **rchit;
  // ???????????????????????????????????????main
  shaders[ 3 ].pName = "main";
  shaders[ 3 ].pSpecializationInfo = nullptr;

  // ?????????????????????????????????
  std::vector< VkRayTracingShaderGroupCreateInfoKHR > shader_groups( 4 );
  shader_groups[ 0 ].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
  shader_groups[ 0 ].pNext = nullptr;
  // ??????(Raygen???Miss???Callable)?????????????????????
  shader_groups[ 0 ].type =
    VkRayTracingShaderGroupTypeKHR::VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  // ?????????0?????????????????????
  shader_groups[ 0 ].generalShader = 0;
  shader_groups[ 0 ].closestHitShader = VK_SHADER_UNUSED_KHR;
  shader_groups[ 0 ].anyHitShader = VK_SHADER_UNUSED_KHR;
  shader_groups[ 0 ].intersectionShader = VK_SHADER_UNUSED_KHR;
  shader_groups[ 0 ].pShaderGroupCaptureReplayHandle = nullptr;
  shader_groups[ 1 ].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
  shader_groups[ 1 ].pNext = nullptr;
  // ??????(Raygen???Miss???Callable)?????????????????????
  shader_groups[ 1 ].type =
    VkRayTracingShaderGroupTypeKHR::VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  // ?????????1?????????????????????
  shader_groups[ 1 ].generalShader = 1;
  shader_groups[ 1 ].closestHitShader = VK_SHADER_UNUSED_KHR;
  shader_groups[ 1 ].anyHitShader = VK_SHADER_UNUSED_KHR;
  shader_groups[ 1 ].intersectionShader = VK_SHADER_UNUSED_KHR;
  shader_groups[ 1 ].pShaderGroupCaptureReplayHandle = nullptr;
  shader_groups[ 2 ].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
  shader_groups[ 2 ].pNext = nullptr;
  // ??????(Raygen???Miss???Callable)?????????????????????
  shader_groups[ 2 ].type =
    VkRayTracingShaderGroupTypeKHR::VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  // ?????????2?????????????????????
  shader_groups[ 2 ].generalShader = 1;
  shader_groups[ 2 ].closestHitShader = VK_SHADER_UNUSED_KHR;
  shader_groups[ 2 ].anyHitShader = VK_SHADER_UNUSED_KHR;
  shader_groups[ 2 ].intersectionShader = VK_SHADER_UNUSED_KHR;
  shader_groups[ 2 ].pShaderGroupCaptureReplayHandle = nullptr;
  shader_groups[ 3 ].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
  shader_groups[ 3 ].pNext = nullptr;
  // ??????????????????????????????????????????????????????????????????
  shader_groups[ 3 ].type =
    VkRayTracingShaderGroupTypeKHR::VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
  shader_groups[ 3 ].generalShader = VK_SHADER_UNUSED_KHR;
  // ?????????????????????????????????????????????3??????????????????????????????
  shader_groups[ 3 ].closestHitShader = 3;
  // ????????????????????????????????????????????????????????????(??????)
  shader_groups[ 3 ].anyHitShader = VK_SHADER_UNUSED_KHR;
  shader_groups[ 3 ].intersectionShader = VK_SHADER_UNUSED_KHR;
  shader_groups[ 3 ].pShaderGroupCaptureReplayHandle = nullptr;

  // ??????????????????????????????
  VkResult(*vkCreateDeferredOperationKHR)(
    VkDevice,
    const VkAllocationCallbacks*,
    VkDeferredOperationKHR*
  ) =
    reinterpret_cast< VkResult(*)(
      VkDevice,
      const VkAllocationCallbacks*,
      VkDeferredOperationKHR*
    ) >(
      vkGetDeviceProcAddr(
        **device,
        "vkCreateDeferredOperationKHR"
      )
    );
  VkDeferredOperationKHR deffered_operation;
  if( vkCreateDeferredOperationKHR(
    **device,
    nullptr,
    &deffered_operation
  ) != VK_SUCCESS ) abort();

  // ???????????????????????????????????????????????????
  VkRayTracingPipelineCreateInfoKHR create_info;
  create_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
  create_info.pNext = nullptr;
  create_info.flags = 0;
  // ???????????????????????????
  create_info.stageCount = shaders.size();
  create_info.pStages = shaders.data();
  // ???????????????????????????????????????
  create_info.groupCount = shader_groups.size();
  create_info.pGroups = shader_groups.data();
  // ???????????????????????????????????????
  create_info.maxPipelineRayRecursionDepth = 5;
  create_info.pLibraryInfo = nullptr;
  create_info.pLibraryInterface = nullptr;
  create_info.pDynamicState = nullptr;
  // ????????????????????????????????????????????????
  create_info.layout = **pipeline_layout;
  create_info.basePipelineHandle = VK_NULL_HANDLE;
  create_info.basePipelineIndex = 0;
  VkPipeline pipeline;
  VkResult(*vkCreateRayTracingPipelinesKHR)(
    VkDevice,
    VkDeferredOperationKHR,
    VkPipelineCache,
    uint32_t,
    const VkRayTracingPipelineCreateInfoKHR*,
    const VkAllocationCallbacks*,
    VkPipeline*
  ) =
    reinterpret_cast< VkResult(*)(
      VkDevice,
      VkDeferredOperationKHR,
      VkPipelineCache,
      uint32_t,
      const VkRayTracingPipelineCreateInfoKHR*,
      const VkAllocationCallbacks*,
      VkPipeline*
    ) >(
      vkGetDeviceProcAddr(
        **device,
        "vkCreateRayTracingPipelinesKHR"
      )
    );
  if( vkCreateRayTracingPipelinesKHR(
    **device,
    // ??????????????????????????????
    deffered_operation,
    // ???????????????????????????????????????????????????
    **pipeline_cache,
    // ?????????????????????1?????????
    1,
    &create_info,
    nullptr,
    &pipeline
  ) != VK_OPERATION_DEFERRED_KHR ) abort();
  // ?????????????????????????????????????????????
  auto future = async( **device, deffered_operation );
  // ??????????????????????????????????????????
  if( future.get() != VK_SUCCESS ) abort();

  // ?????????????????????????????????
  vkDestroyPipeline(
    **device,
    pipeline,
    nullptr
  );

  // ???????????????????????????
  void(*vkDestroyDeferredOperationKHR)(
    VkDevice,
    VkDeferredOperationKHR,
    const VkAllocationCallbacks*
  ) =
    reinterpret_cast< void(*)(
      VkDevice,
      VkDeferredOperationKHR,
      const VkAllocationCallbacks*
    ) >(
      vkGetDeviceProcAddr(
        **device,
        "vkDestroyDeferredOperationKHR"
      )
    );
  vkDestroyDeferredOperationKHR(
    **device,
    deffered_operation,
    nullptr
  );
}

