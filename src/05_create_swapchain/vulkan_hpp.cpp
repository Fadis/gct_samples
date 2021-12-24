#include <iostream>
#include <gct/glfw.hpp>
#include <gct/instance.hpp>
#include <gct/device_create_info.hpp>
#include <gct/device.hpp>
#include <vulkan/vulkan.hpp>

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
  const auto physical_device =
    instance->get_physical_devices( {} )[ 0 ].devices[ 0 ];
  const unsigned int width = 640;
  const unsigned int height = 480;
  gct::glfw_window window(
    width,
    height,
    "my_window",
    false
  );
  auto surface = window.get_surface( *physical_device );
  auto &surface_capabilities = surface->get_caps().get_basic();


  // デバイスに備わっているキューを取得
  const auto queue_props = (*physical_device)->getQueueFamilyProperties();
  
  // 描画コマンドを流せてsurfaceに書けるキューを探す
  uint32_t queue_family_index = 0u;
  for( uint32_t i = 0; i < queue_props.size(); ++i ) {
    if( queue_props[ i ].queueFlags & vk::QueueFlagBits::eGraphics ) {
      if( (*physical_device)->getSurfaceSupportKHR( i, **surface ) ) {
        queue_family_index = i;
        break;
      }
    }
  }

  const float priority = 0.0f;
  // 描画コマンドを流せてsurfaceに書けるキューを1つください
  std::vector< vk::DeviceQueueCreateInfo > queues{
    vk::DeviceQueueCreateInfo()
      .setQueueFamilyIndex( queue_family_index )
      .setQueueCount( 1 )
      .setPQueuePriorities( &priority )
  };

  // 拡張VK_EXT_swapchainを使う
  std::vector< const char* > extension{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

  // 論理デバイスを作る
  auto device = (*physical_device)->createDeviceUnique(
    vk::DeviceCreateInfo()
      .setQueueCreateInfoCount( queues.size() )
      .setPQueueCreateInfos( queues.data() )
      .setEnabledExtensionCount( extension.size() )
      .setPpEnabledExtensionNames( extension.data() )
  );
 
  // サーフェスのイメージとして使えるフォーマットを取得する
  const auto surface_formats = (*physical_device)->getSurfaceFormatsKHR( **surface );
  const auto &surface_format = surface_formats[ 0 ];

  // サーフェスがサポートする範囲でスワップチェーンのイメージの数を決める
  const std::uint32_t image_count =
    std::max(
      std::min(
        2u,
        surface_capabilities.maxImageCount
      ),
      surface_capabilities.minImageCount
    );

  // スワップチェーンを作る
  auto swapchain = device->createSwapchainKHRUnique(
    vk::SwapchainCreateInfoKHR()
      // このサーフェスに対して
      .setSurface( **surface )
      // この枚数のイメージを持ち
      .setMinImageCount( image_count )
      // 単一のキューのみから操作できる
      .setImageSharingMode( vk::SharingMode::eExclusive )
      // 透過しない
      .setCompositeAlpha( vk::CompositeAlphaFlagBitsKHR::eOpaque )
      // 先に流したイメージから順番に表示される
      .setPresentMode( vk::PresentModeKHR::eFifo )
      // 最終的に表示されないピクセルの計算を省略して良い
      .setClipped( true )
      // このフォーマットのイメージで構成される
      .setImageFormat( surface_format.format )
      // この色空間のイメージで構成される
      .setImageColorSpace( surface_format.colorSpace )
      // この大きさのイメージで構成される
      .setImageExtent( surface_capabilities.currentExtent )
      // イメージ配列になっていない
      .setImageArrayLayers( 1 )
      // レンダリング結果の出力先として使える
      .setImageUsage( vk::ImageUsageFlagBits::eColorAttachment )
      // 必要なら画面の回転に合わせる
      .setPreTransform(
        ( surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity ) ?
        vk::SurfaceTransformFlagBitsKHR::eIdentity :
        surface_capabilities.currentTransform
      )
  );

  // スワップチェーンを構成するイメージのvectorを取得
  auto swapchain_images = device->getSwapchainImagesKHR( *swapchain );
  std::cout << "swapchain images : " << swapchain_images.size() << std::endl;
}

