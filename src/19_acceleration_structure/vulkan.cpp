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
#include <gct/acceleration_structure.hpp>
#include <gct/acceleration_structure_geometry.hpp>
#include <gct/acceleration_structure_build_geometry_info.hpp>
#include <gct/acceleration_structure_geometry_triangles_data.hpp>

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
  
  VmaAllocatorCreateInfo allocator_create_info{};
  allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  auto allocator = device->get_allocator(
    allocator_create_info
  );
  
  const auto queue = device->get_queue( 0u );
  
  // ??????????????????
  const std::vector< float > vertex{
    0.f, 0.f, 0.f,
    0.f, 1.f, 0.f,
    1.f, 0.f, 0.f
  };

  glm::mat4x3 trans( 1.0f );

  const auto command_buffer = queue->get_command_pool()->allocate();
  std::shared_ptr< gct::buffer_t > ident_matrix;
  std::shared_ptr< gct::buffer_t > vertex_buffer;
  std::shared_ptr< gct::acceleration_structure_t > blas;
  {
    auto rec = command_buffer->begin();

    ident_matrix = rec.load_buffer(
      allocator,
      reinterpret_cast< const void* >( &trans ),
      sizeof( glm::mat4x3 ),
      vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR|
      vk::BufferUsageFlagBits::eShaderDeviceAddress
    );
    
    vertex_buffer = rec.load_buffer(
      allocator,
      vertex.data(),
      sizeof( float ) * vertex.size(),
      vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR|
      vk::BufferUsageFlagBits::eShaderDeviceAddress
    );

    rec.barrier(
      vk::AccessFlagBits::eTransferWrite,
      vk::AccessFlagBits::eAccelerationStructureReadKHR,
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
      vk::DependencyFlagBits( 0 ),
      { ident_matrix, vertex_buffer },
      {}
    );
 
    // ?????????3(?????????1?????????)
    const unsigned int vertex_count = 3;
    // Acceleration Structure?????????????????????
    VkAccelerationStructureGeometryKHR geometry;
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.pNext = nullptr;
    geometry.geometryType = VkGeometryTypeKHR::VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.pNext = nullptr;
    // ???????????????????????????????????????
    geometry.geometry.triangles.indexType = VkIndexType::VK_INDEX_TYPE_NONE_KHR;
    geometry.geometry.triangles.indexData.hostAddress = nullptr;
    // ?????????float???3??????1??????
    geometry.geometry.triangles.vertexFormat = VkFormat::VK_FORMAT_R32G32B32_SFLOAT;
    // ???????????????????????????????????????
    geometry.geometry.triangles.vertexStride = sizeof( float ) * 3;
    // ?????????3??????????????????
    geometry.geometry.triangles.maxVertex = vertex_count;
    // ???????????????????????????????????????
    geometry.geometry.triangles.vertexData.deviceAddress = *vertex_buffer->get_address();
    // ?????????????????????????????????
    geometry.geometry.triangles.transformData.deviceAddress = *ident_matrix->get_address();
    geometry.flags = 0;
    // Acceleration Structure?????????
    VkAccelerationStructureBuildGeometryInfoKHR asbgi;
    asbgi.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    asbgi.pNext = nullptr;
    // BLAS?????????
    asbgi.type = VkAccelerationStructureTypeKHR::VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    // ????????????????????????
    asbgi.flags = VkBuildAccelerationStructureFlagBitsKHR::VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    // ???????????????????????????????????????????????????
    asbgi.mode = VkBuildAccelerationStructureModeKHR::VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    asbgi.geometryCount = 1;
    asbgi.pGeometries = &geometry;
    asbgi.ppGeometries = nullptr;
    std::vector< std::uint32_t > max_primitive_count{ vertex_count / 3u };
    // Acceleration Structure????????????????????????????????????????????????
    VkAccelerationStructureBuildSizesInfoKHR size;
    size.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    size.pNext = nullptr;
    void(*vkGetAccelerationStructureBuildSizesKHR)(
      VkDevice,
      VkAccelerationStructureBuildTypeKHR,
      const VkAccelerationStructureBuildGeometryInfoKHR*,
      const uint32_t*,
      VkAccelerationStructureBuildSizesInfoKHR*
    ) =
      reinterpret_cast< void(*)(
        VkDevice,
        VkAccelerationStructureBuildTypeKHR,
        const VkAccelerationStructureBuildGeometryInfoKHR*,
        const uint32_t*,
        VkAccelerationStructureBuildSizesInfoKHR*
      ) >(
        vkGetDeviceProcAddr(
          **device,
          "vkGetAccelerationStructureBuildSizesKHR"
        )
      );
    vkGetAccelerationStructureBuildSizesKHR(
      **device,
      VkAccelerationStructureBuildTypeKHR::VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &asbgi,
      max_primitive_count.data(),
      &size
    );
    
    // ?????????????????????????????????????????????????????????
    // ???????????????????????????????????????????????????????????????
    auto scratch_buffer_addr = allocator->create_buffer(
      gct::buffer_create_info_t()
        .set_basic(
          vk::BufferCreateInfo()
            // ??????????????????
            .setSize( size.buildScratchSize )
            // ?????????????????????????????????????????????????????????????????????
            .setUsage(
              vk::BufferUsageFlagBits::eStorageBuffer|
              vk::BufferUsageFlagBits::eShaderDeviceAddress
            )
        ),
      // GPU????????????????????????
      VMA_MEMORY_USAGE_GPU_ONLY
    )->get_address();

    //???Acceleration Structure????????????????????????????????????
    auto asb = allocator->create_buffer(
      gct::buffer_create_info_t()
        .set_basic(
          vk::BufferCreateInfo()
            // ??????????????????
            .setSize( size.accelerationStructureSize )
            // Acceleration Structure????????????????????????????????????
            // ????????????????????????
            .setUsage(
              vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR|
              vk::BufferUsageFlagBits::eShaderDeviceAddress
            )
        ),
      // GPU????????????????????????
      VMA_MEMORY_USAGE_GPU_ONLY
    );
    // ??????????????????Acceleration Structure?????????
    blas = asb->create_acceleration_structure(
        // BLAS???
        vk::AccelerationStructureTypeKHR::eBottomLevel
      );
    
    // 1????????????????????????????????????????????????1???
    VkAccelerationStructureBuildRangeInfoKHR range_info;
    const VkAccelerationStructureBuildRangeInfoKHR *range_info_pointer = &range_info;
    range_info.primitiveCount = vertex_count / 3u;
    range_info.primitiveOffset = 0;
    range_info.firstVertex = 0;
    range_info.transformOffset = 0;
    // ?????????Acceleration Structure?????????????????????
    asbgi.srcAccelerationStructure = **blas;
    asbgi.dstAccelerationStructure = **blas;
    // ????????????????????????????????????????????????????????????
    asbgi.scratchData.deviceAddress = *scratch_buffer_addr;
    void(*vkCmdBuildAccelerationStructuresKHR)(
      VkCommandBuffer,
      uint32_t,
      const VkAccelerationStructureBuildGeometryInfoKHR*,
      const VkAccelerationStructureBuildRangeInfoKHR* const*
    ) =
      reinterpret_cast< void(*)(
        VkCommandBuffer,
        uint32_t,
        const VkAccelerationStructureBuildGeometryInfoKHR*,
        const VkAccelerationStructureBuildRangeInfoKHR* const*
      ) >(
        vkGetDeviceProcAddr(
          **device,
          "vkCmdBuildAccelerationStructuresKHR"
        )
      );
    vkCmdBuildAccelerationStructuresKHR(
      *rec,
      1,
      &asbgi,
      &range_info_pointer
    );
  }
  command_buffer->execute(
    gct::submit_info_t()
  );

  command_buffer->wait_for_executed();


}

