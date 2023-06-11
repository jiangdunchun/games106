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
    struct ShadingRateImage {
        VkImage image;
        VkDeviceMemory memory;
        VkImageView view;
        VkImageLayout         imageLayout;
        uint32_t              width, height;
        VkDescriptorImageInfo descriptor;
        VkSampler             sampler;
    } vrsShadingRateImage;
    vks::Texture vrsColorAttachment;
    struct VRSCompute {
        VkQueue queue;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
        VkFence fence;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
        VkMemoryBarrier copyColorAttachmentMemoryBarrier;
    } vrsCompute;

    struct PreZ {
        VkQueue queue;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
        VkFence fence;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
        VkMemoryBarrier copyColorAttachmentMemoryBarrier;

        std::vector<vks::Texture> depth_textures;

    } preZ;

    bool isFirstTime = true;

    vkglTF::Model scene;

    bool enableShadingRate = true;
    bool colorShadingRate = false;

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

    struct Pipelines {
        VkPipeline opaque;
        VkPipeline masked;
    };

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
    virtual void getEnabledFeatures();
    void prepareTextureTarget(vks::Texture* tex, uint32_t width, uint32_t height, VkFormat format);
    void handleResize();
    void buildCommandBuffers();
    void loadglTFFile(std::string filename);
    void loadAssets();
    void prepareShadingRateImage();
    void copyColorAttachment(VkCommandBuffer& commandBuffer, VkImage& colorAttachment);
    void buildComputeCommandBuffer();
    void prepareCompute();
    void preparePreZPass();
    virtual void setupRenderPass() override;
    void draw();
    void setupDescriptors();
    VkRenderPass preZRenderPass;
    Pipelines preZPipelines;
    std::vector<VkCommandBuffer> preZCmdBuffers;
    void buildPreZCommandBuffer();
    void setupPreZRenderPass();
    void preparePipelines(std::string vert, std::string frag, VkRenderPass renderPass, Pipelines* basePipelines, Pipelines* shadingratePipelines = nullptr);
    void preparePipelines();
    void prepareUniformBuffers();
    void updateUniformBuffers();
    void prepare();
    virtual void render();
    virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay);
    struct DepthStencil {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    };
    std::vector<DepthStencil> depthTextures;
    virtual void setupDepthStencil() override;
    virtual void setupFrameBuffer() override;
};