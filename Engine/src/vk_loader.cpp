#include "vk_loader.h"
#include "vk_engine.h"

#include <iostream>

#include <fastgltf/types.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/util.hpp>

#include <stb_image.h>
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/glm_element_traits.hpp>

namespace Engine
{
	namespace
	{
		VkFilter extractFilter(fastgltf::Filter filter)
		{
			switch (filter) {
				// nearest samplers
			case fastgltf::Filter::Nearest:
			case fastgltf::Filter::NearestMipMapNearest:
			case fastgltf::Filter::NearestMipMapLinear:
				return VK_FILTER_NEAREST;

				// linear samplers
			case fastgltf::Filter::Linear:
			case fastgltf::Filter::LinearMipMapNearest:
			case fastgltf::Filter::LinearMipMapLinear:
			default:
				return VK_FILTER_LINEAR;
			}
		}

		VkSamplerMipmapMode extractMipmapMode(fastgltf::Filter filter)
		{
			switch (filter) {
			case fastgltf::Filter::NearestMipMapNearest:
			case fastgltf::Filter::LinearMipMapNearest:
				return VK_SAMPLER_MIPMAP_MODE_NEAREST;

			case fastgltf::Filter::NearestMipMapLinear:
			case fastgltf::Filter::LinearMipMapLinear:
			default:
				return VK_SAMPLER_MIPMAP_MODE_LINEAR;
			}
		}
	}

