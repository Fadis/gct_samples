#include <iostream>
#include <nlohmann/json.hpp>
#include <vulkan/vulkan.hpp>
#include <gct/instance.hpp>
#include <vulkan2json/PhysicalDeviceVulkan11Properties.hpp>
#include <vulkan2json/PhysicalDeviceVulkan11Features.hpp>

int main() {
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
    )
  );
  
  // デバイスの情報を取得
  auto devices = (*instance)->enumeratePhysicalDevices();
  // デバイスの情報をダンプ
  for( const auto &device: devices ) {
    vk::PhysicalDeviceProperties2 props;
    vk::PhysicalDeviceVulkan11Properties props11;
    props.pNext = &props11;
    device.getProperties2( &props );
    std::cout << nlohmann::json( props11 ).dump( 2 ) << std::endl;
    vk::PhysicalDeviceFeatures2 features;
    vk::PhysicalDeviceVulkan11Features features11;
    features.pNext = &features11;
    device.getFeatures2( &features );
    std::cout << nlohmann::json( features11 ).dump( 2 ) << std::endl;
  }
}

