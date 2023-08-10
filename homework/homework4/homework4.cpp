/*
* Vulkan Example - Cube map texture loading and displaying
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include <ktx.h>
#include <ktxvulkan.h>
#include "omp.h"

#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	bool displaySkybox = true;

	vks::Texture cubeMap;

	struct Meshes {
		vkglTF::Model skybox;
		std::vector<vkglTF::Model> objects;
		int32_t objectIndex = 0;
	} models;

	struct {
		vks::Buffer object;
		vks::Buffer skybox;
	} uniformBuffers;

	struct UBOVS {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::mat4 inverseModelview;
		float lodBias = 0.0f;
	} uboVS;

	struct {
		VkPipeline skybox;
		VkPipeline reflect;
	} pipelines;

	struct {
		VkDescriptorSet object;
		VkDescriptorSet skybox;
	} descriptorSets;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

	std::vector<std::string> objectNames;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Cube map textures";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -4.0f));
		camera.setRotation(glm::vec3(0.0f));
		camera.setRotationSpeed(0.25f);
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
	}

	~VulkanExample()
	{
		// Clean up used Vulkan resources
		// Note : Inherited destructor cleans up resources stored in base class

		// Clean up texture resources
		vkDestroyImageView(device, cubeMap.view, nullptr);
		vkDestroyImage(device, cubeMap.image, nullptr);
		vkDestroySampler(device, cubeMap.sampler, nullptr);
		vkFreeMemory(device, cubeMap.deviceMemory, nullptr);

		vkDestroyPipeline(device, pipelines.skybox, nullptr);
		vkDestroyPipeline(device, pipelines.reflect, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		uniformBuffers.object.destroy();
		uniformBuffers.skybox.destroy();
	}

	void findRGBLine(uint8_t max_Index, std::vector<glm::uvec3>& block, glm::uvec3& E0, glm::uvec3& E1, std::vector<uint8_t>& Weights) {
		float min_error = FLT_MAX;
		for (int i = 0; i < block.size(); ++i) {
			for (int j = i + 1; j < block.size(); ++j) {
				glm::uvec3 e0 = block[i];
				glm::uvec3 e1 = block[j];
				if (e0 == e1) continue;

				std::vector<uint8_t> ws;

				glm::vec3 p0 = glm::vec3(e0);
				glm::vec3 v01 = glm::vec3(e1) - p0;
				float len = glm::length(v01) / max_Index;

				float error = 0;
				for (auto p : block) {
					glm::vec3 v = glm::vec3(p) - p0;
					float l = glm::dot(v, glm::normalize(v01)) / len;
					l = glm::clamp(l, 0.0f, float(max_Index));
					uint8_t w = l - uint8_t(l) < 0.5 ? uint8_t(l) : uint8_t(l) + 1;

					ws.push_back(w);

					error += glm::length(glm::vec3(p) - (p0 + w * len * glm::normalize(v01)));
				}

				if (error < min_error) {
					min_error = error;
					E0 = e0;
					E1 = e1;
					Weights = ws;
				}
			}
		}
		if (min_error == FLT_MAX) {
			E0 = block[0];
			E1 = block[0];
			Weights = std::vector<uint8_t>(block.size(), 0);
		}
	}

	// Enable physical device features required for this example
	virtual void getEnabledFeatures()
	{
		if (deviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		}
	}

	void loadCubemap(std::string filename, VkFormat format, bool forceLinearTiling)
	{
		ktxResult result;
		ktxTexture* ktxTexture;

#if defined(__ANDROID__)
		// Textures are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		if (!asset) {
			vks::tools::exitFatal("Could not load texture from " + filename + "\n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
		}
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		ktx_uint8_t *textureData = new ktx_uint8_t[size];
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);
		result = ktxTexture_CreateFromMemory(textureData, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
		delete[] textureData;
#else
		if (!vks::tools::fileExists(filename)) {
			vks::tools::exitFatal("Could not load texture from " + filename + "\n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
		}
		result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
#endif
		assert(result == KTX_SUCCESS);

		// Get properties required for using and upload texture data from the ktx texture object
		cubeMap.width = ktxTexture->baseWidth;
		cubeMap.height = ktxTexture->baseHeight;
		cubeMap.mipLevels = ktxTexture->numLevels;
		ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
		ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);


		//format = VK_FORMAT_BC7_UNORM_BLOCK;
		//int compress_rate = 4;
		format = VK_FORMAT_BC1_RGB_UNORM_BLOCK; 
		int compress_rate = 8;
		int block_data_size = 64 / compress_rate;
		auto get_offset = [&](int level, int face, int x, int y)->int {
			int ret = 0;
			for (int i = 0; i < level; i++) {
				int width = ktxTexture->baseWidth >> i;
				int height = ktxTexture->baseHeight >> i;

				int block_h_num = (width + 3) / 4;
				int block_v_num = (height + 3) / 4;

				int image_data_size = block_h_num * block_v_num * block_data_size;
				ret += 6 * image_data_size;
			}
			int width = ktxTexture->baseWidth >> level;
			int height = ktxTexture->baseHeight >> level;

			int block_h_num = (width + 3) / 4;
			int block_v_num = (height + 3) / 4;

			int image_data_size = block_h_num * block_v_num * block_data_size;
			ret += face * image_data_size;
			ret += (x + y * block_h_num) * block_data_size;
			return ret;
		};

		int bc_data_size = get_offset(cubeMap.mipLevels + 1, 0, 0, 0);
		uint8_t* bc_data = new uint8_t[bc_data_size]{};

		glm::uvec3 block_buffer[4][4];
		for (uint32_t level = 0; level < cubeMap.mipLevels; level++)
		{
			int width = ktxTexture->baseWidth >> level;
			int height = ktxTexture->baseHeight >> level;

			int block_h_num = (width + 3) / 4;
			int block_v_num = (height + 3) / 4;

			for (int face = 0; face < 6; ++face) {
				ktx_size_t offset;
				ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);

				for (int x = 0; x < block_h_num; x++) {
					for (int y = 0; y < block_v_num; y++) {

						int lt_x = x * 4;
						int lt_y = y * 4;

						for (int i = 0; i < 4; ++i) {
							for (int j = 0; j < 4; ++j) {
								int p_x = lt_x + i;
								int p_y = lt_y + j;

								block_buffer[i][j] = { 0, 0, 0 };
								if (p_x < ktxTexture->baseWidth >> level && p_y < ktxTexture->baseHeight >> level) {
									ktx_uint8_t* ptr_s = ktxTextureData + offset + (p_y * width + p_x) * 4;
									block_buffer[i][j] = { *ptr_s, *(ptr_s + 1), *(ptr_s + 2) };
								}
							}
						}

						std::vector<glm::uvec3> block;
						for (int i = 0; i < 4; ++i) 
							for (int j = 0; j < 4; ++j)
								block.push_back(block_buffer[j][i]);

						glm::uvec3 E0;
						glm::uvec3 E1;
						std::vector<uint8_t> Weights;
						findRGBLine(3, block, E0, E1, Weights);

						uint16_t color0 = 0;
						color0 |= uint16_t(E0.b >> 3) << 0;
						color0 |= uint16_t(E0.g >> 2) << 5;
						color0 |= uint16_t(E0.r >> 3) << 11;

						uint16_t color1 = 0;
						color1 |= uint16_t(E1.b >> 3) << 0;
						color1 |= uint16_t(E1.g >> 2) << 5;
						color1 |= uint16_t(E1.r >> 3) << 11;

						bool is_same = color0 == color1;
						bool is_reverse = color0 < color1;

						uint32_t index = 0;
						if (!is_same) {
							for (int i = 0; i < Weights.size(); ++i) {
								uint8_t id = Weights[i];
								if (!is_reverse) {
									if (id == 3)
										id = 1;
									else if (id == 1)
										id = 2;
									else if (id == 2)
										id = 3;
								}
								else {
									if (id == 0)
										id = 1;
									else if (id == 3)
										id = 0;
									else if (id == 1)
										id = 3;
									else if (id == 2)
										id = 2;
								}
								index |= uint32_t(id) << (2 * i);
							}
						}

						uint8_t* ptr_d = bc_data + get_offset(level, face, x, y);
						*(ptr_d) = is_reverse ? color1 : color0;
						*(ptr_d + 1) = is_reverse ? color1 >> 8 : color0 >> 8;
						*(ptr_d + 2) = is_reverse ? color0 : color1;
						*(ptr_d + 3) = is_reverse ? color0 >> 8 : color1 >> 8;
						*(ptr_d + 4) = index;
						*(ptr_d + 5) = index >> 8;
						*(ptr_d + 6) = index >> 16;
						*(ptr_d + 7) = index >> 24;


						// // BC7
						//uint32_t color_r = 0;
						//color_r |= uint32_t(block_buffer[0][0].r >> 2) << 8;
						//color_r |= uint32_t(block_buffer[0][0].r >> 2) << 14;
						//color_r |= uint32_t(block_buffer[3][3].r >> 2) << 20;
						//color_r |= uint32_t(block_buffer[3][3].r >> 2) << 26;
						//uint32_t color_g = 0;
						//color_g |= uint32_t(block_buffer[0][0].g >> 2) << 8;
						//color_g |= uint32_t(block_buffer[0][0].g >> 2) << 14;
						//color_g |= uint32_t(block_buffer[3][3].g >> 2) << 20;
						//color_g |= uint32_t(block_buffer[3][3].g >> 2) << 26;
						//uint32_t color_b = 0;
						//color_b |= uint32_t(block_buffer[0][0].b >> 2) << 8;
						//color_b |= uint32_t(block_buffer[0][0].b >> 2) << 14;
						//color_b |= uint32_t(block_buffer[3][3].b >> 2) << 20;
						//color_b |= uint32_t(block_buffer[3][3].b >> 2) << 26;

						//uint8_t* ptr_d = bc_data + get_offset(level, face, x, y);
						//*(ptr_d) = (1 << 1);
						//*(ptr_d + 1) = color_r >> 8;
						//*(ptr_d + 2) = color_r >> 16;
						//*(ptr_d + 3) = color_r >> 24;
						//*(ptr_d + 4) = color_g >> 8; 
						//*(ptr_d + 5) = color_g >> 16;
						//*(ptr_d + 6) = color_g >> 24;
						//*(ptr_d + 7) = color_b >> 8; 
						//*(ptr_d + 8) = color_b >> 16; 
						//*(ptr_d + 9) = color_b >> 24;
					}
				}
			}
		}



		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = ktxTextureSize;
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(device, stagingMemory, 0, memReqs.size, 0, (void **)&data));
		//memcpy(data, ktxTextureData, ktxTextureSize);
		memcpy(data, bc_data, bc_data_size);
		vkUnmapMemory(device, stagingMemory);

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = cubeMap.mipLevels;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { cubeMap.width, cubeMap.height, 1 };
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		// Cube faces count as array layers in Vulkan
		imageCreateInfo.arrayLayers = 6;
		// This flag is required for cube map images
		imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &cubeMap.image));

		vkGetImageMemoryRequirements(device, cubeMap.image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &cubeMap.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, cubeMap.image, cubeMap.deviceMemory, 0));

		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Setup buffer copy regions for each face including all of its miplevels
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		uint32_t offset = 0;

		for (uint32_t face = 0; face < 6; face++)
		{
			for (uint32_t level = 0; level < cubeMap.mipLevels; level++)
			{
				// Calculate offset into staging buffer for the current mip level and face
				ktx_size_t offset;
				KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
				assert(ret == KTX_SUCCESS);
				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = level;
				bufferCopyRegion.imageSubresource.baseArrayLayer = face;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
				bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
				bufferCopyRegion.imageExtent.depth = 1;
				//bufferCopyRegion.bufferOffset = offset;
				bufferCopyRegion.bufferOffset = get_offset(level, face, 0, 0);
				bufferCopyRegions.push_back(bufferCopyRegion);
			}
		}

		// Image barrier for optimal image (target)
		// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = cubeMap.mipLevels;
		subresourceRange.layerCount = 6;

		vks::tools::setImageLayout(
			copyCmd,
			cubeMap.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy the cube map faces from the staging buffer to the optimal tiled image
		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			cubeMap.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data()
			);

		// Change texture image layout to shader read after all faces have been copied
		cubeMap.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vks::tools::setImageLayout(
			copyCmd,
			cubeMap.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			cubeMap.imageLayout,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

		// Create sampler
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = cubeMap.mipLevels;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		sampler.maxAnisotropy = 1.0f;
		if (vulkanDevice->features.samplerAnisotropy)
		{
			sampler.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
			sampler.anisotropyEnable = VK_TRUE;
		}
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &cubeMap.sampler));

		// Create image view
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		// Cube map view type
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		view.format = format;
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		// 6 array layers (faces)
		view.subresourceRange.layerCount = 6;
		// Set number of mip levels
		view.subresourceRange.levelCount = cubeMap.mipLevels;
		view.image = cubeMap.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &cubeMap.view));

		// Clean up staging resources
		vkFreeMemory(device, stagingMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		ktxTexture_Destroy(ktxTexture);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = defaultClearColor;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)width,	(float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(width,	height,	0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			// Skybox
			if (displaySkybox)
			{
				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.skybox, 0, NULL);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
				models.skybox.draw(drawCmdBuffers[i]);
			}

			// 3D object
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.object, 0, NULL);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.reflect);
			models.objects[models.objectIndex].draw(drawCmdBuffers[i]);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;
		// Skybox
		models.skybox.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
		// Objects
		std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf" };
		objectNames = { "Sphere", "Teapot", "Torusknot", "Venus" };
		models.objects.resize(filenames.size());
		for (size_t i = 0; i < filenames.size(); i++) {
			models.objects[i].loadFromFile(getAssetPath() + "models/" + filenames[i], vulkanDevice, queue, glTFLoadingFlags);
		}
		// Cubemap texture
		const bool forceLinearTiling = false;
		loadCubemap(getAssetPath() + "textures/cubemap_yokohama_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, forceLinearTiling);
	}

	void setupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vks::initializers::descriptorPoolCreateInfo(
				poolSizes.size(),
				poolSizes.data(),
				2);

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
		{
			// Binding 0 : Uniform buffer
			vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0),
			// Binding 1 : Fragment shader image sampler
			vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				1)
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout =
			vks::initializers::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				setLayoutBindings.size());

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vks::initializers::pipelineLayoutCreateInfo(
				&descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void setupDescriptorSets()
	{
		// Image descriptor for the cube map texture
		VkDescriptorImageInfo textureDescriptor =
			vks::initializers::descriptorImageInfo(
				cubeMap.sampler,
				cubeMap.view,
				cubeMap.imageLayout);

		VkDescriptorSetAllocateInfo allocInfo =
			vks::initializers::descriptorSetAllocateInfo(
				descriptorPool,
				&descriptorSetLayout,
				1);

		// 3D object descriptor set
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.object));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(
				descriptorSets.object,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformBuffers.object.descriptor),
			// Binding 1 : Fragment shader cubemap sampler
			vks::initializers::writeDescriptorSet(
				descriptorSets.object,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&textureDescriptor)
		};
		vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

		// Sky box descriptor set
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.skybox));

		writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(
				descriptorSets.skybox,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformBuffers.skybox.descriptor),
			// Binding 1 : Fragment shader cubemap sampler
			vks::initializers::writeDescriptorSet(
				descriptorSets.skybox,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&textureDescriptor)
		};
		vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = shaderStages.size();
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal });

		// Skybox pipeline (background cube)
		shaderStages[0] = loadShader(getShadersPath() + "texturecubemap/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "texturecubemap/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.skybox));

		// Cube map reflect pipeline
		shaderStages[0] = loadShader(getShadersPath() + "texturecubemap/reflect.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "texturecubemap/reflect.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Enable depth test and write
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthTestEnable = VK_TRUE;
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.reflect));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Object vertex shader uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.object,
			sizeof(uboVS)));

		// Skybox vertex shader uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.skybox,
			sizeof(uboVS)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.object.map());
		VK_CHECK_RESULT(uniformBuffers.skybox.map());

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		// 3D object
		uboVS.projection = camera.matrices.perspective;
		uboVS.modelView = camera.matrices.view;
		uboVS.inverseModelview = glm::inverse(camera.matrices.view);
		memcpy(uniformBuffers.object.mapped, &uboVS, sizeof(uboVS));
		// Skybox
		uboVS.modelView = camera.matrices.view;
		// Cancel out translation
		uboVS.modelView[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		memcpy(uniformBuffers.skybox.mapped, &uboVS, sizeof(uboVS));
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSets();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();
	}

	virtual void viewChanged()
	{
		updateUniformBuffers();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			if (overlay->sliderFloat("LOD bias", &uboVS.lodBias, 0.0f, (float)cubeMap.mipLevels)) {
				updateUniformBuffers();
			}
			if (overlay->comboBox("Object type", &models.objectIndex, objectNames)) {
				buildCommandBuffers();
			}
			if (overlay->checkBox("Skybox", &displaySkybox)) {
				buildCommandBuffers();
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()