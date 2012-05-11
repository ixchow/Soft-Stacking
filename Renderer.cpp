#include "Renderer.hpp"

#include "Constants.hpp"
#include "update_tile_trimmed.hpp"

#include "LayerOps.hpp"

#include <QCoreApplication>
#include <QThread>

#include <cassert>
#include <cstdlib>

RenderPacket::RenderPacket() : at(make_vector(-1U,-1U)), type(RESULT) {
}


QEvent::Type RendererReadyEventType = QEvent::None;
QEvent::Type RendererRequestEventType = QEvent::None;

void setup_renderer_event_types() {
	assert(RendererReadyEventType == QEvent::None);
	assert(RendererRequestEventType == QEvent::None);
	RendererReadyEventType = static_cast< QEvent::Type >(QEvent::registerEventType());
	RendererRequestEventType = static_cast< QEvent::Type >(QEvent::registerEventType());
}

RendererReadyEvent::RendererReadyEvent(Renderer *_renderer, RenderPacket *_completed) : QEvent(RendererReadyEventType), renderer(_renderer), completed(_completed) {
	assert(RendererReadyEventType != QEvent::None);
}

RendererReadyEvent::~RendererReadyEvent() {
}

RendererRequestEvent::RendererRequestEvent(RenderPacket *_request) : QEvent(RendererRequestEventType), request(_request) {
	assert(RendererRequestEventType != QEvent::None);
}

RendererRequestEvent::~RendererRequestEvent() {
}


Renderer::Renderer(QObject *_canvas) : canvas(_canvas), blocks(8), samples(10) {
	//Tell the canvas it's got a ready renderer:
	QCoreApplication::postEvent(canvas, new RendererReadyEvent(this, NULL));
}

Renderer::~Renderer() {
}

void Renderer::set_blocks(int new_blocks) {
	if (new_blocks < 1) new_blocks = 1;
	if (new_blocks > 64) new_blocks = 64;
	unsigned int best_val = 1;
	for (unsigned int i = 0; i < 16; ++i) {
		if (abs((1 << i) - new_blocks) < abs(int(best_val) - new_blocks)) {
			best_val = (1 << i);
		}
	}
	assert(best_val <= 64);
	if (blocks != best_val) {
		blocks = best_val;
		emit blocks_changed(int(blocks));
		std::cerr << "Render switches to " << blocks << " blocks." << std::endl;
	}
}

void Renderer::set_samples(int new_samples) {
	if (new_samples < 1) new_samples = 1;
	if (new_samples > 1000) new_samples = 1000;
	if (samples != (unsigned int)new_samples) {
		samples = new_samples;
		emit samples_changed(int(samples));
		std::cerr << "Render switches to " << samples << " samples." << std::endl;
	}
}


void Renderer::customEvent(QEvent *e) {
	assert(RendererRequestEventType != QEvent::None);
	if (e->type() == RendererRequestEventType) {
		RendererRequestEvent *req = dynamic_cast< RendererRequestEvent * >(e);
		assert(req);
		render(req->request);
		QCoreApplication::postEvent(canvas, new RendererReadyEvent(this, req->request));
		e->accept();
	} else {
		QObject::customEvent(e);
	}
}

void Renderer::render(RenderPacket *packet) {
	assert(packet);
	update_tile_trimmed(packet->layers, packet->strokes, packet->out, samples, TileSize * TileSize / blocks);
}



RendererThread::RendererThread(QObject *_canvas, Misc *_control) : QThread(), canvas(_canvas), control(_control) {
}

RendererThread::~RendererThread() {
}

void RendererThread::run() {
	Renderer renderer(canvas);
	connect(control, SIGNAL(set_blocks(int)), &renderer, SLOT(set_blocks(int)));
	connect(control, SIGNAL(set_samples(int)), &renderer, SLOT(set_samples(int)));
	exec();
}

QThread *start_renderer(QObject *canvas, Misc *control) {
	QThread *thread = new RendererThread(canvas, control);
	thread->setStackSize(8 * 1024 * 1024); //eight meg of stack should be enough.
	thread->start();
	return thread;
}
