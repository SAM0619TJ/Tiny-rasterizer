#include "VulkanRenderer.h"

#include <vulkan/vulkan.h>

#include "Window.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kMaxFramesInFlight = 2;
const std::vector<const char *> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"};
const std::vector<const char *> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef DEBUG
constexpr bool kEnableValidationLayers = true;
#else
constexpr bool kEnableValidationLayers = false;
#endif

void checkVk(VkResult result, const std::string &message) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(message + " (VkResult " +
                             std::to_string(result) + ")");
  }
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData, void *) {
  std::cerr << "[vulkan][validation] " << callbackData->pMessage << std::endl;
  return VK_FALSE;
}

VkResult createDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *createInfo,
    const VkAllocationCallbacks *allocator,
    VkDebugUtilsMessengerEXT *debugMessenger) {
  auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
  if (function == nullptr) {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
  return function(instance, createInfo, allocator, debugMessenger);
}

void destroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *allocator) {
  auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
  if (function != nullptr) {
    function(instance, debugMessenger, allocator);
  }
}

bool hasInstanceExtension(const char *name) {
  uint32_t count = 0;
  checkVk(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr),
          "Failed to count Vulkan instance extensions");
  std::vector<VkExtensionProperties> extensions(count);
  checkVk(vkEnumerateInstanceExtensionProperties(nullptr, &count,
                                                extensions.data()),
          "Failed to enumerate Vulkan instance extensions");

  return std::any_of(extensions.begin(), extensions.end(),
                     [name](const VkExtensionProperties &extension) {
                       return std::strcmp(extension.extensionName, name) == 0;
                     });
}

bool hasDeviceExtension(VkPhysicalDevice device, const char *name) {
  uint32_t count = 0;
  checkVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr),
          "Failed to count Vulkan device extensions");
  std::vector<VkExtensionProperties> extensions(count);
  checkVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &count,
                                              extensions.data()),
          "Failed to enumerate Vulkan device extensions");

  return std::any_of(extensions.begin(), extensions.end(),
                     [name](const VkExtensionProperties &extension) {
                       return std::strcmp(extension.extensionName, name) == 0;
                     });
}

bool validationLayersAvailable() {
  uint32_t count = 0;
  checkVk(vkEnumerateInstanceLayerProperties(&count, nullptr),
          "Failed to count Vulkan instance layers");
  std::vector<VkLayerProperties> layers(count);
  checkVk(vkEnumerateInstanceLayerProperties(&count, layers.data()),
          "Failed to enumerate Vulkan instance layers");

  for (const char *requiredLayer : kValidationLayers) {
    const bool found =
        std::any_of(layers.begin(), layers.end(),
                    [requiredLayer](const VkLayerProperties &layer) {
                      return std::strcmp(layer.layerName, requiredLayer) == 0;
                    });
    if (!found) {
      return false;
    }
  }

  return true;
}

VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerCreateInfo() {
  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
  return createInfo;
}

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool complete() const {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapchainSupport {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device,
                                     VkSurfaceKHR surface) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = VK_FALSE;
    checkVk(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
                                                &presentSupport),
            "Failed to query Vulkan present support");
    if (presentSupport == VK_TRUE) {
      indices.presentFamily = i;
    }

    if (indices.complete()) {
      break;
    }
  }

  return indices;
}

std::optional<uint32_t> findGraphicsQueueFamily(VkPhysicalDevice device) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      return i;
    }
  }

  return std::nullopt;
}

SwapchainSupport querySwapchainSupport(VkPhysicalDevice device,
                                       VkSurfaceKHR surface) {
  SwapchainSupport support;

  checkVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                                   &support.capabilities),
          "Failed to query Vulkan surface capabilities");

  uint32_t formatCount = 0;
  checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                              nullptr),
          "Failed to count Vulkan surface formats");
  support.formats.resize(formatCount);
  if (formatCount > 0) {
    checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                                support.formats.data()),
            "Failed to query Vulkan surface formats");
  }

  uint32_t presentModeCount = 0;
  checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(
              device, surface, &presentModeCount, nullptr),
          "Failed to count Vulkan present modes");
  support.presentModes.resize(presentModeCount);
  if (presentModeCount > 0) {
    checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(
                device, surface, &presentModeCount, support.presentModes.data()),
            "Failed to query Vulkan present modes");
  }

  return support;
}

