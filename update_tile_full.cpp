#include "update_tile_full.hpp"
#include "LayerOps.hpp"
#include "StackOps.hpp"
#include "coefs.hpp"

#include <Vector/Vector.hpp>

#include <memory.h>

#include <iostream>

#ifdef WIN32
#include <unordered_map>
using std::unordered_map;
#else
#include <tr1/unordered_map>
using std::tr1::unordered_map;
#endif

using std::cerr;
using std::endl;

using std::vector;
using std::pair;
using std::make_pair;

namespace {
class LayerToOrder : public vector< unsigned int > {
public:
	LayerToOrder() { }
};

class HashLayerToOrder {
public:
	size_t operator()(LayerToOrder const &o) const {
		size_t ret = 0;
		for (unsigned int i = 0; i < o.size(); ++i) {
			ret ^= ( (ret << 5) | (ret >> (sizeof(size_t)*8-5)) ) ^ o[i];
		}
		return ret;
	}
};

class GreaterCoef {
public:
	bool operator()(pair< float, unsigned int > const &a, pair< float, unsigned int > const &b) const {
		if (a.first > b.first) return true;
		if (a.first < b.first) return false;
		if (a.second > b.second) return true;
		if (a.second < b.second) return false;
		return false;
	}
};

typedef unordered_map< LayerToOrder, uint32_t , HashLayerToOrder > LayerToOrderToInd;

}

