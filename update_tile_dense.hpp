#ifndef UPDATE_TILE_DENSE_HPP
#define UPDATE_TILE_DENSE_HPP

#include <vector>
#include <utility>
#include <stdint.h>

class LayerOp;
class StackOp;

//out should be initialized with the desired background color.
void update_tile_dense(
	std::vector< std::pair< const LayerOp *, const uint32_t * > > const &layers,
	std::vector< std::pair< const StackOp *, const uint8_t * > > const &strokes,
	uint32_t *out);

#endif //UPDATE_TILE_DENSE_HPP
