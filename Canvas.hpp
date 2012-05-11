#ifndef CANVAS_HPP
#define CANVAS_HPP

#include "Tiled.hpp"
#include "LayerOps.hpp"
#include "StrokeDraw.hpp"
#include "Renderer.hpp"

#include <QtOpenGL>

#include <vector>

class LayerList;
class StrokeList;
class RenderPacket;

class Canvas : public QGLWidget {
	Q_OBJECT
public:
	Canvas(QWidget *parent = 0);
	virtual ~Canvas();
	virtual QSize sizeHint() const;
	virtual QSize minimumSizeHint() const;

protected:
	virtual void initializeGL();
	virtual void paintGL();
	virtual void resizeGL(int width, int height);

	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
	virtual void wheelEvent(QWheelEvent *event);

	virtual void enterEvent(QEvent *event);
	virtual void leaveEvent(QEvent *event);

	virtual void customEvent(QEvent *e);
	void got_packet(RenderPacket * &completed); //deletes completed, sets to NULL.
	void dispatch_packets();

	Vector2f widget_to_image(Vector2f const &) const;
	Vector2f widget_to_image(QPointF const &) const;
	void start_stroke(Vector3f const &at);
	void continue_stroke(Vector3f const &at);
	void end_stroke(Vector3f const &at);


public:

	void set_pix_size(Vector2ui new_size);
	void add_layer(QImage const &from, std::string name = "", const LayerOp *op = LayerOp::over());
	void replace_layer(unsigned int layer, QImage const &from);
	//attempt to rename layer; returns 'false' if new name isn't acceptable.
	bool rename_layer(unsigned int layer, std::string new_name);

	void update_layer_op(unsigned int layer, std::string new_op);

	bool update_stroke_op(unsigned int op, std::string new_op);

public slots:
	void mark_dirty();
	void renderer_changed();
	void finish_renderer_changed();

public:

	void add_stroke(QImage const &from, const StackOp *op);
	void add_stroke(const StackOp *op);

public slots:
	void start_removing_stroke(unsigned int stroke);
	void finish_removing_stroke();
	void start_setting_current_stroke(unsigned int stroke);
	void finish_setting_current_stroke();
public:
	void init_for_stroke(unsigned int stroke);

	std::vector< std::string > layer_names() const;

	Vector2ui pix_size;
	std::vector< Layer * > layers;
	LayerList *layer_list;
	std::vector< Stroke * > strokes;
	StrokeList *stroke_list;

	std::vector< QObject * > ready_renderers;


	ScalarTiled< QGLFramebufferObject * > result_fbs;
	ScalarTiled< GLuint > zero_texs;
	ScalarTiled< GLuint > one_texs;

	//various kinds of dirty-ness for tiles:
	TileSet stroked; //need to be re-composed because they were drawn over.

	//tiles that need to be rendered without/with current stroke:
	TileFlags needs;

	//tiles that are being rendered without/with current stroke:
	TileFlags pending;

	bool flushing_render;
signals:
	void render_flushed();

public:

	static const unsigned int FLAG_ZERO = (1 << RenderPacket::ZERO);
	static const unsigned int FLAG_ONE = (1 << RenderPacket::ONE);
	static const unsigned int FLAG_RESULT = (1 << RenderPacket::RESULT);

	//Done drawing current stroke; start to commit it.
	void start_stroke_commit();
public slots:
	void finish_stroke_commit(); //called after render is flushed.
signals:
	void stroke_committed();
public:

	/*
	std::vector< bool > dirty;
	std::vector< bool > pending;
	GLuint result_tex;
	*/

	Vector3f camera; //camera.z == view height
	//Update camera relative to image size. These *don't* call updateGL!
	void show_all();
	void show_one_to_one();

	//stroke drawing stuff:
	StrokeBrush brush;

signals:
	//signals triggered only by _user_ actions:
	void brush_radius_changed(float);
	void brush_softness_changed(float);
public slots:
	void set_brush_radius(float);
	void set_brush_softness(float);
	void set_brush_flow(float);

public:

	//when user has mouse/stylus over window:
	bool has_brush;
	Vector2f brush_at;

	unsigned int pending_removal_stroke;

	unsigned int pending_stroke;
	unsigned int current_stroke;
	StrokeDraw *current_draw;

	QGLShaderProgram *paint_shader;
};

#endif //CANVAS_HPP