VkSurfaceFormatKHR chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &formats) {
  for (const VkSurfaceFormatKHR &format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }
  return formats.front();
}

VkPresentModeKHR choosePresentMode(
    const std::vector<VkPresentModeKHR> &presentModes) {
  for (VkPresentModeKHR mode : presentModes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return mode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                        GLFWwindow *window) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window, &width, &height);

  VkExtent2D actualExtent{static_cast<uint32_t>(width),
                          static_cast<uint32_t>(height)};
  actualExtent.width = std::clamp(actualExtent.width,
                                  capabilities.minImageExtent.width,
                                  capabilities.maxImageExtent.width);
  actualExtent.height = std::clamp(actualExtent.height,
                                   capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
  return actualExtent;
}

// std140 布局：iTime(0) | pad(4) | iResolution(8) | iMouse(16) | pad(24)
struct FrameUniforms {
  float iTime = 0.0f;
  float pad0 = 0.0f;
  float iResolution[2] = {0.0f, 0.0f};
  float iMouse[2] = {0.0f, 0.0f};
  float pad1[2] = {0.0f, 0.0f};
};

// 覆盖整个 NDC 的全屏四边形（两个三角形，逆时针）
const std::array<float, 12> kFullscreenQuad = {
    -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
    -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

std::vector<char> readBinaryFile(const std::string &path) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open SPIR-V file: " + path);
  }
  const auto size = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(size);
  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(size));
  return buffer;
}

// 将场景里的 GLSL 路径映射到构建期生成的 SPIR-V 路径
// 例如 shaders/rotation_matrix.glsl -> shaders_spirv/rotation_matrix.spv
std::string deriveSpirvPath(const std::string &glslPath) {
  const size_t slash = glslPath.find_last_of("/\\");
  const std::string fileName =
      slash == std::string::npos ? glslPath : glslPath.substr(slash + 1);
  const size_t dot = fileName.find_last_of('.');
  const std::string stem =
      dot == std::string::npos ? fileName : fileName.substr(0, dot);
  return "shaders_spirv/" + stem + ".spv";
}

} // namespace

struct VulkanRenderer::Impl {
  Window *window = nullptr;
  VkInstance instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue presentQueue = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainImageViews;
  std::vector<VkFramebuffer> swapchainFramebuffers;
  VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
  VkExtent2D swapchainExtent{};
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline graphicsPipeline = VK_NULL_HANDLE;
  VkBuffer vertexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  std::string vertexShaderPath;
  std::string fragmentShaderPath;

  struct FrameSync {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    VkFence inFlight = VK_NULL_HANDLE;
    VkBuffer uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uniformMemory = VK_NULL_HANDLE;
    void *uniformMapped = nullptr;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  };

  std::array<FrameSync, kMaxFramesInFlight> frames{};
  uint32_t currentFrame = 0;
  uint32_t imageIndex = 0;
  bool initialized = false;
  bool frameReady = false;
  bool framebufferResized = false;
  bool validationEnabled = false;

  void init(Window &targetWindow) {
    window = &targetWindow;
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createVertexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createSyncObjects();
    initialized = true;
  }

  void beginFrame(const FrameParams &params) {
    frameReady = false;
    if (!initialized || framebufferResized || hasZeroFramebufferExtent()) {
      if (framebufferResized && !hasZeroFramebufferExtent()) {
        recreateSwapchain();
      }
      return;
    }

    FrameSync &frame = frames[currentFrame];
    checkVk(vkWaitForFences(device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX),
            "Failed waiting for Vulkan frame fence");

    const VkResult acquireResult = vkAcquireNextImageKHR(
        device, swapchain, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE,
        &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapchain();
      return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
      checkVk(acquireResult, "Failed to acquire Vulkan swapchain image");
    }

    checkVk(vkResetFences(device, 1, &frame.inFlight),
            "Failed resetting Vulkan frame fence");
    checkVk(vkResetCommandBuffer(frame.commandBuffer, 0),
            "Failed resetting Vulkan command buffer");
    updateUniformBuffer(currentFrame, params);
    recordCommandBuffer(frame.commandBuffer, imageIndex);
    frameReady = true;
  }

