#pragma once

#include "defines.h"
#include <cstdint>
#include <vector>
#include <string_view>
#include <span>
#include <variant>

namespace lxd {
	struct MyVec3f {
		float v[3];
		friend bool operator==(const MyVec3f& a, const MyVec3f& b) {
		    float cwiseAbsSum = 0;
			for (int i = 0; i < 3; i++) {
			    cwiseAbsSum += abs(a.v[i] - b.v[i]);
			}
		    return cwiseAbsSum < 1.0e-7;
		}
	};
    struct MyVec3d {
	    double v[3];
	    friend bool operator==(const MyVec3d& a, const MyVec3d& b) {
		    double cwiseAbsSum = 0;
		    for (int i = 0; i < 3; i++) {
			    cwiseAbsSum += abs(a.v[i] - b.v[i]);
		    }
		    return cwiseAbsSum < 1.0e-7;
	    }
    };
	struct Face {
	    int vid[3];
	};
	class DLL_PUBLIC Glb {
	public:
		struct Header {
			uint32_t magic;
			uint32_t version;
			uint32_t length;
		};
		struct Chunk {
			uint32_t type;
			std::vector<char> data;
		};
		struct Accessor {
			int bufferView;
			int byteOffset;
			int componentType;
			int count;
			char type[8];
		};
		struct BufferView {
			int buffer;
			int byteOffset;
			int byteLength;
			int byteStride;
			int target;
		};
		Glb() {
			m_header.magic = 0x46546C67;
			m_header.version = 2;
			m_header.length = 0;
		}
		~Glb() {}
		bool load(std::string_view buffer);
		bool loadFromStl(std::string_view buffer);
	    bool create(const std::vector<MyVec3f>& points, const std::vector<Face>& faces, const std::vector<char>& extraAttribute);
	    bool create(const std::vector<MyVec3f>& points);
	    bool create(const std::vector<MyVec3f>& points, const std::variant<std::vector<uint16_t>, std::vector<uint32_t>>& indicesVar, const std::vector<char>& extraAttribute);
		bool save(const String& path);
		std::vector<uint8_t> searialize();
		//
		std::span<MyVec3f> getPositions();
		std::variant<std::span<uint16_t>, std::span<uint32_t>> getIndices();
	    std::span<char> getExtraAttribute();
	private:
		void clear();
		void extractChunk(const char* data, size_t size);
		void extractJson();
	private:
		Header m_header;
		std::vector<Accessor> m_accessors;
		std::vector<BufferView> m_bufferViews;
		std::vector<Chunk> m_chunks;
	};
}