	std::optional<AllocatedImage> loadImage(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
	{
		AllocatedImage newImage{};

		int width, height, nrChannels;

		std::visit(
			fastgltf::visitor{
				[](auto& arg) {},
				[&](fastgltf::sources::URI& filePath)
			{
					assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
					assert(filePath.uri.isLocalPath()); // We're only capable of loading
					// local files.

					const std::string path(filePath.uri.path().begin(),
					filePath.uri.path().end()); // Thanks C++.
					unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
					if (data)
					{
						VkExtent3D imageSize;
						imageSize.width = width;
						imageSize.height = height;
						imageSize.depth = 1;

						newImage = engine->createImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

						stbi_image_free(data);
					}
			},
				[&](fastgltf::sources::Vector& vector)
		{
					unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
					&width, &height, &nrChannels, 4);
					if (data)
					{
						VkExtent3D imageSize;
						imageSize.width = width;
						imageSize.height = height;
						imageSize.depth = 1;

						newImage = engine->createImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

						stbi_image_free(data);
					}
	},
				[&](fastgltf::sources::BufferView& view)
	{
					auto& bufferView = asset.bufferViews[view.bufferViewIndex];
					auto& buffer = asset.buffers[bufferView.bufferIndex];

		std::visit(fastgltf::visitor {
			// We only care about VectorWithMime here, because we
			// specify LoadExternalBuffers, meaning all buffers
			// are already loaded into a vector.
			[](auto& arg) {},
			[&](fastgltf::sources::Vector& vector) {
				unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
				static_cast<int>(bufferView.byteLength),
				&width, &height, &nrChannels, 4);
			if (data)
			{
				VkExtent3D imageSize;
				imageSize.width = width;
				imageSize.height = height;
				imageSize.depth = 1;

				newImage = engine->createImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM,
					VK_IMAGE_USAGE_SAMPLED_BIT,false);

				stbi_image_free(data);
			}
	} },
	buffer.data);
	},
			},
			image.data);

		// if any of the attempts to load the data failed, we havent written the image
		// so handle is null
		if (newImage.image == VK_NULL_HANDLE) {
			return {};
		}
		else {
			return newImage;
		}
	}

	std::optional<Shared<LoadedGLTF>> loadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath)
	{
		//fmt::print("Loading GLTF: {}", filePath);

		Shared<LoadedGLTF> scene = CreateShared<LoadedGLTF>();
		scene->creator = engine;
		LoadedGLTF& file = *scene.get();

		fastgltf::Parser parser{};

		constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
		fastgltf::GltfDataBuffer data;
		data.loadFromFile(filePath);

		fastgltf::Asset gltf;

		std::filesystem::path path = filePath;

		auto type = fastgltf::determineGltfFileType(&data);
		if (type == fastgltf::GltfType::glTF)
		{
			auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
			if (load) {
				gltf = std::move(load.get());
			}
			else {
				std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
				return {};
			}
		}
		else if (type == fastgltf::GltfType::GLB)
		{
			auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
			if (load) {
				gltf = std::move(load.get());
			}
			else {
				std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
				return {};
			}
		}
		else {
			std::cerr << "Failed to determine glTF container" << std::endl;
			return {};
		}

		std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
			 { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			 { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
		};

		file.descriptorPool.init(engine->m_Device, gltf.materials.size(), sizes);

		for (fastgltf::Sampler& sampler : gltf.samplers) {
			VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
			sampl.maxLod = VK_LOD_CLAMP_NONE;
			sampl.minLod = 0;

			sampl.magFilter = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
			sampl.minFilter = extractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
			sampl.mipmapMode = extractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

			VkSampler newSampler;
			vkCreateSampler(engine->m_Device, &sampl, nullptr, &newSampler);

			file.samplers.push_back(newSampler);
		}

		std::vector<Shared<MeshAsset>> meshes;
		std::vector<Shared<SceneNode>> nodes;
		std::vector<AllocatedImage> images;
		std::vector<Shared<GLTFMaterial>> materials;

		for (fastgltf::Image& image : gltf.images) {
			std::optional<AllocatedImage> img = loadImage(engine, gltf, image);

			if (img.has_value()) {
				images.push_back(*img);
				file.images[image.name.c_str()] = *img;
			}
			else {
				// we failed to load, so lets give the slot a default white texture to not
				// completely break loading
				images.push_back(engine->m_ErrorCheckerboardImage);
				std::cout << "GLTF failed to load texture " << image.name << std::endl;
			}
		}

		file.materialDataBuffer = engine->createBuffer(sizeof(MetallicRoughnessMaterial::MaterialConstants) * gltf.materials.size(),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		int data_index = 0;
		VmaAllocationInfo info;
		vmaGetAllocationInfo(engine->m_Allocator, file.materialDataBuffer.allocation, &info);
		MetallicRoughnessMaterial::MaterialConstants* sceneMaterialConstants = (MetallicRoughnessMaterial::MaterialConstants*)info.pMappedData;

		for (fastgltf::Material& mat : gltf.materials) {
			Shared<GLTFMaterial> newMat = CreateShared<GLTFMaterial>();
			materials.push_back(newMat);
			file.materials[mat.name.c_str()] = newMat;

			MetallicRoughnessMaterial::MaterialConstants constants;
			constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
			constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
			constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
			constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

			constants.metalRoughFactors.x = mat.pbrData.metallicFactor;
			constants.metalRoughFactors.y = mat.pbrData.roughnessFactor;
			// write material parameters to buffer
			sceneMaterialConstants[data_index] = constants;

			MaterialPass passType = MaterialPass::MainColor;
			if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
				passType = MaterialPass::Transparent;
			}

			MetallicRoughnessMaterial::MaterialResources materialResources;
			// default the material textures
			materialResources.colorImage = engine->m_WhiteImage;
			materialResources.colorSampler = engine->m_SamplerLinear;
			materialResources.metalRoughImage = engine->m_WhiteImage;
			materialResources.metalRoughSampler = engine->m_SamplerLinear;

			// set the uniform buffer for the material data
			materialResources.dataBuffer = file.materialDataBuffer.buffer;
			materialResources.dataBufferOffset = data_index * sizeof(MetallicRoughnessMaterial::MaterialConstants);
			// grab textures from gltf file
			if (mat.pbrData.baseColorTexture.has_value()) {
				size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
				size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

				materialResources.colorImage = images[img];
				materialResources.colorSampler = file.samplers[sampler];
			}
			// build material
			newMat->data = engine->m_MetallicRoughnessMaterial.createMaterial(engine->m_Device, passType, materialResources, file.descriptorPool);
			data_index++;
		}
		std::vector<uint32_t> indices;
		std::vector<Vertex> vertices;

		for (fastgltf::Mesh& mesh : gltf.meshes) {
			Shared<MeshAsset> newmesh = CreateShared<MeshAsset>();
			meshes.push_back(newmesh);
			file.meshes[mesh.name.c_str()] = newmesh;
			newmesh->name = mesh.name;

			// clear the mesh arrays each mesh, we dont want to merge them by error
			indices.clear();
			vertices.clear();

			for (auto&& p : mesh.primitives) {
				GeoSurface newSurface;
				newSurface.startIndex = (uint32_t)indices.size();
				newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

				size_t initial_vtx = vertices.size();

				// load indexes
				{
					fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
					indices.reserve(indices.size() + indexaccessor.count);

					fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
						[&](std::uint32_t idx) {
							indices.push_back(idx + initial_vtx);
						});
				}

				// load vertex positions
				{
					fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
					vertices.resize(vertices.size() + posAccessor.count);

					fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
						[&](glm::vec3 v, size_t index) {
							Vertex newvtx;
							newvtx.position = v;
							newvtx.normal = { 1, 0, 0 };
							newvtx.color = glm::vec4{ 1.f };
							newvtx.uvX = 0;
							newvtx.uvY = 0;
							vertices[initial_vtx + index] = newvtx;
						});
				}

				// load vertex normals
				auto normals = p.findAttribute("NORMAL");
				if (normals != p.attributes.end()) {
					fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
						[&](glm::vec3 v, size_t index) {
							vertices[initial_vtx + index].normal = v;
						});
				}

				// load UVs
				auto uv = p.findAttribute("TEXCOORD_0");
				if (uv != p.attributes.end()) {
					fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
						[&](glm::vec2 v, size_t index) {
							vertices[initial_vtx + index].uvX = v.x;
							vertices[initial_vtx + index].uvY = v.y;
						});
				}

				// load vertex colors
				auto colors = p.findAttribute("COLOR_0");
				if (colors != p.attributes.end()) {
					fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
						[&](glm::vec4 v, size_t index) {
							vertices[initial_vtx + index].color = v;
						});
				}

				if (p.materialIndex.has_value()) {
					newSurface.material = materials[p.materialIndex.value()];
				}
				else {
					newSurface.material = materials[0];
				}

				newmesh->surfaces.push_back(newSurface);
			}

			newmesh->meshBuffers = engine->uploadMesh(indices, vertices);
		}
		// load all nodes and their meshes
		for (fastgltf::Node& node : gltf.nodes) {
			Shared<SceneNode> newNode;

			// find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
			if (node.meshIndex.has_value()) {
				newNode = CreateShared<MeshNode>();
				static_cast<MeshNode*>(newNode.get())->meshAsset = meshes[*node.meshIndex];
			}
			else {
				newNode = CreateShared<SceneNode>();
			}

			nodes.push_back(newNode);
			file.nodes[node.name.c_str()];

			std::visit(
				fastgltf::visitor{
				[&](fastgltf::Node::TransformMatrix matrix)
				{
				  memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
				},

				[&](fastgltf::Node::TRS transform)
				{
							   glm::vec3 tl(transform.translation[0], transform.translation[1],
								   transform.translation[2]);
							   glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
								   transform.rotation[2]);
							   glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

							   glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
							   glm::mat4 rm = glm::toMat4(rot);
							   glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

							   newNode->localTransform = tm * rm * sm;
				} },
				node.transform);
		}



		for (int i = 0; i < gltf.nodes.size(); i++) {
			fastgltf::Node& node = gltf.nodes[i];
			std::shared_ptr<SceneNode>& sceneNode = nodes[i];

			if (node.children.size() > 0)
				for (auto& c : node.children) {
					sceneNode->children.push_back(nodes[c]);
					nodes[c]->parent = sceneNode;
				}
		}


		// find the top nodes, with no parents
		for (auto& node : nodes) {
			if (node->parent.lock() == nullptr) {
				file.topNodes.push_back(node);
				node->refreshTransform(glm::mat4{ 1.f });
			}
		}


		return scene;
	}
	void LoadedGLTF::draw(const glm::mat4& topMatrix, DrawingContext& ctx)
	{
		for (auto& n : topNodes) {
			n->draw(topMatrix, ctx);
		}
	}
	void LoadedGLTF::clearAll()
	{
		VkDevice dv = creator->m_Device;

		descriptorPool.destroyPools(dv);
		creator->destroyBuffer(materialDataBuffer);

		for (auto& [k, v] : meshes) {

			creator->destroyBuffer(v->meshBuffers.indexBuffer);
			creator->destroyBuffer(v->meshBuffers.vertexBuffer);
		}

		for (auto& [k, v] : images) {

			if (v.image == creator->m_ErrorCheckerboardImage.image) {
				//dont destroy the default images
				continue;
			}
			creator->destroyImage(v);
		}

		for (auto& sampler : samplers) {
			vkDestroySampler(dv, sampler, nullptr);
		}
	}
}