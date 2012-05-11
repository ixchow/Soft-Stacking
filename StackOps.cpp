#include "StackOps.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>
#include <deque>

using std::set;
using std::sort;
using std::ostringstream;
using std::vector;
using std::string;
using std::deque;


typedef unsigned int LayerID;
typedef unsigned int BinIndex;

const BinIndex AsideBin = -1U;
const BinIndex InstantBin = -2U;

class Step {
public:
	Step() : bins(0), reversed(false) { }
	unsigned int bins;
	vector< BinIndex > layers_to_bins;
		//layers with bin AsideBin are held aside
		//layers with bin InstantBin don't wait
		//bin 0 waits on bin 1 waits on ...
	bool reversed; //do this whole thing on a reversed order?

	bool operator<(Step const &o) const {
		if (bins < o.bins) return true;
		if (bins > o.bins) return false;
		if (reversed && !o.reversed) return true;
		if (!reversed && o.reversed) return false;
		if (layers_to_bins.size() < o.layers_to_bins.size()) return true;
		if (layers_to_bins.size() > o.layers_to_bins.size()) return false;
		for (unsigned int l = 0; l < layers_to_bins.size(); ++l) {
			if (layers_to_bins[l] < o.layers_to_bins[l]) return true;
			if (layers_to_bins[l] > o.layers_to_bins[l]) return false;
		}
		return false;
	}
};

typedef vector< Step > Operation;

