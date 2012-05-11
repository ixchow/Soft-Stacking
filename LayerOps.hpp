#ifndef LAYER_OPS_HPP
#define LAYER_OPS_HPP

#include "Constants.hpp"

#include <stdint.h>

#include <string>
#include <vector>

/*
 * Big note: for speed purposes, all Ops can assume background has opaque alpha.
 */

class LayerOp {
public:
	virtual void compose(uint32_t const *bg_tile, uint32_t const *fg_tile, uint32_t *into, uint32_t count = TileSize * TileSize) const = 0;
	virtual std::string shorthand() const = 0;
	virtual std::string desc() const = 0;

static const LayerOp *over();
static const LayerOp *multiply();
static const LayerOp *named_op(std::string const &name);
static std::vector< std::string > op_shorthands();
};

#endif //LAYER_OPS_HPP
