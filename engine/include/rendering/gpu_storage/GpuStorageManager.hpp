#pragma once

#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include <tiny_gltf.h>
#include <unifex/task.hpp>
#include <unifex/async_mutex.hpp>
#include <unifex/async_manual_reset_event.hpp>

#include "assets/AssetHandle.hpp"
#include "rendering/gpu_storage/StaticMesh.hpp"
#include "rendering/primitives/InflightResource.hpp"
#include "rendering/primitives/UniqueVmaBuffer.hpp"


class GpuStorageManager
{
public:
	struct CreateInfo
	{
		vk::Device device;
		VmaAllocator allocator;
	};

	explicit GpuStorageManager(CreateInfo info);

	unifex::task<StaticMesh*> uploadStaticMesh(AssetHandle handle, const tinygltf::Model& model);
	

	unifex::task<std::vector<unifex::async_manual_reset_event*>>
		frameUpload(std::size_t frame_idx, vk::CommandBuffer cb);
	

private:
	vk::Device device_;
	VmaAllocator allocator_;
	
	std::vector<vk::CopyBufferToImageInfo2KHR> image_uploads_; // guarded by uploads_mtx
	std::vector<vk::CopyBufferInfo2KHR> buffer_uploads_; // guarded by uploads_mtx
	std::vector<unifex::async_manual_reset_event*> waiters_; // guarded by uploads_mtx
	unifex::async_mutex uploads_mtx_;

	std::unordered_map<AssetHandle, StaticMesh> static_meshes_; // guarded by maps_mtx_
	unifex::async_mutex maps_mtx_;
};
