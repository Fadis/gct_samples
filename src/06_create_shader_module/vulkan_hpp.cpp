#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <nlohmann/json.hpp>
#include <gct/instance.hpp>
#include <gct/device.hpp>
#include <gct/allocator.hpp>
#include <gct/device_create_info.hpp>
#include <gct/spirv_reflect.h>
#include <vulkan/vulkan.hpp>
int main() {
  uint32_t iext_count = 0u;
  std::vector< const char* > iext{};
  std::shared_ptr< gct::instance_t > instance(
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
 
  auto device = selected.create_device(
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
  
  // ファイルからSPIR-Vを読む
  std::fstream file( "../shaders/add.comp.spv", std::ios::in|std::ios::binary );
  if( !file.good() ) abort();
  std::vector< std::uint8_t > code;
  code.assign(
    std::istreambuf_iterator< char >( file ),
    std::istreambuf_iterator< char >()
  );

  // シェーダモジュールを作る
  auto shader = (*device)->createShaderModuleUnique(
    vk::ShaderModuleCreateInfo()
      .setCodeSize( code.size() )
      .setPCode( reinterpret_cast< const uint32_t* >( code.data() ) )
  );
  
  // シェーダの内容をパースする
  SpvReflectShaderModule reflect;
  if( spvReflectCreateShaderModule(
    code.size(),
    code.data(),
    &reflect
  ) != SPV_REFLECT_RESULT_SUCCESS ) abort();

  // entry pointとなる関数名を表示する
  std::cout << "entry point : " << reflect.entry_point_name << std::endl;

  // パース結果を捨てる
  spvReflectDestroyShaderModule( &reflect ); 

}

