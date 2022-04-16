#pragma once

#include "defines.h"
#include <cstdint>
#include <vector>
#include <string_view>

namespace lxd {
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
		bool load(std::string_view buffer);
		bool loadFromStl(std::string_view buffer);
		bool save(std::wstring_view path);
	private:
		void extractChunk(const char* data, size_t size);
	private:
		Header m_header;
		std::vector<Chunk> m_chunks;
	};
}

