#include "LayerOps.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "sse_compose.hpp"

class OverOp : public LayerOp {
public:
	virtual ~OverOp() {
	}
	virtual void compose(uint32_t const *bg_tile, uint32_t const *fg_tile, uint32_t *into_tile, uint32_t count) const {
		sse_compose(bg_tile, fg_tile, into_tile, count);
	}
	virtual std::string shorthand() const {
		return "o";
	}
	virtual std::string desc() const {
		return "over";
	}

};

const LayerOp *LayerOp::over() {
	static LayerOp *ret = new OverOp;
	return ret;
}

class MultiplyOp : public LayerOp {
public:
	virtual ~MultiplyOp() {
	}
	virtual void compose(uint32_t const *bg_tile, uint32_t const *fg_tile, uint32_t *into_tile, uint32_t count) const {
		assert(bg_tile);
		assert(fg_tile);
		assert(into_tile);
		sse_multiply(bg_tile, fg_tile, into_tile, count);
	}
	virtual std::string shorthand() const {
		return "*";
	}
	virtual std::string desc() const {
		return "multiply";
	}

};

const LayerOp *LayerOp::multiply() {
	static LayerOp *ret = new MultiplyOp;
	return ret;
}

const LayerOp *LayerOp::named_op(std::string const &name) {
	if (name == "mul" || name == "multiply" || name == "m" || name == "*") {
		return multiply();
	} else if (name == "over" || name == "o") {
		return over();
	} else {
		return NULL;
	}
}

std::vector< std::string > LayerOp::op_shorthands() {
	std::vector< std::string > ret;
	ret.push_back(over()->shorthand());
	ret.push_back(multiply()->shorthand());
	return ret;
}
