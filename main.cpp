#include <QApplication>
#include "App.hpp"
#include "Renderer.hpp"

int main(int argc, char **argv) {
	QApplication app(argc, argv);

	setup_renderer_event_types();

	App sparse;
	sparse.show();

	return app.exec();
}
