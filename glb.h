#pragma once

#include "defines.h"
#include <cstdint>
#include <vector>
#include <string_view>

namespace lxd {
	struct MyVec3 {
		float v[3];
		friend bool operator==(const MyVec3& a, const MyVec3& b) {
			return a.v[0] == b.v[0] && a.v[1] == b.v[1] && a.v[2] == b.v[2];
		}
	};

	template <class T>
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
		bool load(const std::vector<MyVec3>& points, const std::vector<T>& indices);
		bool save(const std::wstring& path);
	private:
		void clear();
		void extractChunk(const char* data, size_t size);
	private:
		Header m_header;
		std::vector<BufferView> m_bufferViews;
		std::vector<Chunk> m_chunks;
	};
}

