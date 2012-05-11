#include "coefs.hpp"

#include <limits>
#include <utility>
#include <iostream>
#include <cmath>

using std::make_pair;
using std::swap;
using std::vector;
using std::cout;
using std::endl;

//TwoSet == pair of elements, with first element less than second.
typedef uint32_t TwoSetIndex;
typedef std::pair< LayerIndex, LayerIndex > TwoSet;

TwoSet to_twoset(TwoSetIndex index) {
	TwoSet ret = make_pair(0,1);
	while (index >= ret.second) {
		index -= ret.second;
		ret.second += 1;
	}
	ret.first = index;
	return ret;
}

TwoSetIndex to_twoset_index(TwoSet const &in) {
	assert(in.first < in.second);
	return in.second * (in.second - 1) / 2 + in.first;
}

TwoSetIndex count_twosets(unsigned int layers) {
	return to_twoset_index(make_pair(0,layers));
}


StackingIndex to_stacking_index(Stacking const &in) {
	vector< unsigned int > num(in.size()); //what is at a given location
	vector< unsigned int > pos(in.size()); //where is a given number
	for (unsigned int i = 0; i < in.size(); ++i) {
		num[i] = i;
		pos[i] = i;
	}
	StackingIndex ret = 0;
	StackingIndex fac = 1;
	for (unsigned int i = 0; i < in.size(); ++i) {
		assert(num[pos[in[i]]] == in[i]);
		unsigned int s = pos[in[i]]; //will swap with where in[i] is at the moment.

		assert(s >= i);
		ret += fac * (s - i);
		fac *= (in.size() - i);
		if (s != i) {
			swap(num[s], num[i]);
			pos[num[s]] = s;
			pos[num[i]] = i;
		}
	}
	return ret;
}

Stacking to_stacking(StackingIndex index, unsigned int layers) {
	Stacking stacking(layers);
	for (unsigned int i = 0; i < layers; ++i) {
		stacking[i] = i;
	}
	for (unsigned int i = 0; i < layers; ++i) {
		swap(stacking[i], stacking[i + (index % (layers - i))]);
		index /= (layers - i);
	}
	return stacking;
}

size_t count_stackings(unsigned int layers) {
	static vector< StackingIndex > factorials(2,1);
	while (factorials.size() <= layers) {
		factorials.push_back(factorials.back() * factorials.size());
	}
	return factorials[layers];
}

DeltaIndex to_delta_index(Delta const &in) {
	assert(in.pos < in.base.size() && in.pos + 1 < in.base.size());
	assert(in.base[in.pos] < in.base[in.pos + 1]);
	Stacking temp = in.base;
	temp.erase(temp.begin() + in.pos, temp.begin() + in.pos + 2);
	for (unsigned int i = 0; i < temp.size(); ++i) {
		if (temp[i] > in.base[in.pos + 1]) temp[i] -= 1;
		if (temp[i] > in.base[in.pos]) temp[i] -= 1;
	}
	return to_twoset_index(make_pair(in.base[in.pos], in.base[in.pos+1])) + count_twosets(in.base.size()) * (in.pos + (in.base.size()-1) * to_stacking_index(temp));
}

Delta to_delta(DeltaIndex index, unsigned int layers) {
	assert(layers >= 2);
	TwoSet elts = to_twoset(index % count_twosets(layers));
	index /= count_twosets(layers);
	unsigned int pos = index % (layers - 1);
	index /= (layers - 1);
	Stacking temp = to_stacking(index, layers-2);
	assert(elts.first < elts.second);
	for (unsigned int i = 0; i < temp.size(); ++i) {
		if (temp[i] >= elts.first) temp[i] += 1;
		if (temp[i] >= elts.second) temp[i] += 1;
	}
	assert(sizeof(TwoSet) == 2 * sizeof(LayerIndex));
	temp.insert(temp.begin() + pos, &elts.first, &elts.first + 2);
	return Delta(temp, pos);
}

size_t count_deltas(unsigned int layers) {
	return count_stackings(layers) * (layers - 1) / 2;
}