  void endFrame() {
    if (!frameReady) {
      return;
    }

    FrameSync &frame = frames[currentFrame];
    const VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.imageAvailable;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame.renderFinished;

    checkVk(vkQueueSubmit(graphicsQueue, 1, &submitInfo, frame.inFlight),
            "Failed submitting Vulkan command buffer");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    const VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
        presentResult == VK_SUBOPTIMAL_KHR || framebufferResized) {
      recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
      checkVk(presentResult, "Failed presenting Vulkan swapchain image");
    }

    currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
    frameReady = false;
  }

  void resize(int width, int height) {
    if (!initialized || width <= 0 || height <= 0) {
      return;
    }
    framebufferResized = true;
  }

  void shutdown() {
    if (!initialized && instance == VK_NULL_HANDLE) {
      return;
    }

    if (device != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device);
      cleanupSwapchain();

      if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
      }
      if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
      }
      if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
      }
      if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
      }

      for (FrameSync &frame : frames) {
        if (frame.uniformBuffer != VK_NULL_HANDLE) {
          vkDestroyBuffer(device, frame.uniformBuffer, nullptr);
        }
        if (frame.uniformMemory != VK_NULL_HANDLE) {
          vkFreeMemory(device, frame.uniformMemory, nullptr);
        }
      }

      if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
      }
      if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
      }

      for (FrameSync &frame : frames) {
        if (frame.imageAvailable != VK_NULL_HANDLE) {
          vkDestroySemaphore(device, frame.imageAvailable, nullptr);
        }
        if (frame.renderFinished != VK_NULL_HANDLE) {
          vkDestroySemaphore(device, frame.renderFinished, nullptr);
        }
        if (frame.inFlight != VK_NULL_HANDLE) {
          vkDestroyFence(device, frame.inFlight, nullptr);
        }
      }

      if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
      }
      vkDestroyDevice(device, nullptr);
    }

    if (surface != VK_NULL_HANDLE) {
      vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    if (debugMessenger != VK_NULL_HANDLE) {
      destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
      vkDestroyInstance(instance, nullptr);
    }

    *this = Impl{};
  }

  void createInstance() {
    if (!glfwVulkanSupported()) {
      throw std::runtime_error("GLFW reports Vulkan is not supported");
    }

    validationEnabled = kEnableValidationLayers && validationLayersAvailable();
    if (kEnableValidationLayers && !validationEnabled) {
      std::cerr << "[vulkan] Validation layer unavailable, continuing without it"
                << std::endl;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Tiny Rasterizer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Tiny Rasterizer";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == nullptr || glfwExtensionCount == 0) {
      throw std::runtime_error("GLFW did not report required Vulkan extensions");
    }

    std::vector<const char *> extensions(glfwExtensions,
                                         glfwExtensions + glfwExtensionCount);
    if (validationEnabled) {
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    bool portabilityEnumerationEnabled = false;
    if (hasInstanceExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
      extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
      portabilityEnumerationEnabled = true;
    }

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo =
        makeDebugMessengerCreateInfo();

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    if (portabilityEnumerationEnabled) {
      createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    if (validationEnabled) {
      createInfo.enabledLayerCount =
          static_cast<uint32_t>(kValidationLayers.size());
      createInfo.ppEnabledLayerNames = kValidationLayers.data();
      createInfo.pNext = &debugCreateInfo;
    }

    checkVk(vkCreateInstance(&createInfo, nullptr, &instance),
            "Failed creating Vulkan instance");
    std::cout << "[vulkan] Instance created" << std::endl;
  }

  void setupDebugMessenger() {
    if (!validationEnabled) {
      return;
    }

    const VkDebugUtilsMessengerCreateInfoEXT createInfo =
        makeDebugMessengerCreateInfo();
    checkVk(createDebugUtilsMessengerEXT(instance, &createInfo, nullptr,
                                        &debugMessenger),
            "Failed creating Vulkan debug messenger");
  }

  void createSurface() {
    checkVk(glfwCreateWindowSurface(instance, window->getGLFWwindow(), nullptr,
                                   &surface),
            "Failed creating Vulkan window surface");
  }

  void pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    checkVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr),
            "Failed counting Vulkan physical devices");
    if (deviceCount == 0) {
      throw std::runtime_error("No Vulkan physical devices found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    checkVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()),
            "Failed enumerating Vulkan physical devices");

    for (VkPhysicalDevice candidate : devices) {
      if (deviceSuitable(candidate)) {
        physicalDevice = candidate;
        break;
      }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
      throw std::runtime_error("No suitable Vulkan physical device found");
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    std::cout << "[vulkan] Physical device: " << properties.deviceName
              << std::endl;
  }

  bool deviceSuitable(VkPhysicalDevice candidate) const {
    const QueueFamilyIndices indices = findQueueFamilies(candidate, surface);
    if (!indices.complete()) {
      return false;
    }

    for (const char *extension : kDeviceExtensions) {
      if (!hasDeviceExtension(candidate, extension)) {
        return false;
      }
    }

    const SwapchainSupport support = querySwapchainSupport(candidate, surface);
    return !support.formats.empty() && !support.presentModes.empty();
  }

  void createLogicalDevice() {
    const QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                              indices.presentFamily.value()};

    const float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
      VkDeviceQueueCreateInfo queueCreateInfo{};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = queueFamily;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
    }

    std::vector<const char *> deviceExtensions = kDeviceExtensions;
    if (hasDeviceExtension(physicalDevice, "VK_KHR_portability_subset")) {
      deviceExtensions.push_back("VK_KHR_portability_subset");
    }

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (validationEnabled) {
      createInfo.enabledLayerCount =
          static_cast<uint32_t>(kValidationLayers.size());
      createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }

    checkVk(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device),
            "Failed creating Vulkan logical device");
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
  }

  void createSwapchain() {
    const SwapchainSupport support =
        querySwapchainSupport(physicalDevice, surface);
    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
    const VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
    const VkExtent2D extent =
        chooseExtent(support.capabilities, window->getGLFWwindow());

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        imageCount > support.capabilities.maxImageCount) {
      imageCount = support.capabilities.maxImageCount;
    }

    const QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
    const uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                           indices.presentFamily.value()};

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (indices.graphicsFamily != indices.presentFamily) {
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    checkVk(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain),
            "Failed creating Vulkan swapchain");
    checkVk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr),
            "Failed counting Vulkan swapchain images");
    swapchainImages.resize(imageCount);
    checkVk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                                   swapchainImages.data()),
            "Failed getting Vulkan swapchain images");

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;
    std::cout << "[vulkan] Swapchain created: " << swapchainExtent.width << "x"
              << swapchainExtent.height << std::endl;
  }

  void createImageViews() {
    swapchainImageViews.resize(swapchainImages.size());
    for (size_t i = 0; i < swapchainImages.size(); ++i) {
      VkImageViewCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image = swapchainImages[i];
      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = swapchainImageFormat;
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;

      checkVk(vkCreateImageView(device, &createInfo, nullptr,
                                &swapchainImageViews[i]),
              "Failed creating Vulkan swapchain image view");
    }
  }

  void createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    checkVk(vkCreateRenderPass(device, &createInfo, nullptr, &renderPass),
            "Failed creating Vulkan render pass");
  }

  void createFramebuffers() {
    swapchainFramebuffers.resize(swapchainImageViews.size());
    for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
      VkImageView attachments[] = {swapchainImageViews[i]};

      VkFramebufferCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      createInfo.renderPass = renderPass;
      createInfo.attachmentCount = 1;
      createInfo.pAttachments = attachments;
      createInfo.width = swapchainExtent.width;
      createInfo.height = swapchainExtent.height;
      createInfo.layers = 1;

      checkVk(vkCreateFramebuffer(device, &createInfo, nullptr,
                                  &swapchainFramebuffers[i]),
              "Failed creating Vulkan framebuffer");
    }
  }

  void createCommandPool() {
    const QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = indices.graphicsFamily.value();

    checkVk(vkCreateCommandPool(device, &createInfo, nullptr, &commandPool),
            "Failed creating Vulkan command pool");
  }

  void createCommandBuffers() {
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = kMaxFramesInFlight;

    std::array<VkCommandBuffer, kMaxFramesInFlight> commandBuffers{};
    checkVk(vkAllocateCommandBuffers(device, &allocateInfo,
                                    commandBuffers.data()),
            "Failed allocating Vulkan command buffers");

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
      frames[i].commandBuffer = commandBuffers[i];
    }
  }

  void recordCommandBuffer(VkCommandBuffer commandBuffer,
                           uint32_t swapchainImageIndex) const {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo),
            "Failed beginning Vulkan command buffer");

    VkClearValue clearColor{};
    clearColor.color = {{0.05f, 0.08f, 0.12f, 1.0f}};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapchainFramebuffers[swapchainImageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphicsPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainExtent.width);
    viewport.height = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    const VkBuffer vertexBuffers[] = {vertexBuffer};
    const VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1,
                            &frames[currentFrame].descriptorSet, 0, nullptr);

    vkCmdDraw(commandBuffer, 6, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    checkVk(vkEndCommandBuffer(commandBuffer),
            "Failed ending Vulkan command buffer");
  }

  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
      const bool typeAllowed = (typeFilter & (1u << i)) != 0;
      const bool propsMatch =
          (memProperties.memoryTypes[i].propertyFlags & properties) ==
          properties;
      if (typeAllowed && propsMatch) {
        return i;
      }
    }
    throw std::runtime_error("Failed to find suitable Vulkan memory type");
  }

  void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkBuffer &buffer,
                    VkDeviceMemory &memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    checkVk(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer),
            "Failed creating Vulkan buffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex =
        findMemoryType(requirements.memoryTypeBits, properties);
    checkVk(vkAllocateMemory(device, &allocateInfo, nullptr, &memory),
            "Failed allocating Vulkan buffer memory");
    checkVk(vkBindBufferMemory(device, buffer, memory, 0),
            "Failed binding Vulkan buffer memory");
  }

  VkShaderModule createShaderModule(const std::vector<char> &code) const {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    checkVk(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule),
            "Failed creating Vulkan shader module");
    return shaderModule;
  }

  void createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = 1;
    createInfo.pBindings = &uboBinding;

    checkVk(vkCreateDescriptorSetLayout(device, &createInfo, nullptr,
                                        &descriptorSetLayout),
            "Failed creating Vulkan descriptor set layout");
  }

  void createGraphicsPipeline() {
    const std::vector<char> vertCode = readBinaryFile(vertexShaderPath);
    const std::vector<char> fragCode = readBinaryFile(fragmentShaderPath);
    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    const std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertStage,
                                                                   fragStage};

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(float) * 2;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescription{};
    attributeDescription.location = 0;
    attributeDescription.binding = 0;
    attributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescription.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDescription;
    vertexInput.vertexAttributeDescriptionCount = 1;
    vertexInput.pVertexAttributeDescriptions = &attributeDescription;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    const std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount =
        static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;
    checkVk(vkCreatePipelineLayout(device, &layoutInfo, nullptr,
                                   &pipelineLayout),
            "Failed creating Vulkan pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    const VkResult result = vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);

    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyShaderModule(device, vertModule, nullptr);

    checkVk(result, "Failed creating Vulkan graphics pipeline");
    std::cout << "[vulkan] Graphics pipeline created" << std::endl;
  }

  void createVertexBuffer() {
    const VkDeviceSize bufferSize = sizeof(kFullscreenQuad);
    createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 vertexBuffer, vertexBufferMemory);

    void *data = nullptr;
    checkVk(vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data),
            "Failed mapping Vulkan vertex buffer");
    std::memcpy(data, kFullscreenQuad.data(),
                static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, vertexBufferMemory);
  }

  void createUniformBuffers() {
    const VkDeviceSize bufferSize = sizeof(FrameUniforms);
    for (FrameSync &frame : frames) {
      createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   frame.uniformBuffer, frame.uniformMemory);
      checkVk(vkMapMemory(device, frame.uniformMemory, 0, bufferSize, 0,
                          &frame.uniformMapped),
              "Failed mapping Vulkan uniform buffer");
    }
  }

  void createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(kMaxFramesInFlight);

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.poolSizeCount = 1;
    createInfo.pPoolSizes = &poolSize;
    createInfo.maxSets = static_cast<uint32_t>(kMaxFramesInFlight);

    checkVk(vkCreateDescriptorPool(device, &createInfo, nullptr,
                                   &descriptorPool),
            "Failed creating Vulkan descriptor pool");
  }

  void createDescriptorSets() {
    std::array<VkDescriptorSetLayout, kMaxFramesInFlight> layouts{};
    layouts.fill(descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = descriptorPool;
    allocateInfo.descriptorSetCount = static_cast<uint32_t>(kMaxFramesInFlight);
    allocateInfo.pSetLayouts = layouts.data();

    std::array<VkDescriptorSet, kMaxFramesInFlight> sets{};
    checkVk(vkAllocateDescriptorSets(device, &allocateInfo, sets.data()),
            "Failed allocating Vulkan descriptor sets");

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
      frames[i].descriptorSet = sets[i];

      VkDescriptorBufferInfo bufferInfo{};
      bufferInfo.buffer = frames[i].uniformBuffer;
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(FrameUniforms);

      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = frames[i].descriptorSet;
      write.dstBinding = 0;
      write.dstArrayElement = 0;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      write.descriptorCount = 1;
      write.pBufferInfo = &bufferInfo;

      vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
  }

  void updateUniformBuffer(uint32_t frameIndex, const FrameParams &params) {
    FrameUniforms ubo{};
    ubo.iTime = params.time;
    ubo.iResolution[0] = static_cast<float>(swapchainExtent.width);
    ubo.iResolution[1] = static_cast<float>(swapchainExtent.height);
    ubo.iMouse[0] = params.mouseX;
    ubo.iMouse[1] = params.mouseY;
    std::memcpy(frames[frameIndex].uniformMapped, &ubo, sizeof(ubo));
  }

  void createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (FrameSync &frame : frames) {
      checkVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                &frame.imageAvailable),
              "Failed creating Vulkan image-available semaphore");
      checkVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                &frame.renderFinished),
              "Failed creating Vulkan render-finished semaphore");
      checkVk(vkCreateFence(device, &fenceInfo, nullptr, &frame.inFlight),
              "Failed creating Vulkan frame fence");
    }
  }

  void recreateSwapchain() {
    if (hasZeroFramebufferExtent()) {
      return;
    }

    vkDeviceWaitIdle(device);
    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createFramebuffers();
    framebufferResized = false;
  }

  void cleanupSwapchain() {
    for (VkFramebuffer framebuffer : swapchainFramebuffers) {
      vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    swapchainFramebuffers.clear();

    if (renderPass != VK_NULL_HANDLE) {
      vkDestroyRenderPass(device, renderPass, nullptr);
      renderPass = VK_NULL_HANDLE;
    }

    for (VkImageView imageView : swapchainImageViews) {
      vkDestroyImageView(device, imageView, nullptr);
    }
    swapchainImageViews.clear();

    if (swapchain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(device, swapchain, nullptr);
      swapchain = VK_NULL_HANDLE;
    }
    swapchainImages.clear();
  }

  bool hasZeroFramebufferExtent() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window->getGLFWwindow(), &width, &height);
    return width == 0 || height == 0;
  }
};