void update_tile_full(std::vector< std::pair< const LayerOp *, const uint32_t * > > const &layers, std::vector< std::pair< const StackOp *, const uint8_t * > > const &strokes, uint32_t *out, unsigned int block_size) {
	assert((TileSize * TileSize) % block_size == 0);
	for (unsigned int block_base = 0; block_base + block_size <= TileSize * TileSize; block_base += block_size) {

	LayerToOrder starting;
	starting.resize(layers.size(), -1U);
	//fill up starting order:
	for (unsigned int l = 0; l < starting.size(); ++l) {
		starting[l] = l;
	}
	//Even NULL layers included because there would be tile artifacts otherwise.

	LayerToOrderToInd l2o_ind;
	vector< LayerToOrder > l2os;
	l2o_ind.insert(make_pair(starting, 0));
	l2os.push_back(starting);
	vector< unsigned int > next_l2o;
	next_l2o.push_back(-1U);
	unsigned int first_free_l2o = -1U;
	unsigned int first_used_l2o = 0;

	//coefs per pixel will kinda string out...
	vector< pair< float, unsigned int > > coefs(block_size * 2);
	vector< pair< float, unsigned int > > new_coefs;
	for (unsigned int i = 0; i < block_size; ++i) {
		coefs[2*i+0] = make_pair(1.0f, 0U);
		coefs[2*i+1] = make_pair(-1.0f,-1U);
	}
	//We'll wrap all the asserts in tight loops:
	#define PARANOID( X ) /* nothing; could be: assert( X ) */
	for (vector< pair< const StackOp *, const uint8_t * > >::const_iterator s = strokes.begin(); s != strokes.end(); ++s) {
		vector< unsigned int > new_inds(l2os.size(), -1U);
		{ //new_inds maps from inds of stackings to inds of their mapped versions:
			unsigned int size = l2os.size();
			assert(next_l2o.size() == l2os.size());
			assert(first_used_l2o < l2os.size()); //can't be using no orderings, right?
			for (unsigned int i = first_used_l2o; i < size; i = next_l2o[i]) {
				LayerToOrder l2o = l2os[i];
				s->first->apply(l2o.size(), &l2o[0]);
				LayerToOrderToInd::iterator f = l2o_ind.find(l2o);
				if (f != l2o_ind.end()) {
					new_inds[i] = f->second;
				} else {
					if (first_free_l2o == -1U) {
						//allocate a new spot at the end:
						first_free_l2o = l2os.size();
						l2os.push_back(LayerToOrder());
						next_l2o.push_back(-1U);
					}
					//Slot in l2o at first free spot:
					unsigned int ind = first_free_l2o;
					PARANOID(ind < l2os.size());
					PARANOID(ind < next_l2o.size());
					first_free_l2o = next_l2o[ind];
					PARANOID(ind < first_free_l2o);
					if (ind > 0) {
						PARANOID(next_l2o[ind-1] > ind);
						next_l2o[ind] = next_l2o[ind-1];
						next_l2o[ind-1] = ind;
					} else {
						PARANOID(ind < first_used_l2o);
						PARANOID(first_used_l2o < next_l2o.size());
						next_l2o[ind] = first_used_l2o;
						first_used_l2o = ind;
					}
					l2o_ind.insert(make_pair(l2o, ind));
					new_inds[i] = ind;
					l2os[ind] = l2o;
				}
			}
		}
		new_coefs.clear();
		//tracks location of various inds:
		vector< unsigned int > ind_loc(l2os.size(), -1U);
		//Fill in new_coefs by mapping coefs one-by-one:
		unsigned int pix = block_base;
		for (vector< pair< float, unsigned int > >::const_iterator c = coefs.begin(); c != coefs.end(); ++c) {
			PARANOID(c->second != -1U); //No empty coef lists, darn it.
			uint8_t alpha = s->second[pix];
			unsigned int new_base = new_coefs.size();
			//Push in the old:
			if (alpha != 255) {
				float amt = 1.0f - alpha / 255.0f;
				vector< pair< float, unsigned int > >::const_iterator c2 = c;
				while (c2->second != -1U) {
					pair< float, unsigned int > old = *c2;
					old.first *= amt;
					PARANOID(old.second < ind_loc.size());
					ind_loc[old.second] = new_coefs.size();
					new_coefs.push_back(old);
					++c2;
					PARANOID(c2 != coefs.end());
				}
			}
			//Push in the new:
			if (alpha != 0) {
				vector< pair< float, unsigned int > >::const_iterator before = c;
				float amt = alpha / 255.0f;
				while (c->second != -1U) {
					pair< float, unsigned int > old = *c;
					old.first *= amt;
					old.second = new_inds[old.second];
					unsigned int loc = ind_loc[old.second];
					if (int(loc) < int(new_base)) {
						ind_loc[old.second] = new_coefs.size();
						new_coefs.push_back(old);
					} else {
						new_coefs[loc].first += old.first;
					}
					++c;
					PARANOID(c != coefs.end());
				}
			} else {
				while (c->second != -1U) {
					++c;
					PARANOID(c != coefs.end());
				}
			}
			PARANOID(c->second == -1U);
			
			PARANOID(new_coefs.size() > new_base); //still have ~some~ coefs.
			new_coefs.push_back(make_pair(-1.0f,-1U));
			++pix;
		}
		//Not a tight-loop assert, so not going to PARANOID it.
		assert(pix == block_base + block_size);
		coefs.swap(new_coefs);
		{ //Trim unused l2os:
			vector< bool > used(l2os.size(), false);
			for (vector< pair< float, unsigned int > >::const_iterator c = coefs.begin(); c != coefs.end(); ++c) {
				if (c->second != -1U) {
					PARANOID(c->second < used.size());
					used[c->second] = true;
				}
			}
			PARANOID(next_l2o.size() == l2os.size());
			//First advance first_used_l2o until it appears to be used:
			while (first_used_l2o < l2os.size() && !used[first_used_l2o]) {
				unsigned int ind = first_used_l2o;
				{ //remove info for this:
					LayerToOrderToInd::iterator f = l2o_ind.find(l2os[ind]);
					PARANOID(f != l2o_ind.end());
					l2o_ind.erase(f);
					l2os[ind].clear();
				}
				first_used_l2o = next_l2o[first_used_l2o];
				if (ind == 0) {
					next_l2o[ind] = first_free_l2o;
					first_free_l2o = ind;
				} else {
					PARANOID(first_free_l2o == 0);
					next_l2o[ind] = next_l2o[ind-1];
					next_l2o[ind-1] = ind;
				}
			}
			unsigned int used_at = first_used_l2o;
			unsigned int free_at = first_free_l2o;
			if (used_at > 0) {
				PARANOID(free_at == 0);
				free_at = used_at - 1;
			}
			while (used_at < l2os.size()) {
				PARANOID(used[used_at]);
				while (next_l2o[used_at] < l2os.size() && !used[next_l2o[used_at]]) {
					unsigned int ind = next_l2o[used_at];
					{ //remove info for this:
						LayerToOrderToInd::iterator f = l2o_ind.find(l2os[ind]);
						PARANOID(f != l2o_ind.end());
						l2o_ind.erase(f);
						l2os[ind].clear();
					}

					//Now: patch ind out of used list and into free list.
					
					//Out of used list is easy:
					next_l2o[used_at] = next_l2o[ind];

					if (used_at + 1 < ind) {
						free_at = ind - 1;
					}
					if (ind < free_at) {
						PARANOID(free_at == first_free_l2o);
						next_l2o[ind] = first_free_l2o;
						first_free_l2o = ind;
					} else {
						PARANOID(!used[free_at]);
						PARANOID(next_l2o[free_at] > ind);
						next_l2o[ind] = next_l2o[free_at];
						next_l2o[free_at] = ind;
					}
					free_at = ind;
				}

				if (used_at + 1 < next_l2o[used_at]) {
					free_at = next_l2o[used_at]-1;
				}
				used_at = next_l2o[used_at];
			} //while (used_at < l2os.size());

			/*
			{ //DEBUG: make sure our free/used list is in good condition.
				unsigned int have_used = 0;
				for (unsigned int ind = first_used_l2o; ind < l2os.size(); ind = next_l2o[ind]) {
					assert(used[ind]);
					assert(next_l2o[ind] > ind);
					++have_used;
				}
				unsigned int have_free = 0;
				for (unsigned int ind = first_free_l2o; ind < l2os.size(); ind = next_l2o[ind]) {
					assert(!used[ind]);
					assert(next_l2o[ind] > ind);
					++have_free;
				}
				assert(have_used + have_free == l2os.size());
			}
			*/
		} //trim
	}

	
	//Pre-composite for all used orders:
	vector< uint32_t * > comps(l2os.size(), NULL);
	for (unsigned int i = first_used_l2o; i < l2os.size(); i = next_l2o[i]) {
		uint32_t *col = new uint32_t[block_size];
		memcpy(col, out + block_base, block_size * sizeof(uint32_t));
		comps[i] = col;
		vector< unsigned int > order(layers.size(), -1U);
		for (vector< unsigned int >::iterator l = l2os[i].begin(); l != l2os[i].end(); ++l) {
			assert(*l < order.size());
			order[*l] = l - l2os[i].begin();
		}
		for (vector< unsigned int >::const_iterator o = order.begin(); o != order.end(); ++o) {
			assert(*o < layers.size());
			//Don't composite 'NULL' of course:
			if (layers[*o].second) {
				layers[*o].first->compose(col, &(layers[*o].second[block_base]), col, block_size);
			}
		}
	}

	//actually blend, per-pixel:
	Vector4f color_acc = make_vector(0.0f, 0.0f, 0.0f, 0.0f);
	float coef_acc = 0.0f;
	unsigned int sum = 0;
	unsigned int pix = 0;
	for (vector< pair< float, unsigned int > >::const_iterator c = coefs.begin(); c != coefs.end(); ++c) {
		if (c->second == -1U) {
			if (coef_acc == 0.0f) {
				cerr << "Dire circumstance: we may have trimmed almost all of the energy from a pixel (has " << sum << " remaining.)" << endl;
			} else {
				color_acc *= 1.0f / coef_acc;
			
			}
			{ //convert to bytes:
				int a = int(color_acc.c[0]);
				int b = int(color_acc.c[1]);
				int g = int(color_acc.c[2]);
				int r = int(color_acc.c[3]);
				if (a < 0) a = 0;
				if (a > 255) a = 255;
				if (b < 0) b = 0;
				if (b > 255) b = 255;
				if (g < 0) g = 0;
				if (g > 255) g = 255;
				if (r < 0) r = 0;
				if (r > 255) r = 255;
				out[block_base + pix] = (a << 24) | (b << 16) | (g << 8) | (r);
			}


			color_acc = make_vector(0.0f, 0.0f, 0.0f, 0.0f);
			coef_acc = 0.0f;
			sum = 0;
			++pix;
		} else {
			assert(c->second < comps.size());
			assert(comps[c->second]);
			uint32_t src = comps[c->second][pix];
			color_acc += c->first * make_vector( float((src >> 24) & 0xff), float((src >> 16) & 0xff), float((src >> 8) & 0xff), float(src & 0xff));
			coef_acc += c->first;
			++sum;
		}
	}
	assert(pix == block_size);

	//free allocated comps:
	for (unsigned int i = 0; i < comps.size(); ++i) {
		if (comps[i]) {
			delete[] comps[i];
		}
	}

	} //end of for(block_base)

}

