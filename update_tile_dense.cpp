#include "update_tile_dense.hpp"
#include "LayerOps.hpp"
#include "StackOps.hpp"
#include "coefs.hpp"
#include <Vector/Vector.hpp>

using std::vector;
using std::pair;

void update_tile_dense(std::vector< std::pair< const LayerOp *, const uint32_t * > > const &layers, std::vector< std::pair< const StackOp *, const uint8_t * > > const &strokes, uint32_t *out) {

	for (unsigned int pix = 0; pix < TileSize * TileSize; ++pix) {
		vector< uint32_t > opaque;
		for (unsigned int l = 0; l < layers.size(); ++l) {
			if (layers[l].second == NULL) {
				/* ignore */
			} else if ((layers[l].second[pix] >> 24) == 0) {
				/* ignore more */
			} else {
				opaque.push_back(l);
			}
		}
		vector< float > coefs(count_stackings(opaque.size()), 0.0f);
		coefs[0] = 1.0f;
		for (vector< pair< const StackOp *, const uint8_t * > >::const_iterator s = strokes.begin(); s != strokes.end(); ++s) {
			float amt = s->second[pix] / 255.0f;
			if (amt == 0.0f) continue;
			vector< float > new_coefs(coefs.size(), 0.0f);
			for (unsigned int idx = 0; idx < coefs.size(); ++idx) {
				if (coefs[idx] == 0.0f) continue;
				vector< uint32_t > order = to_stacking(idx, opaque.size());
				vector< unsigned int > layer_to_order(layers.size(), -1U);
				for (unsigned int i = 0; i < order.size(); ++i) {
					layer_to_order[opaque[order[i]]] = i;
				}
				s->first->apply(layer_to_order.size(), &layer_to_order[0]);
				vector< uint32_t > new_order(opaque.size(), -1U);
				unsigned int o = 0;
				for (unsigned int l = 0; l < layer_to_order.size(); ++l) {
					if (layer_to_order[l] == -1U) continue;
					assert(o < opaque.size());
					assert(l == opaque[o]);
					assert(layer_to_order[l] < new_order.size());
					assert(new_order[layer_to_order[l]] == -1U);
					new_order[layer_to_order[l]] = o;
					++o;
				}
				assert(o == new_order.size());
				unsigned int new_idx = to_stacking_index(new_order);
				assert(new_idx < new_coefs.size());
				new_coefs[new_idx] += amt * coefs[idx];
				new_coefs[idx] += (1.0f - amt) * coefs[idx];
			}
			coefs = new_coefs;
		}
		//Okay, now have proper coefs for all stackings, do a compositing pass:
		Vector4f color = make_vector(0.0f, 0.0f, 0.0f, 0.0f);
		for (unsigned int idx = 0; idx < coefs.size(); ++idx) {
			if (coefs[idx] == 0.0f) continue;
			vector< uint32_t > order = to_stacking(idx, opaque.size());
			uint32_t col = out[pix];
			for (vector< uint32_t >::iterator o = order.begin(); o != order.end(); ++o) {
				layers[opaque[*o]].first->compose(&col, &(layers[opaque[*o]].second[pix]), &col, 1);
			}
			color += coefs[idx] * make_vector< float >( (col >> 24) & 0xff, (col >> 16) & 0xff, (col >> 8) & 0xff, col & 0xff);
		}
		{ //color labels here are maybe not actually right, but whatever:
			int a = color.c[0];
			int b = color.c[1];
			int g = color.c[2];
			int r = color.c[3];
			if (a < 0) a = 0;
			if (a > 255) a = 255;
			if (b < 0) b = 0;
			if (b > 255) b = 255;
			if (g < 0) g = 0;
			if (g > 255) g = 255;
			if (r < 0) r = 0;
			if (r > 255) r = 255;
			out[pix] = (a << 24) | (b << 16) | (g << 8) | (r);
		}
	}
}

