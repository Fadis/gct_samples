#include <vector>
#include <utility>
#include <cstdint>
#include <vulkan/vulkan.hpp>

extern "C" std::vector<std::pair<uint32_t, uint32_t>> custom_stype_info;

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
  const auto app_info = vk::ApplicationInfo()
    // アプリケーションの名前
    .setPApplicationName( "my_application" )
    // アプリケーションのバージョン
    .setApplicationVersion( VK_MAKE_VERSION(1, 0, 0) )
    // エンジンの名前
    .setPEngineName( "my_engine" )
    // エンジンのバージョン
    .setEngineVersion( VK_MAKE_VERSION(1, 0, 0) )
    // 使用するVulkanのバージョンをVulkan 1.2にする
    .setApiVersion( VK_API_VERSION_1_2 );
  // バリデーションレイヤーを使う
  const std::vector< const char * > layers{
    "VK_LAYER_KHRONOS_validation"
  };
  // インスタンスを作成
  auto instance = vk::createInstanceUnique(
    vk::InstanceCreateInfo()
      // アプリケーションの情報を指定
      .setPApplicationInfo( &app_info )
      // 使用するレイヤーを指定
      .setPEnabledLayerNames( layers )
  );
#ifdef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
  VULKAN_HPP_DEFAULT_DISPATCHER.init( *instance );
#endif
}

