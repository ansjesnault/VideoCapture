/**
 * \file opengl_functions.h
 *
 * Utility class for loading OpenGL functions for a specific version using Qt5.
 *
 * Written by Gaurav Chaurasia (gaurav.chaurasia@inria.fr)
 *
 * By default, it loads core profiles for OpenGL 3.0 onwards. In order to load
 * compatibility profile, change the base class for GLFunctions. For example,
 * change
 \code
 class GLFunctions : protected QOpenGLFunctions_3_2_Core
 \endcode
 *
 * to
 *
 \code
 class GLFunctions : protected QOpenGLFunctions_3_2_Compatibilty
 \endcode
 *
 * This should be cleaned up using preprocessor definitions.
 *
 * This code is made available under CECILL-C licence
 * http://www.cecill.info/licences/Licence_CeCILL-C_V1-en.html
 *
 * This software is a computer program whose purpose is to offer a set of
 * tools to simplify programming real-time computer graphics applications
 * under OpenGL and DirectX.
 *
 * This software is governed by the CeCILL-C license under French law and
 * abiding by the rules of distribution of free software.  You can  use,
 * modify and/ or redistribute the software under the terms of the CeCILL-C
 * license as circulated by CEA, CNRS and INRIA at the following URL
 * "http://www.cecill.info".
 *
 * As a counterpart to the access to the source code and  rights to copy,
 * modify and redistribute granted by the license, users are provided only
 * with a limited warranty  and the software's author,  the holder of the
 * economic rights,  and the successive licensors  have only  limited
 * liability.
 *
 * In this respect, the user's attention is drawn to the risks associated
 * with loading,  using,  modifying and/or developing or reproducing the
 * software by the user in light of its specific status of free software,
 * that may mean  that it is complicated to manipulate,  and  that  also
 * therefore means  that it is reserved for developers  and  experienced
 * professionals having in-depth computer knowledge. Users are therefore
 * encouraged to load and test the software's suitability as regards their
 * requirements in conditions enabling the security of their systems and/or
 * data to be ensured and,  more generally, to use and operate it in the
 * same conditions as regards security.
 *
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL-C license and that you accept its terms.
 *
 */

#ifndef _QT_OPENGL_FUNCTIONS_H_
#define _QT_OPENGL_FUNCTIONS_H_

/** Flag to switch to OpenGL ES 2.0, ignoring OPENGL_VERSION_MAJOR and OPENGL_VERSION_MINOR */
#define OPENGL_EMBEDDED_ARCH 0

/** OpenGL major version number 1 to 4 */
#define OPENGL_VERSION_MAJOR 4

/** OpenGL minor version number, certain combinations allowed
 * with majjor version */
#define OPENGL_VERSION_MINOR 2


/** Generic class that inherits OpenGL functions from any OpenGL version
 * specified by the OPENGL_VERSION_MAJOR and OPENG_VERSION_MINOR.
 * Supported major and minor version combinations are
 * 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 2.0, 2.1, 3.0, 3.1, 3.2, 3.3, 4.0,
 * 4.1, 4.2, 4.3. For versions 3.2 onwards, both core and compatibility
 * profiles are made available by this class. Setting OPENGL_EMBEDDED_ARCH
 * to 1 switches to OpenGL ES 2.0, ignoring major and minor versions
 *
 * Any class that intends to call OpenGL functions must inherit GLFunctions
 * and call GLFunctions::init() before using any OpenGL functions.
 */
class GLFunctions;


// ----------------------------------------------------------------------------
#if OPENGL_EMBEDDED_ARCH
#include <QOpenGLFunctions_ES2>
class GLFunctions : protected QOpenGLFunctions_ES2 {
public:
    void init(void) { initializeOpenGLFunctions(); }
};
#else

// ----------------------------------------------------------------------------
// OpenGL 1
#if OPENGL_VERSION_MAJOR==1

#if OPENGL_VERSION_MINOR==0
#include <QOpenGLFunctions_1_0>
class GLFunctions : protected QOpenGLFunctions_1_0 {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==1
#include <QOpenGLFunctions_1_1>
class GLFunctions : protected QOpenGLFunctions_1_1 {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==2
#include <QOpenGLFunctions_1_2>
class GLFunctions : protected QOpenGLFunctions_1_2 {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==3
#include <QOpenGLFunctions_1_3>
class GLFunctions : protected QOpenGLFunctions_1_3 {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==4
#include <QOpenGLFunctions_1_4>
class GLFunctions : protected QOpenGLFunctions_1_4 {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==5
#include <QOpenGLFunctions_1_5>
class GLFunctions : protected QOpenGLFunctions_1_5 {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#else
#pragma(message, "Incorrect OpenGL version minor")

#endif // OPENGL_VERSION_MINOR

// ----------------------------------------------------------------------------
// OpenGL 2
#elif OPENGL_VERSION_MAJOR==2

#if OPENGL_VERSION_MINOR==0
#include <QOpenGLFunctions_2_0>
class GLFunctions : protected QOpenGLFunctions_2_0 {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==1
#include <QOpenGLFunctions_2_1>
class GLFunctions : protected QOpenGLFunctions_2_1 {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#else
#pragma(message, "Incorrect OpenGL version minor")

#endif // OPENGL_VERSION_MINOR

// ----------------------------------------------------------------------------
// OpenGL 3
#elif OPENGL_VERSION_MAJOR==3

#if OPENGL_VERSION_MINOR==0
#include <QOpenGLFunctions_3_0>
class GLFunctions : protected QOpenGLFunctions_3_0 {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==1
#include <QOpenGLFunctions_3_1>
class GLFunctions : protected QOpenGLFunctions_3_1 {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==2
#include <QOpenGLFunctions_3_2_Core>
#include <QOpenGLFunctions_3_2_Compatibility>
class GLFunctions : protected QOpenGLFunctions_3_2_Core {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==3
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFunctions_3_3_Compatibility>
class GLFunctions : protected QOpenGLFunctions_3_3_Core {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#else
#pragma(message, "Incorrect OpenGL version minor")

#endif // OPENGL_VERSION_MINOR

// ----------------------------------------------------------------------------
// OpenGL 4 onwards
#elif OPENGL_VERSION_MAJOR==4

#if OPENGL_VERSION_MINOR==0
#include <QOpenGLFunctions_4_0_Core>
#include <QOpenGLFunctions_4_0_Compatibility>
class GLFunctions : protected QOpenGLFunctions_4_0_Core {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==1
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLFunctions_4_1_Compatibility>
class GLFunctions : protected QOpenGLFunctions_4_1_Core {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==2
#include <QOpenGLFunctions_4_2_Core>
#include <QOpenGLFunctions_4_2_Compatibility>
class GLFunctions : protected QOpenGLFunctions_4_2_Core {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#elif OPENGL_VERSION_MINOR==3
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLFunctions_4_3_Compatibility>
class GLFunctions : protected QOpenGLFunctions_4_3_Core {
public:
  void init(void) { initializeOpenGLFunctions(); }
};

#else
#pragma(message, "Incorrect OpenGL version minor")

#endif // OPENGL_VERSION_MINOR

#else
#pragma(message, "Incorrect OpenGL version major")

#endif // OPENGL_VERSION_MAJOR

#endif // OPENGL_EMBEDDED_ARCH

#endif // _QT_OPENGL_FUNCTIONS_H_
