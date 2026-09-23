#include "ComputeContext.h"

#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

void checkVk(VkResult result, const std::string &message) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(message + " (VkResult " +
                             std::to_string(result) + ")");
  }
}

} // namespace

ComputeContext::~ComputeContext() { destroy(); }

ComputeContext::ComputeContext(ComputeContext &&other) noexcept
    : handles_(other.handles_), label_(std::move(other.label_)),
      available_(other.available_),
      descriptorSetLayout_(other.descriptorSetLayout_),
      pipelineLayout_(other.pipelineLayout_), pipeline_(other.pipeline_),
      descriptorPool_(other.descriptorPool_),
      descriptorSet_(other.descriptorSet_), boundView_(other.boundView_) {
  other.handles_ = DeviceHandles{};
  other.descriptorSetLayout_ = VK_NULL_HANDLE;
  other.pipelineLayout_ = VK_NULL_HANDLE;
  other.pipeline_ = VK_NULL_HANDLE;
  other.descriptorPool_ = VK_NULL_HANDLE;
  other.descriptorSet_ = VK_NULL_HANDLE;
  other.boundView_ = VK_NULL_HANDLE;
  other.available_ = false;
}

ComputeContext &ComputeContext::operator=(ComputeContext &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  destroy();
  handles_ = other.handles_;
  label_ = std::move(other.label_);
  available_ = other.available_;
  descriptorSetLayout_ = other.descriptorSetLayout_;
  pipelineLayout_ = other.pipelineLayout_;
  pipeline_ = other.pipeline_;
  descriptorPool_ = other.descriptorPool_;
  descriptorSet_ = other.descriptorSet_;
  boundView_ = other.boundView_;

  other.handles_ = DeviceHandles{};
  other.descriptorSetLayout_ = VK_NULL_HANDLE;
  other.pipelineLayout_ = VK_NULL_HANDLE;
  other.pipeline_ = VK_NULL_HANDLE;
  other.descriptorPool_ = VK_NULL_HANDLE;
  other.descriptorSet_ = VK_NULL_HANDLE;
  other.boundView_ = VK_NULL_HANDLE;
  other.available_ = false;
  return *this;
}

bool ComputeContext::queryQueueFamily(VkPhysicalDevice physicalDevice,
                                      uint32_t graphicsFamily,
                                      uint32_t *computeFamilyOut) {
  if (physicalDevice == VK_NULL_HANDLE || computeFamilyOut == nullptr) {
    return false;
  }

  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
  if (count == 0) {
    return false;
  }

  std::vector<VkQueueFamilyProperties> families(count);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count,
                                           families.data());

  // 首选与图形队列同族：同一个队列上 dispatch + 绘制，避免跨队列同步。
  if (graphicsFamily < count &&
      (families[graphicsFamily].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
    *computeFamilyOut = graphicsFamily;
    return true;
  }

  for (uint32_t i = 0; i < count; ++i) {
    if (families[i].queueCount > 0 &&
        (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
      *computeFamilyOut = i;
      return true;
    }
  }

  return false;
}

bool ComputeContext::init(const DeviceHandles &handles,
                          const std::string &label) {
  destroy();

  if (handles.device == VK_NULL_HANDLE ||
      handles.queueFamilyIndex == VK_QUEUE_FAMILY_IGNORED) {
    std::cerr << "[compute] No usable compute queue family, compute disabled"
              << std::endl;
    return false;
  }

  handles_ = handles;
  label_ = label;

  createDescriptorSetLayout();
  createPipelineLayout(0);
  createDescriptorPool();
  allocateDescriptorSet();

  available_ = true;
  std::cout << "[compute] ComputeContext ready (" << label_
            << ", queueFamily=" << handles_.queueFamilyIndex << ")" << std::endl;
  return true;
}

void ComputeContext::createDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  createInfo.bindingCount = 1;
  createInfo.pBindings = &binding;

  checkVk(vkCreateDescriptorSetLayout(handles_.device, &createInfo, nullptr,
                                      &descriptorSetLayout_),
          "Failed creating compute descriptor set layout");
}

