#ifndef COMPUTE_CONTEXT_H
#define COMPUTE_CONTEXT_H

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

// Phase 7：Compute 扩展预留。
//
// 职责边界：
//   - 只负责 compute pipeline / descriptor / dispatch 的最小闭环；
//   - 不持有 VkDevice / VkQueue / VkCommandPool 的生命周期，全部由渲染器注入；
//   - 同步点显式暴露（insertWriteToReadBarrier），不隐式做跨队列等待。
class ComputeContext {
public:
  // 渲染器注入的设备资源（所有权仍属于渲染器）。
  struct DeviceHandles {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
  };

  ComputeContext() = default;
  ~ComputeContext();

  ComputeContext(const ComputeContext &) = delete;
  ComputeContext &operator=(const ComputeContext &) = delete;
  // 可移动：渲染器的 Impl 需要在 shutdown 时整体复位。
  ComputeContext(ComputeContext &&other) noexcept;
  ComputeContext &operator=(ComputeContext &&other) noexcept;

  // 探测 compute 队列族：优先选择与图形队列同族的 compute 队列，
  // 这样 dispatch 与后续绘制在同一个队列上，无需额外的 semaphore。
  static bool queryQueueFamily(VkPhysicalDevice physicalDevice,
                               uint32_t graphicsFamily,
                               uint32_t *computeFamilyOut);

  // 创建 descriptor set layout / pipeline layout / descriptor pool。
  // 返回 false 表示 compute 不可用（调用方应退回 CPU 路径）。
  bool init(const DeviceHandles &handles, const std::string &label);

  void destroy();

  bool available() const { return available_; }
  const std::string &label() const { return label_; }

  // 用已编译好的 SPIR-V 创建 compute pipeline（GLSL -> SPIR-V 由
  // ShaderManager 负责）。相同 pipeline 重复创建会先销毁旧的。
  void createPipeline(VkShaderModule module, uint32_t pushConstantSize);

  // 绑定 compute 的输出目标（storage image，使用期间需处于 GENERAL 布局）。
  void updateStorageImage(VkImageView view);

  // 录制一次 dispatch。必须已调用 createPipeline / updateStorageImage。
  // pushConstantSize 需与 createPipeline 声明的大小一致。
  void dispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX,
                uint32_t groupCountY, uint32_t groupCountZ,
                const void *pushConstantData = nullptr,
                uint32_t pushConstantSize = 0) const;

  // 同步点：compute 写 -> fragment 读，并把 image 切到 SHADER_READ_ONLY_OPTIMAL。
  static void insertWriteToReadBarrier(VkCommandBuffer commandBuffer,
                                       VkImage image);

  VkPipeline pipeline() const { return pipeline_; }
  VkPipelineLayout pipelineLayout() const { return pipelineLayout_; }
  VkDescriptorSet descriptorSet() const { return descriptorSet_; }

private:
  DeviceHandles handles_{};
  std::string label_;
  bool available_ = false;

  VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
  VkImageView boundView_ = VK_NULL_HANDLE;

  void createDescriptorSetLayout();
  void createPipelineLayout(uint32_t pushConstantSize);
  void createDescriptorPool();
  void allocateDescriptorSet();
  void destroyPipeline();
};

#endif // COMPUTE_CONTEXT_H
