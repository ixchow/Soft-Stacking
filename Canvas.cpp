#include "Canvas.hpp"

#include "default_bg.hpp"
#include "StrokeDraw.hpp"
#include "LayerOps.hpp"
#include "LayerList.hpp"
#include "StrokeList.hpp"
#include "StackOps.hpp"
#include "gl_errors.hpp"
#include "Renderer.hpp"

#include "GLHacks.hpp"

#include <iostream>
#include <sstream>

using std::cout;
using std::cerr;
using std::endl;

using std::vector;
using std::string;
using std::pair;
using std::make_pair;

Canvas::Canvas(QWidget *parent) : QGLWidget( QGLFormat( /*nothing to request*/ ), parent), pix_size(make_vector(0U,0U)), layer_list(NULL), stroke_list(NULL), flushing_render(false), camera(make_vector(0.0f, 0.0f, 200.0f)), has_brush(false), brush_at(make_vector(0.0f, 0.0f)), pending_removal_stroke(-1U), pending_stroke(-1U), current_stroke(-1U), current_draw(NULL), paint_shader(NULL) {
	set_pix_size(make_vector(TileSize, TileSize));
	setMouseTracking(true);
}

Canvas::~Canvas() {
}

QSize Canvas::sizeHint() const {
	return QSize(800, 800);
}

QSize Canvas::minimumSizeHint() const {
	return QSize(200, 200);
}

