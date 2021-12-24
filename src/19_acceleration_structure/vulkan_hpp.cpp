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
  
  // 頂点バッファ
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
 
    // 頂点数3(三角形1つだけ)
    const unsigned int vertex_count = 3;
    // Acceleration Structureの三角形の定義
    auto geometry = vk::AccelerationStructureGeometryKHR()
      .setGeometryType( vk::GeometryTypeKHR::eTriangles )
      .setGeometry(
        vk::AccelerationStructureGeometryDataKHR()
          .setTriangles(
            vk::AccelerationStructureGeometryTrianglesDataKHR()
              // 頂点インデックスを使わない
              .setIndexType( vk::IndexType::eNoneKHR )
              // 頂点はfloatが3つで1要素
              .setVertexFormat( vk::Format::eR32G32B32Sfloat )
              // それが密にパックされている
              .setVertexStride( sizeof( float ) * 3 )
              // 頂点は3つ並んでいる
              .setMaxVertex( vertex_count )
              // 頂点をこのバッファから読む
              .setVertexData(
                *vertex_buffer->get_address()
              )
              // この行列で変換をかける
              .setTransformData(
                *ident_matrix->get_address()
              )
          )
      );
    // Acceleration Structureの定義
    auto asbgi = vk::AccelerationStructureBuildGeometryInfoKHR()
      // BLASを作る
      .setType(
        vk::AccelerationStructureTypeKHR::eBottomLevel
      )
      // 後から更新できる
      .setFlags(
        vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate
      )
      // 以前の内容を引き継がずに新規に作る
      .setMode(
        vk::BuildAccelerationStructureModeKHR::eBuild
      )
      .setGeometryCount(
        1
      )
      .setPGeometries(
        &geometry
      );
    // プリミティブは1つだけ
    std::vector< std::uint32_t > max_primitive_count{ vertex_count / 3u };
    // Acceleration Structureに必要なバッファのサイズを求める
    auto size = (*device)->getAccelerationStructureBuildSizesKHR(
      vk::AccelerationStructureBuildTypeKHR::eDevice,
      asbgi,
      max_primitive_count
    );
    
    // 一時的なデータを置く為のバッファを作る
    // バッファを作ったら先頭アドレスを取っておく
    auto scratch_buffer_addr = allocator->create_buffer(
      gct::buffer_create_info_t()
        .set_basic(
          vk::BufferCreateInfo()
            // このサイズで
            .setSize( size.buildScratchSize )
            // ストレージバッファとして使えてアドレスを取れる
            .setUsage(
              vk::BufferUsageFlagBits::eStorageBuffer|
              vk::BufferUsageFlagBits::eShaderDeviceAddress
            )
        ),
      // GPUからしか読めない
      VMA_MEMORY_USAGE_GPU_ONLY
    )->get_address();

    //　Acceleration Structureを記録するバッファを作る
    auto asb = allocator->create_buffer(
      gct::buffer_create_info_t()
        .set_basic(
          vk::BufferCreateInfo()
            // このサイズで
            .setSize( size.accelerationStructureSize )
            // Acceleration Structureのストレージとして使えて
            // アドレスを取れる
            .setUsage(
              vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR|
              vk::BufferUsageFlagBits::eShaderDeviceAddress
            )
        ),
      // GPUからしか読めない
      VMA_MEMORY_USAGE_GPU_ONLY
    );
    // バッファからAcceleration Structureを作る
    blas = asb->create_acceleration_structure(
        // BLAS用
        vk::AccelerationStructureTypeKHR::eBottomLevel
      );
    
    // 1つめのジオメトリのプリミティブは1個
    auto range_info = vk::AccelerationStructureBuildRangeInfoKHR()
      .setPrimitiveCount( vertex_count / 3u );
    asbgi
      // 作ったAcceleration Structureを記録先に指定
      .setSrcAccelerationStructure( **blas )
      .setDstAccelerationStructure( **blas )
      // このバッファを一時的なデータの記録に使う
      .setScratchData( *scratch_buffer_addr );
    // Acceleration Structureに頂点配列の内容を記録する
    rec->buildAccelerationStructuresKHR(
      { asbgi },
      std::vector< const vk::AccelerationStructureBuildRangeInfoKHR* >{
        &range_info
      }
    );
  }
  command_buffer->execute(
    gct::submit_info_t()
  );

  command_buffer->wait_for_executed();


}

