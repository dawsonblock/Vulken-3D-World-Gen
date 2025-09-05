#include <gtest/gtest.h>
#include <vulkan/vulkan.h>

static bool create_and_destroy_vulkan_instance_device() {
  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "vk_smoke";
  app.applicationVersion = VK_MAKE_VERSION(1,0,0);
  app.pEngineName = "none";
  app.engineVersion = VK_MAKE_VERSION(1,0,0);
  app.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo ici{};
  ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ici.pApplicationInfo = &app;

  VkInstance instance = VK_NULL_HANDLE;
  if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS || instance == VK_NULL_HANDLE) {
    return false;
  }

  uint32_t count = 0;
  if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || count == 0) {
    vkDestroyInstance(instance, nullptr);
    return false;
  }
  std::vector<VkPhysicalDevice> phys(count);
  if (vkEnumeratePhysicalDevices(instance, &count, phys.data()) != VK_SUCCESS) {
    vkDestroyInstance(instance, nullptr);
    return false;
  }
  VkPhysicalDevice pd = phys[0];

  uint32_t qCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
  if (qCount == 0) { vkDestroyInstance(instance, nullptr); return false; }
  std::vector<VkQueueFamilyProperties> qprops(qCount);
  vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qprops.data());
  uint32_t gfxIndex = UINT32_MAX;
  for (uint32_t i = 0; i < qCount; ++i) {
    if (qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { gfxIndex = i; break; }
  }
  if (gfxIndex == UINT32_MAX) { vkDestroyInstance(instance, nullptr); return false; }

  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{};
  qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  qci.queueFamilyIndex = gfxIndex;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;

  VkDeviceCreateInfo dci{};
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;

  VkDevice device = VK_NULL_HANDLE;
  VkResult dr = vkCreateDevice(pd, &dci, nullptr, &device);
  if (dr != VK_SUCCESS || device == VK_NULL_HANDLE) {
    vkDestroyInstance(instance, nullptr);
    return false;
  }

  vkDestroyDevice(device, nullptr);
  vkDestroyInstance(instance, nullptr);
  return true;
}

TEST(EngineSmoke, VulkanInit) {
  EXPECT_TRUE(create_and_destroy_vulkan_instance_device());
}