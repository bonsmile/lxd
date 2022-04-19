#include "glb.h"
#include <cassert>
#include "json.h"
#include <unordered_map>
#include "debug.h"
#include "fileio.h"

namespace lxd {
	template <class T>
	bool Glb<T>::load(std::string_view buffer) {
		assert(buffer.size() > sizeof(Header));

		auto magic = buffer.substr(0, 4);
		if(magic != "glTF")
			return false;
		auto pLength = reinterpret_cast<const uint32_t*>(buffer.data() + offsetof(Header, length));
		if(*pLength != buffer.size())
			return false;

		extractChunk(buffer.data() + sizeof(Header), buffer.size() - sizeof(Header));

		return true;
	}

	template <class T>
	bool Glb<T>::load(const std::vector<MyVec3>& points, const std::vector<T>& indices) {
		clear();
		// Binary Buffer
		Chunk chunk;
		chunk.type = 0x004E4942; // BIN
		{
			auto pIndices = reinterpret_cast<const char*>(indices.data());
			chunk.data.assign(pIndices, pIndices + sizeof(T) * indices.size());
			BufferView bufferView{.buffer = 0, .byteOffset = 0, .byteLength = static_cast<int>(chunk.data.size()), .target = 34963};
			m_bufferViews.push_back(bufferView);
		}
		{
			BufferView bufferView{.buffer = 0, .byteOffset = static_cast<int>(chunk.data.size()), .target = 34962};
			auto pPoints = reinterpret_cast<const char*>(points.data());
			chunk.data.insert(chunk.data.end(), pPoints, pPoints + sizeof(MyVec3) * points.size());
			// 每个 Chunk 末尾需要 4 字节对齐
			auto curSize = chunk.data.size();
			if(curSize % 4 != 0) {
				size_t extra = 4 - curSize % 4;
				for(size_t i = 0; i < extra; i++) {
					chunk.data.push_back(0x00);
				}
			}
			bufferView.byteLength = static_cast<int>(chunk.data.size() - bufferView.byteOffset);
			m_bufferViews.push_back(bufferView);
		}
		// JSON
		char* json = NULL;
		int length = 0;
		{
			// asset
			ksJson* rootNode = ksJson_SetObject(ksJson_Create());
			ksJson* asset = ksJson_SetObject(ksJson_AddObjectMember(rootNode, "asset"));
			ksJson_SetString(ksJson_AddObjectMember(asset, "version"), "2.0");
			// buffers
			{
				ksJson* buffers = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "buffers"));
				ksJson* idxBuffer = ksJson_SetObject(ksJson_AddArrayElement(buffers));
				ksJson_SetUint64(ksJson_AddObjectMember(idxBuffer, "byteLength"), chunk.data.size());
			}
			// buffer views
			ksJson* bufferViews = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "bufferViews"));
			for(auto& bufferView : m_bufferViews) {
				ksJson* pBufferView = ksJson_SetObject(ksJson_AddArrayElement(bufferViews));
				ksJson_SetUint32(ksJson_AddObjectMember(pBufferView, "buffer"), bufferView.buffer);
				ksJson_SetUint32(ksJson_AddObjectMember(pBufferView, "byteOffset"), bufferView.byteOffset);
				ksJson_SetUint64(ksJson_AddObjectMember(pBufferView, "byteLength"), bufferView.byteLength);
				ksJson_SetUint32(ksJson_AddObjectMember(pBufferView, "target"), bufferView.target);
			}
			// accessors
			{
				ksJson* accessors = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "accessors"));
				ksJson* idxAccessor = ksJson_SetObject(ksJson_AddArrayElement(accessors));
				ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "bufferView"), 0);
				ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "byteOffset"), 0);
				ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "componentType"), sizeof(T) == 2 ?  5123 : 5125); // unsigned short
				ksJson_SetUint64(ksJson_AddObjectMember(idxAccessor, "count"), indices.size());
				ksJson_SetString(ksJson_AddObjectMember(idxAccessor, "type"), "SCALAR");
				ksJson* posAccessor = ksJson_SetObject(ksJson_AddArrayElement(accessors));
				ksJson_SetUint32(ksJson_AddObjectMember(posAccessor, "bufferView"), 1);
				ksJson_SetUint32(ksJson_AddObjectMember(posAccessor, "byteOffset"), 0);
				ksJson_SetUint32(ksJson_AddObjectMember(posAccessor, "componentType"), 5126);// float
				ksJson_SetUint64(ksJson_AddObjectMember(posAccessor, "count"), points.size());
				ksJson_SetString(ksJson_AddObjectMember(posAccessor, "type"), "VEC3");
			}
			// meshes
			{
				ksJson* meshes = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "meshes"));
				ksJson* mesh0 = ksJson_SetObject(ksJson_AddArrayElement(meshes));
				ksJson* primitives = ksJson_SetArray(ksJson_AddObjectMember(mesh0, "primitives"));
				ksJson* primitive0 = ksJson_SetObject(ksJson_AddArrayElement(primitives));
				ksJson* attributes = ksJson_SetObject(ksJson_AddObjectMember(primitive0, "attributes"));
				ksJson_SetUint32(ksJson_AddObjectMember(primitive0, "indices"), 0); // accessor 0
				ksJson_SetUint32(ksJson_AddObjectMember(attributes, "POSITION"), 1); // accessor 1
			}
			// nodes
			{
				ksJson* nodes = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "nodes"));
				ksJson* node0 = ksJson_SetObject(ksJson_AddArrayElement(nodes));
				ksJson_SetUint32(ksJson_AddObjectMember(node0, "mesh"), 0);
			}
			// scene
			{
				ksJson_SetUint32(ksJson_AddObjectMember(rootNode, "scene"), 0);
				ksJson* scenes = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "scenes"));
				ksJson* scene0 = ksJson_SetObject(ksJson_AddArrayElement(scenes));
				ksJson* nodes = ksJson_SetArray(ksJson_AddObjectMember(scene0, "nodes"));
				ksJson_SetUint32(ksJson_AddArrayElement(nodes), 0);
			}
			ksJson_WriteToBuffer(rootNode, &json, &length);
			ksJson_Destroy(rootNode);
		}
		lxd::print("{}\n", json);
		// JSON
		Chunk jsonChunk;
		jsonChunk.type = 0x4E4F534A; // JSON
		{
			jsonChunk.data.assign(json, json + length);
			// 每个 Chunk 末尾需要 4 字节对齐
			auto curSize = jsonChunk.data.size();
			if(curSize % 4 != 0) {
				auto extra = 4 - curSize % 4;
				for(size_t n = 0; n < extra; n++) {
					jsonChunk.data.push_back(' '); // JSON 利用空格字符对齐
				}
			}
		}
		free(json);
		m_chunks.emplace_back(std::move(jsonChunk));
		m_chunks.emplace_back(std::move(chunk));
		return true;
	}

	template <class T>
	bool Glb<T>::save(const std::wstring& path) {
		lxd::File file(path, lxd::WriteOnly | lxd::Truncate);
		m_header.length = static_cast<uint32_t>(sizeof(Header));
		for(const auto& chunk : m_chunks) {
			m_header.length += static_cast<uint32_t>(2 * sizeof(uint32_t) + chunk.data.size());
		}
		bool result = true;
		result &= file.write(&m_header, sizeof(Header));
		for(const auto& chunk : m_chunks) {
			uint32_t length = static_cast<uint32_t>(chunk.data.size());
			uint32_t type = chunk.type;
			result &= file.write(&length, sizeof(length));
			result &= file.write(&type, sizeof(type));
			result &= file.write(chunk.data.data(), chunk.data.size());
		}
		assert(file.size() == m_header.length);
		return result;
	}

	template <class T>
	void Glb<T>::clear() {
		m_chunks.clear();
		m_header.length = 0;
	}

	template <class T>
	void Glb<T>::extractChunk(const char* data, size_t size) {
		Chunk chunk;
		uint32_t length = *reinterpret_cast<const uint32_t*>(data);
		chunk.type = *reinterpret_cast<const uint32_t*>(data + 4);
		chunk.data.assign(const_cast<char*>(data) + 4 + 4, const_cast<char*>(data) + 4 + 4 + length);
		m_chunks.emplace_back(std::move(chunk));
		if(4 + 4 + length < size)
			extractChunk(data + 4 + 4 + length, size - 4 - 4 - length);
	}

	// 顶点索引暂时只支持 uint16/uint32
	template class Glb<uint16_t>;
	template class Glb<uint32_t>;
}
