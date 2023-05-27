#include <iostream>
#include <nlohmann/json.hpp>
#include <vulkan/vulkan.h>
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

  // デバイスの個数を取得
  uint32_t device_count = 0u;
  if( vkEnumeratePhysicalDevices( **instance, &device_count, nullptr ) != VK_SUCCESS )
    return 1;
  // デバイスの情報を取得
  std::vector< VkPhysicalDevice > devices( device_count );
  if( vkEnumeratePhysicalDevices( **instance, &device_count, devices.data() ) != VK_SUCCESS )
    return 1;
  // デバイスの情報をダンプ
  for( const auto &device: devices ) {
    VkPhysicalDeviceProperties2 props;
    VkPhysicalDeviceVulkan11Properties props11;
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props.pNext = &props11;
    props11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
    props11.pNext = nullptr;
    vkGetPhysicalDeviceProperties2( device, &props );
    std::cout << nlohmann::json( props11 ).dump( 2 ) << std::endl;
    VkPhysicalDeviceFeatures2 features;
    VkPhysicalDeviceVulkan11Features features11;
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features11;
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.pNext = nullptr;
    vkGetPhysicalDeviceFeatures2( device, &features );
    std::cout << nlohmann::json( features11 ).dump( 2 ) << std::endl;
  }
}

