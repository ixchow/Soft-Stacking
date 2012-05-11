#include "default_bg.hpp"

#include "Constants.hpp"
#include <cstdlib>

const uint32_t *default_bg() {
	static uint32_t *ret = NULL;
	if (!ret) {
		ret = new uint32_t[TileSize * TileSize];
		for (unsigned int i = 0; i < TileSize * TileSize; ++i) {
			if (((i % TileSize) ^ (i / TileSize)) & 16) {
				ret[i] = 0xffdddddd;
			} else {
				ret[i] = 0xff888888;
			}
		}
	}
	return ret;
}

