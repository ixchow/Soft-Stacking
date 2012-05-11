#ifndef GL_ERRORS_HPP
#define GL_ERRORS_HPP

#include <string>
#include <iostream>
#include <QtOpenGL>

namespace {

void gl_errors(std::string const &where) {
	GLuint err;
	while ((err = glGetError()) != GL_NO_ERROR) {
	std::cerr << "(in " << where << ") OpenGL error #" << err
	     << ": " << "(error string would go here if Qt's GL headers had a prototype for gluErrorString)" /*gluErrorString(err)*/ << std::endl;
	}
}

}


#endif //GL_ERRORS_HPP
