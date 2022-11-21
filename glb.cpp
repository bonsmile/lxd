#include "glb.h"
#include <cassert>
#include "json.h"
#include <unordered_map>
#include "debug.h"
#include "fileio.h"

namespace std {
	template <>
	struct hash<lxd::MyVec3> {
		size_t operator()(const lxd::MyVec3& vec) const {
			size_t seed = 0;
			for(int i = 0; i < 3; ++i) {
				seed ^=
					std::hash<float>()(vec.v[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			}
			return seed;
		}
	};
}

namespace lxd {
	bool Glb::load(std::string_view buffer) {
		assert(buffer.size() > sizeof(Header));

		auto magic = buffer.substr(0, 4);
		if(magic != "glTF")
			return false;
		auto pLength = reinterpret_cast<const uint32_t*>(buffer.data() + offsetof(Header, length));
		if(*pLength != buffer.size())
			return false;

		extractChunk(buffer.data() + sizeof(Header), buffer.size() - sizeof(Header));
		extractJson();

		return true;
	}

	bool Glb::loadFromStl(std::string_view buffer) {
		clear();

		if(buffer.size() < 80)
			return false;
#pragma pack(1)
		struct StlFacet {
			float faceNormal[3];
			float v1[3];
			float v2[3];
			float v3[3];
			unsigned short attribute;
		};
#pragma pack()
		// *.stl format:
		//UINT8[80]    – Header - 80 bytes
		//UINT32       – Number of triangles - 4 bytes
		auto nFacet = *reinterpret_cast<const uint32_t*>(buffer.data() + 80);
		std::unordered_map<lxd::MyVec3, int> mapVtxId;
		std::unordered_map<lxd::MyVec3, int>::iterator iter;
		mapVtxId.reserve(nFacet);
		std::vector<lxd::MyVec3> points; // 顶点
		std::variant<std::vector<uint16_t>, std::vector<uint32_t>> indicesVar;// 索引
		if(3 * nFacet < USHRT_MAX) {
			indicesVar = std::vector<uint16_t>();
		} else {
			indicesVar = std::vector<uint32_t>();
		}
		
		uint32_t vId = 0;
		for(uint32_t i = 0; i < nFacet; i++) {
			auto pFacet = reinterpret_cast<const StlFacet*>(buffer.data() + 84 + i * sizeof(StlFacet));
			auto pV1 = pFacet->v1;
			for(int j = 0; j < 3; j++) {
				const lxd::MyVec3* pVec3 = reinterpret_cast<const lxd::MyVec3*>(pV1 + j * 3);
				iter = mapVtxId.find(*pVec3);
				if(iter == mapVtxId.end()) {
					points.push_back(*pVec3);
					mapVtxId.insert(std::make_pair(*pVec3, vId));
					if(auto p = std::get_if<std::vector<uint16_t>>(&indicesVar))
						p->push_back((uint16_t)vId);
					else if(auto p1 = std::get_if<std::vector<uint32_t>>(&indicesVar))
						p1->push_back(vId);
					vId++;
				} else {
					if(auto p = std::get_if<std::vector<uint16_t>>(&indicesVar))
						p->push_back((uint16_t)iter->second);
					else if(auto p1 = std::get_if<std::vector<uint32_t>>(&indicesVar))
						p1->push_back(iter->second);
				}
			}
		}
		// Binary Buffer
		Chunk chunk;
		chunk.type = 0x004E4942; // BIN
		{
			std::visit([&](auto& indices) {
				auto pIndices = reinterpret_cast<const char*>(indices.data());
				chunk.data.assign(pIndices, pIndices + sizeof(indices[0]) * indices.size());
			}, indicesVar);
			BufferView bufferView{.buffer = 0, .byteOffset = 0, .byteLength = static_cast<int>(chunk.data.size()), .target = 34963};
			m_bufferViews.push_back(bufferView);
			// 每个 Chunk 长度需要是 4 的倍数, 此 chunk = indices + positions, postions 永远是 4 的倍数，因此只需要处理 ushort index 这种情况
			auto curSize = chunk.data.size();
			if(curSize % 4 != 0) {
				size_t extra = 4 - curSize % 4;
				for(size_t i = 0; i < extra; i++) {
					chunk.data.push_back(0x00);
				}
			}
		}
		{
			BufferView bufferView{.buffer = 0, .byteOffset = static_cast<int>(chunk.data.size()), .target = 34962};
			auto pPoints = reinterpret_cast<const char*>(points.data());
			chunk.data.insert(chunk.data.end(), pPoints, pPoints + sizeof(MyVec3) * points.size());
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
				ksJson_SetUint32(ksJson_AddObjectMember(pBufferView, "byteOffset"), bufferView.byteOffset); // buffer 内的偏移
				ksJson_SetUint64(ksJson_AddObjectMember(pBufferView, "byteLength"), bufferView.byteLength);
				ksJson_SetUint32(ksJson_AddObjectMember(pBufferView, "target"), bufferView.target);
			}
			// accessors
			{
				size_t idxSize, idxCnt;
				std::visit([&](auto& indices) {
					idxSize = sizeof(indices[0]);
					idxCnt = indices.size();
				}, indicesVar);
				ksJson* accessors = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "accessors"));
				ksJson* idxAccessor = ksJson_SetObject(ksJson_AddArrayElement(accessors));
				ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "bufferView"), 0);
				ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "byteOffset"), 0); // bufferView 内的偏移
				ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "componentType"), idxSize == 2 ? 5123 : 5125); // unsigned short
				ksJson_SetUint64(ksJson_AddObjectMember(idxAccessor, "count"), idxCnt);
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

	bool Glb::createFromPolygonSoup(const std::vector<MyVec3>& triangles) {
		if(triangles.empty() || triangles.size() % 3 != 0)
			return false;

		size_t nFacet = triangles.size() / 3;
		std::unordered_map<lxd::MyVec3, int> mapVtxId;
		std::unordered_map<lxd::MyVec3, int>::iterator iter;
		mapVtxId.reserve(nFacet);
		std::vector<lxd::MyVec3> points; // 顶点
		std::variant<std::vector<uint16_t>, std::vector<uint32_t>> indicesVar;// 索引
		if(3 * nFacet < USHRT_MAX) {
			indicesVar = std::vector<uint16_t>();
		} else {
			indicesVar = std::vector<uint32_t>();
		}

		uint32_t vId = 0;
		for(size_t i = 0; i < nFacet; i++) {
			for(int j = 0; j < 3; j++) {
				const lxd::MyVec3& v = triangles[i * 3 + j];
				iter = mapVtxId.find(v);
				if(iter == mapVtxId.end()) {
					points.push_back(v);
					mapVtxId.insert(std::make_pair(v, vId));
					if(auto p = std::get_if<std::vector<uint16_t>>(&indicesVar))
						p->push_back((uint16_t)vId);
					else if(auto p1 = std::get_if<std::vector<uint32_t>>(&indicesVar))
						p1->push_back(vId);
					vId++;
				} else {
					if(auto p = std::get_if<std::vector<uint16_t>>(&indicesVar))
						p->push_back((uint16_t)iter->second);
					else if(auto p1 = std::get_if<std::vector<uint32_t>>(&indicesVar))
						p1->push_back(iter->second);
				}
			}
		}
		// Binary Buffer
		Chunk chunk;
		chunk.type = 0x004E4942; // BIN
		{
			std::visit([&](auto& indices) {
				auto pIndices = reinterpret_cast<const char*>(indices.data());
				chunk.data.assign(pIndices, pIndices + sizeof(indices[0]) * indices.size());
				}, indicesVar);
			BufferView bufferView{.buffer = 0, .byteOffset = 0, .byteLength = static_cast<int>(chunk.data.size()), .target = 34963};
			m_bufferViews.push_back(bufferView);
			// 每个 Chunk 长度需要是 4 的倍数, 此 chunk = indices + positions, postions 永远是 4 的倍数，因此只需要处理 ushort index 这种情况
			auto curSize = chunk.data.size();
			if(curSize % 4 != 0) {
				size_t extra = 4 - curSize % 4;
				for(size_t i = 0; i < extra; i++) {
					chunk.data.push_back(0x00);
				}
			}
		}
		{
			BufferView bufferView{.buffer = 0, .byteOffset = static_cast<int>(chunk.data.size()), .target = 34962};
			auto pPoints = reinterpret_cast<const char*>(points.data());
			chunk.data.insert(chunk.data.end(), pPoints, pPoints + sizeof(MyVec3) * points.size());
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
				ksJson_SetUint32(ksJson_AddObjectMember(pBufferView, "byteOffset"), bufferView.byteOffset); // buffer 内的偏移
				ksJson_SetUint64(ksJson_AddObjectMember(pBufferView, "byteLength"), bufferView.byteLength);
				ksJson_SetUint32(ksJson_AddObjectMember(pBufferView, "target"), bufferView.target);
			}
			// accessors
			{
				size_t idxSize, idxCnt;
				std::visit([&](auto& indices) {
					idxSize = sizeof(indices[0]);
					idxCnt = indices.size();
					}, indicesVar);
				ksJson* accessors = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "accessors"));
				ksJson* idxAccessor = ksJson_SetObject(ksJson_AddArrayElement(accessors));
				ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "bufferView"), 0);
				ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "byteOffset"), 0); // bufferView 内的偏移
				ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "componentType"), idxSize == 2 ? 5123 : 5125); // unsigned short
				ksJson_SetUint64(ksJson_AddObjectMember(idxAccessor, "count"), idxCnt);
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

	bool Glb::save(const String& path) {
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

	std::vector<uint8_t> Glb::searialize() {
		m_header.length = static_cast<uint32_t>(sizeof(Header));
		for(const auto& chunk : m_chunks) {
			m_header.length += static_cast<uint32_t>(2 * sizeof(uint32_t) + chunk.data.size());
		}
		std::vector<uint8_t> result;
		result.reserve(m_header.length);
		auto pHeader = reinterpret_cast<uint8_t*>(&m_header);
		result.insert(result.end(), pHeader, pHeader+sizeof(Header));
		for(const auto& chunk : m_chunks) {
			uint32_t length = static_cast<uint32_t>(chunk.data.size());
			uint32_t type = chunk.type;
			auto pLength = reinterpret_cast<uint8_t*>(&length);
			result.insert(result.end(), pLength, pLength + sizeof(uint32_t));
			auto pType = reinterpret_cast<uint8_t*>(&type);
			result.insert(result.end(), pType, pType + sizeof(uint32_t));
			auto pData = reinterpret_cast<const uint8_t*>(chunk.data.data());
			result.insert(result.end(), pData, pData + chunk.data.size());
		}
		assert(result.size() == m_header.length);
		return result;
	}

	std::span<MyVec3> Glb::getPositions() {
		assert(m_accessors.size() >= 2 && m_bufferViews.size() >= 2);
		assert(m_accessors[1].componentType == 5126 && std::strcmp(m_accessors[1].type, "VEC3") == 0);
		std::span<MyVec3> result{reinterpret_cast<MyVec3*>(m_chunks[1].data.data() + m_accessors[1].byteOffset + m_bufferViews[m_accessors[1].bufferView].byteOffset), static_cast<size_t>(m_accessors[1].count)};
		return result;
	}

	std::variant<std::span<uint16_t>, std::span<uint32_t>> Glb::getIndices() {
		assert(m_accessors.size() >= 2 && m_bufferViews.size() >= 2);
		std::variant<std::span<uint16_t>, std::span<uint32_t>> result;
		assert(std::strcmp(m_accessors[0].type,"SCALAR") == 0);
		if(m_accessors[0].componentType == 5123) {
			result = std::span<uint16_t>{reinterpret_cast<uint16_t*>(m_chunks[1].data.data()), static_cast<size_t>(m_accessors[0].count)};
		} else if(m_accessors[0].componentType == 5125) {
			result = std::span<uint32_t>{reinterpret_cast<uint32_t*>(m_chunks[1].data.data()), static_cast<size_t>(m_accessors[0].count)};
		}
		return result;
	}

	void Glb::clear() {
		m_accessors.clear();
		m_bufferViews.clear();
		m_chunks.clear();
		m_header.length = 0;
	}

	void Glb::extractChunk(const char* data, size_t size) {
		Chunk chunk;
		uint32_t length = *reinterpret_cast<const uint32_t*>(data);
		chunk.type = *reinterpret_cast<const uint32_t*>(data + 4);
		chunk.data.assign(const_cast<char*>(data) + 4 + 4, const_cast<char*>(data) + 4 + 4 + length);
		m_chunks.emplace_back(std::move(chunk));
		if(4 + 4 + length < size)
			extractChunk(data + 4 + 4 + length, size - 4 - 4 - length);
	}

	void Glb::extractJson() {
		assert(!m_chunks.empty() && m_chunks[0].type == 0x4E4F534A);
		const char* buffer = m_chunks[0].data.data();
		ksJson* rootNode = ksJson_Create();
		if(ksJson_ReadFromBuffer(rootNode, buffer, NULL)) {
			const ksJson* accessors = ksJson_GetMemberByName(rootNode, "accessors");
			const ksJson* bufferViews = ksJson_GetMemberByName(rootNode, "bufferViews");
			for(int i = 0; i < ksJson_GetMemberCount(accessors); i++) {
				const ksJson* accessorNode = ksJson_GetMemberByIndex(accessors, i);
				Accessor accessor;
				accessor.bufferView = ksJson_GetInt32(ksJson_GetMemberByName(accessorNode, "bufferView"), 0);
				accessor.byteOffset = ksJson_GetInt32(ksJson_GetMemberByName(accessorNode, "byteOffset"), 0);
				accessor.componentType = ksJson_GetInt32(ksJson_GetMemberByName(accessorNode, "componentType"), 0);
				accessor.count = ksJson_GetInt32(ksJson_GetMemberByName(accessorNode, "count"), 0);
				strcpy(accessor.type, ksJson_GetString(ksJson_GetMemberByName(accessorNode, "type"), ""));
				m_accessors.push_back(accessor);
			}
			for(int i = 0; i < ksJson_GetMemberCount(bufferViews); i++) {
				const ksJson* bufferviewNode = ksJson_GetMemberByIndex(bufferViews, i);
				BufferView bufferview;
				bufferview.buffer = ksJson_GetInt32(ksJson_GetMemberByName(bufferviewNode, "buffer"), 0);
				bufferview.byteOffset = ksJson_GetInt32(ksJson_GetMemberByName(bufferviewNode, "byteOffset"), 0);
				bufferview.byteLength = ksJson_GetInt32(ksJson_GetMemberByName(bufferviewNode, "byteLength"), 0);
				bufferview.target = ksJson_GetInt32(ksJson_GetMemberByName(bufferviewNode, "target"), 0);
				m_bufferViews.push_back(bufferview);
			}
		}
		ksJson_Destroy(rootNode);
	}


}
