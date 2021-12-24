#include <cstdlib>
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

int main() {
  
  // GLFWを初期化する
  if( !glfwInit() ) abort();

  // GLFWでサーフェスを作るのに必要なインスタンス拡張を取得
  uint32_t iext_count = 0u;
  auto exts = glfwGetRequiredInstanceExtensions( &iext_count );
  std::vector< const char* > iext{};
  for( uint32_t i = 0u; i != iext_count; ++i )
    iext.push_back( exts[ i ] );

  // インスタンスを作る
  VkApplicationInfo application_info;
  application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application_info.pNext = nullptr;
  application_info.pApplicationName = "my_application";
  application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  application_info.pEngineName ="my_engine";
  application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  application_info.apiVersion = VK_API_VERSION_1_2;
  const std::vector< const char * > layers{
    "VK_LAYER_KHRONOS_validation"
  };
  VkInstanceCreateInfo create_instance_info;
  create_instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_instance_info.pNext = nullptr;
  create_instance_info.flags = 0;
  create_instance_info.pApplicationInfo = &application_info;
  create_instance_info.enabledLayerCount = layers.size();
  create_instance_info.ppEnabledLayerNames = layers.data();
  // GLFWが要求している拡張を使う
  create_instance_info.enabledExtensionCount = iext.size();
  create_instance_info.ppEnabledExtensionNames = iext.data();
  VkInstance instance;
  if( vkCreateInstance(
    &create_instance_info,
    nullptr,
    &instance
  ) != VK_SUCCESS ) abort();

  // デバイスの個数を取得
  uint32_t device_count = 0u;
  if( vkEnumeratePhysicalDevices(
    instance,
    &device_count,
    nullptr
  ) != VK_SUCCESS ) abort();
  // デバイスの情報を取得
  std::vector< VkPhysicalDevice > devices( device_count );
  if( vkEnumeratePhysicalDevices(
    instance,
    &device_count,
    devices.data()
  ) != VK_SUCCESS ) abort();
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
  VkSurfaceKHR surface;
  if( glfwCreateWindowSurface(
    instance,
    window,
    nullptr,
    &surface
  ) != VK_SUCCESS ) abort();

  //
  VkSurfaceCapabilitiesKHR surface_capabilities;
  if( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    physical_device,
    surface,
    &surface_capabilities
  ) != VK_SUCCESS ) abort();

  // サーフェスを捨てる
  vkDestroySurfaceKHR(
    instance,
    surface,
    nullptr
  );

  // ウィンドウを捨てる
  glfwDestroyWindow( window );

  // インスタンスを捨てる
  vkDestroyInstance(
    instance,
    nullptr
  );
}

