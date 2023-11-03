#include <iostream>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/buffer.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <vulkan/vulkan.hpp>

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
 
  // デスクリプタセット内にはストレージバッファのデスクリプタが最大5個
  const std::vector< vk::DescriptorPoolSize > descriptor_pool_size{
    vk::DescriptorPoolSize()
      .setType( vk::DescriptorType::eStorageBuffer )
      .setDescriptorCount( 5 )
  };

  // デスクリプタプールを作る
  const auto descriptor_pool = (*device)->createDescriptorPoolUnique(
    vk::DescriptorPoolCreateInfo()
      // vkFreeDescriptorSetでデスクリプタセットを解放できる
      .setFlags( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet )
      // 最大10セットのデスクリプタセットを確保できる
      .setMaxSets( 10 )
      .setPoolSizeCount( descriptor_pool_size.size() )
      .setPPoolSizes( descriptor_pool_size.data() )
  );
  
  // 必要なデスクリプタを指定
  const auto descriptor_set_layout_create_info =
    vk::DescriptorSetLayoutBinding()
      // binding=0に結びつける
      .setBinding( 0 )
      // ストレージバッファのデスクリプタが
      .setDescriptorType( vk::DescriptorType::eStorageBuffer )
      // 1個あって
      .setDescriptorCount( 1 )
      // コンピュートシェーダで使える
      .setStageFlags( vk::ShaderStageFlagBits::eCompute );
  
  // デスクリプタセットレイアウトを作る
  const auto descriptor_set_layout = (*device)->createDescriptorSetLayoutUnique(
    vk::DescriptorSetLayoutCreateInfo()
      // bindingが1つ
      .setBindingCount( 1u )
      // 各bindingの設定はこれ
      .setPBindings( &descriptor_set_layout_create_info )
  );

  // デスクリプタセットを作る
  const auto descriptor_set = std::move( (*device)->allocateDescriptorSetsUnique(
    vk::DescriptorSetAllocateInfo()
      // このデスクリプタプールから
      .setDescriptorPool( *descriptor_pool )
      // 1セット
      .setDescriptorSetCount( 1 )
      // この内容のデスクリプタセットを
      .setPSetLayouts( &*descriptor_set_layout )
  )[ 0 ] );

  // 更新するデスクリプタの情報
  const auto descriptor_buffer_info =
    vk::DescriptorBufferInfo()
      // このバッファの
      .setBuffer( **device_local_buffer )
      // 先頭から
      .setOffset( 0 )
      // 1024バイトの範囲をシェーダから触れるようにする
      .setRange( 1024 );

  // デスクリプタの内容を更新
  (*device)->updateDescriptorSets(
    {
      vk::WriteDescriptorSet()
        // このデスクリプタセットの
        .setDstSet( *descriptor_set )
        // binding=0の
        .setDstBinding( 0 )
        // 0要素目から
        .setDstArrayElement( 0 )
        // 1個の
        .setDescriptorCount( 1u )
        // ストレージバッファのデスクリプタを
        .setDescriptorType( vk::DescriptorType::eStorageBuffer )
        // この内容にする
        .setPBufferInfo( &descriptor_buffer_info )
    },
    {}
  );

}

