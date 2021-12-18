#include "rendering/gpu_storage/GpuStorageManager.hpp"

#include <unifex/on.hpp>

#include "util/Defer.hpp"



GpuStorageManager::GpuStorageManager(CreateInfo info)
	: device_{info.device}
	, allocator_{info.allocator}
{
}

unifex::task<StaticMesh*> GpuStorageManager::uploadStaticMesh(AssetHandle handle, const tinygltf::Model& model)
{
	{
		co_await maps_mtx_.async_lock();
		Defer defer{[this]() { maps_mtx_.unlock(); }};

		if (static_meshes_.contains(handle))
		{
			co_return &static_meshes_.at(handle);
		}
	}

	
	co_await unifex::schedule(g_engine.blockingScheduler());

	StaticMesh result;
	
	std::size_t staging_size = 0;
	for (auto& buffer : model.buffers)
	{
		staging_size += buffer.data.size();
	}

	for (auto& image : model.images)
	{
		staging_size += image.image.size();
	}

	auto staging = UniqueVmaBuffer(allocator_, staging_size,
		vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_TO_GPU);
	
	std::vector<vk::CopyBufferToImageInfo2KHR> image_uploads;
	std::vector<vk::BufferImageCopy2KHR> image_regions;
	std::vector<vk::CopyBufferInfo2KHR> buffer_uploads;
	std::vector<vk::BufferCopy2KHR> buffer_regions;

	
	auto makeGpuImage = [this](const tinygltf::Image& img)
		{
			return UniqueVmaImage(allocator_,
				vk::Format::eR8G8B8A8Srgb,
				vk::Extent2D(img.width, img.height),
				vk::ImageTiling::eLinear,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
				VMA_MEMORY_USAGE_GPU_ONLY);
		};

	{
		auto data = staging.map();

		uint32_t offset = 0;

		
		auto uploadImage =
			[&image_uploads, &image_regions, &staging, &offset, data]
			(vk::Image dst, const tinygltf::Image& image)
			{
				image_uploads.emplace_back(vk::CopyBufferToImageInfo2KHR{
					.srcBuffer = staging.get(),
					.dstImage = dst,
					.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
					.regionCount = 1,
					.pRegions = image_regions.data() + image_regions.size(),
				});

				image_regions.emplace_back(vk::BufferImageCopy2KHR{
					.bufferOffset = offset,
					.bufferRowLength = static_cast<uint32_t>(image.width * 4),
					.bufferImageHeight = static_cast<uint32_t>(image.height),
					.imageSubresource = vk::ImageSubresourceLayers{
						.aspectMask = vk::ImageAspectFlagBits::eColor,
						.mipLevel = 0,
						.baseArrayLayer = 0,
						.layerCount = 1
					},
					.imageOffset = {0, 0},
					.imageExtent = {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height)},
				});

				std::memcpy(data + offset, image.image.data(), image.image.size());
				offset += image.image.size();
			};

		for (auto& material : model.materials)
		{
			auto& base_color =
				model.images[model.textures[material.pbrMetallicRoughness.baseColorTexture.index].source];
			auto& omr =
				model.images[model.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index].source];


			auto& mat = result.materials.emplace_back(
				Material{
					.ubo =
						MaterialUBO{
							// TODO: base color is actually a vec3, dang
							//.baseColorFactor = material.pbrMetallicRoughness.baseColorFactor,
							.metallicFactor = static_cast<float>(material.pbrMetallicRoughness.metallicFactor),
							.roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor)
						},
					.base_color = makeGpuImage(base_color),
					.occlusion_metalic_roughness = makeGpuImage(omr),
				});
				
			uploadImage(mat.base_color.get(), base_color);
			uploadImage(mat.occlusion_metalic_roughness.get(), omr);
		}

		
		uint32_t vertex_offset = 0;
		uint32_t index_offset = 0;
		for (auto& mesh : model.meshes)
		{
			for (auto& prim : mesh.primitives)
			{
				auto& indices = model.accessors[prim.indices];
				auto& position = model.accessors[prim.attributes.at("POSITION")];
				auto& normal = model.accessors[prim.attributes.at("NORMAL")];
				auto& uv = model.accessors[prim.attributes.at("TEXCOORD_0")];

				uint32_t idx_count = static_cast<uint32_t>(indices.count);
				uint32_t vtx_count = static_cast<uint32_t>(position.count);

				// Spec guarantees this, but we check it just to be sure
				NG_ASSERT(indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
				NG_ASSERT(indices.type == TINYGLTF_TYPE_SCALAR);
				NG_ASSERT(position.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				NG_ASSERT(position.type == TINYGLTF_TYPE_VEC3);
				NG_ASSERT(normal.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				NG_ASSERT(normal.type == TINYGLTF_TYPE_VEC3);
				// this could be different, but we don't support it
				NG_ASSERT(uv.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				NG_ASSERT(position.count == normal.count);
				NG_ASSERT(position.count == uv.count);

				result.meshlets.emplace_back(Meshlet{
					.vertex_offset = vertex_offset,
					.index_offset = index_offset,
					.index_count = idx_count,
					.material = &result.materials[prim.material]
				});

				index_offset += idx_count;
				vertex_offset += vtx_count;
			}
		}
		
		result.index_buffer = UniqueVmaBuffer(allocator_, index_offset,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);

		result.vertex_buffer = UniqueVmaBuffer(allocator_, vertex_offset,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);

		auto uploadBuffer =
			[this, &model, &data, &offset, &staging, &buffer_regions]
			(uint32_t src_offset, uint32_t src_size, vk::Buffer target, uint32_t target_offset)
			{
				buffer_uploads_.emplace_back(vk::CopyBufferInfo2KHR{
					.srcBuffer = staging.get(),
					.dstBuffer = target,
					.regionCount = 1,
					.pRegions = buffer_regions.data() + buffer_regions.size(),
				});

				buffer_regions.push_back(vk::BufferCopy2KHR{
					.srcOffset = src_offset,
					.dstOffset = target_offset,
					.size = src_size,
				});
			};

		std::size_t meshlet_idx = 0;
		for (auto& mesh : model.meshes)
		{
			for (auto& prim : mesh.primitives)
			{
				{
					auto& index_acc = model.accessors[prim.indices];
					auto& index_view = model.bufferViews[index_acc.bufferView];
					auto& index_buf = model.buffers[index_view.buffer];

					uint32_t sz = tinygltf::GetComponentSizeInBytes(index_acc.componentType)
						* tinygltf::GetNumComponentsInType(index_acc.type);

					auto idx_start = offset;
					auto curr = reinterpret_cast<const std::byte*>(index_buf.data.data()) + index_acc.byteOffset;
					for (std::size_t i = 0; i < index_acc.count; ++i)
					{
						std::memcpy(data + offset, curr, sz);
						offset += sz;

						curr += index_acc.ByteStride(index_view);
					}

					uploadBuffer(idx_start, offset - idx_start, result.index_buffer.get(),
						result.meshlets[meshlet_idx].index_offset * sz);
				}

				{
					std::array accs{
						&model.accessors[prim.attributes.at("POSITION")],
						&model.accessors[prim.attributes.at("NORMAL")],
						&model.accessors[prim.attributes.at("TEXCOORD_0")]
					};
					std::array views{
						&model.bufferViews[accs[0]->bufferView],
						&model.bufferViews[accs[1]->bufferView],
						&model.bufferViews[accs[2]->bufferView],
					};
					std::array buffers{
						&model.buffers[views[0]->buffer],
						&model.buffers[views[1]->buffer],
						&model.buffers[views[2]->buffer],
					};

					std::array sizes{
						static_cast<uint32_t>(
							tinygltf::GetComponentSizeInBytes(accs[0]->componentType)
							* tinygltf::GetNumComponentsInType(accs[0]->type)),
						static_cast<uint32_t>(
							tinygltf::GetComponentSizeInBytes(accs[1]->componentType)
							* tinygltf::GetNumComponentsInType(accs[1]->type)),
						static_cast<uint32_t>(
							tinygltf::GetComponentSizeInBytes(accs[2]->componentType)
							* tinygltf::GetNumComponentsInType(accs[2]->type)),
					};

					auto idx_start = offset;
					std::array currs{
						reinterpret_cast<const std::byte*>(buffers[0]->data.data()) + accs[0]->byteOffset,
						reinterpret_cast<const std::byte*>(buffers[1]->data.data()) + accs[1]->byteOffset,
						reinterpret_cast<const std::byte*>(buffers[2]->data.data()) + accs[2]->byteOffset,
					};

					for (std::size_t i = 0; i < accs[0]->count; ++i)
					{
						for (std::size_t j = 0; j < 3; ++j)
						{
							std::memcpy(data + offset, currs[j], sizes[j]);
							offset += sizes[j];

							currs[j] += accs[j]->ByteStride(*views[j]);
						}
					}

					uploadBuffer(idx_start, offset - idx_start, result.vertex_buffer.get(),
						result.meshlets[meshlet_idx].vertex_offset
							* (sizes[0] + sizes[1] + sizes[2]));
				}

				++meshlet_idx;
			}
		}
		

		staging.unmap();
	}

	unifex::async_manual_reset_event done;

	co_await uploads_mtx_.async_lock();
	{
		// ranges STILL not available in libc++
		std::move(image_uploads.begin(), image_uploads.end(), std::back_inserter(image_uploads_));
		std::move(buffer_uploads.begin(), buffer_uploads.end(), std::back_inserter(buffer_uploads_));
		waiters_.emplace_back(&done);
	}
	uploads_mtx_.unlock();

	co_await unifex::on(g_engine.mainScheduler(), done.async_wait());

	co_return &result;
}

unifex::task<std::vector<unifex::async_manual_reset_event*>>
	GpuStorageManager::frameUpload(std::size_t frame_idx, vk::CommandBuffer cb)
{
	decltype(buffer_uploads_) buffer_uploads;
	decltype(image_uploads_) image_uploads;
	decltype(waiters_) waiters;
	{
		co_await uploads_mtx_.async_lock();
		Defer defer{[this](){ uploads_mtx_.unlock(); }};

		buffer_uploads = std::move(buffer_uploads_);
		image_uploads = std::move(image_uploads_);
		waiters = std::move(waiters_);

		if (buffer_uploads.empty() && image_uploads.empty())
		{
			for (auto waiter : waiters)
			{
				waiter->set();
			}
			co_return {};
		}
	}

	for (auto& buf : buffer_uploads)
	{
		cb.copyBuffer2KHR(buf);
	}

	std::vector<vk::ImageMemoryBarrier2KHR> barriers;

	for (auto& img : image_uploads)
	{
		cb.copyBufferToImage2KHR(img);

		// barrier only for layout transition, synchronization is done via submits
	    barriers.emplace_back(vk::ImageMemoryBarrier2KHR{
			.srcStageMask = vk::PipelineStageFlagBits2KHR::eTransfer,
	        .srcAccessMask = vk::AccessFlagBits2KHR::eTransferWrite,
			.dstStageMask = {},
	        .dstAccessMask = {},
	        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
	        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
	        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	        .image = img.dstImage,
	        .subresourceRange = vk::ImageSubresourceRange{
	            vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1,
	        }
	    });
	}
	
	vk::DependencyInfoKHR dep{
		.dependencyFlags = {},
		.memoryBarrierCount = 0,
		.pMemoryBarriers = nullptr,
		.bufferMemoryBarrierCount = 0,
		.pBufferMemoryBarriers = nullptr,
		.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
		.pImageMemoryBarriers = barriers.data(),
	};
    cb.pipelineBarrier2KHR(dep);
	
	co_return std::move(waiters);
}
