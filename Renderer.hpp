#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "Constants.hpp"
#include "Misc.hpp"

#include <Vector/Vector.hpp>

#include <QObject>
#include <QEvent>
#include <QThread>

#include <vector>
#include <utility>

class LayerOp;
class StackOp;

class RenderPacket {
public:
	RenderPacket();
	std::vector< std::pair< const LayerOp *, const uint32_t * > > layers;
	std::vector< std::pair< const StackOp *, const uint8_t * > > strokes;
	uint32_t out[TileSize * TileSize];
	Vector2ui at;
	static const unsigned int ZERO = 0;
	static const unsigned int ONE = 1;
	static const unsigned int RESULT = 2;
	unsigned int type;
};

extern QEvent::Type RendererReadyEventType;
extern QEvent::Type RendererRequestEventType;

//call on main thread:
void setup_renderer_event_types();

class Renderer;

//Sent from renderer -> canvas to indicate that it is ready (first event passes a NULL packet back, others pass completed packets)
class RendererReadyEvent : public QEvent {
public:
	RendererReadyEvent(Renderer *renderer, RenderPacket *completed = NULL);
	virtual ~RendererReadyEvent();
	Renderer *renderer;
	RenderPacket *completed; //may be NULL
};

//Sent from canvas -> renderer to request that a tile be rendered
class RendererRequestEvent : public QEvent {
public:
	RendererRequestEvent(RenderPacket *request);
	virtual ~RendererRequestEvent();
	RenderPacket *request; //now "owned" by Renderer until Renderer
};

class Renderer : public QObject {
	Q_OBJECT
public:
	Renderer(QObject *canvas);
	virtual ~Renderer();
public slots:
	void set_blocks(int);
	void set_samples(int);
signals:
	void blocks_changed(int);
	void samples_changed(int);
protected:
	virtual void customEvent(QEvent *e);
public:
	void render(RenderPacket *packet);
	QObject *canvas;
	unsigned int blocks;
	unsigned int samples;
};

class RendererThread : public QThread {
	Q_OBJECT
public:
	RendererThread(QObject *canvas, Misc *control);
	virtual ~RendererThread();
protected:
	void run();
	QObject *canvas;
	Misc *control;
};


QThread *start_renderer(QObject *canvas, Misc *control);

#endif //RENDERER_HPP
