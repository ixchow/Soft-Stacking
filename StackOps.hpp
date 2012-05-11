#ifndef STACK_OPS_HPP
#define STACK_OPS_HPP

#include "Constants.hpp"

#include <stdint.h>

#include <string>
#include <vector>

const unsigned int InvalidLayerCharCount = 7;
const char InvalidLayerChars[InvalidLayerCharCount] = { '>', '<', '&', ',', ';', '|', '*' };

class StackOp {
public:
	virtual void apply(size_t count, unsigned int *layer_to_order) const = 0;
	virtual std::string description(std::vector< std::string > const &layer_names) const = 0;
	virtual std::string shorthand(std::vector< std::string > const &layer_names) const = 0;

static const StackOp *from_shorthand(std::string desc, std::vector< std::string > const &layer_names, std::string *error_desc = NULL);
};




#endif //STACK_OPS_HPP
