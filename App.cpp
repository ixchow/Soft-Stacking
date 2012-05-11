#include "App.hpp"
#include "Canvas.hpp"
#include "LayerList.hpp"
#include "StrokeList.hpp"
#include "BrushParams.hpp"
#include "StackSelect.hpp"
#include "Misc.hpp"

#include "default_bg.hpp"
#include "update_tile_trimmed.hpp"
#include "update_tile_full.hpp"

#include "LayerOps.hpp"
#include "StackOps.hpp"
#include "Renderer.hpp"

#include <sstream>
#include <vector>
#include <iostream>

using std::vector;
using std::pair;
using std::make_pair;
using std::string;

typedef void (*RenderFn)(std::vector< std::pair< const LayerOp *, const uint32_t * > > const &, std::vector< std::pair< const StackOp *, const uint8_t * > > const &, uint32_t *);

App::App() {
	QAction *add_layer_action = new QAction(tr("&Load Layer"), this);
	add_layer_action->setIcon(QIcon("open.png"));
	connect(add_layer_action, SIGNAL(triggered()), this, SLOT(open()));

	QAction *add_stroke_action = new QAction(tr("Add &Constraint"), this);
	add_stroke_action->setIcon(QIcon("add.png"));
	connect(add_stroke_action, SIGNAL(triggered()), this, SLOT(add_stroke()));

	QAction *open_stroke_action = new QAction(tr("Load Constraint"), this);
	open_stroke_action->setIcon(QIcon("open.png"));
	connect(open_stroke_action, SIGNAL(triggered()), this, SLOT(open_stroke()));


	//menuBar()->setNativeMenuBar(false);
	//menuBar()->addAction(add_layer_action);

	canvas = new Canvas(this);
	setCentralWidget(canvas);

	layer_list = new LayerList(canvas, this);
	layer_list->add->setDefaultAction(add_layer_action);

	stroke_list = new StrokeList(canvas, this);
	stroke_list->add->setDefaultAction(add_stroke_action);
	stroke_list->open->setDefaultAction(open_stroke_action);

	BrushParams *brush_params = new BrushParams(this);

	stack_select = new StackSelect(canvas, NULL);

	canvas->layer_list = layer_list;
	canvas->stroke_list = stroke_list;

	connect(layer_list, SIGNAL(reload_layer(unsigned int)), this, SLOT(reopen(unsigned int)));

	connect(stroke_list, SIGNAL(stroke_selected(unsigned int)), canvas, SLOT(start_setting_current_stroke(unsigned int)));
	connect(stroke_list, SIGNAL(delete_stroke(unsigned int)), canvas, SLOT(start_removing_stroke(unsigned int)));
	connect(stroke_list, SIGNAL(save_stroke(unsigned int)), this, SLOT(save_stroke(unsigned int)));
	connect(stroke_list->selector, SIGNAL(toggled(bool)), this, SLOT(selector_toggled(bool)));
	connect(stack_select, SIGNAL(did_hide()), this, SLOT(selector_hidden()));
	
	connect(stack_select->shorthand, SIGNAL(textEdited(const QString &)), stroke_list->add_text, SLOT(setText(const QString &)));
	connect(stroke_list->add_text, SIGNAL(textEdited(const QString &)), stack_select->shorthand, SLOT(setText(const QString &)));

	connect(canvas, SIGNAL(brush_radius_changed(float)), brush_params, SLOT(set_radius(float)));
	connect(canvas, SIGNAL(brush_softness_changed(float)), brush_params, SLOT(set_softness(float)));

	connect(brush_params, SIGNAL(edited_radius(float)), canvas, SLOT(set_brush_radius(float)));
	connect(brush_params, SIGNAL(edited_softness(float)), canvas, SLOT(set_brush_softness(float)));
	connect(brush_params, SIGNAL(edited_flow(float)), canvas, SLOT(set_brush_flow(float)));

	brush_params->set_radius(canvas->brush.radius);
	brush_params->set_softness(canvas->brush.softness);
	brush_params->set_flow(canvas->brush.flow);

	{
		QDockWidget *layer_dock = new QDockWidget(tr("Layers"), this);
		layer_dock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
		layer_dock->setWidget(layer_list);
		addDockWidget(Qt::RightDockWidgetArea, layer_dock);
	}

	{
		QDockWidget *stroke_dock = new QDockWidget(tr("Mappings"), this);
		stroke_dock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
		stroke_dock->setWidget(stroke_list);
		addDockWidget(Qt::RightDockWidgetArea, stroke_dock);
	}

	{
		QDockWidget *brush_dock = new QDockWidget(tr("Brush"), this);
		brush_dock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable );
		brush_dock->setWidget(brush_params);
		addDockWidget(Qt::TopDockWidgetArea, brush_dock);
	}


	Misc *misc = new Misc(this);
	{
		QDockWidget *misc_dock = new QDockWidget(tr("Misc"), this);
		misc_dock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable );
		misc_dock->setWidget(misc);
		addDockWidget(Qt::TopDockWidgetArea, misc_dock);
	}

	QAction *save_action = new QAction(tr("Save Image"), this);
	save_action->setIcon(QIcon("save.png"));
	connect(save_action, SIGNAL(triggered()), this, SLOT(save()));
	misc->save->setDefaultAction(save_action);

	

	QStringList args = qApp->arguments();
	bool first_arg = true;
	bool run_timing = false;
	bool run_timing_short = false;
	bool arg_error = false;
	string run_name = "";
	while (!args.empty()) {
		QString opt = args.front();
		args.pop_front();
		bool was_first_arg = first_arg;
		first_arg = false;
		if (opt == "--run-timings" || opt == "--run-timings-short"){ 
			if (opt == "--run-timings-short") {
				run_timing_short = true;
			}
			run_timing = true;
			if (!args.empty()) {
				run_name = qPrintable(args.front());
				args.pop_front();
				std::cerr << "Run will be called '" << run_name << "'" << std::endl;
			}
		} else if (opt == "-l") {
			if (args.size() < 3) {
				std::cerr << "ERROR: Expecting '-l' to be followed by a name, mode, and image file." << std::endl;
				arg_error = true;
				break;
			}
			QString name = args.front();
			args.pop_front();
			QString mode = args.front();
			args.pop_front();
			QString image = args.front();
			args.pop_front();
			const LayerOp *op = LayerOp::named_op(qPrintable(mode));
			if (!op) {
				std::cerr << "WARNING: Not adding layer -- unrecognized layer mode '" << qPrintable(mode) << "'." << std::endl;
				arg_error = true;
				continue;
			}
			QImage loaded(image);
			if (loaded.isNull()) {
				std::cerr << "WARNING: Not adding layer -- can't read image from '" << qPrintable(image) << "'." << std::endl;
				arg_error = true;
				continue;
			}
			{
				vector< string > names = canvas->layer_names();
				bool unique = true;
				for (vector< string >::iterator n = names.begin(); n != names.end(); ++n) {
					if (*n == qPrintable(name)) {
						std::cerr << "WARNING: Not adding layer -- name '" << qPrintable(name) << "' isn't unique." << std::endl;
						unique = false;
						arg_error = true;
						break;
					}
				}
				if (!unique) {
					continue;
				}
			}

			canvas->add_layer(loaded, qPrintable(name), op);
			std::cerr << "Layer " << (canvas->layers.size()-1) << " (\"" << canvas->layers.back()->name << "\") is '" << qPrintable(image) << "'." << std::endl;


		} else if (opt == "-s") {
			if (args.size() < 2) {
				std::cerr << "Expecting '-s' to be followed by an shorthand opspec and image file." << std::endl;
				arg_error = true;
				break;
			}
			std::string spec = qPrintable(args.front());
			args.pop_front();
			QString image = args.front();
			args.pop_front();

			std::string error = "";
			const StackOp *op = StackOp::from_shorthand(spec, canvas->layer_names(), &error);
			if (op == NULL) {
				std::cerr << "WARNING: Not adding stroke -- invalid shorthand '" << spec << "': " << error << std::endl;
				arg_error = true;
				continue;
			}

			QImage loaded(image);
			if (loaded.isNull()) {
				std::cerr << "Can't read image from '" << qPrintable(image) << "'." << std::endl;
				arg_error = true;
				continue;
			}
			std::cerr << "Stroke " << canvas->strokes.size() << " is '" << qPrintable(image) << "', and does '" << op->shorthand(canvas->layer_names()) << "' == \"" << op->description(canvas->layer_names()) << "\"" << std::endl;
			canvas->add_stroke(loaded, op);

		} else {
			//ignore other args.
			if (!was_first_arg) {
				std::cerr << "WARNING: Ignoring argument '" << qPrintable(opt) << "'." << std::endl;
				arg_error = true;
			}
		}
	}
	if (arg_error) {
		std::cerr << "WARNING: there were error parsing the arguments." << std::endl;
		if (run_timing) {
			std::cerr << "  (aborting before running timings)" << std::endl;
			exit(1);
		}
	}

	if (run_timing) {
		vector< RenderPacket * > workload;
		for (unsigned int y = 0; y < canvas->result_fbs.size.y; ++y) {
			for (unsigned int x = 0; x < canvas->result_fbs.size.x; ++x) {
				Vector2ui at = make_vector(x, y);
				RenderPacket *pkt = new RenderPacket();
				pkt->at = at;
				pkt->type = RenderPacket::RESULT;
				for (vector< Layer * >::iterator l = canvas->layers.begin(); l != canvas->layers.end(); ++l) {
					pkt->layers.push_back(make_pair((*l)->op, (*l)->get_tile_or_null(pkt->at)));
				}
				for (vector< Stroke * >::iterator s = canvas->strokes.begin(); s != canvas->strokes.end(); ++s) {
					if ((*s)->get_tile_or_null(pkt->at)) {
						pkt->strokes.push_back(make_pair((*s)->op, (*s)->get_tile_or_null(pkt->at)));
					}
				}
				memcpy(&(pkt->out[0]), default_bg(), sizeof(uint32_t) * TileSize * TileSize);
				workload.push_back(pkt);
			}
		}
		assert(!workload.empty());

		printf("%dx%d is %d tiles\n",canvas->pix_size.x,canvas->pix_size.y,int(workload.size()));

		vector< pair< RenderFn, string > > modes;
		//Full is a fine reference for small stuff:
		//Tiny blocks because, well, it might help.
		{
			RenderFn full = update_tile_full< TileSize*TileSize/32 >;
			modes.push_back(make_pair(full, "full"));
		}
		#define TRIMMED( S ) \
		{ \
			RenderFn trimmed = update_tile_trimmed< S, TileSize*TileSize/16 >; \
			modes.push_back(make_pair(trimmed, "trimmed-" #S "-b16")); \
		}

		if (run_timing_short) {
			TRIMMED( 40 );
			TRIMMED( 20 );
			TRIMMED( 10 );
			TRIMMED( 5 );
		} else {
			TRIMMED( 1000 );
			TRIMMED( 500 );
			TRIMMED( 200 );
			TRIMMED( 100 );
			TRIMMED( 50 );
			TRIMMED( 20 );
			TRIMMED( 18 );
			TRIMMED( 16 );
			TRIMMED( 14 );
			TRIMMED( 12 );
			TRIMMED( 10 );
			TRIMMED( 9 );
			TRIMMED( 8 );
			TRIMMED( 7 );
			TRIMMED( 6 );
			TRIMMED( 5 );
			TRIMMED( 4 );
			TRIMMED( 3 );
			TRIMMED( 2 );
			TRIMMED( 1 );
		}
		for (vector< pair< RenderFn, string > >::iterator m = modes.begin(); m != modes.end(); ++m) {
			//run through once to take care of any once-off costs (e.g. computing background checkerboard pattern):
			std::cerr << "Running " << m->second << " "; std::cerr.flush();
			/*
			 * Not running this might result in slightly worse numbers, but running it is slowing down parameter sweeps!
				std::cerr << "(init "; std::cerr.flush();
				for (vector< RenderPacket * >::iterator p = workload.begin(); p != workload.end(); ++p) {
					m->first((*p)->layers, (*p)->strokes, (*p)->out);
					std::cerr << '.'; std::cerr.flush();
				}
				std::cerr << ") "; std::cerr.flush();
			*/
			const unsigned int Iters = 1;
			int elapsed = 1000000;
			if (run_timing_short && m == modes.begin()) {
				assert(m->second == "full");
				std::cerr << "Skipping full" << std::endl;
			} else {
				//Actually time:
				QTime timer;
				timer.start();
				unsigned int count = 0;
				for (unsigned int iter = 0; iter < Iters; ++iter) {
					for (vector< RenderPacket * >::iterator p = workload.begin(); p != workload.end(); ++p) {
						m->first((*p)->layers, (*p)->strokes, (*p)->out);
						std::cerr << "."; std::cerr.flush();
						++count;
					}
				}
				std::cerr << " done. [" << count << "]" << std::endl;
				elapsed = timer.elapsed();
			}

			//-----------------------------------------------------------
			//Save image for later reference:
			
			vector< uint32_t > pix(canvas->pix_size.x * canvas->pix_size.y, 0xff000000);

			if (run_timing_short && m == modes.begin()) {
				assert(m->second == "full");
			} else {
				//Make tiled image into linear image:
				for (unsigned int y = 0; y < canvas->pix_size.y; ++y) {
					for (unsigned int x = 0; x < canvas->pix_size.x; ++x) {
						Vector2ui at = make_vector(x / TileSize, y / TileSize);
						unsigned int p  = at.y * canvas->result_fbs.size.x + at.x;
						assert(p < workload.size());
						assert(workload[p]->at == at);
						uint32_t *tile = workload[p]->out;
						assert(tile);
						uint32_t val = tile[(y % TileSize) * TileSize + (x % TileSize)];
						pix[y * canvas->pix_size.x + x] = 0xff000000 | (val & 0xff) << 16 | (val & 0x0000ff00) | ((val >> 16) & 0xff);
					}
				}
			}

			static vector< uint32_t > ref;
			if (ref.empty()) {
				ref = pix;

				if (run_timing_short) {
					printf("%s : % 5d / %d * %d = %0.4f ; maxerr: X sumerr: X (reference SKIPPED)\n",m->second.c_str(), elapsed, Iters, (int)workload.size(), elapsed / float(Iters * workload.size()));
				} else {
					printf("%s : % 5d / %d * %d = %0.4f ; maxerr: 0 sumerr: 0 (reference)\n",m->second.c_str(), elapsed, Iters, (int)workload.size(), elapsed / float(Iters * workload.size()));
				}
			} else {
				assert(ref.size() == pix.size());
				int32_t maxerr = 0;
				long long unsigned int sumerr = 0;
				for (unsigned int i = 0; i < ref.size(); ++i) {
					assert(sizeof(Vector< uint8_t, 4 >) == sizeof(uint32_t));
					Vector< uint8_t, 4 > rc = *(Vector< uint8_t, 4 > *)(&ref[i]);
					Vector< uint8_t, 4 > pc = *(Vector< uint8_t, 4 > *)(&pix[i]);
					for (unsigned int v = 0; v < 4; ++v) {
						int32_t a = rc.c[v];
						int32_t b = pc.c[v];
						sumerr += abs(a-b);
						if (abs(a-b) > maxerr) {
							maxerr = abs(a-b);
						}
					}
				}
				if (run_timing_short) {
					printf("%s : % 5d / %d * %d = %0.4f ; maxerr: X sumerr: X\n",m->second.c_str(), elapsed, Iters, (int)workload.size(), elapsed / float(Iters * workload.size()));
				} else {
					printf("%s : % 5d / %d * %d = %0.4f ; maxerr: %d sumerr: %llu\n",m->second.c_str(), elapsed, Iters, (int)workload.size(), elapsed / float(Iters * workload.size()), maxerr, sumerr);
				}

			}
			


			if (run_name != "") {
				QImage image = QImage(reinterpret_cast< uchar * >(&pix[0]), canvas->pix_size.x, canvas->pix_size.y, QImage::Format_RGB32);
	
				std::string filename = run_name + "-" + m->second + ".png";
				if (!image.save(filename.c_str())) {
					std::cout << "ERROR saving result!" << std::endl;
					exit(1);
				}
			}
		}

		exit(0);
	}

	//Create and connect up renderer(s):
	for (unsigned int i = 0; i < 4; ++i) {
		QThread *thread = start_renderer(canvas, misc);
		thread->setParent(this);
		connect(qApp, SIGNAL(aboutToQuit()), thread, SLOT(quit()));
	}
	std::cerr << "Created renderer threads." << std::endl;

	connect(misc, SIGNAL(set_blocks(int)), canvas, SLOT(renderer_changed()));
	connect(misc, SIGNAL(set_samples(int)), canvas, SLOT(renderer_changed()));

	misc->quality->setCurrentIndex(2);

}

void App::closeEvent(QCloseEvent *event) {
	event->accept();
	stack_select->close();
}

void App::open() {
	QString file = QFileDialog::getOpenFileName(this, tr("Add Layer"), "", tr("Images (*.png  *.jpg);;All files (*)"));

	if (!file.isNull()) {
		QImage loaded = QImage(file);
		if (loaded.isNull()) {
			QMessageBox::critical(this, tr("Error"), tr("Layer could not be loaded."));
			return;
		}

		canvas->add_layer(loaded);
	}
}

void App::reopen(unsigned int layer) {
	assert(layer < canvas->layers.size());
	QString file = QFileDialog::getOpenFileName(this, tr("Replace Layer"), "", tr("Images (*.png  *.jpg);;All files (*)"));

	if (!file.isNull()) {
		QImage loaded = QImage(file);
		if (loaded.isNull()) {
			QMessageBox::critical(this, tr("Error"), tr("Layer could not be loaded."));
			return;
		}

		canvas->replace_layer(layer, loaded);
	}
}


void App::save() {
	canvas->show_all(); //HAAAACCCCKKKK!!!!

	disconnect(canvas, SIGNAL(stroke_committed()), this, SLOT(save()));
	disconnect(canvas, SIGNAL(render_flushed()), this, SLOT(save()));
	if (canvas->current_stroke != -1U) {
		connect(canvas, SIGNAL(stroke_committed()), this, SLOT(save()));
		QMessageBox::information(this, tr("Wait a moment..."), "We'll pop up a save dialog after this stroke is done rendering");
		canvas->start_stroke_commit();
		return;
	}
	if (!canvas->needs.empty() || !canvas->pending.empty()) {
		QMessageBox::information(this, tr("Wait a moment more..."), "We'll pop up a save dialog after we finish the remaining rendering bits...");
		canvas->flushing_render = true;
		connect(canvas, SIGNAL(render_flushed()), this, SLOT(save()));
		return;
	}

	QString file = QFileDialog::getSaveFileName(this, tr("Save Composition"), "", tr("Images (*.png  *.jpg  *.jpeg);;All files (*)"));

	if (!file.isNull()) {
	
		canvas->makeCurrent();

		glPushAttrib(GL_VIEWPORT_BIT);
		glViewport(0,0,TileSize,TileSize);

		//Read back result from graphics card:
		Tiled< uint32_t > result(canvas->pix_size);
		assert(result.size.x == canvas->result_fbs.size.x);
		assert(result.size.y == canvas->result_fbs.size.y);
		for (unsigned int y = 0; y < result.size.y; ++y) {
			for (unsigned int x = 0; x < result.size.x; ++x) {
				Vector2ui at = make_vector(x, y);
				uint32_t *into = result.get_tile(at);
				QGLFramebufferObject *fb = canvas->result_fbs.get(at);
				if (!fb) {
					memcpy(into, default_bg(), sizeof(uint32_t) * TileSize * TileSize);
				} else {
					assert(fb->isValid());
					fb->bind();
					glReadPixels(0, 0, TileSize, TileSize, GL_RGBA, GL_UNSIGNED_BYTE, into);
					fb->release();
				}
			}
		}
		glPopAttrib();

		//Make tiled image into linear image:
		vector< uint32_t > pix(canvas->pix_size.x * canvas->pix_size.y, 0xff000000);
		for (unsigned int y = 0; y < canvas->pix_size.y; ++y) {
			for (unsigned int x = 0; x < canvas->pix_size.x; ++x) {
				uint32_t *tile = result.get_tile_or_null(make_vector(x / TileSize, y / TileSize));
				assert(tile);
				uint32_t val = tile[(y % TileSize) * TileSize + (x % TileSize)];
				pix[y * canvas->pix_size.x + x] = 0xff000000 | (val & 0xff) << 16 | (val & 0x0000ff00) | ((val >> 16) & 0xff);
			}
		}

		QImage image = QImage(reinterpret_cast< uchar * >(&pix[0]), canvas->pix_size.x, canvas->pix_size.y, QImage::Format_RGB32);
		if (!image.save(file)) {
			QMessageBox::critical(this, tr("Error"), tr("Image could not be saved."));
		}
	}


}

void App::open_stroke() {
	std::string error = "";
	const StackOp *op = StackOp::from_shorthand(qPrintable(stroke_list->add_text->text()), canvas->layer_names(), &error);
	if (op == NULL) {
		error = "Invalid constraint shorthand '" + string(qPrintable(stroke_list->add_text->text())) + "': " + error;
		QMessageBox::critical(this, tr("Error"), error.c_str());
		return;
	}

	QString file = QFileDialog::getOpenFileName(this, tr("Add Stroke"), "", tr("Images (*.png  *.jpg);;All files (*)"));

	if (!file.isNull()) {
		QImage loaded = QImage(file);
		if (loaded.isNull()) {
			QMessageBox::critical(this, tr("Error"), tr("Stroke could not be loaded."));
			return;
		}

		canvas->add_stroke(loaded, op);
	}
}


void App::add_stroke() {
	std::string error = "";
	const StackOp *op = StackOp::from_shorthand(qPrintable(stroke_list->add_text->text()), canvas->layer_names(), &error);
	if (op == NULL) {
		error = "Invalid constraint shorthand '" + string(qPrintable(stroke_list->add_text->text())) + "': " + error;
		QMessageBox::critical(this, tr("Error"), error.c_str());
		return;
	} else {
		canvas->add_stroke(op);
	}
}

void App::save_stroke(unsigned int stroke) {
	assert(stroke < canvas->strokes.size());
	QString file = QFileDialog::getSaveFileName(this, tr("Save Stroke"), "", tr("Images (*.png);;All files (*)"));

	if (!file.isNull()) {
		vector< uint32_t > pix(canvas->pix_size.x * canvas->pix_size.y, 0xff000000);
		for (unsigned int y = 0; y < canvas->pix_size.y; ++y) {
			for (unsigned int x = 0; x < canvas->pix_size.x; ++x) {
				uint8_t *tile = canvas->strokes[stroke]->get_tile_or_null(make_vector(x / TileSize, y / TileSize));
				if (tile) {
					uint32_t val = tile[(y % TileSize) * TileSize + (x % TileSize)];
					pix[y * canvas->pix_size.x + x] = 0xff000000 | (val << 16) | (val << 8) | val;
				}
			}
		}
		
		QImage image = QImage(reinterpret_cast< uchar * >(&pix[0]), canvas->pix_size.x, canvas->pix_size.y, QImage::Format_RGB32);
		if (!image.save(file)) {
			QMessageBox::critical(this, tr("Error"), tr("Stroke could not be saved."));
		}
	}
}

void App::selector_toggled(bool state) {
	if (state) {
		stack_select->show();
	} else {
		stack_select->hide();
	}
}

void App::selector_hidden() {
	if (stroke_list->selector->isChecked()) {
		stroke_list->selector->setChecked(false);
	}
}
