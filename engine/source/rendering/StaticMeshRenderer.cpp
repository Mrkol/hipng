#include "rendering/StaticMeshRenderer.hpp"

#include "rendering/primitives/Shader.hpp"


StaticMeshRenderer::StaticMeshRenderer(CreateInfo info)
	: device_{info.device}
	, ds_pool_{info.ds_pool}
{
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
			.descriptorType = vk::DescriptorType::eUniformBuffer,
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
		},
		vk::DescriptorSetLayoutBinding{
			.binding = 3,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = 0,
			.stageFlags = vk::ShaderStageFlagBits::eFragment
		},
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

		
	    vk::PipelineRasterizationStateCreateInfo rasterizer_info{
	        .polygonMode = vk::PolygonMode::eFill,
	    	.cullMode = vk::CullModeFlagBits::eFront,
	    	.frontFace = vk::FrontFace::eClockwise,
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
			vk::DynamicState::eViewport,
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

void StaticMeshRenderer::render(std::size_t frame_index, vk::CommandBuffer cb)
{
	cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.get());
}