void ComputeContext::createPipelineLayout(uint32_t pushConstantSize) {
  if (pipelineLayout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(handles_.device, pipelineLayout_, nullptr);
    pipelineLayout_ = VK_NULL_HANDLE;
  }

  VkPushConstantRange pushRange{};
  pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  pushRange.offset = 0;
  pushRange.size = pushConstantSize;

  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &descriptorSetLayout_;
  if (pushConstantSize > 0) {
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
  }

  checkVk(vkCreatePipelineLayout(handles_.device, &layoutInfo, nullptr,
                                 &pipelineLayout_),
          "Failed creating compute pipeline layout");
}

void ComputeContext::createPipeline(VkShaderModule module,
                                    uint32_t pushConstantSize) {
  if (!available_) {
    return;
  }
  destroyPipeline();
  createPipelineLayout(pushConstantSize);

  VkPipelineShaderStageCreateInfo stage{};
  stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage.module = module;
  stage.pName = "main";

  VkComputePipelineCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  createInfo.stage = stage;
  createInfo.layout = pipelineLayout_;

  checkVk(vkCreateComputePipelines(handles_.device, VK_NULL_HANDLE, 1,
                                   &createInfo, nullptr, &pipeline_),
          "Failed creating compute pipeline (" + label_ + ")");
  std::cout << "[compute] Pipeline created (" << label_ << ")" << std::endl;
}

void ComputeContext::createDescriptorPool() {
  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  poolSize.descriptorCount = 1;

  VkDescriptorPoolCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  createInfo.poolSizeCount = 1;
  createInfo.pPoolSizes = &poolSize;
  createInfo.maxSets = 1;

  checkVk(vkCreateDescriptorPool(handles_.device, &createInfo, nullptr,
                                 &descriptorPool_),
          "Failed creating compute descriptor pool");
}

void ComputeContext::allocateDescriptorSet() {
  VkDescriptorSetAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocateInfo.descriptorPool = descriptorPool_;
  allocateInfo.descriptorSetCount = 1;
  allocateInfo.pSetLayouts = &descriptorSetLayout_;

  checkVk(vkAllocateDescriptorSets(handles_.device, &allocateInfo,
                                   &descriptorSet_),
          "Failed allocating compute descriptor set");
}

void ComputeContext::updateStorageImage(VkImageView view) {
  if (!available_ || view == VK_NULL_HANDLE) {
    return;
  }
  boundView_ = view;

  VkDescriptorImageInfo imageInfo{};
  // storage image 在 compute 使用期间必须处于 GENERAL 布局。
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageInfo.imageView = view;
  imageInfo.sampler = VK_NULL_HANDLE;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptorSet_;
  write.dstBinding = 0;
  write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  write.descriptorCount = 1;
  write.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(handles_.device, 1, &write, 0, nullptr);
}

void ComputeContext::dispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX,
                             uint32_t groupCountY, uint32_t groupCountZ,
                             const void *pushConstantData,
                             uint32_t pushConstantSize) const {
  if (!available_ || pipeline_ == VK_NULL_HANDLE) {
    return;
  }

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
  if (pushConstantData != nullptr && pushConstantSize > 0) {
    vkCmdPushConstants(commandBuffer, pipelineLayout_,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize,
                       pushConstantData);
  }
  vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void ComputeContext::insertWriteToReadBarrier(VkCommandBuffer commandBuffer,
                                              VkImage image) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  // 显式同步点：compute 写 -> fragment 采样读。
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}

void ComputeContext::destroyPipeline() {
  if (pipeline_ != VK_NULL_HANDLE && handles_.device != VK_NULL_HANDLE) {
    vkDestroyPipeline(handles_.device, pipeline_, nullptr);
  }
  pipeline_ = VK_NULL_HANDLE;
}

void ComputeContext::destroy() {
  if (handles_.device != VK_NULL_HANDLE) {
    destroyPipeline();
    if (pipelineLayout_ != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(handles_.device, pipelineLayout_, nullptr);
    }
    if (descriptorPool_ != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(handles_.device, descriptorPool_, nullptr);
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(handles_.device, descriptorSetLayout_,
                                   nullptr);
    }
  }

  pipelineLayout_ = VK_NULL_HANDLE;
  descriptorPool_ = VK_NULL_HANDLE;
  descriptorSetLayout_ = VK_NULL_HANDLE;
  descriptorSet_ = VK_NULL_HANDLE;
  boundView_ = VK_NULL_HANDLE;
  handles_ = DeviceHandles{};
  available_ = false;
}
