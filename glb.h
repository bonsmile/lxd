#pragma once

#include "defines.h"
#include <cstdint>
#include <vector>
#include <string_view>
#include <span>
#include <variant>

namespace lxd {
	struct MyVec3 {
		float v[3];
		friend bool operator==(const MyVec3& a, const MyVec3& b) {
			return a.v[0] == b.v[0] && a.v[1] == b.v[1] && a.v[2] == b.v[2];
		}
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
		bool createFromPolygonSoup(const std::vector<MyVec3>& points);
		bool save(const std::wstring& path);
		std::vector<uint8_t> searialize(int id);
		//
		std::span<MyVec3> getPositions();
		std::variant<std::span<uint16_t>, std::span<uint32_t>> getIndices();
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