class GeneralOp : public StackOp {
public:
	GeneralOp(Operation const &_op) : op(_op) {
	}
	virtual void apply(size_t count, unsigned int *layer_to_order) const {
		vector< LayerID > order(count, -1U);
		for (unsigned int l = 0; l < count; ++l) {
			if (layer_to_order[l] < order.size()) {
				order[layer_to_order[l]] = l;
			}
		}
		//----------------------------------------------------------
		//If we go to dense orders, this shouldn't be an issue. But.
		while (order.back() == -1U) {
			order.pop_back();
		}
		for (vector< LayerID >::iterator o = order.begin(); o != order.end(); ++o) {
			assert(*o != -1U);
		}
		//----------------------------------------------------------
		//Actually run steps:
		for (Operation::const_iterator step = op.begin(); step != op.end(); ++step) {
			if (step->reversed) {
				reverse(order.begin(), order.end());
			}
			//Allocate bins:
			vector< unsigned int > bin_counts(step->bins, 0);
			//Layers get binned:
			for (vector< LayerID >::const_iterator o = order.begin(); o != order.end(); ++o) {
				assert(*o < step->layers_to_bins.size());
				BinIndex bi =  step->layers_to_bins[*o];
				if (bi == AsideBin) continue; //set aside.
				if (bi != InstantBin) {
					assert(bi < bin_counts.size());
					bin_counts[bi] += 1;
				}
			}

			vector< LayerID > stream;
			vector< vector< LayerID > > waiting(bin_counts.size());
			//Layers get sorted:
			for (vector< LayerID >::const_iterator o = order.begin(); o != order.end(); ++o) {
				assert(*o < step->layers_to_bins.size());
				BinIndex bi =  step->layers_to_bins[*o];
				if (bi == AsideBin) continue; //set aside.
				if (bi == InstantBin) {
					stream.push_back(*o);
				} else if (bi + 1 >= bin_counts.size() || bin_counts[bi + 1] == 0) {
					deque< LayerID > to_emit;
					to_emit.push_back(*o);
					while (!to_emit.empty()) {
						LayerID id = to_emit.front();
						to_emit.pop_front();
						stream.push_back(id);
						BinIndex bin = step->layers_to_bins[id];
						assert(bin < bin_counts.size());
						assert(bin_counts[bin] > 0);
						bin_counts[bin] -= 1;
						if (bin_counts[bin] == 0) {
							to_emit.insert(to_emit.end(), waiting[bin].begin(), waiting[bin].end());
							waiting[bin].clear();
						}
					}
				} else {
					//record that this layer is waiting on a bin:
					waiting[bi + 1].push_back(*o);
				}
			}
			vector< LayerID >::iterator s = stream.begin();
			//stream gets re-inserted:
			for (vector< LayerID >::iterator o = order.begin(); o != order.end(); ++o) {
				assert(*o < step->layers_to_bins.size());
				BinIndex bi =  step->layers_to_bins[*o];
				if (bi == AsideBin) continue;
				assert(s != stream.end());
				*o = *s;
				++s;
			}
			assert(s == stream.end());
			if (step->reversed) {
				reverse(order.begin(), order.end());
			}
		}

		//Fill in layer-to-order mapping with output:
		for (unsigned int i = 0; i < order.size(); ++i) {
			assert(order[i] < count);
			layer_to_order[order[i]] = i;
		}
	}
	virtual std::string description(std::vector< std::string > const &layer_names) const {
		std::string ret = "";
		for (Operation::const_iterator step = op.begin(); step != op.end(); ++step) {
			assert(step->layers_to_bins.size() == layer_names.size());
			vector< std::string > aside;
			vector< std::string > instant;
			vector< vector< std::string > > bins(step->bins);
			for (unsigned int l = 0; l < layer_names.size(); ++l) {
				unsigned int bin = step->layers_to_bins[l];
				if (bin == AsideBin) {
					aside.push_back(layer_names[l]);
				} else if (bin == InstantBin) {
					instant.push_back(layer_names[l]);
				} else {
					assert(bin < bins.size());
					bins[bin].push_back(layer_names[l]);
				}
			}
			if (bins.empty()) continue;
			if (ret != "") {
				ret += "; then place";
			} else {
				ret += "Place";
			}
			for (unsigned int bin = 0; bin < bins.size(); ++bin) {
				if (bin) {
					if (step->reversed) {
						ret += " under";
					} else {
						ret += " over";
					}
				}
				for (unsigned int e = 0; e < bins[bin].size(); ++e) {
					if (e) {
						if (bins[bin].size() > 2) {
							ret += ",";
						}
						if (e + 1 == bins[bin].size()) {
							ret += " and";
						}
					}
					ret += " " + bins[bin][e];
				}
			}
			if (!aside.empty()) {
				ret += " (set aside";
				for (unsigned int e = 0; e < aside.size(); ++e) {
					if (e) {
						if (aside.size() > 2) {
							ret += ",";
						}
						if (e + 1 == aside.size()) {
							ret += " and";
						}
					}
					ret += " " + aside[e];
				}
				ret += " during this process)";
			}
		} //for (step in operation)
		if (ret == "") {
			ret = "Do nothing";
		}
		ret += ".";
		return ret;
	}
	virtual std::string shorthand(std::vector< std::string > const &layer_names) const {
		if (short_seps.empty()) return "";
		assert(short_layers.size() == short_seps.size() + 1);
		string ret;
		for (unsigned int i = 0; i < short_layers.size(); ++i) {
			if (i) {
				ret += short_seps[i-1];
			}
			if (short_layers[i] == -1U) {
				ret += '*';
			} else {
				assert(short_layers[i] < layer_names.size());
				ret += layer_names[short_layers[i]];
			}
		}
		return ret;
	}
	Operation op;
	vector< unsigned int > short_layers;
	vector< char > short_seps;

	bool operator<(GeneralOp const &o) const {
		if (op.size() < o.op.size()) return true;
		if (op.size() > o.op.size()) return false;
		for (unsigned int i = 0; i < op.size(); ++i) {
			if (op[i] < o.op[i]) return true;
			if (o.op[i] < op[i]) return false;
		}
		return false;
	}
};

class GeneralOpPtrLess {
public:
	bool operator()(const GeneralOp *a, const GeneralOp *b) const {
		assert(a);
		assert(b);
		return *a < *b;
	}
};