VulkanRenderer::VulkanRenderer() : impl(std::make_unique<Impl>()) {}

VulkanRenderer::~VulkanRenderer() { shutdown(); }

void VulkanRenderer::configureWindowHints() {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  if (std::getenv("TINY_RASTERIZER_MAX_FRAMES") != nullptr) {
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  }
}

void VulkanRenderer::runHeadlessSmokeTest() {
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Tiny Rasterizer Headless Test";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "Tiny Rasterizer";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  std::vector<const char *> instanceExtensions;
  bool portabilityEnumerationEnabled = false;
  if (hasInstanceExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    portabilityEnumerationEnabled = true;
  }

  VkInstanceCreateInfo instanceCreateInfo{};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pApplicationInfo = &appInfo;
  instanceCreateInfo.enabledExtensionCount =
      static_cast<uint32_t>(instanceExtensions.size());
  instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
  if (portabilityEnumerationEnabled) {
    instanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  }

  VkInstance instance = VK_NULL_HANDLE;
  checkVk(vkCreateInstance(&instanceCreateInfo, nullptr, &instance),
          "Failed creating headless Vulkan instance");

  uint32_t deviceCount = 0;
  checkVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr),
          "Failed counting headless Vulkan physical devices");
  if (deviceCount == 0) {
    vkDestroyInstance(instance, nullptr);
    throw std::runtime_error("No Vulkan physical devices found");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  checkVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()),
          "Failed enumerating headless Vulkan physical devices");

  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  uint32_t graphicsFamily = 0;
  for (VkPhysicalDevice candidate : devices) {
    const std::optional<uint32_t> family = findGraphicsQueueFamily(candidate);
    if (family.has_value()) {
      physicalDevice = candidate;
      graphicsFamily = family.value();
      break;
    }
  }

  if (physicalDevice == VK_NULL_HANDLE) {
    vkDestroyInstance(instance, nullptr);
    throw std::runtime_error("No Vulkan graphics queue family found");
  }

  const float queuePriority = 1.0f;
  VkDeviceQueueCreateInfo queueCreateInfo{};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = graphicsFamily;
  queueCreateInfo.queueCount = 1;
  queueCreateInfo.pQueuePriorities = &queuePriority;

  std::vector<const char *> deviceExtensions;
  if (hasDeviceExtension(physicalDevice, "VK_KHR_portability_subset")) {
    deviceExtensions.push_back("VK_KHR_portability_subset");
  }

  VkPhysicalDeviceFeatures features{};
  VkDeviceCreateInfo deviceCreateInfo{};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
  deviceCreateInfo.pEnabledFeatures = &features;
  deviceCreateInfo.enabledExtensionCount =
      static_cast<uint32_t>(deviceExtensions.size());
  deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

  VkDevice device = VK_NULL_HANDLE;
  checkVk(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device),
          "Failed creating headless Vulkan logical device");

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);
  std::cout << "[test] Headless Vulkan core init passed on "
            << properties.deviceName << std::endl;

  vkDestroyDevice(device, nullptr);
  vkDestroyInstance(instance, nullptr);
}

void VulkanRenderer::init(Window &targetWindow, const ShaderScene &scene,
                          const WindowConfig &) {
  impl->vertexShaderPath = deriveSpirvPath(scene.vertexShader);
  impl->fragmentShaderPath = deriveSpirvPath(scene.fragmentShader);
  impl->init(targetWindow);
  std::cout << "[shader] Active scene pipeline ready: " << scene.name << " ("
            << impl->vertexShaderPath << " + " << impl->fragmentShaderPath
            << ")" << std::endl;
}

void VulkanRenderer::beginFrame(const FrameParams &params) {
  impl->beginFrame(params);
}

void VulkanRenderer::draw(const FrameParams &) {}

void VulkanRenderer::endFrame() { impl->endFrame(); }

void VulkanRenderer::resize(int width, int height) {
  impl->resize(width, height);
}

void VulkanRenderer::shutdown() { impl->shutdown(); }
