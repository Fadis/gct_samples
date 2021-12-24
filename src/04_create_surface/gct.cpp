#include <iostream>
#include <gct/glfw.hpp>
#include <gct/instance.hpp>
#include <gct/physical_device.hpp>

int main() {
  // GLFWを初期化する
  gct::glfw::get();
  // GLFWでサーフェスを作るのに必要なインスタンス拡張を取得
  uint32_t iext_count = 0u;
  auto exts = glfwGetRequiredInstanceExtensions( &iext_count );
  std::vector< const char* > iext{};
  for( uint32_t i = 0u; i != iext_count; ++i )
    iext.push_back( exts[ i ] );

  // インスタンスを作る
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
        // GLFWが要求している拡張を使う
        .add_extension(
          iext.begin(), iext.end()
        )
    )
  );
  auto groups = instance->get_physical_devices( {} );
  auto physical_device = groups[ 0 ].with_extensions( {} );

  const unsigned int width = 640;
  const unsigned int height = 480;

  // ウィンドウを作る
  gct::glfw_window window(
    width,
    height,
    "my_window",
    false
  );

  // サーフェスを作る
  auto surface = window.get_surface( *groups[ 0 ].devices[ 0 ] );

  auto capabilities = surface->get_caps();
}

