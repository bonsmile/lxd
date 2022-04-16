#include "glb.h"
#include <cassert>
#include "json.h"
#include <unordered_map>
#include "debug.h"

struct Vec3 {
	float v[3];
	friend bool operator==(const Vec3& a, const Vec3& b) {
		return a.v[0] == b.v[0] && a.v[1] == b.v[1] && a.v[2] == b.v[2];
	}
};

namespace std {
	template <>
	struct hash<Vec3> {
		size_t operator()(const Vec3& vec) const {
			size_t seed = 0;
			for(int i = 0; i < 3; ++i) {
				seed ^=
					std::hash<float>()(vec.v[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			}
			return seed;
		}
	};
}

bool lxd::Glb::load(std::string_view buffer) {
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

bool lxd::Glb::loadFromStl(std::string_view buffer) {
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
	std::unordered_map<Vec3, int> mapVtxId;
	std::unordered_map<Vec3, int>::iterator iter;
	mapVtxId.reserve(nFacet);
	std::vector<Vec3> points; // 顶点
	std::vector<uint16_t> indices; // 索引
	int vId = 0;
	for(uint32_t i = 0; i < nFacet; i++) {
		auto pFacet = reinterpret_cast<const StlFacet*>(buffer.data() + 84 + i * sizeof(StlFacet));
		auto pV1 = pFacet->v1;
		for(int j = 0; j < 3; j++) {
			const Vec3* pVec3 = reinterpret_cast<const Vec3*>(pV1 + j * 3);
			iter = mapVtxId.find(*pVec3);
			if(iter == mapVtxId.end()) {
				points.push_back(*pVec3);
				mapVtxId.insert(std::make_pair(*pVec3, vId));
				indices.push_back(static_cast<uint16_t>(vId));
				vId++;
			} else {
				indices.push_back(static_cast<uint16_t>(iter->second));
			}
		}
	}
	// JSON
	char* json = NULL;
	int length = 0;
	{
		// asset
		ksJson* rootNode = ksJson_SetObject(ksJson_Create());
		ksJson* asset = ksJson_SetObject(ksJson_AddObjectMember(rootNode, "asset"));
		ksJson_SetFloat(ksJson_AddObjectMember(asset, "version"), 2.0);
		// buffers
		{
			ksJson* buffers = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "buffers"));
			ksJson* posBuffer = ksJson_SetObject(ksJson_AddArrayElement(buffers));
			ksJson_SetUint32(ksJson_AddObjectMember(posBuffer, "byteLength"), points.size() * sizeof(Vec3));
			ksJson* idxBuffer = ksJson_SetObject(ksJson_AddArrayElement(buffers));
			ksJson_SetUint32(ksJson_AddObjectMember(idxBuffer, "byteLength"), indices.size() * sizeof(uint16_t));
		}
		// buffer views
		{
			ksJson* bufferViews = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "bufferViews"));
			ksJson* posBufferView = ksJson_SetObject(ksJson_AddArrayElement(bufferViews));
			ksJson_SetUint32(ksJson_AddObjectMember(posBufferView, "buffer"), 0);
			ksJson_SetUint32(ksJson_AddObjectMember(posBufferView, "byteOffset"), 0);
			ksJson_SetUint32(ksJson_AddObjectMember(posBufferView, "byteLength"), points.size() * sizeof(Vec3));
			ksJson_SetUint32(ksJson_AddObjectMember(posBufferView, "target"), 34963); // ELEMENT_ARRAY_BUFFER
			ksJson* idxBufferView = ksJson_SetObject(ksJson_AddArrayElement(bufferViews));
			ksJson_SetUint32(ksJson_AddObjectMember(idxBufferView, "buffer"), 1);
			ksJson_SetUint32(ksJson_AddObjectMember(idxBufferView, "byteOffset"), 0);
			ksJson_SetUint32(ksJson_AddObjectMember(idxBufferView, "byteLength"), points.size() * sizeof(Vec3));
		}
		// accessors
		{
			ksJson* accessors = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "accessors"));
			ksJson* posAccessor = ksJson_SetObject(ksJson_AddArrayElement(accessors));
			ksJson_SetUint32(ksJson_AddObjectMember(posAccessor, "bufferView"), 0);
			ksJson_SetUint32(ksJson_AddObjectMember(posAccessor, "byteOffset"), 0);
			ksJson_SetUint32(ksJson_AddObjectMember(posAccessor, "componentType"), 5126);// float
			ksJson_SetUint32(ksJson_AddObjectMember(posAccessor, "count"), points.size());
			ksJson_SetString(ksJson_AddObjectMember(posAccessor, "type"), "VEC3");
			ksJson* idxAccessor = ksJson_SetObject(ksJson_AddArrayElement(accessors));
			ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "bufferView"), 1);
			ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "byteOffset"), 0);
			ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "componentType"), 5123); // unsigned short
			ksJson_SetUint32(ksJson_AddObjectMember(idxAccessor, "count"), indices.size());
			ksJson_SetString(ksJson_AddObjectMember(idxAccessor, "type"), "SCALAR");
		}
		// meshes
		{
			ksJson* meshes = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "meshes"));
			ksJson* mesh0 = ksJson_SetObject(ksJson_AddArrayElement(meshes));
			ksJson* primitives = ksJson_SetArray(ksJson_AddObjectMember(mesh0, "primitives"));
			ksJson* primitive0 = ksJson_SetObject(ksJson_AddArrayElement(primitives));
			ksJson* attributes = ksJson_AddObjectMember(primitive0, "attributes");
			ksJson_SetUint32(ksJson_AddObjectMember(attributes, "POSITION"), 0); // accessor 0
			ksJson_SetUint32(ksJson_AddObjectMember(primitive0, "indices"), 1); // accessor 1
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
			ksJson_SetUint32(ksJson_AddArrayElement(nodes), 0) ;
		}
		ksJson_WriteToBuffer(rootNode, &json, &length);
		ksJson_Destroy(rootNode);
	}
	lxd::print("{}\n", json);
	return true;
}

void createJson() {
	
}

bool lxd::Glb::save(std::wstring_view path) {

	return false;
}

void lxd::Glb::extractChunk(const char* data, size_t size) {
	Chunk chunk;
	uint32_t length = *reinterpret_cast<const uint32_t*>(data);
	chunk.type = *reinterpret_cast<const uint32_t*>(data + 4);
	chunk.data.assign(const_cast<char*>(data)+4+4, const_cast<char*>(data)+4+4+length);
	m_chunks.emplace_back(std::move(chunk));
	if(4 + 4 + length < size)
		extractChunk(data + 4 + 4 + length, size - 4 - 4 - length);
}
