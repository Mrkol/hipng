#include "rendering/StaticMeshRenderer.hpp"

#include <unordered_set>
#include <glm/ext/matrix_clip_space.hpp>

#include "assets/AssetHandle.hpp"
#include "rendering/gpu_storage/GpuStorageManager.hpp"
#include "rendering/primitives/Shader.hpp"
#include "shader_cpp_bridge/static_mesh.h"
#include "util/Align.hpp"


StaticMeshRenderer::StaticMeshRenderer(CreateInfo info)
	: device_{info.device}
	, allocator_{info.allocator}
	, ds_pool_{info.ds_pool}
	, storage_manager_{info.storage_manager}
	, per_frame_dses_{std::in_place}
{
	sampler_ = device_.createSamplerUnique(vk::SamplerCreateInfo{
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eClampToBorder,
		.addressModeV = vk::SamplerAddressMode::eClampToBorder,
		.addressModeW = vk::SamplerAddressMode::eClampToBorder,
		.borderColor = vk::BorderColor::eIntOpaqueBlack,
	});

	std::array global_bindings{
		vk::DescriptorSetLayoutBinding{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eAll
		}
	};

	global_dsl_ = device_.createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
		.bindingCount = static_cast<uint32_t>(global_bindings.size()),
		.pBindings = global_bindings.data(),
	});

	std::array material_bindings{
		vk::DescriptorSetLayoutBinding{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eUniformBufferDynamic,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eFragment
		},
		vk::DescriptorSetLayoutBinding{
			.binding = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eFragment
		},
		vk::DescriptorSetLayoutBinding{
			.binding = 2,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eFragment
		}
	};
	
	material_dsl_ = device_.createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
		.bindingCount = static_cast<uint32_t>(material_bindings.size()),
		.pBindings = material_bindings.data(),
	});
	
	std::array object_bindings{
		vk::DescriptorSetLayoutBinding{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eUniformBufferDynamic,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eVertex
		},
	};

	object_dsl_ = device_.createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
		.bindingCount = static_cast<uint32_t>(object_bindings.size()),
		.pBindings = object_bindings.data(),
	});

	std::array dsls { global_dsl_.get(), material_dsl_.get(), object_dsl_.get() };

	pipeline_layout_ = device_.createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{
		.setLayoutCount = static_cast<uint32_t>(dsls.size()),
		.pSetLayouts = dsls.data(),
	});
	
	{
		Shader vert(device_, "static_mesh.vert");
		Shader frag(device_, "static_mesh.frag");

		
	    std::vector shader_stages{
	        vk::PipelineShaderStageCreateInfo{
	        	.stage = vk::ShaderStageFlagBits::eVertex,
	        	.module = vert.get(),
	        	.pName = "main",
	        },
	        vk::PipelineShaderStageCreateInfo{
	        	.stage = vk::ShaderStageFlagBits::eFragment,
	        	.module = frag.get(),
	        	.pName = "main",
	        },
	    };

		
	    vk::VertexInputBindingDescription vertex_input_binding_description{
	    	.binding =  0,
	    	.stride = sizeof(float) * 8,
	    	.inputRate = vk::VertexInputRate::eVertex
	    };
		
	    std::array vertex_input_attribute_descriptions{
	        vk::VertexInputAttributeDescription{
	        	.location = 0,
	        	.binding = 0,
	        	.format = vk::Format::eR32G32B32A32Sfloat,
	        	.offset = 0
	        },
	        vk::VertexInputAttributeDescription{
	        	.location = 1,
	            .binding = 0,
	        	.format = vk::Format::eR32G32B32A32Sfloat,
	        	.offset = sizeof(float)*4
	        }
	    };
		
	    vk::PipelineVertexInputStateCreateInfo vertex_input_info{
	        .vertexBindingDescriptionCount =  1,
	    	.pVertexBindingDescriptions = &vertex_input_binding_description,
			.vertexAttributeDescriptionCount =
				static_cast<uint32_t>(vertex_input_attribute_descriptions.size()),
	    	.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data(),
	    };

	    vk::PipelineInputAssemblyStateCreateInfo input_assembly_info{
	        .topology = vk::PrimitiveTopology::eTriangleList,
	    };

		// all dynamic baby
		vk::PipelineViewportStateCreateInfo viewport_info{
			.viewportCount = 1,
			.scissorCount = 1,
		};
		
	    vk::PipelineRasterizationStateCreateInfo rasterizer_info{
	        .polygonMode = vk::PolygonMode::eFill,
	    	.cullMode = vk::CullModeFlagBits::eBack,
	    	.frontFace = vk::FrontFace::eCounterClockwise,
	        .lineWidth =  1
	    };

	    vk::PipelineMultisampleStateCreateInfo multisample_info{
	        .rasterizationSamples = vk::SampleCountFlagBits::e1,
	    };

	    vk::PipelineColorBlendAttachmentState color_blend_attachment_state{
			.blendEnable = false,
	        .srcColorBlendFactor = vk::BlendFactor::eOne,
	    	.dstColorBlendFactor = vk::BlendFactor::eZero,
	    	.colorBlendOp = vk::BlendOp::eAdd,
	        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
	    	.dstAlphaBlendFactor = vk::BlendFactor::eZero,
	    	.alphaBlendOp = vk::BlendOp::eAdd,
	        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
	            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	    };

	    vk::PipelineColorBlendStateCreateInfo color_blend_state_create_info{
	    	.attachmentCount = 1,
	    	.pAttachments = &color_blend_attachment_state,
	    };

	    vk::PipelineDepthStencilStateCreateInfo depth_stencil_info{
	        .depthTestEnable =  true,
			.depthWriteEnable = true,
			.depthCompareOp = vk::CompareOp::eLess,
			.depthBoundsTestEnable = false,
			.stencilTestEnable = false,
	    };

		std::array dynamic_state{
			vk::DynamicState::eViewport, vk::DynamicState::eScissor
		};

		vk::PipelineDynamicStateCreateInfo dyn_state_info{
			.dynamicStateCount = static_cast<uint32_t>(dynamic_state.size()),
			.pDynamicStates = dynamic_state.data(),
		};

		auto res = device_.createGraphicsPipelineUnique(VK_NULL_HANDLE,
			vk::GraphicsPipelineCreateInfo{
				.stageCount = static_cast<uint32_t>(shader_stages.size()),
				.pStages = shader_stages.data(),
				.pVertexInputState = &vertex_input_info,
				.pInputAssemblyState = &input_assembly_info,
				.pViewportState = &viewport_info,
				.pRasterizationState = &rasterizer_info,
				.pMultisampleState = &multisample_info,
				.pDepthStencilState = &depth_stencil_info,
				.pColorBlendState = &color_blend_state_create_info,
				.pDynamicState = &dyn_state_info,
				.layout = pipeline_layout_.get(),
				.renderPass = info.renderpass,
			});

		NG_VERIFY(res.result == vk::Result::eSuccess);
		pipeline_ = std::move(res.value);
	}
}

