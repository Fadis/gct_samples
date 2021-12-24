#include <iostream>
#include <gct/glfw.hpp>
#include <gct/instance.hpp>
#include <gct/device_create_info.hpp>
#include <gct/device.hpp>
#include <gct/swapchain.hpp>

int main() {
  gct::glfw::get();
  uint32_t iext_count = 0u;
  auto exts = glfwGetRequiredInstanceExtensions( &iext_count );
  std::vector< const char* > iext{};
  for( uint32_t i = 0u; i != iext_count; ++i )
    iext.push_back( exts[ i ] );
  std::shared_ptr< gct::instance_t > instance(
    new gct::instance_t(
      gct::instance_create_info_t()
        .set_application_info(
          vk::ApplicationInfo()
            .setPApplicationName( "my_application" )
            .setApplicationVersion(  VK_MAKE_VERSION( 1, 0, 0 ) )
            .setApiVersion( VK_API_VERSION_1_2 )
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

  const unsigned int width = 640;
  const unsigned int height = 480;

  gct::glfw_window window(
    width,
    height,
    "my_window",
    false
  );

  auto surface = window.get_surface( *groups[ 0 ].devices[ 0 ] );

  // デバイス拡張VK_KHR_swapchainを使う
  auto physical_device = groups[ 0 ].with_extensions( {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  } );
  
  // デバイスを作る
  auto device =
    physical_device
      .create_device(
        std::vector< gct::queue_requirement_t >{
          // こんな条件を満たすキューをください
          gct::queue_requirement_t{
            // 描画コマンドを流せるキューをください
            vk::QueueFlagBits::eGraphics,
            0u,
            vk::Extent3D(),
#ifdef VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME
            vk::QueueGlobalPriorityEXT(),
#endif
            // このサーフェスに書けるキューをください
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
  std::cout << "swapchain images : " << swapchain_images.size() << std::endl;
}

