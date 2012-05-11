#ifndef UPDATE_TILE_TRIMMED_HPP
#define UPDATE_TILE_TRIMMED_HPP

#include "Constants.hpp"

#include <vector>
#include <utility>
#include <stdint.h>

class LayerOp;
class StackOp;

//For these calls, out should be initialized with the desired background color.

void update_tile_trimmed(
	std::vector< std::pair< const LayerOp *, const uint32_t * > > const &layers,
	std::vector< std::pair< const StackOp *, const uint8_t * > > const &strokes,
	uint32_t *out,
	unsigned int coefs_to_keep,
	unsigned int block_size = TileSize * TileSize);

template< unsigned int COUNT, unsigned int BS > 
void update_tile_trimmed(
	std::vector< std::pair< const LayerOp *, const uint32_t * > > const &layers,
	std::vector< std::pair< const StackOp *, const uint8_t * > > const &strokes,
	uint32_t *out) {
	update_tile_trimmed(layers, strokes, out, COUNT, BS);
}


#endif //UPDATE_TILE_TRIMMED_HPP
