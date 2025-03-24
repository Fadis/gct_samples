#include <iostream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <gct/instance.hpp>
#include <gct/physical_device.hpp>

int main() {

#ifdef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#if VK_HEADER_VERSION >= 301
    vk::detail::DynamicLoader dl;
#else
    vk::DynamicLoader dl;
#endif
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
  dl.getProcAddress<PFN_vkGetInstanceProcAddr>( "vkGetInstanceProcAddr" );
  VULKAN_HPP_DEFAULT_DISPATCHER.init( vkGetInstanceProcAddr );
#endif

  // GLFWを初期化する
  if( !glfwInit() ) abort();

  // GLFWでサーフェスを作るのに必要なインスタンス拡張を取得
  uint32_t iext_count = 0u;
  auto exts = glfwGetRequiredInstanceExtensions( &iext_count );
  std::vector< const char* > iext{};
  for( uint32_t i = 0u; i != iext_count; ++i )
    iext.push_back( exts[ i ] );
  
  // インスタンスを作る
  const auto app_info = vk::ApplicationInfo()
    .setPApplicationName( "my_application" )
    .setApplicationVersion( VK_MAKE_VERSION(1, 0, 0) )
    .setPEngineName( "my_engine" )
    .setEngineVersion( VK_MAKE_VERSION(1, 0, 0) )
    .setApiVersion( VK_API_VERSION_1_2 );
  const std::vector< const char * > layers{
    "VK_LAYER_KHRONOS_validation"
  };
  auto instance = vk::createInstanceUnique(
    vk::InstanceCreateInfo()
      .setPApplicationInfo( &app_info )
      .setEnabledLayerCount( layers.size() )
      .setPpEnabledLayerNames( layers.data() )
      // GLFWが要求している拡張を使う
      .setEnabledExtensionCount( iext.size() )
      .setPpEnabledExtensionNames( iext.data() )
  );

#ifdef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
  VULKAN_HPP_DEFAULT_DISPATCHER.init( *instance );
#endif

  // デバイスの情報を取得
  auto devices = instance->enumeratePhysicalDevices();
  if( devices.empty() ) abort();
  auto physical_device = devices[ 0 ];

  const unsigned int width = 640;
  const unsigned int height = 480;

  // ウィンドウを作る
  glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
  auto window = glfwCreateWindow(
    width,
    height,
    "my_window",
    nullptr,
    nullptr
  );
  if( !window ) abort();

  // サーフェスを作る
  VkSurfaceKHR raw_surface;
  if( glfwCreateWindowSurface(
    *instance,
    window,
    nullptr,
    &raw_surface
  ) != VK_SUCCESS ) abort();
  vk::UniqueHandle<
    vk::SurfaceKHR,
    VULKAN_HPP_DEFAULT_DISPATCHER_TYPE
  > surface(
    raw_surface,
    *instance
  );

  //
  auto surface_capabilities = physical_device.getSurfaceCapabilitiesKHR( *surface );

  // ウィンドウを捨てる
  glfwDestroyWindow( window );
}