void tile_draw_tex_params() {
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Canvas::initializeGL() {
	show_all();

	glEnable(GL_LINE_SMOOTH);

	paint_shader = new QGLShaderProgram(context(), this);
	bool res = paint_shader->addShaderFromSourceCode(QGLShader::Fragment,
	"uniform sampler2D one;\n"
	"uniform sampler2D stroke;\n"
	"uniform sampler2D zero;\n"
	"void main() {\n"
	"	float amt = texture2D(stroke, gl_TexCoord[0].xy).r;\n"
	"	vec4 r0 = texture2D(zero, gl_TexCoord[0].xy);\n"
	"	vec4 r1 = texture2D(one, gl_TexCoord[0].xy);\n"
	"	gl_FragColor = mix(r0, r1, amt);\n"
	"}\n"
	);
	if (!res) {
		cerr << "Error compiling fragment shader:\n" << qPrintable(paint_shader->log()) << endl;
		assert(0);
		exit(1);
	}
	res = paint_shader->link();
	if (!res) {
		cerr << "Error linking shader:\n" << qPrintable(paint_shader->log()) << endl;
		assert(0);
		exit(1);
	}
	paint_shader->bind();
	paint_shader->setUniformValue("zero",0);
	paint_shader->setUniformValue("stroke",1);
	paint_shader->setUniformValue("one",2);
	paint_shader->release();

	gl_errors("initialize");
}

void Canvas::paintGL() {
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	if (width() == 0 || height() == 0) return;

	if (current_draw && current_draw->acc.size() > 1) {
		assert(current_stroke < strokes.size());
		assert(strokes[current_stroke] == current_draw->into);
		current_draw->commit(stroked);
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glScalef(float(height()) / float(width()), 1.0f, 1.0f);
	glScalef(1.0f / camera.z,-1.0f / camera.z, 1.0f);
	glTranslatef(-camera.x, -camera.y, 0.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_LINE_LOOP);
	glColor3f(1.0f, 1.0f, 1.0f);
	glVertex2f( 0.0f, 0.0f);
	glVertex2f( pix_size.x, 0.0f);
	glVertex2f( pix_size.x, pix_size.y);
	glVertex2f( 0.0f, pix_size.y);
	glEnd();


	static GLuint missing_tex = 0;
	if (missing_tex == 0) {
		vector< uint32_t > missing(TileSize * TileSize);
		for (unsigned int i = 0; i < TileSize * TileSize; ++i) {
			Vector2ui at;
			at.x = i % TileSize;
			at.y = i / TileSize;
			at = make_vector((at.x + at.y) % TileSize, (at.y + TileSize - at.x) % TileSize);
			if ((at.x ^ at.y) & 16) {
				missing[i] = 0xff7777dd;
			} else {
				missing[i] = 0xff444488;
			}
		}
		glGenTextures(1, &missing_tex);
		glBindTexture(GL_TEXTURE_2D, missing_tex);
		tile_draw_tex_params();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TileSize, TileSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, &(missing[0]));
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	glDisable(GL_BLEND);

	glActiveTextureScope();

	for (unsigned int y = 0; y * TileSize < pix_size.y; ++y) {
		for (unsigned int x = 0; x * TileSize < pix_size.x; ++x) {
			Vector2ui at = make_vector(x,y);
			Vector2ui pix_at = at * TileSize;
			Vector2ui pix_max = pix_at + make_vector(TileSize, TileSize);
			if (pix_max.x > pix_size.x) pix_max.x = pix_size.x;
			if (pix_max.y > pix_size.y) pix_max.y = pix_size.y;

			Vector2f tex_size = make_vector< float >(pix_max - pix_at);
			tex_size /= float(TileSize);

			glColor3f(1.0f, 1.0f, 1.0f);
			if (stroked.count(at)) {
				//if stroked, then -- by default -- we're painting missing
				// over missing (boring!):
				GLuint zero_tex = missing_tex;
				GLuint one_tex = missing_tex;

				//if we have a result, we'll paint missing over that:
				if (result_fbs.get(at)) {
					zero_tex = result_fbs.get(at)->texture();
				}

				unsigned int flags = 0;
				if (needs.count(at)) {
					flags |= needs[at];
				}
				if (pending.count(at)) {
					flags |= pending[at];
				}
				if (!(flags & FLAG_ZERO)) {
					zero_tex = zero_texs.get(at);
					assert(zero_tex);
				}
				if (!(flags & FLAG_ONE)) {
					one_tex = one_texs.get(at);
					assert(one_tex);
				}
				assert(current_draw->fbs.get(at));

				glActiveTexture(GL_TEXTURE2);
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, one_tex);
				glActiveTexture(GL_TEXTURE1);
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, current_draw->fbs.get(at)->texture());
				glActiveTexture(GL_TEXTURE0);
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, zero_tex);
				paint_shader->bind();

			} else if (result_fbs.get(at)) {
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, result_fbs.get(at)->texture());
				tile_draw_tex_params();
			} else {
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, missing_tex);
			}

			//leftover from multitexture version:
			#define TEX(X, Y) \
				glTexCoord2f( X, Y); \

			glBegin(GL_QUADS);
			TEX(0.0f, 0.0f);
			glVertex2f(pix_at.x, pix_at.y);
			TEX(tex_size.x, 0.0f);
			glVertex2f(pix_max.x, pix_at.y);
			TEX(tex_size.x, tex_size.y);
			glVertex2f(pix_max.x, pix_max.y);
			TEX(0.0f, tex_size.y);
			glVertex2f(pix_at.x, pix_max.y);
			glEnd();

			#undef TEX

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);

			paint_shader->release();
			//Can't do this in linux, unfortunately:
			//glUseProgram(0);

		}
	}
	glDisable(GL_TEXTURE_2D);


	glEnable(GL_BLEND);

	bool show_pending = true;
	if (show_pending) {
		for (TileFlags::iterator p = pending.begin(); p != pending.end(); ++p) {
			glBegin(GL_LINE_LOOP);
			glColor3f(1.0f, 0.0f, 1.0f);
			glVertex2f((p->first.x+0.1f)*TileSize, (p->first.y+0.1f)*TileSize);
			glVertex2f((p->first.x+0.1f)*TileSize, (p->first.y+0.9f)*TileSize);
			glVertex2f((p->first.x+0.9f)*TileSize, (p->first.y+0.9f)*TileSize);
			glVertex2f((p->first.x+0.9f)*TileSize, (p->first.y+0.1f)*TileSize);
			glEnd();
		}
	}

	if (has_brush) {
		if (flushing_render) {
			setCursor(Qt::WaitCursor);
		} else if (!current_draw) {
			setCursor(Qt::ForbiddenCursor);
		} else {
			glLineWidth(2.0f);
			setCursor(Qt::BlankCursor);
			glBegin(GL_LINES);
			float s = camera.z * 0.01f;
			glColor3f(1.0f, 1.0f, 1.0f);
			glVertex2f(brush_at.x - s, brush_at.y);
			glVertex2f(brush_at.x + s, brush_at.y);
			glVertex2f(brush_at.x, brush_at.y - s);
			glVertex2f(brush_at.x, brush_at.y + s);
			glEnd();

			unsigned int steps = 2.0f * M_PI * brush.radius / camera.z * 100.0f;
			if (steps < 16) steps = 16;
			if (steps > 200) steps = 200;
			if (steps % 2 == 0) steps += 1;
			static vector< Vector2f > verts;
			if (verts.size() != steps) {
				verts.resize(steps);
				for (unsigned int i = 0; i < steps; ++i) {
					float ang = i / float(steps-1) * 2.0f * M_PI;
					verts[i] = make_vector(cosf(ang), sinf(ang));
				}
			}
			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(2, GL_FLOAT, sizeof(Vector2f), &(verts[0]));
			glPushMatrix();
			glTranslatef(brush_at.x, brush_at.y, 0.0f);
			glScalef(brush.radius, brush.radius, 1.0f);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			glDrawArrays(GL_LINES, 0, verts.size()-1);
			glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
			glDrawArrays(GL_LINES, 1, verts.size()-1);
			glPopMatrix();

			glPushMatrix();
			glTranslatef(brush_at.x, brush_at.y, 0.0f);
			glScalef(brush.radius * (1.0f - brush.softness), brush.radius * (1.0f - brush.softness), 1.0f);
			glColor4f(1.0f, 1.0f, 1.0f, 0.3f);
			glDrawArrays(GL_LINES, 0, verts.size()-1);
			glColor4f(0.0f, 0.0f, 0.0f, 0.3f);
			glDrawArrays(GL_LINES, 1, verts.size()-1);
			glPopMatrix();

			glDisableClientState(GL_VERTEX_ARRAY);

			glLineWidth(1.0f);

		}
	}

	gl_errors("paint");
}

