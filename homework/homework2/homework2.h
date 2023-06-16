/*
* Vulkan Example - Variable rate shading
*
* Copyright (C) 2020 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
    bool enableShadingRate = true;
    bool colorShadingRate = false;

    bool isFirstTime = true;

    vkglTF::Model scene;

    vks::Texture shadingRateImage;
    std::vector<vks::Texture> depthAttachments;
    vks::Texture colorAttachmentCopy;

    struct Pipelines {
        VkPipeline opaque;
        VkPipeline masked;
    };

    struct ShaderData {
        vks::Buffer buffer;
        struct Values {
            glm::mat4 projection;
            glm::mat4 view;
            glm::mat4 model = glm::mat4(1.0f);
            glm::vec4 lightPos = glm::vec4(0.0f, 2.5f, 0.0f, 1.0f);
            glm::vec4 viewPos;
            int32_t colorShadingRate;
        } values;
    } shaderData;
    Pipelines basePipelines;
    Pipelines shadingRatePipelines;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPhysicalDeviceShadingRateImagePropertiesNV physicalDeviceShadingRateImagePropertiesNV{};
    VkPhysicalDeviceShadingRateImageFeaturesNV enabledPhysicalDeviceShadingRateImageFeaturesNV{};
    PFN_vkCmdBindShadingRateImageNV vkCmdBindShadingRateImageNV;

    

    VulkanExample();
    ~VulkanExample();

    void loadAssets();

    virtual void getEnabledFeatures() override;
    virtual void setupDepthStencil() override;
    virtual void setupRenderPass() override;
    virtual void setupFrameBuffer() override;

    void prepareShadingRateImage();
    void prepareColorAttachmentCopy();
    void prepareUniformBuffers();

    void setupDescriptors();
    void preparePipelines(
        std::string vert_path, std::string frag_path,
        VkRenderPass renderPass,
        Pipelines* basePipelines, Pipelines* shadingratePipelines = nullptr);

    void copyColorAttachment(VkCommandBuffer& commandBuffer, VkImage& colorAttachment);
    void buildCommandBuffers();

    struct PreZ {
        VkRenderPass renderPass;
        Pipelines pipelines;
        std::vector<VkCommandBuffer> commandBuffers;
    } preZ;

    void setupPreZRenderPass();
    void buildPreZCommandBuffer();

    struct ShadingRateCompute {
        struct ShaderData {
            vks::Buffer buffer;
            struct Values {
                glm::mat4 previousPV;
                glm::mat4 currentPV;
            } values;
        } shaderData;
        VkQueue queue;
        VkCommandPool commandPool;
        std::vector<VkCommandBuffer> commandBuffers;
        VkFence fence;
        VkDescriptorSetLayout descriptorSetLayout;
        std::vector<VkDescriptorSet> descriptorSets;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
    } shadingRateCompute;

    void prepareCompute();
    void buildComputeCommandBuffer();
    
    virtual void prepare() override;

    void updateUniformBuffers();

    void handleResize();
    virtual void render() override;
};