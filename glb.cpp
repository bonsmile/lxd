#include "glb.h"
#include <cassert>
#include "json.h"
#include <unordered_map>

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
		auto pV1 = reinterpret_cast<const Vec3*>(pFacet->v1);
		for(int j = 0; j < 3; j++) {
			const Vec3* pVec3 = pV1 + i;
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
	return false;
}

void createJson() {
	char* buffer = NULL;
	int length = 0;
	// asset
	{
		ksJson* rootNode = ksJson_SetObject(ksJson_Create());
		ksJson* asset = ksJson_SetObject(ksJson_AddObjectMember(rootNode, "asset"));
		ksJson_SetFloat(ksJson_AddObjectMember(asset, "version"), 2.0);
		// accessors
		ksJson* accessors = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "accessors"));
		ksJson* accessor0 = ksJson_AddArrayElement(accessors);

		// buffers
		ksJson* buffers = ksJson_SetArray(ksJson_AddObjectMember(rootNode, "buffers"));
		ksJson* glbBuffer = ksJson_AddArrayElement(buffers);
		ksJson_SetUint32(ksJson_AddObjectMember(glbBuffer, "byteLength"), 1000);
		// meshes
		// nodes
		// scene
		ksJson_WriteToBuffer(rootNode, &buffer, &length);
		ksJson_Destroy(rootNode);
	}
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
