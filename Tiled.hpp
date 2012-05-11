#ifndef TILED_HPP
#define TILED_HPP

#include <Vector/Vector.hpp>

#include <QImage>

#include <vector>
#ifdef WIN32
#include <unordered_set>
#include <unordered_map>
typedef std::unordered_set< Vector2ui, HashVector< unsigned int, 2 > > TileSet;
typedef std::unordered_map< Vector2ui, uint32_t, HashVector< unsigned int, 2 > > TileFlags;
#else
#include <tr1/unordered_set>
#include <tr1/unordered_map>
typedef std::tr1::unordered_set< Vector2ui, HashVector< unsigned int, 2 > > TileSet;
typedef std::tr1::unordered_map< Vector2ui, uint32_t, HashVector< unsigned int, 2 > > TileFlags;
#endif

#include "Constants.hpp"


template< typename TYPE >
class ScalarTiled {
public:
	ScalarTiled(Vector2ui _size = make_vector(0U,0U), TYPE const &t = TYPE()) : size(_size), tiles(size.x * size.y, t) {
	}
	void expand(Vector2ui new_size, TYPE const &t = TYPE()) {
		if (new_size.x < size.x) new_size.x = size.x;
		if (new_size.y < size.y) new_size.y = size.y;
		if (new_size == size) return;
		tiles.resize(new_size.x * new_size.y, t);
		for (unsigned int y = new_size.y - 1; y < new_size.y; --y) {
			for (unsigned int x = new_size.x - 1; x < new_size.x; --x) {
				assert(y * size.x + x <= y * new_size.x + x); //make sure we aren't going to want something we already overwrote.
				if (x < size.x && y < size.y) {
					tiles[y * new_size.x + x] = tiles[y * size.x + x];
				} else {
					tiles[y * new_size.x + x] = t;
				}
			}
		}
		size = new_size;
	}
	TYPE &get(Vector2ui t) {
		assert(t.x < size.x && t.y < size.y);
		return tiles[t.y * size.x + t.x];
	}
	TYPE &get(unsigned int x, unsigned int y) {
		return get(make_vector(x,y));
	}
	Vector2ui size;
	std::vector< TYPE > tiles;
};

template< typename PIX >
class Tiled {
public:
	Tiled(Vector2ui pix_size = make_vector(0U, 0U), PIX *source = NULL) : size(make_vector(0U,0U)) {
		set(pix_size, source);
	}
	~Tiled() {
		clear();
	}
	Tiled< PIX > &operator=(Tiled< PIX > const &o); //not defined
	void set(Vector2ui pix_size, PIX *source = NULL) {
		clear();
		size.x = (pix_size.x + TileSize - 1) / TileSize;
		size.y = (pix_size.y + TileSize - 1) / TileSize;
		tiles.resize(size.x * size.y, NULL);
		if (source) {
			for (unsigned int y = 0; y < pix_size.y; ++y) {
				for (unsigned int x = 0; x < pix_size.x; ++x) {
					PIX p = source[y * pix_size.x + x];
					if (p) {
						PIX *tile = get_tile(x / TileSize, y / TileSize);
						tile[(y % TileSize) * TileSize + (x % TileSize)] = p;
					}
				}
			}
		}
	}
	void clear() {
		for (typename std::vector< PIX * >::iterator t = tiles.begin();
		t != tiles.end();
		++t) {
			if (*t) {
				delete[] *t;
				*t = NULL;
			}
		}
		tiles.clear();
	}
	void expand(Vector2ui pix_size) {
		Vector2ui new_size;
		new_size.x = (pix_size.x + TileSize - 1) / TileSize;
		new_size.y = (pix_size.y + TileSize - 1) / TileSize;
		if (new_size.x < size.x) new_size.x = size.x;
		if (new_size.y < size.y) new_size.y = size.y;
		if (new_size == size) return;
		tiles.resize(new_size.x * new_size.y, NULL);
		for (unsigned int y = new_size.y - 1; y < new_size.y; --y) {
			for (unsigned int x = new_size.x - 1; x < new_size.x; --x) {
				assert(y * size.x + x <= y * new_size.x + x); //make sure we aren't going to want something we already overwrote.
				if (x < size.x && y < size.y) {
					tiles[y * new_size.x + x] = tiles[y * size.x + x];
				} else {
					tiles[y * new_size.x + x] = NULL;
				}
			}
		}
		size = new_size;
	}
	PIX *get_tile_or_null(Vector2ui t) {
		if (t.x >= size.x) return NULL;
		if (t.y >= size.y) return NULL;
		return tiles[t.y * size.x + t.x];
	}
	PIX *get_tile(Vector2ui t) {
		assert(t.x < size.x && t.y < size.y);
		PIX **ret = &(tiles[t.y * size.x + t.x]);
		if (!*ret) {
			*ret = new PIX[TileSize * TileSize];
			memset(*ret, 0, sizeof(PIX) * TileSize * TileSize);
		}
		return *ret;
	}
	PIX *get_tile(unsigned int x, unsigned int y) {
		return get_tile(make_vector(x,y));
	}
	Vector2ui size; //tiles x tiles
	std::vector< PIX * > tiles; //tile storage, 0 == "fully transparent"
};

class LayerOp;

class Layer : public Tiled< uint32_t > {
public:
	Layer(std::string const &name, QImage const &from, const LayerOp *op); //load image contents
	Layer(std::string const &name, Vector2ui pix_size, const LayerOp *op); //blank layer
	Layer &operator=(Layer const &o); //not defined yet.
	std::string name;
	QImage thumbnail;
	const LayerOp *op;
};

class StackOp;

class Stroke : public Tiled< uint8_t > {
public:
	Stroke(QImage const &from, const StackOp *op);
	Stroke(Vector2ui pix_size, const StackOp *op);
	Stroke &operator=(Stroke const &o); //not defined yet.
	const StackOp *op;
};

#endif //TILED_HPP
