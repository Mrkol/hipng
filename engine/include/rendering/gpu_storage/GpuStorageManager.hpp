#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vulkan/vulkan.hpp>
#include <tiny_gltf.h>
#include <unifex/task.hpp>
#include <unifex/async_mutex.hpp>
#include <unifex/async_manual_reset_event.hpp>

#include "assets/AssetHandle.hpp"
#include "concurrency/Spinlock.hpp"
#include "rendering/gpu_storage/StaticMesh.hpp"


struct ImGuiContext;

class GpuStorageManager
{
public:
	struct CreateInfo
	{
		vk::Device device;
		VmaAllocator allocator;
	};

	explicit GpuStorageManager(CreateInfo info);

	unifex::task<void> uploadStaticMesh(AssetHandle handle, const tinygltf::Model& model);
	StaticMesh* getStaticMesh(AssetHandle handle);

	unifex::task<void> uploadGuiData(ImGuiContext* context);


	struct UploadResult
	{
		std::vector<unifex::async_manual_reset_event*> waiters;
		std::vector<std::pair<AssetHandle, StaticMesh>> static_meshes;
		std::vector<ImGuiContext*> gui_contexts;
	};

	unifex::task<UploadResult> frameUpload(vk::CommandBuffer cb);

	void frameUploadDone(UploadResult result);

private:
	std::vector<glm::mat4x4> calculate_node_total_transforms(const tinygltf::Model& model);

private:
	vk::Device device_;
	VmaAllocator allocator_;
	
	std::vector<vk::CopyBufferToImageInfo2KHR> image_uploads_; // guarded by uploads_mtx
	std::vector<vk::CopyBufferInfo2KHR> buffer_uploads_; // guarded by uploads_mtx
	std::vector<std::pair<AssetHandle, StaticMesh>> static_mesh_uploads_; // guarded by uploads_mtx
	std::vector<unifex::async_manual_reset_event*> waiters_; // guarded by uploads_mtx
	std::vector<ImGuiContext*> gui_context_uploads_; // guarded by uploads_mtx
	unifex::async_mutex uploads_mtx_;

	std::unordered_set<AssetHandle> uploaded_assets_; // guarded by uploaded_mtx_
	unifex::async_mutex uploaded_mtx_;

	std::unordered_map<AssetHandle, StaticMesh> static_meshes_;
};
