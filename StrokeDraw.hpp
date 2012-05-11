#ifndef STROKE_DRAW_HPP
#define STROKE_DRAW_HPP

#include "Tiled.hpp"

#include <QtOpenGL>

#include <vector>

class StrokeBrush {
public:
	StrokeBrush(float softness = 0.5f, float radius = 10.0f);
	float rate; //stamps / radius
	float flow; //paint / stamp
	float radius; //radius, in pixels

	//These require re-rendering tex:
	float softness;
	GLuint get_tex();
private:
	GLuint tex; //GL_TEXTURE_2D texture for brush; updated, if needed, when you call 'get_tex()'. NOTE: always use in same GL context!
	float tex_softness;
};

//NOTE: call StrokeDraw functions with a consistent current GL context!
class StrokeDraw {
public:
	StrokeDraw(StrokeBrush *brush, Stroke *into);
	~StrokeDraw();

	void start(Vector3f const &first_point, TileSet &changed);

	void add_point(Vector3f const &); //actually, it's okay to call 'add_point' without a context.


	void commit(TileSet &changed); //draw accumulated points so far.

	void finish(TileSet &changed); //finish off acculated points

	std::vector< Vector4f > acc; //z,y,radius,opacity
	float along;

	ScalarTiled< QGLFramebufferObject * > fbs;

	StrokeBrush *brush;
	Stroke *into;
	bool eraser;
};

#endif //STROKE_DRAW_HPP
