#include "StrokeDraw.hpp"
#include "gl_errors.hpp"
#include "GLHacks.hpp"

using std::vector;

StrokeBrush::StrokeBrush(float _softness, float _radius) : rate(5.0f), flow(1.0f), radius(_radius), softness(_softness), tex(0), tex_softness(-1.0f) {

}


GLuint StrokeBrush::get_tex() {
	if (tex == 0 || tex_softness != softness) {
		if (tex == 0) {
			glGenTextures(1, &tex);
		}
		tex_softness = softness;
		
		vector< float > brush(BrushSize * BrushSize);

		for (unsigned int y = 0; y < BrushSize; ++y) {
			for (unsigned int x = 0; x < BrushSize; ++x) {
				Vector2f p;
				p.x = 2.0f * ((x + 0.5f) / BrushSize - 0.5f);
				p.y = 2.0f * ((y + 0.5f) / BrushSize - 0.5f);

				float rad = length(p);

				float &out = brush[y * BrushSize + x];
				if (rad >= 1.0f) {
					out = 0.0f;
				} else if (rad >= 1.0f - softness) {
					out = (1.0f - rad) / softness;
				} else {
					out = 1.0f;
				}
			}
		}

		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		float col[4] = {1.0f, 1.0f, 1.0f, 0.0f};
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, col);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, BrushSize, BrushSize, 0, GL_ALPHA, GL_FLOAT, &brush[0]);
		glBindTexture(GL_TEXTURE_2D, 0);

		gl_errors("build brush texture");
	}
	return tex;
}

StrokeDraw::StrokeDraw(StrokeBrush *_brush, Stroke *_into) : along(0.0f), brush(_brush), into(_into), eraser(false) {
	assert(brush);
	assert(into);
	fbs.expand(into->size, NULL);
}

StrokeDraw::~StrokeDraw() {
	while (fbs.tiles.size()) {
		if (fbs.tiles.back()) {
			delete fbs.tiles.back();
			fbs.tiles.back() = NULL;
		}
		fbs.tiles.pop_back();
	}
}

void StrokeDraw::start(Vector3f const &at, TileSet &changed) {
	if (!acc.empty()) {
		std::cerr << "Double-starting a stroke?" << std::endl;
		acc.clear();
	}
	add_point(at);
	along = -1.0f;
	commit(changed);
}

void StrokeDraw::add_point(Vector3f const &pt) {
	acc.push_back(make_vector(pt.xy, pt.z * brush->radius, brush->flow));
}