const StackOp * StackOp::from_shorthand(std::string shorthand, std::vector< std::string > const &layer_names, std::string *error) {
	vector< uint32_t > layers;
	vector< char > seps;
	{ //split spec into layers and separators:
		std::set< char > valid_seps;
		for (unsigned int i = 0; i < InvalidLayerCharCount; ++i) {
			if (InvalidLayerChars[i] != '*') {
				valid_seps.insert(InvalidLayerChars[i]);
			}
		}
		shorthand += ',';
		string name_acc = "";
		for (unsigned int i = 0; i < shorthand.size(); ++i) {
			if (valid_seps.count(shorthand[i])) {
				if (name_acc == "*") {
					layers.push_back(-1U);
				} else {
					bool found = false;
					for (unsigned int l = 0; l < layer_names.size(); ++l) {
						if (layer_names[l] == name_acc) {
							found = true;
							layers.push_back(l);
							break;
						}
					}
					if (!found) {
						if (error) {
							*error = "unknown layer '" + name_acc + "'";
						}
						return NULL;
					}
				}
				seps.push_back(shorthand[i]);
				name_acc = "";
			} else {
				name_acc += shorthand[i];
			}
		}
		assert(seps.size() && seps.back() == ',');
		seps.pop_back();
	}
	Operation op;
	vector< char > op_seps = seps;
	vector< unsigned int > op_layers = layers;
	while (!seps.empty()) {
		assert(seps.size() + 1 == layers.size());
		vector< char > phase_seps;
		vector< unsigned int > phase_layers;
		{ //grab everything up to the first ';':
			unsigned int s = 0;
			while (s < seps.size() && seps[s] != ';') {
				++s;
			}
			phase_seps = vector< char >(seps.begin(), seps.begin() + s);
			seps.erase(seps.begin(), seps.begin() + s);
			phase_layers = vector< unsigned int >(layers.begin(), layers.begin() + s + 1);
			layers.erase(layers.begin(), layers.begin() + s + 1);
			if (!seps.empty()) {
				assert(seps[0] == ';');
				seps.erase(seps.begin());
			}
		}
		assert(phase_seps.size() + 1 == phase_layers.size());

		//Okay, have a phase in hand.
		vector< bool > used(layer_names.size(), false);
		bool wild = false;
		//Make sure that layers are used once only:
		for (vector< unsigned int >::iterator l = phase_layers.begin(); l != phase_layers.end(); ++l) {
			if (*l == -1U) {
				if (wild) {
					if (error) {
						*error = "Wildcard used twice inside a phase.";
					}
					return NULL;
				}
				wild = true;
				continue;
			}
			assert(*l < used.size());
			if (used[*l]) {
				if (error) {
					*error = "Layer " + layer_names[*l] + " used twice in a phase.";
				}
				return NULL;
			}
			used[*l] = true;
		}
		//Wildcard is always implicitly part of last partition unless you say otherwise.
		if (!wild) {
			phase_seps.push_back(',');
			phase_layers.push_back(-1U);
			wild = true;
		}

		while (!phase_seps.empty()) {
			vector< char > part_seps;
			vector< unsigned int > part_layers;
			{ //grab everything to first '|':
				unsigned int s = 0;
				while (s < phase_seps.size() && phase_seps[s] != '|') {
					++s;
				}
				part_seps = vector< char >(phase_seps.begin(), phase_seps.begin() + s);
				phase_seps.erase(phase_seps.begin(), phase_seps.begin() + s);
				part_layers = vector< unsigned int >(phase_layers.begin(), phase_layers.begin() + s + 1);
				phase_layers.erase(phase_layers.begin(), phase_layers.begin() + s + 1);
				if (!phase_seps.empty()) {
					assert(phase_seps[0] == '|');
					phase_seps.erase(phase_seps.begin());
				}
			}
			assert(part_seps.size() + 1 == part_layers.size());

			vector< bool > part_used(layer_names.size(), false);
			bool part_wild = false;
			for (vector< unsigned int >::iterator l = part_layers.begin(); l != part_layers.end(); ++l) {
				if (*l == -1U) {
					assert(!part_wild);
					part_wild = true;
				} else {
					assert(*l < part_used.size());
					assert(!part_used[*l]);
					part_used[*l] = true;
				}
			}
			char mode = '\0';
			vector< vector< unsigned int > > bins;
			part_seps.push_back(',');
			while (!part_seps.empty()) {
				assert(part_layers.size() == part_seps.size());
				char sep = part_seps[0];
				unsigned int layer = part_layers[0];
				part_seps.erase(part_seps.begin());
				part_layers.erase(part_layers.begin());
				if (sep == ',') {
					if (mode == '\0') {
						//might have had 'foo&bar&baz,'
						bins.clear();
					} else {
						assert(mode == '>' || mode == '<');
						assert(!bins.empty());
						bins.back().push_back(layer);
						Step step;
						if (mode == '<') {
							step.reversed = true;
						}
						step.bins = bins.size();
						step.layers_to_bins.resize(layer_names.size(), AsideBin);
						//Mark everything that appears in this part as destined for the instant bin:
						for (unsigned int l = 0; l < layer_names.size(); ++l) {
							if (part_used[l] || (part_wild && !used[l])) {
								step.layers_to_bins[l] = InstantBin;
							}
						}
						//Rescue anything actually binned:
						for (unsigned int b = 0; b < bins.size(); ++b) {
							for (unsigned int i = 0; i < bins[b].size(); ++i) {
								unsigned int bl = bins[b][i];
								if (bl == -1U) {
									//okay, everything !used goes in this bin.
									for (unsigned int l = 0; l < layer_names.size(); ++l) {
										if (!used[l]) {
											//should have been set earlier:
											assert(step.layers_to_bins[l] == InstantBin);
											step.layers_to_bins[l] = b;
										}
									}
								} else {
									assert(bl < step.layers_to_bins.size());
									step.layers_to_bins[bl] = b;
								}
							}
						}
						op.push_back(step);
						bins.clear();
						mode = '\0';
					} //end of handling ',' after '>' or '<'
				} else if (sep == '&') {
					if (bins.empty()) {
						bins.push_back(vector< unsigned int >());
					}
					bins.back().push_back(layer);
				} else if (sep == '<' || sep == '>') {
					if (mode != '\0' && mode != sep) {
						if (error) {
							*error = "Can't mix > and < in one chain.";
						}
						return NULL;
					}
					if (bins.empty()) {
						bins.push_back(vector< unsigned int >());
					}
					bins.back().push_back(layer);
					bins.push_back(vector< unsigned int >());
					mode = sep;
				} else {
					//No other seps should be allowed to reach this point
					assert(0);
				}
			} //while have part_sep

		} //while have phase_seps
	} //while have seps

	//-------------------------------------------------------------------------
	//Cache ops we create, and track the canonical (== shortest) form for 'em:

	static set< GeneralOp *, GeneralOpPtrLess > existing;
	GeneralOp new_op(op);
	new_op.short_layers = op_layers;
	new_op.short_seps = op_seps;
	set< GeneralOp *, GeneralOpPtrLess >::iterator f = existing.find(&new_op);
	if (f == existing.end()) {
		f = existing.insert(new GeneralOp(new_op)).first;
		std::cerr << "Allocated new GeneralOp: \"" << new_op.description(layer_names) << "\"" << std::endl;
	} else {
		if ((*f)->short_seps.size() > op_seps.size()) {
			std::cerr << "New canonical form for " << (*f)->shorthand(layer_names) << " is ";
			(*f)->short_seps = op_seps;
			(*f)->short_layers = op_layers;
			std::cerr << (*f)->shorthand(layer_names) << std::endl;
		}
	}

	return *f;
}

