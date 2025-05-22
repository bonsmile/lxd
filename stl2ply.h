#pragma once

#ifdef __cplusplus
extern "C" {
#endif
	bool stl2ply(char* stlBuffer, int stlBufferSize, char** outBuffer, int* outBufferSize);
#ifdef __cplusplus
}
#endif