void Canvas::resizeGL(int width, int height) {
	glViewport(0,0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (width > height) {
		glScalef(float(height) / float(width), 1.0f, 1.0f);
	} else {
		glScalef(1.0f, float(width) / float(height), 1.0f);
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gl_errors("resize");
}

void Canvas::mousePressEvent(QMouseEvent *event) {
	event->accept();
	if (event->button() == Qt::LeftButton) {
		if (current_draw && !flushing_render) {
			current_draw->eraser = (QApplication::keyboardModifiers() & Qt::ShiftModifier);
			start_stroke(make_vector(widget_to_image(event->posF()), 1.0f));
		}
	}
}

void Canvas::mouseMoveEvent(QMouseEvent *event) {
	event->accept();
	if (event->buttons() & Qt::LeftButton) {
		if (current_draw && !flushing_render) {
			continue_stroke(make_vector(widget_to_image(event->posF()), 1.0f));
		}
	}
	brush_at = widget_to_image(event->posF());
	update();
}

void Canvas::mouseReleaseEvent(QMouseEvent *event) {
	event->accept();
	if (event->button() == Qt::LeftButton) {
		if (current_draw && !flushing_render) {
			end_stroke(make_vector(widget_to_image(event->posF()), 1.0f));
		}
	}
}

void Canvas::wheelEvent(QWheelEvent *event) {
	event->accept();
	if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
		brush.softness += 0.5f * event->delta() / (8 * 360.0f);
		if (brush.softness < 0.0f) {
			brush.softness = 0.0f;
		}
		if (brush.softness > 1.0f) {
			brush.softness = 1.0f;
		}
		emit brush_softness_changed(brush.softness);
	} else {
		brush.radius *= powf(0.1f, event->delta() / (8 * 360.0f));
		if (brush.radius < 1.0f) {
			brush.radius = 1.0f;
		}
		if (brush.radius > 500.0f) {
			brush.radius = 500.0f;
		}
		emit brush_radius_changed(brush.radius);
	}
	update();
}

void Canvas::enterEvent(QEvent *event) {
	event->accept();
	has_brush = true;
	update();
}

void Canvas::leaveEvent(QEvent *event) {
	event->accept();
	has_brush = false;
	update();
}

void Canvas::customEvent(QEvent *e) {
	if (e->type() == RendererReadyEventType) {
		RendererReadyEvent *ready = dynamic_cast< RendererReadyEvent * >(e);
		assert(ready);
		if (ready->completed) {
			got_packet(ready->completed);
			assert(ready->completed == NULL);
		}
		assert(ready->renderer);
		ready_renderers.push_back(ready->renderer);
		dispatch_packets();
		if (pending.empty() && needs.empty() && flushing_render) {
			emit render_flushed();
			flushing_render = false;
		}
		e->accept();
	} else {
		QGLWidget::customEvent(e);
	}
}

void Canvas::got_packet(RenderPacket * &pkt) {
	//std::cerr << "Got packet " << pkt << " for " << pkt->at << "/" << pkt->type << std::endl;
	assert(pkt);
	assert(pkt->at.x < result_fbs.size.x);
	assert(pkt->at.y < result_fbs.size.y);
	assert(pkt->out);

	{ //Mark tile as no longer pending:
		TileFlags::iterator p = pending.find(pkt->at);
		assert(p != pending.end());
		assert(p->second & (1 << pkt->type));
		p->second &= ~(1 << pkt->type);
		if (p->second == 0) {
			pending.erase(p);
		}
	}

	makeCurrent();
	if (pkt->type == RenderPacket::ZERO) {
		GLuint &tex = zero_texs.get(pkt->at);
		if (tex == 0) {
			glGenTextures(1, &tex);
			assert(zero_texs.get(pkt->at) == tex);
			glBindTexture(GL_TEXTURE_2D, tex);
		}
		glBindTexture(GL_TEXTURE_2D, tex);
		tile_draw_tex_params();
	} else if (pkt->type == RenderPacket::ONE) {
		GLuint &tex = one_texs.get(pkt->at);
		if (tex == 0) {
			glGenTextures(1, &tex);
			assert(one_texs.get(pkt->at) == tex);
		}
		glBindTexture(GL_TEXTURE_2D, tex);
		tile_draw_tex_params();
	} else if (pkt->type == RenderPacket::RESULT) {
		QGLFramebufferObject *&fb = result_fbs.get(pkt->at);
		if (!fb) {
			fb = new QGLFramebufferObject(TileSize, TileSize, GL_TEXTURE_2D);
			assert(result_fbs.get(pkt->at) == fb); //always suspicious of refs
		}
		assert(fb->isValid());
		glBindTexture(GL_TEXTURE_2D, fb->texture());
		tile_draw_tex_params();
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TileSize, TileSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pkt->out);

	glBindTexture(GL_TEXTURE_2D, 0);

	gl_errors("got_pkt");

	delete pkt;
	pkt = NULL;

	//indicate repaint would be good:
	update();
}

void Canvas::dispatch_packets() {
	
	while (!ready_renderers.empty()) {

		vector< pair< Vector2ui, unsigned int > > possible;
		//prioritize tiles that have been drawn in:
		for (TileSet::const_iterator s = stroked.begin(); s != stroked.end(); ++s) {
			if (needs.count(*s)) {
				if (needs[*s] & FLAG_ZERO) {
					possible.push_back(make_pair(*s, RenderPacket::ZERO));
				} else if (needs[*s] & FLAG_ONE) {
					possible.push_back(make_pair(*s, RenderPacket::ONE));
				} else if (needs[*s] & FLAG_RESULT) {
					possible.push_back(make_pair(*s, RenderPacket::RESULT));
				}
			}
		}
		//if still no priority tiles, just use all tiles:
		if (possible.empty()) {
			for (TileFlags::const_iterator n = needs.begin(); n != needs.end(); ++n) {
				if (n->second & FLAG_ZERO) {
					possible.push_back(make_pair(n->first, RenderPacket::ZERO));
				} else if (n->second & FLAG_ONE) {
					possible.push_back(make_pair(n->first, RenderPacket::ONE));
				} else if (n->second & FLAG_RESULT) {
					possible.push_back(make_pair(n->first, RenderPacket::RESULT));
				} else {
					assert(0);
				}
			}
		}


		if (possible.empty()) return;
		if (!has_brush) {
			swap(possible.back(), possible[rand() % possible.size()]);
		} else {
			unsigned int closest = 0;
			float dis = length_squared(make_vector(possible[0].first.x + 0.5f, possible[0].first.y + 0.5f) * TileSize - brush_at);
			for (unsigned int i = 1; i < possible.size(); ++i) {
				float test = length_squared(make_vector(possible[i].first.x + 0.5f, possible[i].first.y + 0.5f) * TileSize - brush_at);
				if (test < dis) {
					dis = test;
					closest = i;
				}
			}
			swap(possible.back(), possible[closest]);
		}

		RenderPacket *pkt = new RenderPacket();
		{ //assemble tile info into pkt:
			pkt->at = possible.back().first;
			pkt->type = possible.back().second;
			possible.pop_back();


			for (vector< Layer * >::iterator l = layers.begin(); l != layers.end(); ++l) {
				pkt->layers.push_back(make_pair((*l)->op, (*l)->get_tile_or_null(pkt->at)));
			}
			for (vector< Stroke * >::iterator s = strokes.begin(); s != strokes.end(); ++s) {
				if (s - strokes.begin() == current_stroke) {
					//current stroke might be changed (dependin'):
					if (pkt->type == RenderPacket::ZERO) {
						//Skip constraint.
					} else if (pkt->type == RenderPacket::ONE) {
						static uint8_t *ones = NULL;
						if (!ones) {
							ones = new uint8_t[TileSize * TileSize];
							memset(ones, 0xff, TileSize * TileSize);
						}
						//constraint is all ones:
						pkt->strokes.push_back(make_pair((*s)->op, ones));
					} else if (pkt->type == RenderPacket::RESULT) {
						if ((*s)->get_tile_or_null(pkt->at)) {
							pkt->strokes.push_back(make_pair((*s)->op, (*s)->get_tile_or_null(pkt->at)));
						}
					} else {
						assert(0);
					}
				} else {
					//treat non-current stroke normally:
					if ((*s)->get_tile_or_null(pkt->at)) {
						pkt->strokes.push_back(make_pair((*s)->op, (*s)->get_tile_or_null(pkt->at)));
					}
				}
			}

			memcpy(&(pkt->out[0]), default_bg(), sizeof(uint32_t) * TileSize * TileSize);
		}

		{ //twiddle pending and needs correctly:
			//pending gets bit for this packet:
			TileFlags::iterator p = pending.find(pkt->at);
			if (p == pending.end()) {
				p = pending.insert(make_pair(pkt->at, 0)).first;
			}
			assert(!(p->second & (1 << pkt->type)));
			p->second |= (1 << pkt->type);

			//needs gets bit for this packet cleared:
			TileFlags::iterator n = needs.find(pkt->at);
			assert(n != needs.end());
			assert(n->second & (1 << pkt->type));
			n->second &= ~(1 << pkt->type);
			if (n->second == 0) {
				needs.erase(n);
			}
		}

		//Dispatch packet to renderer:
		{
			QObject *renderer = ready_renderers.back();
			ready_renderers.pop_back();
			QCoreApplication::postEvent(renderer, new RendererRequestEvent(pkt));
			//std::cerr << "Dispatching packet " << pkt << " for " << pkt->at << "/" << pkt->type << std::endl;
		}
		
	} //while ( renderers still ready )
}

Vector2f Canvas::widget_to_image(Vector2f const &widget) const {
	Vector2f ret = 2.0f * make_vector((widget.x - 0.5f * width()) / height(), (widget.y - 0.5f * height()) / height());
	return ret * camera.z + camera.xy;
}

Vector2f Canvas::widget_to_image(QPointF const &pt) const {
	return widget_to_image(make_vector< float >(pt.x(), pt.y()));
}

void Canvas::start_stroke(Vector3f const &at) {
	assert(current_stroke < strokes.size());
	assert(current_draw);
	assert(current_draw->into == strokes[current_stroke]);

	makeCurrent();
	current_draw->start(at, stroked);
	update();
}

void Canvas::continue_stroke(Vector3f const &at) {
	assert(current_stroke < strokes.size());
	assert(current_draw);
	assert(current_draw->into == strokes[current_stroke]);

	current_draw->add_point(at);
	update();
}

void Canvas::end_stroke(Vector3f const &at) {
	assert(current_stroke < strokes.size());
	assert(current_draw);
	assert(current_draw->into == strokes[current_stroke]);

	makeCurrent();
	current_draw->add_point(at);
	current_draw->finish(stroked);
	update();
}

void Canvas::set_pix_size(Vector2ui new_size) {
	pix_size = new_size;

	//TODO: There might be some potentially terrible interactions with current_stroke here.

	Vector2ui tile_size = make_vector(0U, 0U);
	if (pix_size.x) {
		tile_size.x = (pix_size.x - 1) / TileSize + 1;
	}
	if (pix_size.y) {
		tile_size.y = (pix_size.y - 1) / TileSize + 1;
	}

	{ //expand our result-carrying bits:
		result_fbs.expand(tile_size, NULL);
		zero_texs.expand(tile_size, 0);
		one_texs.expand(tile_size, 0);
	}

	//Should be equivalent but safer:
	renderer_changed();
	/*
	//Mark everything dirty, I guess.
	for (unsigned int y = 0; y < tile_size.y; ++y) {
		for (unsigned int x = 0; x < tile_size.x; ++x) {
			Vector2ui at = make_vector(x,y);
			TileFlags::iterator n = needs.insert(make_pair(at, 0)).first;
			n->second |= FLAG_RESULT;
		}
	}
	dispatch_packets();
	update();
	*/
}

void Canvas::add_layer(QImage const &from, std::string name, const LayerOp *op) {
	if ((int)pix_size.x < from.width()) {
		pix_size.x = from.width();
	}
	if ((int)pix_size.y < from.height()) {
		pix_size.y = from.height();
	}
	{ //make name unique:
		std::string base = name;
		vector< string > existing = layer_names();
		bool unique = false;
		unsigned int ind = 0;
		while (!unique) {
			if (name != "") {
				unique = true;
				for (vector< string >::const_iterator n = existing.begin(); n != existing.end(); ++n) {
					if (*n == name) {
						unique = false;
						break;
					}
				}
			}
			if (!unique) {
				std::ostringstream new_name;
				new_name << base << ind;
				name = new_name.str();
				++ind;
			}
		}
	}
	layers.push_back(new Layer(name, from, op));
	set_pix_size(pix_size);
	
	show_all();

	update();

	if (layer_list) {
		layer_list->update_list(layers);
	}
}

void Canvas::replace_layer(unsigned int layer, QImage const &from) {
	assert(layer < layers.size());

	if ((int)pix_size.x < from.width()) {
		pix_size.x = from.width();
	}
	if ((int)pix_size.y < from.height()) {
		pix_size.y = from.height();
	}
	Layer *old = layers[layer];
	layers[layer] = new Layer(old->name, from, old->op);
	delete old;
	set_pix_size(pix_size);
	
	show_all();

	update();

	if (layer_list) {
		layer_list->update_list(layers);
	}
}


bool Canvas::rename_layer(unsigned int layer, std::string new_name) {
	if (new_name.empty()) return false;

	for (unsigned int i = 0; i < new_name.size(); ++i) {
		for (unsigned int s = 0; s < InvalidLayerCharCount; ++s) {
			if (new_name[i] == InvalidLayerChars[s]) {
				return false;
			}
		}
	}
	assert(layer < layers.size());
	for (unsigned int l = 0; l < layers.size(); ++l) {
		if (l == layer) continue;
		if (layers[l]->name == new_name) return false;
	}
	layers[layer]->name = new_name;
	stroke_list->update_list(strokes, layer_names());
	return true;
}


void Canvas::update_layer_op(unsigned int layer, std::string new_op) {
	const LayerOp *op = LayerOp::named_op(new_op);
	if (!op) {
		cerr << "Unknown op name '" << new_op << "'." << endl;
		return;
	}
	assert(layer < layers.size());
	layers[layer]->op = op;
	mark_dirty();
}

bool Canvas::update_stroke_op(unsigned int stroke, std::string new_op) {
	const StackOp *op = StackOp::from_shorthand(new_op, layer_names());
	if (!op) {
		cerr << "Unknown stack op '" << new_op << "'." << endl;
		return false;
	}
	assert(stroke < strokes.size());
	strokes[stroke]->op = op;
	mark_dirty();
	return true;
}



void Canvas::mark_dirty() {
	if (!pending.empty()) {
		cerr << "**** Warning, marking dirty while things are pending; this is likely to cause a crash ****" << endl;
	}
	//Mark everything as needing to be recalculated.
	for (unsigned int y = 0; y < result_fbs.size.y; ++y) {
		for (unsigned int x = 0; x < result_fbs.size.x; ++x) {
			Vector2ui at = make_vector(x,y);
			TileFlags::iterator n = needs.insert(make_pair(at, 0)).first;
			if (current_stroke != -1U) {
				n->second |= FLAG_ZERO;
				n->second |= FLAG_ONE;
			}
			if (!stroked.count(at)) {
				n->second |= FLAG_RESULT;
			}
		}
	}
	dispatch_packets();
}

void Canvas::renderer_changed() {
	needs.clear();
	flushing_render = true;
	connect(this, SIGNAL(render_flushed()), this, SLOT(finish_renderer_changed()));
	dispatch_packets(); //just in case?
	if (pending.empty()) {
		emit render_flushed();
	}
}

void Canvas::finish_renderer_changed() {
	bool result = disconnect(this, SIGNAL(render_flushed()), this, SLOT(finish_renderer_changed()));
	if (!result) {
		cerr << "Strange, got finish_renderer_changed without being connected to flushed signal." << endl;
	}
	mark_dirty();
}

void Canvas::add_stroke(QImage const &from, const StackOp *op) {
	if ((int)pix_size.x < from.width()) {
		pix_size.x = from.width();
	}
	if ((int)pix_size.y < from.height()) {
		pix_size.y = from.height();
	}
	strokes.push_back(new Stroke(from, op));
	set_pix_size(pix_size); //dirties everything
	
	show_all();

	update();

	if (stroke_list) {
		stroke_list->update_list(strokes, layer_names());
	}
}

void Canvas::add_stroke(const StackOp *op) {
	strokes.push_back(new Stroke(pix_size, op));
	if (stroke_list) {
		stroke_list->update_list(strokes, layer_names());
	}

	start_setting_current_stroke(strokes.size()-1);
	
	show_all();

	update();

}

void Canvas::start_removing_stroke(unsigned int stroke) {
	if (pending_removal_stroke != -1U) {
		cerr << "WARNING: ignoring a stroke removal request while waiting to remove another stroke." << endl;
		return;
	}
	pending_removal_stroke = stroke;
	//if we try to remove a stroke while setting it to current, we can 
	// adjust this to changing to no current stroke:
	if (stroke == pending_stroke) {
		pending_stroke = -1U;
	}
	//if the stroke *is* the current, however, we need to commit it:
	if (stroke == current_stroke) {
		connect(this, SIGNAL(stroke_committed()), this, SLOT(finish_removing_stroke()));
		start_stroke_commit();
		return;
	} else {
		//we can directly proceed to finishing stroke removal:
		connect(this, SIGNAL(stroke_committed()), this, SLOT(finish_removing_stroke()));
		finish_removing_stroke();
	}
}

void Canvas::finish_removing_stroke() {
	disconnect(this, SIGNAL(stroke_committed()), this, SLOT(finish_removing_stroke()));

	assert(pending_stroke == -1U);
	assert(current_stroke == -1U);

	assert(pending_removal_stroke < strokes.size());

	Stroke *stroke = strokes[pending_removal_stroke];
	strokes.erase(strokes.begin() + pending_removal_stroke);
	delete stroke;

	pending_removal_stroke = -1U;

	if (stroke_list) {
		stroke_list->update_list(strokes, layer_names());
	}

	mark_dirty();

	show_all();

	update();
}

void Canvas::start_setting_current_stroke(unsigned int stroke) {
	stroke_list->set_active_stroke(stroke, true);

	//if we're already setting the current stroke, just update the transition:
	if (pending_stroke != -1U) {
		pending_stroke = stroke;
		return;
	}

	pending_stroke = stroke;

	if (current_stroke == -1U) {
		if (!needs.empty() || !pending.empty()) {
			flushing_render = true;
			connect(this, SIGNAL(render_flushed()), this, SLOT(finish_setting_current_stroke()));
			dispatch_packets(); //shouldn't need to call this here.
		} else {
			connect(this, SIGNAL(stroke_committed()), this, SLOT(finish_setting_current_stroke()));
			finish_setting_current_stroke();
		}
		return;
	}

	connect(this, SIGNAL(stroke_committed()), this, SLOT(finish_setting_current_stroke()));
	start_stroke_commit();

}

void Canvas::finish_setting_current_stroke() {
	{ //disconnect from render_flushed:
		bool result = disconnect(this, SIGNAL(stroke_committed()), this, SLOT(finish_setting_current_stroke()));
		if (!result) {
			result = disconnect(this, SIGNAL(render_flushed()), this, SLOT(finish_setting_current_stroke()));
			if (!result) {
				cerr << "WARNING: finish_setting_current_stroke not connected but called; weird." << endl;
			}
		}
	}

	//Make sure we're done rendering & don't have a current stroke:
	assert(current_stroke == -1U);
	assert(current_draw == NULL);
	assert(needs.empty());
	assert(pending.empty());
	assert(stroked.empty());

	//And move pending stroke over to current stroke:
	current_stroke = pending_stroke;
	pending_stroke = -1U;

	//inform stroke list:
	stroke_list->set_active_stroke(current_stroke, false);

	if (current_stroke < strokes.size()) {
		makeCurrent();
		current_draw = new StrokeDraw(&brush, strokes[current_stroke]);

		//set needs, and kick off rendering:
		for (unsigned int y = 0; y < result_fbs.size.y; ++y) {
			for (unsigned int x = 0; x < result_fbs.size.x; ++x) {
				Vector2ui at = make_vector(x,y);
				TileFlags::iterator n = needs.insert(make_pair(at, FLAG_ONE)).first;
				if (strokes[current_stroke]->get_tile_or_null(at)) {
					//anywhere the stroke is, can't use result.
					n->second |= FLAG_ZERO;
				} else {
					//TODO: if stroke isn't there, can copy result to zero.
					n->second |= FLAG_ZERO;
				}
			}
		}
		dispatch_packets();
	
	}
}

void Canvas::start_stroke_commit() {
	makeCurrent();

	assert(!flushing_render);
	assert(current_stroke < strokes.size());
	assert(current_draw);
	assert(current_draw->into == strokes[current_stroke]);

	if (!current_draw->acc.empty()) {
		current_draw->finish(stroked);
	}

	//We don't need to render stuff that wasn't stroked:
	for (TileFlags::const_iterator t = needs.begin(); t != needs.end(); /* later */) {
		if (!stroked.count(t->first)) t = needs.erase(t);
		else ++t;
	}

	//TODO: could go through here and find stuff we were going to request zero/one and
	//change to requesting 'RESULT', while at the same time removing from 'stroked'
	//(might save a bit of time)

	//Don't need to dispatch_packets, because we haven't increased the number of tiles needed.

	flushing_render = true;
	connect(this, SIGNAL(render_flushed()), this, SLOT(finish_stroke_commit()));

	//just in case we're already done:
	if (needs.empty() && pending.empty()) {
		emit render_flushed();
		flushing_render = false;
	}
}

void Canvas::finish_stroke_commit() {
	{ //disconnect from render_flushed:
		bool result = disconnect(this, SIGNAL(render_flushed()), this, SLOT(finish_stroke_commit()));
		if (!result) {
			cerr << "WARNING: finish_stroke_commit not connected but called; weird." << endl;
		}
	}
	assert(needs.empty());
	assert(pending.empty());

	makeCurrent();

	glPushAttrib(GL_VIEWPORT_BIT);
	glViewport(0,0,TileSize,TileSize);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_BLEND);

	glActiveTextureScope();

	for (TileSet::iterator s = stroked.begin(); s != stroked.end(); ++s) {

		QGLFramebufferObject *&fb = result_fbs.get(*s);
		if (!fb) {
			fb = new QGLFramebufferObject(TileSize, TileSize, GL_TEXTURE_2D);
			assert(result_fbs.get(*s) == fb); //always suspicious of refs
		}
		assert(fb->isValid());
		fb->bind();

		GLuint zero_tex = zero_texs.get(*s);
		GLuint one_tex = one_texs.get(*s);
		assert(zero_tex);
		assert(one_tex);
		assert(current_draw->fbs.get(*s));
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, one_tex);
		glActiveTexture(GL_TEXTURE1);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, current_draw->fbs.get(*s)->texture());
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, zero_tex);

		paint_shader->bind();

		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f,-1.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f,-1.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f, 1.0f);
		glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, 1.0f);
		glEnd();

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);

		paint_shader->release();

		fb->release();

	}

	glPopAttrib();

	gl_errors("composite in stroke commit");

	stroked.clear();

	delete current_draw;
	current_draw = NULL;
	current_stroke = -1U;

	emit stroke_committed();
}

