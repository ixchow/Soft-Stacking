#ifndef UPDATE_TILE_FULL_HPP
#define UPDATE_TILE_FULL_HPP

#include <vector>
#include <utility>
#include <stdint.h>

class LayerOp;
class StackOp;

//For these calls, out should be initialized with the desired background color.
void update_tile_full(
	std::vector< std::pair< const LayerOp *, const uint32_t * > > const &layers,
	std::vector< std::pair< const StackOp *, const uint8_t * > > const &strokes,
	uint32_t *out, unsigned int block_size);

template< unsigned int BS > 
void update_tile_full(
	std::vector< std::pair< const LayerOp *, const uint32_t * > > const &layers,
	std::vector< std::pair< const StackOp *, const uint8_t * > > const &strokes,
	uint32_t *out) {
	update_tile_full(layers, strokes, out, BS);
}


#endif //UPDATE_TILE_FULL_HPP
