#include <iostream>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/descriptor_pool.hpp>
#include <gct/descriptor_set_layout.hpp>
#include <gct/write_descriptor_set.hpp>
#include <gct/shader_module.hpp>

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
  const auto allocator = device->get_allocator();
  const auto device_local_buffer = allocator->create_buffer(
    gct::buffer_create_info_t()
      .set_basic(
        vk::BufferCreateInfo()
          .setSize( 1024 )
          .setUsage( vk::BufferUsageFlagBits::eStorageBuffer|vk::BufferUsageFlagBits::eTransferSrc|vk::BufferUsageFlagBits::eTransferDst )
      ),
    VMA_MEMORY_USAGE_GPU_ONLY
  );
  // シェーダモジュールを作る
  const auto shader = device->get_shader_module(
    // このファイルから読む
    "../shaders/add.comp.spv"
  );


  // デスクリプタプールを作る
  const auto descriptor_pool = device->get_descriptor_pool(
    gct::descriptor_pool_create_info_t()
      .set_basic(
        vk::DescriptorPoolCreateInfo()
          // vkFreeDescriptorSetでデスクリプタセットを解放できる
          .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
          // 最大10セットのデスクリプタセットを確保できる
          .setMaxSets( 10 )
      )
      // デスクリプタセット内にはストレージバッファのデスクリプタが最大5個
      .set_descriptor_pool_size( vk::DescriptorType::eStorageBuffer, 5 )
      .rebuild_chain()
  );

  // デスクリプタセットを作る
  const auto descriptor_set = descriptor_pool->allocate(
    // デスクリプタセットレイアウトを作る
    device->get_descriptor_set_layout(
      gct::descriptor_set_layout_create_info_t()
        // このシェーダに必要なデスクリプタを追加
        .add_binding( shader->get_props().get_reflection() )
        .rebuild_chain()
    )
  );
  
  // デスクリプタの内容を更新
  descriptor_set->update(
    {
      gct::write_descriptor_set_t()
        .set_basic(
          // シェーダ上でlayout1という名前のデスクリプタを
          (*descriptor_set)[ "layout1" ]
        )
        .add_buffer(
          gct::descriptor_buffer_info_t()
            // このバッファのものに変更する
            .set_buffer( device_local_buffer )
            .set_basic(
              vk::DescriptorBufferInfo()
                // バッファの先頭から
                .setOffset( 0 )
                // 1024バイトの範囲をシェーダから触れるようにする
                .setRange( 1024 )
            )
        )
    }
  );

}