void StaticMeshRenderer::render(std::size_t frame_index, vk::CommandBuffer cb, const FramePacket& packet)
{
	std::vector<StaticMeshPacket> static_meshes;
	static_meshes.reserve(packet.static_meshes.size());
	for (auto& mesh : packet.static_meshes)
	{
		if (storage_manager_->getStaticMesh(mesh.model) != nullptr)
		{
			static_meshes.emplace_back(mesh);
		}
	}

	struct PerMaterial
	{
		uint32_t index;
		std::unordered_set<Meshlet*> meshlets; // drawn for each instance
		std::vector<uint32_t> objects;
		StaticMesh* model;
	};

	uint32_t material_count = 0;
	uint32_t object_idx = 0;
	std::unordered_map<Material*, PerMaterial> materials;
	for (auto&& mesh : static_meshes)
	{
		auto model = storage_manager_->getStaticMesh(mesh.model);

		for(auto& meshlet : model->meshlets)
		{
			if (!materials.contains(meshlet.material))
			{
				materials.emplace(meshlet.material,
					PerMaterial{
						.index = material_count++,
						.model = model,
					});
			}

			auto& per = materials.at(meshlet.material);
			per.meshlets.insert(&meshlet);
			per.objects.push_back(object_idx);
		}

		++object_idx;
	}

	auto& per_frame = per_frame_dses_.get(frame_index)->emplace();
	
	auto gubo_size = align(sizeof(GlobalUBO), std::size_t{64});
	auto mubo_size = align(sizeof(MaterialUBO), std::size_t{64});
	auto oubo_size = align(sizeof(ObjectUBO), std::size_t{64});

	per_frame.global_ubo = UniqueVmaBuffer(allocator_, gubo_size,
		vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);

	if (material_count > 0)
	{
		per_frame.material_ubos = UniqueVmaBuffer(allocator_, mubo_size * material_count,
			vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
	}

	if (static_meshes.size() > 0)
	{
		per_frame.object_ubos = UniqueVmaBuffer(allocator_, oubo_size * static_meshes.size(),
			vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
	}
	
	per_frame.materials.resize(material_count);

	{
		std::vector layouts(material_count, material_dsl_.get());
		layouts.push_back(global_dsl_.get());
		layouts.push_back(object_dsl_.get());
	
		auto sets = device_.allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo{
				.descriptorPool = ds_pool_,
				.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
				.pSetLayouts = layouts.data(),
			});

		std::move(sets.begin(), sets.end() - 2, per_frame.materials.begin());
		
		per_frame.global = std::move(sets[material_count]);
		per_frame.object = std::move(sets[material_count + 1]);
	}
	
	std::vector<vk::WriteDescriptorSet> writes;
	vk::DescriptorBufferInfo global_buffer_write{
			.buffer = per_frame.global_ubo.get(),
			.range = sizeof(GlobalUBO),
		};
	vk::DescriptorBufferInfo material_buffer_write{
			.buffer = per_frame.material_ubos.get(),
			.range = sizeof(MaterialUBO),
		};
	vk::DescriptorBufferInfo object_buffer_write{
			.buffer = per_frame.object_ubos.get(),
			.range = sizeof(ObjectUBO),
		};
	std::vector<vk::DescriptorImageInfo> texture_writes;
	texture_writes.reserve(materials.size() * 2);

	{
		auto data = reinterpret_cast<GlobalUBO*>(per_frame.global_ubo.map());

		data->view = packet.view;
		data->proj = glm::perspective(glm::radians(packet.fov / packet.aspect), packet.aspect,
				packet.near, packet.far);
		data->proj[1][1] *= -1;

		data->ambientLight = {0.3f, 0.3f, 0.3f, 0.f};

		per_frame.global_ubo.unmap();

		
		writes.emplace_back(vk::WriteDescriptorSet{
				.dstSet = per_frame.global.get(),
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &global_buffer_write,
			});
	}

	if (material_count > 0)
	{
		auto data = per_frame.material_ubos.map();

		for (auto&[material, permat] : materials)
		{
			auto mat = data + mubo_size * permat.index;

			std::memcpy(mat, &material->ubo, sizeof(MaterialUBO));
			
			writes.emplace_back(vk::WriteDescriptorSet{
					.dstSet = per_frame.materials[permat.index].get(),
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBufferDynamic,
					.pBufferInfo = &material_buffer_write,
				});
			
			writes.emplace_back(vk::WriteDescriptorSet{
					.dstSet = per_frame.materials[permat.index].get(),
					.dstBinding = 1,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eCombinedImageSampler,
					.pImageInfo = &texture_writes.emplace_back(vk::DescriptorImageInfo{
						.sampler = sampler_.get(),
						.imageView = material->base_color_view.get(),
						.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
					})
				});
			
			writes.emplace_back(vk::WriteDescriptorSet{
					.dstSet = per_frame.materials[permat.index].get(),
					.dstBinding = 2,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eCombinedImageSampler,
					.pImageInfo = &texture_writes.emplace_back(vk::DescriptorImageInfo{
						.sampler = sampler_.get(),
						.imageView = material->occlusion_metalic_roughness_view.get(),
						.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
					})
				});
		}

		per_frame.material_ubos.unmap();
	}

	if (static_meshes.size() > 0)
	{
		auto data = per_frame.object_ubos.map();

		for(auto& mesh : static_meshes)
		{
			std::memcpy(data, &mesh.ubo, sizeof(ObjectUBO));

			data += oubo_size;
		}

		per_frame.object_ubos.unmap();

		writes.emplace_back(vk::WriteDescriptorSet{
				.dstSet = per_frame.object.get(),
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBufferDynamic,
				.pBufferInfo = &object_buffer_write,
			});
	}

	device_.updateDescriptorSets(writes, {});


	cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.get());

	cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout_.get(), 0, 1,
		&per_frame.global.get(), 0, nullptr);

	for (auto&[mat, permat] : materials)
	{
		uint32_t dyn_offset = permat.index * mubo_size;
		cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout_.get(), 1, 1,
			&per_frame.materials[permat.index].get(), 1, &dyn_offset);

		cb.bindIndexBuffer(permat.model->index_buffer.get(), 0, vk::IndexType::eUint16);
		vk::DeviceSize offsets = 0;
		auto buf = permat.model->vertex_buffer.get();
		cb.bindVertexBuffers(0, 1, &buf, &offsets);

		for (auto obj_idx : permat.objects)
		{
			uint32_t dyn_off_obj = obj_idx * oubo_size;
			cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout_.get(), 2, 1,
				&per_frame.object.get(), 1, &dyn_off_obj);

			for (auto meshlet : permat.meshlets)
			{
				cb.drawIndexed(meshlet->index_count, 1, meshlet->index_offset, meshlet->vertex_offset, 0);
			}
		}
	}
}
