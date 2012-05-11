#include "update_tile_sampled.hpp"
#include "LayerOps.hpp"
#include "StackOps.hpp"
#include "coefs.hpp"
#include <Vector/Vector.hpp>
#include <tr1/unordered_map>

using std::vector;
using std::pair;
using std::tr1::unordered_map;

class ConstraintInfo {
public:
	ConstraintInfo(float _energy) : energy(_energy) { }
	float energy; //energy (== summed opacity) for which this constraint accounts
};

void update_tile_sampled_tile(unsigned int samples, std::vector< std::pair< const LayerOp *, const uint32_t * > > const &layers, std::vector< std::pair< const StackOp *, const uint8_t * > > const &strokes, uint32_t *out) {


	//calculate the 'energy' == 'average alpha' of each constraint:
	vector< ConstraintInfo > infos;
	for (vector< pair< const StackOp *, const uint8_t * > >::const_iterator s = strokes.begin(); s != strokes.end(); ++s) {
		float sum = 0.0f;
		for (unsigned int i = 0; i < TileSize * TileSize; ++i) {
			sum += s->second[i];
		}
		sum /= TileSize * TileSize;
		infos.push_back(ConstraintInfo(sum / (TileSize * TileSize * 255.0f)));
	}
	//path says we should either take constraint (true) or not take constraint (false).
	vector< bool > path(infos.size(), false);
	//init path to highest-energy setting:
	for (unsigned int i = 0; i < path.size(); ++i) {
		//take constraint if it has more than 50% of the energy:
		path[i] = (infos[i].energy > 0.5f);
	}
	vector< uint16_t > path_order(infos.size());
	{
		vector< pair< float, uint16_t > > path_energy(infos.size());
		for (unsigned int i = 0; i < infos.size(); ++i) {
			path_energy[i].first = infos[i].energy;
			path_energy[i].second = i;
		}
		sort(path_energy.begin(), path_energy.end());
		assert(path_energy.empty() || path_energy[0].first <= path_energy.back().first);
		for (unsigned int i = 0; i < infos.size(); ++i) {
			path_order[i] = path_energy[i].second;
		}
	}

	
	Vector< float, TileSize * TileSize > coef_sums = make_vector< float, TileSize * TileSize >(0.0f);
	vector< Vector4f, TileSize * TileSize > final_colors = make_vector< Vector4f, TileSize * TileSize >(make_vector(0.0f, 0.0f, 0.0f, 0.0f));

	for (unsigned int sample = 0; sample < samples; ++sample) {
		//Run the current path:
		vector< unsigned int > layer_to_order(layers.size(), -1U);
		{ //set up default order:
			unsigned int o = 0;
			for (unsigned int l = 0; l < layers.size(); ++l) {
				if (layers[l].second == NULL) {
					/* ignore */
				} else {
					layer_to_order[l] = o;
					++o;
				}
			}
		}
		{ //apply the constraints along this path:
			vector< bool >::const_iterator p = path.begin();
			for (vector< pair< const StackOp *, const uint8_t * > >::const_iterator s = strokes.begin(); s != strokes.end(); ++s, ++p) {
				if (*p) {
					s->first->apply(layer_to_order.size(), &layer_to_order[0]);
				}
			}
		}
		//Compute the final order after constraints applied:
		vector< uint16_t > final_order;
		for (unsigned int l = 0; l < layer_to_order.size(); ++l) {
			if (layer_to_order[l] == -1U) continue;
			while (layer_to_order[l] >= final_order.size()) {
				final_order.push_back(uint16_t(-1U));
			}
			assert(final_order[layer_to_order[l]] == uint16_t(-1U));
			final_order[layer_to_order[l]] = l;
		}

		Vector< uint32_t, TileSize * TileSize > col;
		//copy background color from out:
		memcpy(colors.c, out, sizeof(uint32_t) * TileSize * TileSize);
		//And compose the other layers:
		for (vector< uint16_t >::const_iterator o = final_order.begin(); o != final_order.end(); ++o) {
			layers[*o].first->compose(&(col[0]), &(layers[*o].second[0]), &(col[0]), TileSize * TileSize);
		}

		Vector< float, TileSize * TileSize > coefs = make_vector< float, TileSize * TileSize >(1.0f);
		{ //figure out how important this path was:
			vector< bool >::const_iterator p = path.begin();
			for (vector< pair< const StackOp *, const uint8_t * > >::const_iterator s = strokes.begin(); s != strokes.end(); ++s, ++p) {
				if (*p) {
					for (unsigned int i = 0; i < TileSize * TileSize; ++i) {
						coefs[i] *= s->second[i] / 255.0f;
					}
				} else {
					for (unsigned int i = 0; i < TileSize * TileSize; ++i) {
						coefs[i] *= 1.0f - (s->second[i] / 255.0f);
					}
				}
			}
		}

		//Accumulate color info:
		for (unsigned int i = 0; i < TileSize * TileSize; ++i) {
			final_colors[i] += coefs[i] * make_vector< float >( (col[i] >> 24) & 0xff, (col[i] >> 16) & 0xff, (col[i] >> 8) & 0xff, col[i] & 0xff);
		}
		//Accumulate coefs:
		coef_sums += coefs;

		//Advance to next path:
		//TODO.
	}

	//Compute colors based on samples:
	for (unsigned int i = 0; i < TileSize * TileSize; ++i) {
		if (coefs_sums[i] == 0.0f) continue;
		Vector4f color = final_colors[i] / coef_sums[i];
		//color labels here are maybe not actually right, but whatever:
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
		out[i] = (a << 24) | (b << 16) | (g << 8) | (r);
	}
}

