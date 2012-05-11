#ifndef GLHACKS_HPP
#define GLHACKS_HPP

#ifdef WIN32

//We're basically back in OpenGL 1.1 land, so define a few things that
//the underyling implimentation... should probably... one hopes... understand.

// (taken from glext.h)
#define GL_GENERATE_MIPMAP                0x8191
#define GL_CLAMP_TO_BORDER                0x812D
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_TEXTURE2                       0x84C2

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

//Hacking around having proper extension loading support:
typedef void (APIENTRYP PFNGLACTIVETEXTUREARBPROC) (GLenum texture);
//Even *more* hacky:

#define glActiveTextureScope() \
	PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL; \
	if (context() && context()->isValid()) { \
		assert(context() == QGLContext::currentContext()); \
		glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)context()->getProcAddress("glActiveTextureARB"); \
	} \

#define glActiveTexture(tex) \
	do { \
		if (glActiveTextureARB) { \
			glActiveTextureARB(tex); \
		} else { \
			qDebug("No glActiveTextureARB function; this will mess up drawing."); \
		} \
	} while(0)

#else //WIN32

#define glActiveTextureScope() /*nothing*/

#endif //WIN32


#endif //GLHACKS_HPP