std::vector< std::string > Canvas::layer_names() const {
	vector< string > ret;
	for (vector< Layer * >::const_iterator l = layers.begin(); l != layers.end(); ++l) {
		ret.push_back((*l)->name);
	}
	return ret;
}

void Canvas::show_all() {
	camera.x = pix_size.x * 0.5f;
	camera.y = pix_size.y * 0.5f;
	camera.z = 10.0f;
	//0.55f -> 10% margin
	if (camera.z < 0.55f * pix_size.y) {
		camera.z = 0.55f * pix_size.y;
	}
	if (width() && height() && camera.z * width() / float(height()) < 0.55f * pix_size.x) {
		camera.z = 0.55f * pix_size.x * height() / float(width());
	}
	update();
}

void Canvas::show_one_to_one() {
	camera.z = height() / 2.0f;
}

void Canvas::set_brush_radius(float rad) {
	if (rad < 1.0f) rad = 1.0f;
	if (rad > 500.0f) rad = 500.0f;
	brush.radius = rad;
	if (!has_brush) {
		has_brush = true;
		brush_at = camera.xy;
	}
	update();
}

void Canvas::set_brush_softness(float softness) {
	if (softness < 0.0f) softness = 0.0f;
	if (softness > 1.0f) softness = 1.0f;
	brush.softness = softness;
	if (!has_brush) {
		has_brush = true;
		brush_at = camera.xy;
	}
	update();
}

void Canvas::set_brush_flow(float flow) {
	if (flow < 0.0f) flow = 0.0f;
	if (flow > 1.0f) flow = 1.0f;
	brush.flow = flow;
}
