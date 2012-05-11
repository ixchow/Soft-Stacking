#ifndef UPDATE_TILE_SAMPLED_HPP
#define UPDATE_TILE_SAMPLED_HPP

#include <vector>
#include <utility>
#include <stdint.h>

class LayerOp;
class StackOp;

//For these calls, out should be initialized with the desired background color.

//per-pixel is possibly slower and likely a bit nicer:
/* TBD
void update_tile_sampled_pixel(
	unsigned int samples,
	std::vector< std::pair< const LayerOp *, const uint32_t * > > const &layers,
	std::vector< std::pair< const StackOp *, const uint8_t * > > const &strokes,
	uint32_t *out);
	*/

//Whole-tile (possibly less accurate):
void update_tile_sampled_tile(
	unsigned int samples,
	std::vector< std::pair< const LayerOp *, const uint32_t * > > const &layers,
	std::vector< std::pair< const StackOp *, const uint8_t * > > const &strokes,
	uint32_t *out);

#endif //UPDATE_TILE_SAMPLED_HPP