void StrokeDraw::commit(TileSet &changed) {
	if (acc.empty()) return;

	vector< Vector4f > splats;
	if (along < 0.0f) {
		//special flag for "draw the first point"
		splats.push_back(acc[0]);
		along = 0.0f;
	}
	for (unsigned int p = 0; p + 1 < acc.size(); ++p) {
		Vector4f a = acc[p];
		Vector4f b = acc[p+1];
		//draw splat whenever along > radius / rate
		float a_thresh = a.z / brush->rate;
		if (a_thresh < 0.5f) a_thresh = 0.5f; //at least a half-pixel between splats.
		float b_thresh = b.z / brush->rate;
		if (b_thresh < 0.5f) b_thresh = 0.5f;
		//So thresh is changing from a_thresh to b_thresh.
		//while along is increasing from (along + 0) to (along + d).
		float d = length(b.xy - a.xy);
		
		float dslack = (b_thresh - a_thresh) - d;

		float t = 0.0f;
		while (t < 1.0f) {
			float slack = ((b_thresh - a_thresh) * t + a_thresh) - along;
			if (slack < 0) {
				//don't move forward, just emit:
				along = 0.0f;
				splats.push_back((b - a) * t + a);
			} else if (dslack * (1.0f - t) < -slack) {
				float delta =-slack / dslack;
				t += delta;
				along = 0.0f;
				splats.push_back((b - a) * t + a);
			} else {
				along += (1.0f - t) * d;
				break;
			}
		}
	}

	//Clear out points in the stroke accumulator we've used:
	// (last point is saved in case we need to continue)
	acc.erase(acc.begin(), acc.end()-1);

	if (splats.empty()) return;
	if (into->size.x == 0 || into->size.y == 0) return;

	//Calculate verts for stroke:
	vector< Vector2f > quads(splats.size()*4);
	vector< Vector2f > texcoords(splats.size()*4);
	vector< Vector4f > colors(splats.size()*4);

	Vector3f col = make_vector(1.0f, 1.0f, 1.0f);
	if (eraser) {
		col = make_vector(0.0f, 0.0f, 0.0f);
	}
	//Remember where this stroke lands so we can draw in those tiles:
	Vector2f min_loc = splats[0].xy;
	Vector2f max_loc = splats[0].xy;
	for (vector< Vector4f >::iterator s = splats.begin(); s != splats.end(); ++s) {
		min_loc = min(min_loc, s->xy - make_vector(s->z, s->z));
		max_loc = max(max_loc, s->xy + make_vector(s->z, s->z));
		unsigned int ind = s - splats.begin();
		quads[4*ind+0] = make_vector(s->x - s->z, s->y - s->z);
		quads[4*ind+1] = make_vector(s->x + s->z, s->y - s->z);
		quads[4*ind+2] = make_vector(s->x + s->z, s->y + s->z);
		quads[4*ind+3] = make_vector(s->x - s->z, s->y + s->z);

		texcoords[4*ind+0] = make_vector(0.0f, 0.0f);
		texcoords[4*ind+1] = make_vector(1.0f, 0.0f);
		texcoords[4*ind+2] = make_vector(1.0f, 1.0f);
		texcoords[4*ind+3] = make_vector(0.0f, 1.0f);

		colors[4*ind+0] = make_vector(col, s->w);
		colors[4*ind+1] = make_vector(col, s->w);
		colors[4*ind+2] = make_vector(col, s->w);
		colors[4*ind+3] = make_vector(col, s->w);
	}
	Vector2i min_tile = make_vector< int >((int)min_loc.x / (int)TileSize, (int)min_loc.y / (int)TileSize);
	Vector2i max_tile = make_vector< int >((int)(max_loc.x + 1.0f) / (int)TileSize, (int)(max_loc.y + 1.0f) / (int)TileSize);

	if (min_tile.x < 0) min_tile.x = 0;
	if (max_tile.x > (int)into->size.x - 1) max_tile.x = into->size.x - 1;
	if (min_tile.y < 0) min_tile.y = 0;
	if (max_tile.y > (int)into->size.y - 1) max_tile.y = into->size.y - 1;

	if (max_tile.x < 0) return;
	if (max_tile.y < 0) return;
	if (min_tile.x >= (int)into->size.x) return;
	if (min_tile.y >= (int)into->size.y) return;

	assert(min_tile.x <= max_tile.x);
	assert(min_tile.y <= max_tile.y);

	Vector2ui t;

	glPushAttrib(GL_VIEWPORT_BIT);
	glViewport(0,0,TileSize,TileSize);
	
	glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(2.0f / TileSize, 2.0f / TileSize, 1.0f);
	glTranslatef(-0.5f * TileSize, -0.5f * TileSize, 0.0f);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GLuint brush_tex = brush->get_tex();
	glBindTexture(GL_TEXTURE_2D, brush_tex);
	glEnable(GL_TEXTURE_2D);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(2, GL_FLOAT, sizeof(Vector2f), &quads[0]);
	glTexCoordPointer(2, GL_FLOAT, sizeof(Vector2f), &texcoords[0]);
	glColorPointer(4, GL_FLOAT, sizeof(Vector4f), &colors[0]);
	for (t.y = (unsigned)min_tile.y; t.y <= (unsigned)max_tile.y; ++t.y) {
		for (t.x = (unsigned)min_tile.x; t.x <= (unsigned)max_tile.x; ++t.x) {
			assert(t.y < into->size.y);
			assert(t.x < into->size.x);
			QGLFramebufferObject * &fb = fbs.get(t);
			if (fb == NULL) {
				fb = new QGLFramebufferObject(TileSize, TileSize, GL_TEXTURE_2D);
				assert(fbs.get(t) == fb);

				//if there was data in the stroke here, preserve it:
				if (into->get_tile_or_null(t)) {
					glBindTexture(GL_TEXTURE_2D, fb->texture());
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TileSize, TileSize, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, into->get_tile(t));
					glBindTexture(GL_TEXTURE_2D, 0);
					gl_errors("stroke texture upload");
					assert(fb->isValid());
					fb->bind();
				} else {
					assert(fb->isValid());
					fb->bind();
					glClear(GL_COLOR_BUFFER_BIT);
				}

				//making a new QGLFramebuffer trashes texture bind state.
				glBindTexture(GL_TEXTURE_2D, brush_tex);
			} else {
				assert(fb->isValid());
				fb->bind();
			}

			glPushMatrix();
			glTranslatef(-1.0f * TileSize * t.x, -1.0f * TileSize * t.y, 0.0f);

			glDrawArrays(GL_QUADS, 0, quads.size());

			glPopMatrix();

			glReadPixels(0, 0, TileSize, TileSize, GL_RED, GL_UNSIGNED_BYTE, into->get_tile(t));

			//mark tile dirty:
			changed.insert(t);

			fb->release();
		}
	}
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	glPopClientAttrib();
	
	glPopAttrib();
	
	gl_errors("stroke::commit()");
}

void StrokeDraw::finish(TileSet &changed) {
	if (acc.empty()) {
		std::cerr << "Finishing a not-started stroke?" << std::endl;
		return;
	}
	commit(changed);
	along = -1.0f;
	commit(changed);
	acc.clear();
}
