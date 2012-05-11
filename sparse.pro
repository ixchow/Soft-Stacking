

HEADERS += gl_errors.hpp
HEADERS += coefs.hpp
HEADERS += update_tile_dense.hpp
HEADERS += update_tile_trimmed.hpp
HEADERS += update_tile_full.hpp
HEADERS += default_bg.hpp
HEADERS += sse_compose.hpp
HEADERS += Constants.hpp
HEADERS += LayerList.hpp
HEADERS += StrokeList.hpp
HEADERS += BrushParams.hpp
HEADERS += StackSelect.hpp
HEADERS += Canvas.hpp
HEADERS += Renderer.hpp
HEADERS += Misc.hpp
HEADERS += Tiled.hpp
HEADERS += LayerOps.hpp
HEADERS += StackOps.hpp
HEDDERS += StrokeDraw.hpp
HEADERS += App.hpp
HEADERS += GLHacks.hpp

SOURCES += update_tile_dense.cpp
SOURCES += update_tile_trimmed.cpp
SOURCES += update_tile_full.cpp
SOURCES += default_bg.cpp
SOURCES += coefs.cpp
SOURCES += Canvas.cpp
SOURCES += Renderer.cpp
SOURCES += LayerList.cpp
SOURCES += StrokeList.cpp
SOURCES += BrushParams.cpp
SOURCES += StackSelect.cpp
SOURCES += Misc.cpp
SOURCES += Tiled.cpp
SOURCES += LayerOps.cpp
SOURCES += StackOps.cpp
SOURCES += StrokeDraw.cpp
SOURCES += App.cpp
SOURCES += main.cpp

QT += opengl
!win32 {
	QMAKE_CFLAGS_DEBUG += -Werror -g -O3
	QMAKE_CFLAGS_RELEASE += -Werror -g -O3
	QMAKE_CXXFLAGS += -Werror -g -O3
}
win32 {
	QMAKE_CXXFLAGS += /Iutil /wd4146
	CONFIG += console
}
