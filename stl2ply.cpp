#include "stl2ply.h"
#include "smallvector.h"
#include <vector>
#include <unordered_map>
#include <span>

// STL 二进制文件结构
#pragma pack(push, 1)
struct STLTriangle {
	float normal[3];    // 法向量（通常忽略）
	float v1[3];        // 顶点1
	float v2[3];        // 顶点2
	float v3[3];        // 顶点3
	uint16_t attr;      // 属性（通常忽略）
};
#pragma pack(pop)

struct Vertex { 
    float v[3];
    // 口扫的精度通常 > 10um, 这里认为 x,y,z 累计误差不超过 1um 为同一点
    friend bool operator==(const Vertex& a, const Vertex& b) {
        float cwiseAbsSum = 0;
        for (int i = 0; i < 3; i++) {
            cwiseAbsSum += abs(a.v[i] - b.v[i]);
        }
        return cwiseAbsSum < 1.0e-3;
    }
};

// 顶点哈希函数（用于去重）

namespace std {
    template<>
    struct hash<Vertex> {
        size_t operator()(const Vertex& vec) const {
            size_t seed = 0;
            for (int i = 0; i < 3; ++i) {
                seed ^=
                    std::hash<float>()(vec.v[i]) + 0x9e3779b9U + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
}

struct Face { uint8_t count; llvm::SmallVector<uint32_t, 3> indices; };

int countTriangles(char* data, int size) {
	if (size < 84)
		return 0;

	int nTriangle = *(reinterpret_cast<int*>(data + 80));
	int validFileSize = 84 + 50 * nTriangle;
	if (size != validFileSize)
		return 0;
	else
		return nTriangle;
}

bool stl2ply(char* stlBuffer, int stlBufferSize, char** outBuffer, int* outBufferSize) {
	int nTriangle = countTriangles(stlBuffer, stlBufferSize);
	if (nTriangle == 0)
		return false;

    auto triangles = std::span(
        reinterpret_cast<STLTriangle*>(stlBuffer + 84),
        nTriangle
    );

    // 2. 处理顶点和面片（去重）
    std::unordered_map<Vertex, uint32_t> vertex_map;
    std::vector<Vertex> vertices;
    std::vector<Face> faces;

    for (const auto& tri : triangles) {
        Face face{ 3, {} };
        auto process_vertex = [&](float x, float y, float z) {
            Vertex v{ x, y, z };
            auto [it, inserted] = vertex_map.try_emplace(v, static_cast<uint32_t>(vertices.size()));
            if (inserted) vertices.push_back(v);
            face.indices.push_back(it->second);
        };
        process_vertex(tri.v1[0], tri.v1[1], tri.v1[2]);
        process_vertex(tri.v2[0], tri.v2[1], tri.v2[2]);
        process_vertex(tri.v3[0], tri.v3[1], tri.v3[2]);
        faces.push_back(face);
    }

    // 3. 预计算PLY头长度
    constexpr size_t kMaxHeaderSize = 256;
    char header[kMaxHeaderSize];
    int header_len = snprintf(
        header, sizeof(header),
        "ply\n"
        "format binary_little_endian 1.0\n"
        "element vertex %zu\n"
        "property float x\nproperty float y\nproperty float z\n"
        "element face %zu\n"
        "property list uchar uint vertex_indices\n"
        "end_header\n",
        vertices.size(), faces.size()
    );

    // 4. 计算总大小并一次性分配内存
    size_t total_size = header_len
        + vertices.size() * sizeof(Vertex)
        + faces.size() * (1 + 3 * sizeof(uint32_t));
    char* ply_data = static_cast<char*>(malloc(total_size));
    if (!ply_data) return false;

    *outBuffer = ply_data;
    *outBufferSize = static_cast<int>(total_size);

    // 5. 直接写入内存（无中间拷贝）
    char* ptr = ply_data;

    // 写入头
    std::memcpy(ptr, header, header_len);
    ptr += header_len;

    // 写入顶点
    std::memcpy(ptr, vertices.data(), vertices.size() * sizeof(Vertex));
    ptr += vertices.size() * sizeof(Vertex);

    // 写入面片
    for (const auto& face : faces) {
        *ptr++ = 3; // 顶点数
        std::memcpy(ptr, face.indices.data(), 3 * sizeof(uint32_t));
        ptr += 3 * sizeof(uint32_t);
    }

    return true;
}