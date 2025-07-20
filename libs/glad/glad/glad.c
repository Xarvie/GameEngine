#include "glad.h"



#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifndef PAGL_IMPL_UTIL_C_
#define PAGL_IMPL_UTIL_C_

#ifdef _MSC_VER
#define PAGL_IMPL_UTIL_SSCANF sscanf_s
#else
#define PAGL_IMPL_UTIL_SSCANF sscanf
#endif

#endif /* PAGL_IMPL_UTIL_C_ */



#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !defined(__ANDROID__) && defined(__linux__)
// #include <GLES3/gl3.h>
#define GL_OS_LINUX
#endif

#if defined(__ANDROID__)
//#include <GLES3/gl3.h>
#define GL_OS_ANDROID
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define GL_OS_IPHONE
//#include <OpenGLES/ES3/gl.h>
//#include <OpenGLES/ES3/glext.h>

#elif defined(TARGET_OS_MAC) && TARGET_OS_MAC
#define GL_OS_MAC
//#include <OpenGL/gl3.h>
#elif defined(_WIN32)
#define GL_OS_WINDOWS
#endif


#if !defined(__khrplatform_h_)
#define __khrplatform_h_

#if defined(__SCITECH_SNAP__) && !defined(KHRONOS_STATIC)
#   define KHRONOS_STATIC 1
#endif

/*-------------------------------------------------------------------------
 * Definition of KHRONOS_APICALL
 *-------------------------------------------------------------------------
 * This precedes the return type of the function in the function prototype.
 */
#if defined(KHRONOS_STATIC)
/* If the preprocessor constant KHRONOS_STATIC is defined, make the
     * header compatible with static linking. */
#   define KHRONOS_APICALL
#elif defined(_WIN32)
#   define KHRONOS_APICALL __declspec(dllimport)
#elif defined (__SYMBIAN32__)
#   define KHRONOS_APICALL IMPORT_C
#elif defined(__ANDROID__)
#   define KHRONOS_APICALL __attribute__((visibility("default")))
#else
#   define KHRONOS_APICALL
#endif

/*-------------------------------------------------------------------------
 * Definition of KHRONOS_APIENTRY
 *-------------------------------------------------------------------------
 * This follows the return type of the function  and precedes the function
 * name in the function prototype.
 */
#if defined(_WIN32) && !defined(_WIN32_WCE) && !defined(__SCITECH_SNAP__)
/* Win32 but not WinCE */
#   define KHRONOS_APIENTRY __stdcall
#else
#   define KHRONOS_APIENTRY
#endif

/*-------------------------------------------------------------------------
 * Definition of KHRONOS_APIATTRIBUTES
 *-------------------------------------------------------------------------
 * This follows the closing parenthesis of the function prototype arguments.
 */
#if defined (__ARMCC_2__)
#define KHRONOS_APIATTRIBUTES __softfp
#else
#define KHRONOS_APIATTRIBUTES
#endif

/*-------------------------------------------------------------------------
 * basic type definitions
 *-----------------------------------------------------------------------*/
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || defined(__GNUC__) || defined(__SCO__) || defined(__USLC__)


/*
 * Using <stdint.h>
 */
#include <stdint.h>
//typedef int32_t                 khronos_int32_t;
//typedef uint32_t                khronos_uint32_t;
//typedef int64_t                 khronos_int64_t;
//typedef uint64_t                khronos_uint64_t;
#define KHRONOS_SUPPORT_INT64   1
#define KHRONOS_SUPPORT_FLOAT   1
/*
 * To support platform where unsigned long cannot be used interchangeably with
 * inptr_t (e.g. CHERI-extended ISAs), we can use the stdint.h intptr_t.
 * Ideally, we could just use (u)intptr_t everywhere, but this could result in
 * ABI breakage if khronos_uintptr_t is changed from unsigned long to
 * unsigned long long or similar (this results in different C++ name mangling).
 * To avoid changes for existing platforms, we restrict usage of intptr_t to
 * platforms where the size of a pointer is larger than the size of long.
 */
#if defined(__SIZEOF_LONG__) && defined(__SIZEOF_POINTER__)
#if __SIZEOF_POINTER__ > __SIZEOF_LONG__
#define KHRONOS_USE_INTPTR_T
#endif
#endif

#elif defined(__VMS ) || defined(__sgi)

/*
 * Using <inttypes.h>
 */
#include <inttypes.h>
typedef int32_t                 khronos_int32_t;
typedef uint32_t                khronos_uint32_t;
typedef int64_t                 khronos_int64_t;
typedef uint64_t                khronos_uint64_t;
#define KHRONOS_SUPPORT_INT64   1
#define KHRONOS_SUPPORT_FLOAT   1

#elif defined(_WIN32) && !defined(__SCITECH_SNAP__)

/*
 * Win32
 */
typedef __int32                 khronos_int32_t;
typedef unsigned __int32        khronos_uint32_t;
typedef __int64                 khronos_int64_t;
typedef unsigned __int64        khronos_uint64_t;
#define KHRONOS_SUPPORT_INT64   1
#define KHRONOS_SUPPORT_FLOAT   1

#elif defined(__sun__) || defined(__digital__)

/*
 * Sun or Digital
 */
typedef int                     khronos_int32_t;
typedef unsigned int            khronos_uint32_t;
#if defined(__arch64__) || defined(_LP64)
typedef long int                khronos_int64_t;
typedef unsigned long int       khronos_uint64_t;
#else
typedef long long int           khronos_int64_t;
typedef unsigned long long int  khronos_uint64_t;
#endif /* __arch64__ */
#define KHRONOS_SUPPORT_INT64   1
#define KHRONOS_SUPPORT_FLOAT   1

#elif 0

/*
 * Hypothetical platform with no float or int64 support
 */
typedef int                     khronos_int32_t;
typedef unsigned int            khronos_uint32_t;
#define KHRONOS_SUPPORT_INT64   0
#define KHRONOS_SUPPORT_FLOAT   0

#else

/*
 * Generic fallback
 */
#include <stdint.h>
typedef int32_t                 khronos_int32_t;
typedef uint32_t                khronos_uint32_t;
typedef int64_t                 khronos_int64_t;
typedef uint64_t                khronos_uint64_t;
#define KHRONOS_SUPPORT_INT64   1
#define KHRONOS_SUPPORT_FLOAT   1

#endif


/*
 * Types that are (so far) the same on all platforms
 */


/*
 * Types that differ between LLP64 and LP64 architectures - in LLP64,
 * pointers are 64 bits, but 'long' is still 32 bits. Win64 appears
 * to be the only LLP64 architecture in current use.
 */
//#ifdef KHRONOS_USE_INTPTR_T
//typedef intptr_t               khronos_intptr_t;
//typedef uintptr_t              khronos_uintptr_t;
//#elif defined(_WIN64)
//typedef signed   long long int khronos_intptr_t;
//typedef unsigned long long int khronos_uintptr_t;
//#else
//typedef signed   long  int     khronos_intptr_t;
//typedef unsigned long  int     khronos_uintptr_t;
//#endif
//
//#if defined(_WIN64)
//typedef signed   long long int khronos_ssize_t;
//typedef unsigned long long int khronos_usize_t;
//#else
//typedef signed   long  int     khronos_ssize_t;
//typedef unsigned long  int     khronos_usize_t;
//#endif

#if KHRONOS_SUPPORT_FLOAT
/*
 * Float type
 */
//typedef          float         khronos_float_t;
#endif

#if KHRONOS_SUPPORT_INT64
/* Time types
 *
 * These types can be used to represent a time interval in nanoseconds or
 * an absolute Unadjusted System Time.  Unadjusted System Time is the number
 * of nanoseconds since some arbitrary system event (e.g. since the last
 * time the system booted).  The Unadjusted System Time is an unsigned
 * 64 bit value that wraps back to 0 every 584 years.  Time intervals
 * may be either signed or unsigned.
 */
//typedef khronos_uint64_t       khronos_utime_nanoseconds_t;
//typedef khronos_int64_t        khronos_stime_nanoseconds_t;
#endif

/*
 * Dummy value used to pad enum types to 32 bits.
 */
#ifndef KHRONOS_MAX_ENUM
#define KHRONOS_MAX_ENUM 0x7FFFFFFF
#endif

/*
 * Enumerated boolean type
 *
 * Values other than zero should be considered to be true.  Therefore
 * comparisons should not be made against KHRONOS_TRUE.
 */
typedef enum {
    KHRONOS_FALSE = 0,
    KHRONOS_TRUE  = 1,
    KHRONOS_BOOLEAN_ENUM_FORCE_SIZE = KHRONOS_MAX_ENUM
} khronos_boolean_enum_t;

#endif /* __khrplatform_h_ */


#if !defined(PAGL_GLES2_H_) && !defined(__ANDROID__) && !defined(TARGET_OS_IOS)
#define PAGL_GLES2_H_

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif
#ifdef __gl2_h_
#error OpenGL ES 2 header already included (API: gles2), remove previous include!
#endif
#define __gl2_h_ 1
#ifdef __gles2_gl2_h_
#error OpenGL ES 2 header already included (API: gles2), remove previous include!
#endif
#define __gles2_gl2_h_ 1
#ifdef __gl3_h_
#error OpenGL ES 3 header already included (API: gles2), remove previous include!
#endif
#define __gl3_h_ 1
#ifdef __gles2_gl3_h_
//  #error OpenGL ES 3 header already included (API: gles2), remove previous include!
#endif
#define __gles2_gl3_h_ 1
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#define PAGL_GLES2



#ifndef PAGL_PLATFORM_H_
#define PAGL_PLATFORM_H_

#ifndef PAGL_PLATFORM_WIN32
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__MINGW32__)
#define PAGL_PLATFORM_WIN32 1
#else
#define PAGL_PLATFORM_WIN32 0
#endif
#endif

#ifndef PAGL_PLATFORM_APPLE
#ifdef __APPLE__
#define PAGL_PLATFORM_APPLE 1
#else
#define PAGL_PLATFORM_APPLE 0
#endif
#endif

#ifndef PAGL_PLATFORM_EMSCRIPTEN
#ifdef __EMSCRIPTEN__
#define PAGL_PLATFORM_EMSCRIPTEN 1
#else
#define PAGL_PLATFORM_EMSCRIPTEN 0
#endif
#endif

#ifndef PAGL_PLATFORM_UWP
#if defined(_MSC_VER) && !defined(PAGL_INTERNAL_HAVE_WINAPIFAMILY)
#ifdef __has_include
      #if __has_include(<winapifamily.h>)
        #define PAGL_INTERNAL_HAVE_WINAPIFAMILY 1
      #endif
    #elif _MSC_VER >= 1700 && !_USING_V110_SDK71_
      #define PAGL_INTERNAL_HAVE_WINAPIFAMILY 1
    #endif
#endif

#ifdef PAGL_INTERNAL_HAVE_WINAPIFAMILY
#include <winapifamily.h>
    #if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
      #define PAGL_PLATFORM_UWP 1
    #endif
#endif

#ifndef PAGL_PLATFORM_UWP
#define PAGL_PLATFORM_UWP 0
#endif
#endif

#ifdef __GNUC__
#define PAGL_GNUC_EXTENSION __extension__
#else
#define PAGL_GNUC_EXTENSION
#endif

#define PAGL_UNUSED(x) (void)(x)

#ifndef PAGL_API_CALL
#if defined(PAGL_API_CALL_EXPORT)
#if PAGL_PLATFORM_WIN32 || defined(__CYGWIN__)
      #if defined(PAGL_API_CALL_EXPORT_BUILD)
        #if defined(__GNUC__)
          #define PAGL_API_CALL __attribute__ ((dllexport)) extern
        #else
          #define PAGL_API_CALL __declspec(dllexport) extern
        #endif
      #else
        #if defined(__GNUC__)
          #define PAGL_API_CALL __attribute__ ((dllimport)) extern
        #else
          #define PAGL_API_CALL __declspec(dllimport) extern
        #endif
      #endif
    #elif defined(__GNUC__) && defined(PAGL_API_CALL_EXPORT_BUILD)
      #define PAGL_API_CALL __attribute__ ((visibility ("default"))) extern
    #else
      #define PAGL_API_CALL extern
    #endif
#else
#define PAGL_API_CALL extern
#endif
#endif

#ifdef APIENTRY
#define PAGL_API_PTR APIENTRY
#elif PAGL_PLATFORM_WIN32
#define PAGL_API_PTR __stdcall
#else
#define PAGL_API_PTR
#endif

#ifndef GLAPI
#define GLAPI PAGL_API_CALL
#endif

#ifndef GLAPIENTRY
#define GLAPIENTRY PAGL_API_PTR
#endif

#define PAGL_MAKE_VERSION(major, minor) (major * 10000 + minor)
#define PAGL_VERSION_MAJOR(version) (version / 10000)
#define PAGL_VERSION_MINOR(version) (version % 10000)

#define PAGL_GENERATOR_VERSION "2.0.2"

typedef void (*PAGLapiproc)(void);

typedef PAGLapiproc (*PAGLloadfunc)(const char *name);
typedef PAGLapiproc (*PAGLuserptrloadfunc)(void *userptr, const char *name);

typedef void (*PAGLprecallback)(const char *name, PAGLapiproc apiproc, int len_args, ...);
typedef void (*PAGLpostcallback)(void *ret, const char *name, PAGLapiproc apiproc, int len_args, ...);

#endif /* PAGL_PLATFORM_H_ */


#endif


#ifndef PAGL_API_CALL
#if defined(PAGL_API_CALL_EXPORT)
    #if PAGL_PLATFORM_WIN32 || defined(__CYGWIN__)
      #if defined(PAGL_API_CALL_EXPORT_BUILD)
        #if defined(__GNUC__)
          #define PAGL_API_CALL __attribute__ ((dllexport)) extern
        #else
          #define PAGL_API_CALL __declspec(dllexport) extern
        #endif
      #else
        #if defined(__GNUC__)
          #define PAGL_API_CALL __attribute__ ((dllimport)) extern
        #else
          #define PAGL_API_CALL __declspec(dllimport) extern
        #endif
      #endif
    #elif defined(__GNUC__) && defined(PAGL_API_CALL_EXPORT_BUILD)
      #define PAGL_API_CALL __attribute__ ((visibility ("default"))) extern
    #else
      #define PAGL_API_CALL extern
    #endif
  #else
    #define PAGL_API_CALL extern
  #endif
#endif

#ifdef APIENTRY
#define PAGL_API_PTR APIENTRY
#elif PAGL_PLATFORM_WIN32
#define PAGL_API_PTR __stdcall
#else
#define PAGL_API_PTR
#endif

#ifndef GLAPI
#define GLAPI PAGL_API_CALL
#endif

#ifndef GLAPIENTRY
#define GLAPIENTRY PAGL_API_PTR
#endif

typedef unsigned short GLhalfNV;
typedef GLintptr GLvdpauSurfaceNV;
typedef void (PAGL_API_PTR *GLVULKANPROCNV)(void);


#define GL_ES_VERSION_2_0 1
PAGL_API_CALL int PAGL_GL_ES_VERSION_2_0;
#define GL_ES_VERSION_3_0 1
PAGL_API_CALL int PAGL_GL_ES_VERSION_3_0;
#define GL_KHR_debug 1
PAGL_API_CALL int PAGL_GL_KHR_debug;


typedef void (PAGL_API_PTR *PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void (PAGL_API_PTR *PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (PAGL_API_PTR *PFNGLBEGINQUERYPROC)(GLenum target, GLuint id);
typedef void (PAGL_API_PTR *PFNGLBEGINTRANSFORMFEEDBACKPROC)(GLenum primitiveMode);
typedef void (PAGL_API_PTR *PFNGLBINDATTRIBLOCATIONPROC)(GLuint program, GLuint index, const GLchar * name);
typedef void (PAGL_API_PTR *PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (PAGL_API_PTR *PFNGLBINDBUFFERBASEPROC)(GLenum target, GLuint index, GLuint buffer);
typedef void (PAGL_API_PTR *PFNGLBINDBUFFERRANGEPROC)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (PAGL_API_PTR *PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (PAGL_API_PTR *PFNGLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
typedef void (PAGL_API_PTR *PFNGLBINDSAMPLERPROC)(GLuint unit, GLuint sampler);
typedef void (PAGL_API_PTR *PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (PAGL_API_PTR *PFNGLBINDTRANSFORMFEEDBACKPROC)(GLenum target, GLuint id);
typedef void (PAGL_API_PTR *PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (PAGL_API_PTR *PFNGLBLENDCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (PAGL_API_PTR *PFNGLBLENDEQUATIONPROC)(GLenum mode);
typedef void (PAGL_API_PTR *PFNGLBLENDEQUATIONSEPARATEPROC)(GLenum modeRGB, GLenum modeAlpha);
typedef void (PAGL_API_PTR *PFNGLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void (PAGL_API_PTR *PFNGLBLENDFUNCSEPARATEPROC)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void (PAGL_API_PTR *PFNGLBLITFRAMEBUFFERPROC)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (PAGL_API_PTR *PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void * data, GLenum usage);
typedef void (PAGL_API_PTR *PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void * data);
typedef GLenum (PAGL_API_PTR *PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void (PAGL_API_PTR *PFNGLCLEARPROC)(GLbitfield mask);
typedef void (PAGL_API_PTR *PFNGLCLEARBUFFERFIPROC)(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
typedef void (PAGL_API_PTR *PFNGLCLEARBUFFERFVPROC)(GLenum buffer, GLint drawbuffer, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLCLEARBUFFERIVPROC)(GLenum buffer, GLint drawbuffer, const GLint * value);
typedef void (PAGL_API_PTR *PFNGLCLEARBUFFERUIVPROC)(GLenum buffer, GLint drawbuffer, const GLuint * value);
typedef void (PAGL_API_PTR *PFNGLCLEARCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (PAGL_API_PTR *PFNGLCLEARDEPTHFPROC)(GLfloat d);
typedef void (PAGL_API_PTR *PFNGLCLEARSTENCILPROC)(GLint s);
//typedef GLenum (PAGL_API_PTR *PFNGLCLIENTWAITSYNCPROC)(GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void (PAGL_API_PTR *PFNGLCOLORMASKPROC)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (PAGL_API_PTR *PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (PAGL_API_PTR *PFNGLCOMPRESSEDTEXIMAGE2DPROC)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void * data);
typedef void (PAGL_API_PTR *PFNGLCOMPRESSEDTEXIMAGE3DPROC)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void * data);
typedef void (PAGL_API_PTR *PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void * data);
typedef void (PAGL_API_PTR *PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void * data);
typedef void (PAGL_API_PTR *PFNGLCOPYBUFFERSUBDATAPROC)(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
typedef void (PAGL_API_PTR *PFNGLCOPYTEXIMAGE2DPROC)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (PAGL_API_PTR *PFNGLCOPYTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (PAGL_API_PTR *PFNGLCOPYTEXSUBIMAGE3DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef GLuint (PAGL_API_PTR *PFNGLCREATEPROGRAMPROC)(void);
typedef GLuint (PAGL_API_PTR *PFNGLCREATESHADERPROC)(GLenum type);
typedef void (PAGL_API_PTR *PFNGLCULLFACEPROC)(GLenum mode);
typedef void (PAGL_API_PTR *PFNGLDEBUGMESSAGECALLBACKKHRPROC)(GLDEBUGPROCKHR callback, const void * userParam);
typedef void (PAGL_API_PTR *PFNGLDEBUGMESSAGECONTROLKHRPROC)(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint * ids, GLboolean enabled);
typedef void (PAGL_API_PTR *PFNGLDEBUGMESSAGEINSERTKHRPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * buf);
typedef void (PAGL_API_PTR *PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint * buffers);
typedef void (PAGL_API_PTR *PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint * framebuffers);
typedef void (PAGL_API_PTR *PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (PAGL_API_PTR *PFNGLDELETEQUERIESPROC)(GLsizei n, const GLuint * ids);
typedef void (PAGL_API_PTR *PFNGLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint * renderbuffers);
typedef void (PAGL_API_PTR *PFNGLDELETESAMPLERSPROC)(GLsizei count, const GLuint * samplers);
typedef void (PAGL_API_PTR *PFNGLDELETESHADERPROC)(GLuint shader);
typedef void (PAGL_API_PTR *PFNGLDELETESYNCPROC)(GLsync sync);
typedef void (PAGL_API_PTR *PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint * textures);
typedef void (PAGL_API_PTR *PFNGLDELETETRANSFORMFEEDBACKSPROC)(GLsizei n, const GLuint * ids);
typedef void (PAGL_API_PTR *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint * arrays);
typedef void (PAGL_API_PTR *PFNGLDEPTHFUNCPROC)(GLenum func);
typedef void (PAGL_API_PTR *PFNGLDEPTHMASKPROC)(GLboolean flag);
typedef void (PAGL_API_PTR *PFNGLDEPTHRANGEFPROC)(GLfloat n, GLfloat f);
typedef void (PAGL_API_PTR *PFNGLDETACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (PAGL_API_PTR *PFNGLDISABLEPROC)(GLenum cap);
typedef void (PAGL_API_PTR *PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (PAGL_API_PTR *PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void (PAGL_API_PTR *PFNGLDRAWARRAYSINSTANCEDPROC)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
typedef void (PAGL_API_PTR *PFNGLDRAWBUFFERSPROC)(GLsizei n, const GLenum * bufs);
typedef void (PAGL_API_PTR *PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const void * indices);
typedef void (PAGL_API_PTR *PFNGLDRAWELEMENTSINSTANCEDPROC)(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount);
typedef void (PAGL_API_PTR *PFNGLDRAWRANGEELEMENTSPROC)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void * indices);
typedef void (PAGL_API_PTR *PFNGLENABLEPROC)(GLenum cap);
typedef void (PAGL_API_PTR *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (PAGL_API_PTR *PFNGLENDQUERYPROC)(GLenum target);
typedef void (PAGL_API_PTR *PFNGLENDTRANSFORMFEEDBACKPROC)(void);
typedef GLsync (PAGL_API_PTR *PFNGLFENCESYNCPROC)(GLenum condition, GLbitfield flags);
typedef void (PAGL_API_PTR *PFNGLFINISHPROC)(void);
typedef void (PAGL_API_PTR *PFNGLFLUSHPROC)(void);
typedef void (PAGL_API_PTR *PFNGLFLUSHMAPPEDBUFFERRANGEPROC)(GLenum target, GLintptr offset, GLsizeiptr length);
typedef void (PAGL_API_PTR *PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (PAGL_API_PTR *PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (PAGL_API_PTR *PFNGLFRAMEBUFFERTEXTURELAYERPROC)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (PAGL_API_PTR *PFNGLFRONTFACEPROC)(GLenum mode);
typedef void (PAGL_API_PTR *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint * buffers);
typedef void (PAGL_API_PTR *PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint * framebuffers);
typedef void (PAGL_API_PTR *PFNGLGENQUERIESPROC)(GLsizei n, GLuint * ids);
typedef void (PAGL_API_PTR *PFNGLGENRENDERBUFFERSPROC)(GLsizei n, GLuint * renderbuffers);
typedef void (PAGL_API_PTR *PFNGLGENSAMPLERSPROC)(GLsizei count, GLuint * samplers);
typedef void (PAGL_API_PTR *PFNGLGENTEXTURESPROC)(GLsizei n, GLuint * textures);
typedef void (PAGL_API_PTR *PFNGLGENTRANSFORMFEEDBACKSPROC)(GLsizei n, GLuint * ids);
typedef void (PAGL_API_PTR *PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint * arrays);
typedef void (PAGL_API_PTR *PFNGLGENERATEMIPMAPPROC)(GLenum target);
typedef void (PAGL_API_PTR *PFNGLGETACTIVEATTRIBPROC)(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name);
typedef void (PAGL_API_PTR *PFNGLGETACTIVEUNIFORMPROC)(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name);
typedef void (PAGL_API_PTR *PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC)(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei * length, GLchar * uniformBlockName);
typedef void (PAGL_API_PTR *PFNGLGETACTIVEUNIFORMBLOCKIVPROC)(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETACTIVEUNIFORMSIVPROC)(GLuint program, GLsizei uniformCount, const GLuint * uniformIndices, GLenum pname, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETATTACHEDSHADERSPROC)(GLuint program, GLsizei maxCount, GLsizei * count, GLuint * shaders);
typedef GLint (PAGL_API_PTR *PFNGLGETATTRIBLOCATIONPROC)(GLuint program, const GLchar * name);
typedef void (PAGL_API_PTR *PFNGLGETBOOLEANVPROC)(GLenum pname, GLboolean * data);
typedef void (PAGL_API_PTR *PFNGLGETBUFFERPARAMETERIVPROC)(GLenum target, GLenum pname, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETBUFFERPOINTERVPROC)(GLenum target, GLenum pname, void ** params);
typedef GLuint (PAGL_API_PTR *PFNGLGETDEBUGMESSAGELOGKHRPROC)(GLuint count, GLsizei bufSize, GLenum * sources, GLenum * types, GLuint * ids, GLenum * severities, GLsizei * lengths, GLchar * messageLog);
typedef GLuint (PAGL_API_PTR *PFNGLGETDEBUGMESSAGELOGPROC)(GLuint count, GLsizei bufSize, GLenum * sources, GLenum * types, GLuint * ids, GLenum * severities, GLsizei * lengths, GLchar * messageLog);
typedef GLenum (PAGL_API_PTR *PFNGLGETERRORPROC)(void);
typedef void (PAGL_API_PTR *PFNGLGETFLOATVPROC)(GLenum pname, GLfloat * data);
typedef GLint (PAGL_API_PTR *PFNGLGETFRAGDATALOCATIONPROC)(GLuint program, const GLchar * name);
typedef void (PAGL_API_PTR *PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)(GLenum target, GLenum attachment, GLenum pname, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETINTEGERI_VPROC)(GLenum target, GLuint index, GLint * data);
typedef void (PAGL_API_PTR *PFNGLGETINTEGERVPROC)(GLenum pname, GLint * data);
typedef void (PAGL_API_PTR *PFNGLGETINTERNALFORMATIVPROC)(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETOBJECTLABELKHRPROC)(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei * length, GLchar * label);
typedef void (PAGL_API_PTR *PFNGLGETOBJECTPTRLABELKHRPROC)(const void * ptr, GLsizei bufSize, GLsizei * length, GLchar * label);
typedef void (PAGL_API_PTR *PFNGLGETPOINTERVKHRPROC)(GLenum pname, void ** params);
typedef void (PAGL_API_PTR *PFNGLGETPROGRAMBINARYPROC)(GLuint program, GLsizei bufSize, GLsizei * length, GLenum * binaryFormat, void * binary);
typedef void (PAGL_API_PTR *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog);
typedef void (PAGL_API_PTR *PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETQUERYOBJECTUIVPROC)(GLuint id, GLenum pname, GLuint * params);
typedef void (PAGL_API_PTR *PFNGLGETQUERYIVPROC)(GLenum target, GLenum pname, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETRENDERBUFFERPARAMETERIVPROC)(GLenum target, GLenum pname, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETSAMPLERPARAMETERFVPROC)(GLuint sampler, GLenum pname, GLfloat * params);
typedef void (PAGL_API_PTR *PFNGLGETSAMPLERPARAMETERIVPROC)(GLuint sampler, GLenum pname, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog);
typedef void (PAGL_API_PTR *PFNGLGETSHADERPRECISIONFORMATPROC)(GLenum shadertype, GLenum precisiontype, GLint * range, GLint * precision);
typedef void (PAGL_API_PTR *PFNGLGETSHADERSOURCEPROC)(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * source);
typedef void (PAGL_API_PTR *PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint * params);
typedef const GLubyte * (PAGL_API_PTR *PFNGLGETSTRINGPROC)(GLenum name);
typedef const GLubyte * (PAGL_API_PTR *PFNGLGETSTRINGIPROC)(GLenum name, GLuint index);
typedef void (PAGL_API_PTR *PFNGLGETSYNCIVPROC)(GLsync sync, GLenum pname, GLsizei count, GLsizei * length, GLint * values);
typedef void (PAGL_API_PTR *PFNGLGETTEXPARAMETERFVPROC)(GLenum target, GLenum pname, GLfloat * params);
typedef void (PAGL_API_PTR *PFNGLGETTEXPARAMETERIVPROC)(GLenum target, GLenum pname, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETTRANSFORMFEEDBACKVARYINGPROC)(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLsizei * size, GLenum * type, GLchar * name);
typedef GLuint (PAGL_API_PTR *PFNGLGETUNIFORMBLOCKINDEXPROC)(GLuint program, const GLchar * uniformBlockName);
typedef void (PAGL_API_PTR *PFNGLGETUNIFORMINDICESPROC)(GLuint program, GLsizei uniformCount, const GLchar *const* uniformNames, GLuint * uniformIndices);
typedef GLint (PAGL_API_PTR *PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar * name);
typedef void (PAGL_API_PTR *PFNGLGETUNIFORMFVPROC)(GLuint program, GLint location, GLfloat * params);
typedef void (PAGL_API_PTR *PFNGLGETUNIFORMIVPROC)(GLuint program, GLint location, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETUNIFORMUIVPROC)(GLuint program, GLint location, GLuint * params);
typedef void (PAGL_API_PTR *PFNGLGETVERTEXATTRIBIIVPROC)(GLuint index, GLenum pname, GLint * params);
typedef void (PAGL_API_PTR *PFNGLGETVERTEXATTRIBIUIVPROC)(GLuint index, GLenum pname, GLuint * params);
typedef void (PAGL_API_PTR *PFNGLGETVERTEXATTRIBPOINTERVPROC)(GLuint index, GLenum pname, void ** pointer);
typedef void (PAGL_API_PTR *PFNGLGETVERTEXATTRIBFVPROC)(GLuint index, GLenum pname, GLfloat * params);
typedef void (PAGL_API_PTR *PFNGLGETVERTEXATTRIBIVPROC)(GLuint index, GLenum pname, GLint * params);
typedef void (PAGL_API_PTR *PFNGLHINTPROC)(GLenum target, GLenum mode);
typedef void (PAGL_API_PTR *PFNGLINVALIDATEFRAMEBUFFERPROC)(GLenum target, GLsizei numAttachments, const GLenum * attachments);
typedef void (PAGL_API_PTR *PFNGLINVALIDATESUBFRAMEBUFFERPROC)(GLenum target, GLsizei numAttachments, const GLenum * attachments, GLint x, GLint y, GLsizei width, GLsizei height);
typedef GLboolean (PAGL_API_PTR *PFNGLISBUFFERPROC)(GLuint buffer);
typedef GLboolean (PAGL_API_PTR *PFNGLISENABLEDPROC)(GLenum cap);
typedef GLboolean (PAGL_API_PTR *PFNGLISFRAMEBUFFERPROC)(GLuint framebuffer);
typedef GLboolean (PAGL_API_PTR *PFNGLISPROGRAMPROC)(GLuint program);
typedef GLboolean (PAGL_API_PTR *PFNGLISQUERYPROC)(GLuint id);
typedef GLboolean (PAGL_API_PTR *PFNGLISRENDERBUFFERPROC)(GLuint renderbuffer);
typedef GLboolean (PAGL_API_PTR *PFNGLISSAMPLERPROC)(GLuint sampler);
typedef GLboolean (PAGL_API_PTR *PFNGLISSHADERPROC)(GLuint shader);
typedef GLboolean (PAGL_API_PTR *PFNGLISSYNCPROC)(GLsync sync);
typedef GLboolean (PAGL_API_PTR *PFNGLISTEXTUREPROC)(GLuint texture);
typedef GLboolean (PAGL_API_PTR *PFNGLISTRANSFORMFEEDBACKPROC)(GLuint id);
typedef GLboolean (PAGL_API_PTR *PFNGLISVERTEXARRAYPROC)(GLuint array);
typedef void (PAGL_API_PTR *PFNGLLINEWIDTHPROC)(GLfloat width);
typedef void (PAGL_API_PTR *PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void * (PAGL_API_PTR *PFNGLMAPBUFFERRANGEPROC)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (PAGL_API_PTR *PFNGLOBJECTLABELKHRPROC)(GLenum identifier, GLuint name, GLsizei length, const GLchar * label);
typedef void (PAGL_API_PTR *PFNGLOBJECTPTRLABELKHRPROC)(const void * ptr, GLsizei length, const GLchar * label);
typedef void (PAGL_API_PTR *PFNGLPAUSETRANSFORMFEEDBACKPROC)(void);
typedef void (PAGL_API_PTR *PFNGLPIXELSTOREIPROC)(GLenum pname, GLint param);
typedef void (PAGL_API_PTR *PFNGLPOLYGONOFFSETPROC)(GLfloat factor, GLfloat units);
typedef void (PAGL_API_PTR *PFNGLPOPDEBUGGROUPKHRPROC)(void);
typedef void (PAGL_API_PTR *PFNGLPROGRAMBINARYPROC)(GLuint program, GLenum binaryFormat, const void * binary, GLsizei length);
typedef void (PAGL_API_PTR *PFNGLPROGRAMPARAMETERIPROC)(GLuint program, GLenum pname, GLint value);
typedef void (PAGL_API_PTR *PFNGLPUSHDEBUGGROUPKHRPROC)(GLenum source, GLuint id, GLsizei length, const GLchar * message);
typedef void (PAGL_API_PTR *PFNGLPUSHDEBUGGROUPPROC)(GLenum source, GLuint id, GLsizei length, const GLchar * message);
typedef void (PAGL_API_PTR *PFNGLREADBUFFERPROC)(GLenum src);
typedef void (PAGL_API_PTR *PFNGLREADPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void * pixels);
typedef void (PAGL_API_PTR *PFNGLRELEASESHADERCOMPILERPROC)(void);
typedef void (PAGL_API_PTR *PFNGLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (PAGL_API_PTR *PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (PAGL_API_PTR *PFNGLRESUMETRANSFORMFEEDBACKPROC)(void);
typedef void (PAGL_API_PTR *PFNGLSAMPLECOVERAGEPROC)(GLfloat value, GLboolean invert);
typedef void (PAGL_API_PTR *PFNGLSAMPLERPARAMETERFPROC)(GLuint sampler, GLenum pname, GLfloat param);
typedef void (PAGL_API_PTR *PFNGLSAMPLERPARAMETERFVPROC)(GLuint sampler, GLenum pname, const GLfloat * param);
typedef void (PAGL_API_PTR *PFNGLSAMPLERPARAMETERIPROC)(GLuint sampler, GLenum pname, GLint param);
typedef void (PAGL_API_PTR *PFNGLSAMPLERPARAMETERIVPROC)(GLuint sampler, GLenum pname, const GLint * param);
typedef void (PAGL_API_PTR *PFNGLSCISSORPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (PAGL_API_PTR *PFNGLSHADERBINARYPROC)(GLsizei count, const GLuint * shaders, GLenum binaryFormat, const void * binary, GLsizei length);
typedef void (PAGL_API_PTR *PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar *const* string, const GLint * length);
typedef void (PAGL_API_PTR *PFNGLSTENCILFUNCPROC)(GLenum func, GLint ref, GLuint mask);
typedef void (PAGL_API_PTR *PFNGLSTENCILFUNCSEPARATEPROC)(GLenum face, GLenum func, GLint ref, GLuint mask);
typedef void (PAGL_API_PTR *PFNGLSTENCILMASKPROC)(GLuint mask);
typedef void (PAGL_API_PTR *PFNGLSTENCILMASKSEPARATEPROC)(GLenum face, GLuint mask);
typedef void (PAGL_API_PTR *PFNGLSTENCILOPPROC)(GLenum fail, GLenum zfail, GLenum zpass);
typedef void (PAGL_API_PTR *PFNGLSTENCILOPSEPARATEPROC)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
typedef void (PAGL_API_PTR *PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void * pixels);
typedef void (PAGL_API_PTR *PFNGLTEXIMAGE3DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void * pixels);
typedef void (PAGL_API_PTR *PFNGLTEXPARAMETERFPROC)(GLenum target, GLenum pname, GLfloat param);
typedef void (PAGL_API_PTR *PFNGLTEXPARAMETERFVPROC)(GLenum target, GLenum pname, const GLfloat * params);
typedef void (PAGL_API_PTR *PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void (PAGL_API_PTR *PFNGLTEXPARAMETERIVPROC)(GLenum target, GLenum pname, const GLint * params);
typedef void (PAGL_API_PTR *PFNGLTEXSTORAGE2DPROC)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (PAGL_API_PTR *PFNGLTEXSTORAGE3DPROC)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (PAGL_API_PTR *PFNGLTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void * pixels);
typedef void (PAGL_API_PTR *PFNGLTEXSUBIMAGE3DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void * pixels);
typedef void (PAGL_API_PTR *PFNGLTRANSFORMFEEDBACKVARYINGSPROC)(GLuint program, GLsizei count, const GLchar *const* varyings, GLenum bufferMode);
typedef void (PAGL_API_PTR *PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (PAGL_API_PTR *PFNGLUNIFORM1FVPROC)(GLint location, GLsizei count, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (PAGL_API_PTR *PFNGLUNIFORM1IVPROC)(GLint location, GLsizei count, const GLint * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORM1UIPROC)(GLint location, GLuint v0);
typedef void (PAGL_API_PTR *PFNGLUNIFORM1UIVPROC)(GLint location, GLsizei count, const GLuint * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (PAGL_API_PTR *PFNGLUNIFORM2FVPROC)(GLint location, GLsizei count, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORM2IPROC)(GLint location, GLint v0, GLint v1);
typedef void (PAGL_API_PTR *PFNGLUNIFORM2IVPROC)(GLint location, GLsizei count, const GLint * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORM2UIPROC)(GLint location, GLuint v0, GLuint v1);
typedef void (PAGL_API_PTR *PFNGLUNIFORM2UIVPROC)(GLint location, GLsizei count, const GLuint * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (PAGL_API_PTR *PFNGLUNIFORM3FVPROC)(GLint location, GLsizei count, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORM3IPROC)(GLint location, GLint v0, GLint v1, GLint v2);
typedef void (PAGL_API_PTR *PFNGLUNIFORM3IVPROC)(GLint location, GLsizei count, const GLint * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORM3UIPROC)(GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (PAGL_API_PTR *PFNGLUNIFORM3UIVPROC)(GLint location, GLsizei count, const GLuint * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (PAGL_API_PTR *PFNGLUNIFORM4FVPROC)(GLint location, GLsizei count, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORM4IPROC)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (PAGL_API_PTR *PFNGLUNIFORM4IVPROC)(GLint location, GLsizei count, const GLint * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORM4UIPROC)(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef void (PAGL_API_PTR *PFNGLUNIFORM4UIVPROC)(GLint location, GLsizei count, const GLuint * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORMBLOCKBINDINGPROC)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
typedef void (PAGL_API_PTR *PFNGLUNIFORMMATRIX2FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORMMATRIX2X3FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORMMATRIX2X4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORMMATRIX3FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORMMATRIX3X2FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORMMATRIX3X4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORMMATRIX4X2FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
typedef void (PAGL_API_PTR *PFNGLUNIFORMMATRIX4X3FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
typedef GLboolean (PAGL_API_PTR *PFNGLUNMAPBUFFERPROC)(GLenum target);
typedef void (PAGL_API_PTR *PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (PAGL_API_PTR *PFNGLVALIDATEPROGRAMPROC)(GLuint program);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIB1FPROC)(GLuint index, GLfloat x);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIB1FVPROC)(GLuint index, const GLfloat * v);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIB2FPROC)(GLuint index, GLfloat x, GLfloat y);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIB2FVPROC)(GLuint index, const GLfloat * v);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIB3FPROC)(GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIB3FVPROC)(GLuint index, const GLfloat * v);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIB4FPROC)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIB4FVPROC)(GLuint index, const GLfloat * v);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIBDIVISORPROC)(GLuint index, GLuint divisor);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIBI4IPROC)(GLuint index, GLint x, GLint y, GLint z, GLint w);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIBI4IVPROC)(GLuint index, const GLint * v);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIBI4UIPROC)(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIBI4UIVPROC)(GLuint index, const GLuint * v);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIBIPOINTERPROC)(GLuint index, GLint size, GLenum type, GLsizei stride, const void * pointer);
typedef void (PAGL_API_PTR *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void * pointer);
typedef void (PAGL_API_PTR *PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);

typedef void (PAGL_API_PTR *PFNGLPOLYGONMODEPROC)(GLenum face, GLenum mode);//EXT
typedef void (PAGL_API_PTR *PFNGLDEBUGMESSAGECALLBACKPROC)(
        void (PAGL_API_PTR *callback)(
                GLenum source,
                GLenum type,
                GLuint id,
                GLenum severity,
                GLsizei length,
                const GLchar *message,
                const void *userParam
        ),
        const void *userParam
);

typedef void (PAGL_API_PTR *PFNGLDEBUGMESSAGECONTROLPROC)(
        GLenum source,
        GLenum type,
        GLenum severity,
        GLsizei count,
        const GLuint *ids,
        GLboolean enabled
);

typedef void (PAGL_API_PTR *PFNGLDEBUGMESSAGEINSERTPROC)(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar *buf
);

// --------------------------------------------------------------------------------
// 全局函数指针声明（extern）
//
// 你可以在 debug 初始化代码中动态加载并赋值，比如：
//   glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)load("glDebugMessageCallback");
// --------------------------------------------------------------------------------




PAGL_API_CALL PFNGLACTIVETEXTUREPROC PAGL_ActiveTexture;
PAGL_API_CALL PFNGLATTACHSHADERPROC PAGL_AttachShader;
PAGL_API_CALL PFNGLBEGINQUERYPROC PAGL_BeginQuery;
PAGL_API_CALL PFNGLBEGINTRANSFORMFEEDBACKPROC PAGL_BeginTransformFeedback;
PAGL_API_CALL PFNGLBINDATTRIBLOCATIONPROC PAGL_BindAttribLocation;
PAGL_API_CALL PFNGLBINDBUFFERPROC PAGL_BindBuffer;
PAGL_API_CALL PFNGLBINDBUFFERBASEPROC PAGL_BindBufferBase;
PAGL_API_CALL PFNGLBINDBUFFERRANGEPROC PAGL_BindBufferRange;
PAGL_API_CALL PFNGLBINDFRAMEBUFFERPROC PAGL_BindFramebuffer;
PAGL_API_CALL PFNGLBINDRENDERBUFFERPROC PAGL_BindRenderbuffer;
PAGL_API_CALL PFNGLBINDSAMPLERPROC PAGL_BindSampler;
PAGL_API_CALL PFNGLBINDTEXTUREPROC PAGL_BindTexture;
PAGL_API_CALL PFNGLBINDTRANSFORMFEEDBACKPROC PAGL_BindTransformFeedback;
PAGL_API_CALL PFNGLBINDVERTEXARRAYPROC PAGL_BindVertexArray;
PAGL_API_CALL PFNGLBLENDCOLORPROC PAGL_BlendColor;
PAGL_API_CALL PFNGLBLENDEQUATIONPROC PAGL_BlendEquation;
PAGL_API_CALL PFNGLBLENDEQUATIONSEPARATEPROC PAGL_BlendEquationSeparate;
PAGL_API_CALL PFNGLBLENDFUNCPROC PAGL_BlendFunc;
PAGL_API_CALL PFNGLBLENDFUNCSEPARATEPROC PAGL_BlendFuncSeparate;
PAGL_API_CALL PFNGLBLITFRAMEBUFFERPROC PAGL_BlitFramebuffer;
PAGL_API_CALL PFNGLBUFFERDATAPROC PAGL_BufferData;
PAGL_API_CALL PFNGLBUFFERSUBDATAPROC PAGL_BufferSubData;
PAGL_API_CALL PFNGLCHECKFRAMEBUFFERSTATUSPROC PAGL_CheckFramebufferStatus;
PAGL_API_CALL PFNGLCLEARPROC PAGL_Clear;
PAGL_API_CALL PFNGLCLEARBUFFERFIPROC PAGL_ClearBufferfi;
PAGL_API_CALL PFNGLCLEARBUFFERFVPROC PAGL_ClearBufferfv;
PAGL_API_CALL PFNGLCLEARBUFFERIVPROC PAGL_ClearBufferiv;
PAGL_API_CALL PFNGLCLEARBUFFERUIVPROC PAGL_ClearBufferuiv;
PAGL_API_CALL PFNGLCLEARCOLORPROC PAGL_ClearColor;
PAGL_API_CALL PFNGLCLEARDEPTHFPROC PAGL_ClearDepthf;
PAGL_API_CALL PFNGLCLEARSTENCILPROC PAGL_ClearStencil;
PAGL_API_CALL PFNGLCOLORMASKPROC PAGL_ColorMask;
PAGL_API_CALL PFNGLCOMPILESHADERPROC PAGL_CompileShader;
PAGL_API_CALL PFNGLCOMPRESSEDTEXIMAGE2DPROC PAGL_CompressedTexImage2D;
PAGL_API_CALL PFNGLCOMPRESSEDTEXIMAGE3DPROC PAGL_CompressedTexImage3D;
PAGL_API_CALL PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC PAGL_CompressedTexSubImage2D;
PAGL_API_CALL PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC PAGL_CompressedTexSubImage3D;
PAGL_API_CALL PFNGLCOPYBUFFERSUBDATAPROC PAGL_CopyBufferSubData;
PAGL_API_CALL PFNGLCOPYTEXIMAGE2DPROC PAGL_CopyTexImage2D;
PAGL_API_CALL PFNGLCOPYTEXSUBIMAGE2DPROC PAGL_CopyTexSubImage2D;
PAGL_API_CALL PFNGLCOPYTEXSUBIMAGE3DPROC PAGL_CopyTexSubImage3D;
PAGL_API_CALL PFNGLCREATEPROGRAMPROC PAGL_CreateProgram;
PAGL_API_CALL PFNGLCREATESHADERPROC PAGL_CreateShader;
PAGL_API_CALL PFNGLCULLFACEPROC PAGL_CullFace;
PAGL_API_CALL PFNGLDEBUGMESSAGECALLBACKKHRPROC PAGL_DebugMessageCallbackKHR;
PAGL_API_CALL PFNGLDEBUGMESSAGECONTROLKHRPROC PAGL_DebugMessageControlKHR;
PAGL_API_CALL PFNGLDEBUGMESSAGEINSERTKHRPROC PAGL_DebugMessageInsertKHR;
PAGL_API_CALL PFNGLDELETEBUFFERSPROC PAGL_DeleteBuffers;
PAGL_API_CALL PFNGLDELETEFRAMEBUFFERSPROC PAGL_DeleteFramebuffers;
PAGL_API_CALL PFNGLDELETEPROGRAMPROC PAGL_DeleteProgram;
PAGL_API_CALL PFNGLDELETEQUERIESPROC PAGL_DeleteQueries;
PAGL_API_CALL PFNGLDELETERENDERBUFFERSPROC PAGL_DeleteRenderbuffers;
PAGL_API_CALL PFNGLDELETESAMPLERSPROC PAGL_DeleteSamplers;
PAGL_API_CALL PFNGLDELETESHADERPROC PAGL_DeleteShader;
PAGL_API_CALL PFNGLDELETESYNCPROC PAGL_DeleteSync;
PAGL_API_CALL PFNGLDELETETEXTURESPROC PAGL_DeleteTextures;
PAGL_API_CALL PFNGLDELETETRANSFORMFEEDBACKSPROC PAGL_DeleteTransformFeedbacks;
PAGL_API_CALL PFNGLDELETEVERTEXARRAYSPROC PAGL_DeleteVertexArrays;
PAGL_API_CALL PFNGLDEPTHFUNCPROC PAGL_DepthFunc;
PAGL_API_CALL PFNGLDEPTHMASKPROC PAGL_DepthMask;
PAGL_API_CALL PFNGLDEPTHRANGEFPROC PAGL_DepthRangef;
PAGL_API_CALL PFNGLDETACHSHADERPROC PAGL_DetachShader;
PAGL_API_CALL PFNGLDISABLEPROC PAGL_Disable;
PAGL_API_CALL PFNGLDISABLEVERTEXATTRIBARRAYPROC PAGL_DisableVertexAttribArray;
PAGL_API_CALL PFNGLDRAWARRAYSPROC PAGL_DrawArrays;
PAGL_API_CALL PFNGLDRAWARRAYSINSTANCEDPROC PAGL_DrawArraysInstanced;
PAGL_API_CALL PFNGLDRAWBUFFERSPROC PAGL_DrawBuffers;
PAGL_API_CALL PFNGLDRAWELEMENTSPROC PAGL_DrawElements;
PAGL_API_CALL PFNGLDRAWELEMENTSINSTANCEDPROC PAGL_DrawElementsInstanced;
PAGL_API_CALL PFNGLDRAWRANGEELEMENTSPROC PAGL_DrawRangeElements;
PAGL_API_CALL PFNGLENABLEPROC PAGL_Enable;
PAGL_API_CALL PFNGLENABLEVERTEXATTRIBARRAYPROC PAGL_EnableVertexAttribArray;
PAGL_API_CALL PFNGLENDQUERYPROC PAGL_EndQuery;
PAGL_API_CALL PFNGLENDTRANSFORMFEEDBACKPROC PAGL_EndTransformFeedback;
PAGL_API_CALL PFNGLFENCESYNCPROC PAGL_FenceSync;
PAGL_API_CALL PFNGLFINISHPROC PAGL_Finish;
PAGL_API_CALL PFNGLFLUSHPROC PAGL_Flush;
PAGL_API_CALL PFNGLFLUSHMAPPEDBUFFERRANGEPROC PAGL_FlushMappedBufferRange;
PAGL_API_CALL PFNGLFRAMEBUFFERRENDERBUFFERPROC PAGL_FramebufferRenderbuffer;
PAGL_API_CALL PFNGLFRAMEBUFFERTEXTURE2DPROC PAGL_FramebufferTexture2D;
PAGL_API_CALL PFNGLFRAMEBUFFERTEXTURELAYERPROC PAGL_FramebufferTextureLayer;
PAGL_API_CALL PFNGLFRONTFACEPROC PAGL_FrontFace;
PAGL_API_CALL PFNGLGENBUFFERSPROC PAGL_GenBuffers;
PAGL_API_CALL PFNGLGENFRAMEBUFFERSPROC PAGL_GenFramebuffers;
PAGL_API_CALL PFNGLGENQUERIESPROC PAGL_GenQueries;
PAGL_API_CALL PFNGLGENRENDERBUFFERSPROC PAGL_GenRenderbuffers;
PAGL_API_CALL PFNGLGENSAMPLERSPROC PAGL_GenSamplers;
PAGL_API_CALL PFNGLGENTEXTURESPROC PAGL_GenTextures;
PAGL_API_CALL PFNGLGENTRANSFORMFEEDBACKSPROC PAGL_GenTransformFeedbacks;
PAGL_API_CALL PFNGLGENVERTEXARRAYSPROC PAGL_GenVertexArrays;
PAGL_API_CALL PFNGLGENERATEMIPMAPPROC PAGL_GenerateMipmap;
PAGL_API_CALL PFNGLGETACTIVEATTRIBPROC PAGL_GetActiveAttrib;
PAGL_API_CALL PFNGLGETACTIVEUNIFORMPROC PAGL_GetActiveUniform;
PAGL_API_CALL PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC PAGL_GetActiveUniformBlockName;
PAGL_API_CALL PFNGLGETACTIVEUNIFORMBLOCKIVPROC PAGL_GetActiveUniformBlockiv;
PAGL_API_CALL PFNGLGETACTIVEUNIFORMSIVPROC PAGL_GetActiveUniformsiv;
PAGL_API_CALL PFNGLGETATTACHEDSHADERSPROC PAGL_GetAttachedShaders;
PAGL_API_CALL PFNGLGETATTRIBLOCATIONPROC PAGL_GetAttribLocation;
PAGL_API_CALL PFNGLGETBOOLEANVPROC PAGL_GetBooleanv;
PAGL_API_CALL PFNGLGETBUFFERPARAMETERIVPROC PAGL_GetBufferParameteriv;
PAGL_API_CALL PFNGLGETBUFFERPOINTERVPROC PAGL_GetBufferPointerv;
PAGL_API_CALL PFNGLGETDEBUGMESSAGELOGKHRPROC PAGL_GetDebugMessageLogKHR;
PAGL_API_CALL PFNGLGETDEBUGMESSAGELOGPROC PAGL_GetDebugMessageLog;
PAGL_API_CALL PFNGLGETERRORPROC PAGL_GetError;
PAGL_API_CALL PFNGLGETFLOATVPROC PAGL_GetFloatv;
PAGL_API_CALL PFNGLGETFRAGDATALOCATIONPROC PAGL_GetFragDataLocation;
PAGL_API_CALL PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC PAGL_GetFramebufferAttachmentParameteriv;
PAGL_API_CALL PFNGLGETINTEGERI_VPROC PAGL_GetIntegeri_v;
PAGL_API_CALL PFNGLGETINTEGERVPROC PAGL_GetIntegerv;
PAGL_API_CALL PFNGLGETINTERNALFORMATIVPROC PAGL_GetInternalformativ;
PAGL_API_CALL PFNGLGETOBJECTLABELKHRPROC PAGL_GetObjectLabelKHR;
PAGL_API_CALL PFNGLGETOBJECTPTRLABELKHRPROC PAGL_GetObjectPtrLabelKHR;
PAGL_API_CALL PFNGLGETPOINTERVKHRPROC PAGL_GetPointervKHR;
PAGL_API_CALL PFNGLGETPROGRAMBINARYPROC PAGL_GetProgramBinary;
PAGL_API_CALL PFNGLGETPROGRAMINFOLOGPROC PAGL_GetProgramInfoLog;
PAGL_API_CALL PFNGLGETPROGRAMIVPROC PAGL_GetProgramiv;
PAGL_API_CALL PFNGLGETQUERYOBJECTUIVPROC PAGL_GetQueryObjectuiv;
PAGL_API_CALL PFNGLGETQUERYIVPROC PAGL_GetQueryiv;
PAGL_API_CALL PFNGLGETRENDERBUFFERPARAMETERIVPROC PAGL_GetRenderbufferParameteriv;
PAGL_API_CALL PFNGLGETSAMPLERPARAMETERFVPROC PAGL_GetSamplerParameterfv;
PAGL_API_CALL PFNGLGETSAMPLERPARAMETERIVPROC PAGL_GetSamplerParameteriv;
PAGL_API_CALL PFNGLGETSHADERINFOLOGPROC PAGL_GetShaderInfoLog;
PAGL_API_CALL PFNGLGETSHADERPRECISIONFORMATPROC PAGL_GetShaderPrecisionFormat;
PAGL_API_CALL PFNGLGETSHADERSOURCEPROC PAGL_GetShaderSource;
PAGL_API_CALL PFNGLGETSHADERIVPROC PAGL_GetShaderiv;
PAGL_API_CALL PFNGLGETSTRINGPROC PAGL_GetString;
PAGL_API_CALL PFNGLGETSTRINGIPROC PAGL_GetStringi;
PAGL_API_CALL PFNGLGETSYNCIVPROC PAGL_GetSynciv;
PAGL_API_CALL PFNGLGETTEXPARAMETERFVPROC PAGL_GetTexParameterfv;
PAGL_API_CALL PFNGLGETTEXPARAMETERIVPROC PAGL_GetTexParameteriv;
PAGL_API_CALL PFNGLGETTRANSFORMFEEDBACKVARYINGPROC PAGL_GetTransformFeedbackVarying;
PAGL_API_CALL PFNGLGETUNIFORMBLOCKINDEXPROC PAGL_GetUniformBlockIndex;
PAGL_API_CALL PFNGLGETUNIFORMINDICESPROC PAGL_GetUniformIndices;
PAGL_API_CALL PFNGLGETUNIFORMLOCATIONPROC PAGL_GetUniformLocation;
PAGL_API_CALL PFNGLGETUNIFORMFVPROC PAGL_GetUniformfv;
PAGL_API_CALL PFNGLGETUNIFORMIVPROC PAGL_GetUniformiv;
PAGL_API_CALL PFNGLGETUNIFORMUIVPROC PAGL_GetUniformuiv;
PAGL_API_CALL PFNGLGETVERTEXATTRIBIIVPROC PAGL_GetVertexAttribIiv;
PAGL_API_CALL PFNGLGETVERTEXATTRIBIUIVPROC PAGL_GetVertexAttribIuiv;
PAGL_API_CALL PFNGLGETVERTEXATTRIBPOINTERVPROC PAGL_GetVertexAttribPointerv;
PAGL_API_CALL PFNGLGETVERTEXATTRIBFVPROC PAGL_GetVertexAttribfv;
PAGL_API_CALL PFNGLGETVERTEXATTRIBIVPROC PAGL_GetVertexAttribiv;
PAGL_API_CALL PFNGLHINTPROC PAGL_Hint;
PAGL_API_CALL PFNGLINVALIDATEFRAMEBUFFERPROC PAGL_InvalidateFramebuffer;
PAGL_API_CALL PFNGLINVALIDATESUBFRAMEBUFFERPROC PAGL_InvalidateSubFramebuffer;
PAGL_API_CALL PFNGLISBUFFERPROC PAGL_IsBuffer;
PAGL_API_CALL PFNGLISENABLEDPROC PAGL_IsEnabled;
PAGL_API_CALL PFNGLISFRAMEBUFFERPROC PAGL_IsFramebuffer;
PAGL_API_CALL PFNGLISPROGRAMPROC PAGL_IsProgram;
PAGL_API_CALL PFNGLISQUERYPROC PAGL_IsQuery;
PAGL_API_CALL PFNGLISRENDERBUFFERPROC PAGL_IsRenderbuffer;
PAGL_API_CALL PFNGLISSAMPLERPROC PAGL_IsSampler;
PAGL_API_CALL PFNGLISSHADERPROC PAGL_IsShader;
PAGL_API_CALL PFNGLISSYNCPROC PAGL_IsSync;
PAGL_API_CALL PFNGLISTEXTUREPROC PAGL_IsTexture;
PAGL_API_CALL PFNGLISTRANSFORMFEEDBACKPROC PAGL_IsTransformFeedback;
PAGL_API_CALL PFNGLISVERTEXARRAYPROC PAGL_IsVertexArray;
PAGL_API_CALL PFNGLLINEWIDTHPROC PAGL_LineWidth;
PAGL_API_CALL PFNGLLINKPROGRAMPROC PAGL_LinkProgram;
PAGL_API_CALL PFNGLMAPBUFFERRANGEPROC PAGL_MapBufferRange;
PAGL_API_CALL PFNGLOBJECTLABELKHRPROC PAGL_ObjectLabelKHR;
PAGL_API_CALL PFNGLOBJECTPTRLABELKHRPROC PAGL_ObjectPtrLabelKHR;
PAGL_API_CALL PFNGLPAUSETRANSFORMFEEDBACKPROC PAGL_PauseTransformFeedback;
PAGL_API_CALL PFNGLPIXELSTOREIPROC PAGL_PixelStorei;
PAGL_API_CALL PFNGLPOLYGONOFFSETPROC PAGL_PolygonOffset;
PAGL_API_CALL PFNGLPOPDEBUGGROUPKHRPROC PAGL_PopDebugGroupKHR;
PAGL_API_CALL PFNGLPROGRAMBINARYPROC PAGL_ProgramBinary;
PAGL_API_CALL PFNGLPROGRAMPARAMETERIPROC PAGL_ProgramParameteri;
PAGL_API_CALL PFNGLPUSHDEBUGGROUPKHRPROC PAGL_PushDebugGroupKHR;
PAGL_API_CALL PFNGLREADBUFFERPROC PAGL_ReadBuffer;
PAGL_API_CALL PFNGLREADPIXELSPROC PAGL_ReadPixels;
PAGL_API_CALL PFNGLRELEASESHADERCOMPILERPROC PAGL_ReleaseShaderCompiler;
PAGL_API_CALL PFNGLRENDERBUFFERSTORAGEPROC PAGL_RenderbufferStorage;
PAGL_API_CALL PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC PAGL_RenderbufferStorageMultisample;
PAGL_API_CALL PFNGLRESUMETRANSFORMFEEDBACKPROC PAGL_ResumeTransformFeedback;
PAGL_API_CALL PFNGLSAMPLECOVERAGEPROC PAGL_SampleCoverage;
PAGL_API_CALL PFNGLSAMPLERPARAMETERFPROC PAGL_SamplerParameterf;
PAGL_API_CALL PFNGLSAMPLERPARAMETERFVPROC PAGL_SamplerParameterfv;
PAGL_API_CALL PFNGLSAMPLERPARAMETERIPROC PAGL_SamplerParameteri;
PAGL_API_CALL PFNGLSAMPLERPARAMETERIVPROC PAGL_SamplerParameteriv;
PAGL_API_CALL PFNGLSCISSORPROC PAGL_Scissor;
PAGL_API_CALL PFNGLSHADERBINARYPROC PAGL_ShaderBinary;
PAGL_API_CALL PFNGLSHADERSOURCEPROC PAGL_ShaderSource;
PAGL_API_CALL PFNGLSTENCILFUNCPROC PAGL_StencilFunc;
PAGL_API_CALL PFNGLSTENCILFUNCSEPARATEPROC PAGL_StencilFuncSeparate;
PAGL_API_CALL PFNGLSTENCILMASKPROC PAGL_StencilMask;
PAGL_API_CALL PFNGLSTENCILMASKSEPARATEPROC PAGL_StencilMaskSeparate;
PAGL_API_CALL PFNGLSTENCILOPPROC PAGL_StencilOp;
PAGL_API_CALL PFNGLSTENCILOPSEPARATEPROC PAGL_StencilOpSeparate;
PAGL_API_CALL PFNGLTEXIMAGE2DPROC PAGL_TexImage2D;
PAGL_API_CALL PFNGLTEXIMAGE3DPROC PAGL_TexImage3D;
PAGL_API_CALL PFNGLTEXPARAMETERFPROC PAGL_TexParameterf;
PAGL_API_CALL PFNGLTEXPARAMETERFVPROC PAGL_TexParameterfv;
PAGL_API_CALL PFNGLTEXPARAMETERIPROC PAGL_TexParameteri;
PAGL_API_CALL PFNGLTEXPARAMETERIVPROC PAGL_TexParameteriv;
PAGL_API_CALL PFNGLTEXSTORAGE2DPROC PAGL_TexStorage2D;
PAGL_API_CALL PFNGLTEXSTORAGE3DPROC PAGL_TexStorage3D;
PAGL_API_CALL PFNGLTEXSUBIMAGE2DPROC PAGL_TexSubImage2D;
PAGL_API_CALL PFNGLTEXSUBIMAGE3DPROC PAGL_TexSubImage3D;
PAGL_API_CALL PFNGLTRANSFORMFEEDBACKVARYINGSPROC PAGL_TransformFeedbackVaryings;
PAGL_API_CALL PFNGLUNIFORM1FPROC PAGL_Uniform1f;
PAGL_API_CALL PFNGLUNIFORM1FVPROC PAGL_Uniform1fv;
PAGL_API_CALL PFNGLUNIFORM1IPROC PAGL_Uniform1i;
PAGL_API_CALL PFNGLUNIFORM1IVPROC PAGL_Uniform1iv;
PAGL_API_CALL PFNGLUNIFORM1UIPROC PAGL_Uniform1ui;
PAGL_API_CALL PFNGLUNIFORM1UIVPROC PAGL_Uniform1uiv;
PAGL_API_CALL PFNGLUNIFORM2FPROC PAGL_Uniform2f;
PAGL_API_CALL PFNGLUNIFORM2FVPROC PAGL_Uniform2fv;
PAGL_API_CALL PFNGLUNIFORM2IPROC PAGL_Uniform2i;
PAGL_API_CALL PFNGLUNIFORM2IVPROC PAGL_Uniform2iv;
PAGL_API_CALL PFNGLUNIFORM2UIPROC PAGL_Uniform2ui;
PAGL_API_CALL PFNGLUNIFORM2UIVPROC PAGL_Uniform2uiv;
PAGL_API_CALL PFNGLUNIFORM3FPROC PAGL_Uniform3f;
PAGL_API_CALL PFNGLUNIFORM3FVPROC PAGL_Uniform3fv;
PAGL_API_CALL PFNGLUNIFORM3IPROC PAGL_Uniform3i;
PAGL_API_CALL PFNGLUNIFORM3IVPROC PAGL_Uniform3iv;
PAGL_API_CALL PFNGLUNIFORM3UIPROC PAGL_Uniform3ui;
PAGL_API_CALL PFNGLUNIFORM3UIVPROC PAGL_Uniform3uiv;
PAGL_API_CALL PFNGLUNIFORM4FPROC PAGL_Uniform4f;
PAGL_API_CALL PFNGLUNIFORM4FVPROC PAGL_Uniform4fv;
PAGL_API_CALL PFNGLUNIFORM4IPROC PAGL_Uniform4i;
PAGL_API_CALL PFNGLUNIFORM4IVPROC PAGL_Uniform4iv;
PAGL_API_CALL PFNGLUNIFORM4UIPROC PAGL_Uniform4ui;
PAGL_API_CALL PFNGLUNIFORM4UIVPROC PAGL_Uniform4uiv;
PAGL_API_CALL PFNGLUNIFORMBLOCKBINDINGPROC PAGL_UniformBlockBinding;
PAGL_API_CALL PFNGLUNIFORMMATRIX2FVPROC PAGL_UniformMatrix2fv;
PAGL_API_CALL PFNGLUNIFORMMATRIX2X3FVPROC PAGL_UniformMatrix2x3fv;
PAGL_API_CALL PFNGLUNIFORMMATRIX2X4FVPROC PAGL_UniformMatrix2x4fv;
PAGL_API_CALL PFNGLUNIFORMMATRIX3FVPROC PAGL_UniformMatrix3fv;
PAGL_API_CALL PFNGLUNIFORMMATRIX3X2FVPROC PAGL_UniformMatrix3x2fv;
PAGL_API_CALL PFNGLUNIFORMMATRIX3X4FVPROC PAGL_UniformMatrix3x4fv;
PAGL_API_CALL PFNGLUNIFORMMATRIX4FVPROC PAGL_UniformMatrix4fv;
PAGL_API_CALL PFNGLUNIFORMMATRIX4X2FVPROC PAGL_UniformMatrix4x2fv;
PAGL_API_CALL PFNGLUNIFORMMATRIX4X3FVPROC PAGL_UniformMatrix4x3fv;
PAGL_API_CALL PFNGLUNMAPBUFFERPROC PAGL_UnmapBuffer;
PAGL_API_CALL PFNGLUSEPROGRAMPROC PAGL_UseProgram;
PAGL_API_CALL PFNGLVALIDATEPROGRAMPROC PAGL_ValidateProgram;
PAGL_API_CALL PFNGLVERTEXATTRIB1FPROC PAGL_VertexAttrib1f;
PAGL_API_CALL PFNGLVERTEXATTRIB1FVPROC PAGL_VertexAttrib1fv;
PAGL_API_CALL PFNGLVERTEXATTRIB2FPROC PAGL_VertexAttrib2f;
PAGL_API_CALL PFNGLVERTEXATTRIB2FVPROC PAGL_VertexAttrib2fv;
PAGL_API_CALL PFNGLVERTEXATTRIB3FPROC PAGL_VertexAttrib3f;
PAGL_API_CALL PFNGLVERTEXATTRIB3FVPROC PAGL_VertexAttrib3fv;
PAGL_API_CALL PFNGLVERTEXATTRIB4FPROC PAGL_VertexAttrib4f;
PAGL_API_CALL PFNGLVERTEXATTRIB4FVPROC PAGL_VertexAttrib4fv;
PAGL_API_CALL PFNGLVERTEXATTRIBDIVISORPROC PAGL_VertexAttribDivisor;
PAGL_API_CALL PFNGLVERTEXATTRIBI4IPROC PAGL_VertexAttribI4i;
PAGL_API_CALL PFNGLVERTEXATTRIBI4IVPROC PAGL_VertexAttribI4iv;
PAGL_API_CALL PFNGLVERTEXATTRIBI4UIPROC PAGL_VertexAttribI4ui;
PAGL_API_CALL PFNGLVERTEXATTRIBI4UIVPROC PAGL_VertexAttribI4uiv;
PAGL_API_CALL PFNGLVERTEXATTRIBIPOINTERPROC PAGL_VertexAttribIPointer;
PAGL_API_CALL PFNGLVERTEXATTRIBPOINTERPROC PAGL_VertexAttribPointer;
PAGL_API_CALL PFNGLVIEWPORTPROC PAGL_Viewport;
PAGL_API_CALL PFNGLDEBUGMESSAGECALLBACKPROC PAGL_DebugMessageCallback;
PAGL_API_CALL PFNGLDEBUGMESSAGECONTROLPROC  PAGL_DebugMessageControl;






int PAGL_GL_ES_VERSION_2_0 = 1;
int PAGL_GL_ES_VERSION_3_0 = 1;
int PAGL_GL_KHR_debug = 1;



PFNGLACTIVETEXTUREPROC PAGL_ActiveTexture = NULL;
PFNGLATTACHSHADERPROC PAGL_AttachShader = NULL;
PFNGLBEGINQUERYPROC PAGL_BeginQuery = NULL;
PFNGLBEGINTRANSFORMFEEDBACKPROC PAGL_BeginTransformFeedback = NULL;
PFNGLBINDATTRIBLOCATIONPROC PAGL_BindAttribLocation = NULL;
PFNGLBINDBUFFERPROC PAGL_BindBuffer = NULL;
PFNGLBINDBUFFERBASEPROC PAGL_BindBufferBase = NULL;
PFNGLBINDBUFFERRANGEPROC PAGL_BindBufferRange = NULL;
PFNGLBINDFRAMEBUFFERPROC PAGL_BindFramebuffer = NULL;
PFNGLBINDRENDERBUFFERPROC PAGL_BindRenderbuffer = NULL;
PFNGLBINDSAMPLERPROC PAGL_BindSampler = NULL;
PFNGLBINDTEXTUREPROC PAGL_BindTexture = NULL;
PFNGLBINDTRANSFORMFEEDBACKPROC PAGL_BindTransformFeedback = NULL;
PFNGLBINDVERTEXARRAYPROC PAGL_BindVertexArray = NULL;
PFNGLBLENDCOLORPROC PAGL_BlendColor = NULL;
PFNGLBLENDEQUATIONPROC PAGL_BlendEquation = NULL;
PFNGLBLENDEQUATIONSEPARATEPROC PAGL_BlendEquationSeparate = NULL;
PFNGLBLENDFUNCPROC PAGL_BlendFunc= NULL;
PFNGLBLENDFUNCSEPARATEPROC PAGL_BlendFuncSeparate = NULL;
PFNGLBLITFRAMEBUFFERPROC PAGL_BlitFramebuffer = NULL;
PFNGLBUFFERDATAPROC PAGL_BufferData = NULL;
PFNGLBUFFERSUBDATAPROC PAGL_BufferSubData = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC PAGL_CheckFramebufferStatus = NULL;
PFNGLCLEARPROC PAGL_Clear = NULL;
PFNGLCLEARBUFFERFIPROC PAGL_ClearBufferfi = NULL;
PFNGLCLEARBUFFERFVPROC PAGL_ClearBufferfv = NULL;
PFNGLCLEARBUFFERIVPROC PAGL_ClearBufferiv = NULL;
PFNGLCLEARBUFFERUIVPROC PAGL_ClearBufferuiv = NULL;
PFNGLCLEARCOLORPROC PAGL_ClearColor = NULL;
PFNGLCLEARDEPTHFPROC PAGL_ClearDepthf = NULL;
PFNGLCLEARSTENCILPROC PAGL_ClearStencil = NULL;
PFNGLCOLORMASKPROC PAGL_ColorMask = NULL;
PFNGLCOMPILESHADERPROC PAGL_CompileShader = NULL;
PFNGLCOMPRESSEDTEXIMAGE2DPROC PAGL_CompressedTexImage2D = NULL;
PFNGLCOMPRESSEDTEXIMAGE3DPROC PAGL_CompressedTexImage3D = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC PAGL_CompressedTexSubImage2D = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC PAGL_CompressedTexSubImage3D = NULL;
PFNGLCOPYBUFFERSUBDATAPROC PAGL_CopyBufferSubData = NULL;
PFNGLCOPYTEXIMAGE2DPROC PAGL_CopyTexImage2D = NULL;
PFNGLCOPYTEXSUBIMAGE2DPROC PAGL_CopyTexSubImage2D = NULL;
PFNGLCOPYTEXSUBIMAGE3DPROC PAGL_CopyTexSubImage3D = NULL;
PFNGLCREATEPROGRAMPROC PAGL_CreateProgram = NULL;
PFNGLCREATESHADERPROC PAGL_CreateShader = NULL;
PFNGLCULLFACEPROC PAGL_CullFace = NULL;

PFNGLDELETEBUFFERSPROC PAGL_DeleteBuffers = NULL;
PFNGLDELETEFRAMEBUFFERSPROC PAGL_DeleteFramebuffers = NULL;
PFNGLDELETEPROGRAMPROC PAGL_DeleteProgram = NULL;
PFNGLDELETEQUERIESPROC PAGL_DeleteQueries = NULL;
PFNGLDELETERENDERBUFFERSPROC PAGL_DeleteRenderbuffers = NULL;
PFNGLDELETESAMPLERSPROC PAGL_DeleteSamplers = NULL;
PFNGLDELETESHADERPROC PAGL_DeleteShader = NULL;
PFNGLDELETESYNCPROC PAGL_DeleteSync= NULL;
PFNGLDELETETEXTURESPROC PAGL_DeleteTextures = NULL;
PFNGLDELETETRANSFORMFEEDBACKSPROC PAGL_DeleteTransformFeedbacks = NULL;
PFNGLDELETEVERTEXARRAYSPROC PAGL_DeleteVertexArrays = NULL;
PFNGLDEPTHFUNCPROC PAGL_DepthFunc= NULL;
PFNGLDEPTHMASKPROC PAGL_DepthMask = NULL;
PFNGLDEPTHRANGEFPROC PAGL_DepthRangef = NULL;
PFNGLDETACHSHADERPROC PAGL_DetachShader = NULL;
PFNGLDISABLEPROC PAGL_Disable = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYPROC PAGL_DisableVertexAttribArray = NULL;
PFNGLDRAWARRAYSPROC PAGL_DrawArrays = NULL;
PFNGLDRAWARRAYSINSTANCEDPROC PAGL_DrawArraysInstanced = NULL;
PFNGLDRAWBUFFERSPROC PAGL_DrawBuffers = NULL;
PFNGLDRAWELEMENTSPROC PAGL_DrawElements = NULL;
PFNGLDRAWELEMENTSINSTANCEDPROC PAGL_DrawElementsInstanced = NULL;
PFNGLDRAWRANGEELEMENTSPROC PAGL_DrawRangeElements = NULL;
PFNGLENABLEPROC PAGL_Enable = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC PAGL_EnableVertexAttribArray = NULL;
PFNGLENDQUERYPROC PAGL_EndQuery = NULL;
PFNGLENDTRANSFORMFEEDBACKPROC PAGL_EndTransformFeedback = NULL;
PFNGLFENCESYNCPROC PAGL_FenceSync= NULL;
PFNGLFINISHPROC PAGL_Finish = NULL;
PFNGLFLUSHPROC PAGL_Flush = NULL;
PFNGLFLUSHMAPPEDBUFFERRANGEPROC PAGL_FlushMappedBufferRange = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC PAGL_FramebufferRenderbuffer = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC PAGL_FramebufferTexture2D = NULL;
PFNGLFRAMEBUFFERTEXTURELAYERPROC PAGL_FramebufferTextureLayer = NULL;
PFNGLFRONTFACEPROC PAGL_FrontFace = NULL;
PFNGLGENBUFFERSPROC PAGL_GenBuffers = NULL;
PFNGLGENFRAMEBUFFERSPROC PAGL_GenFramebuffers = NULL;
PFNGLGENQUERIESPROC PAGL_GenQueries = NULL;
PFNGLGENRENDERBUFFERSPROC PAGL_GenRenderbuffers = NULL;
PFNGLGENSAMPLERSPROC PAGL_GenSamplers = NULL;
PFNGLGENTEXTURESPROC PAGL_GenTextures = NULL;
PFNGLGENTRANSFORMFEEDBACKSPROC PAGL_GenTransformFeedbacks = NULL;
PFNGLGENVERTEXARRAYSPROC PAGL_GenVertexArrays = NULL;
PFNGLGENERATEMIPMAPPROC PAGL_GenerateMipmap = NULL;
PFNGLGETACTIVEATTRIBPROC PAGL_GetActiveAttrib = NULL;
PFNGLGETACTIVEUNIFORMPROC PAGL_GetActiveUniform = NULL;
PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC PAGL_GetActiveUniformBlockName = NULL;
PFNGLGETACTIVEUNIFORMBLOCKIVPROC PAGL_GetActiveUniformBlockiv = NULL;
PFNGLGETACTIVEUNIFORMSIVPROC PAGL_GetActiveUniformsiv = NULL;
PFNGLGETATTACHEDSHADERSPROC PAGL_GetAttachedShaders = NULL;
PFNGLGETATTRIBLOCATIONPROC PAGL_GetAttribLocation = NULL;
PFNGLGETBOOLEANVPROC PAGL_GetBooleanv = NULL;
PFNGLGETBUFFERPARAMETERIVPROC PAGL_GetBufferParameteriv = NULL;
PFNGLGETBUFFERPOINTERVPROC PAGL_GetBufferPointerv = NULL;
PFNGLGETDEBUGMESSAGELOGKHRPROC PAGL_GetDebugMessageLogKHR = NULL;
PFNGLGETDEBUGMESSAGELOGPROC PAGL_GetDebugMessageLog = NULL;
PFNGLGETERRORPROC PAGL_GetError = NULL;
PFNGLGETFLOATVPROC PAGL_GetFloatv = NULL;
PFNGLGETFRAGDATALOCATIONPROC PAGL_GetFragDataLocation = NULL;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC PAGL_GetFramebufferAttachmentParameteriv = NULL;
PFNGLGETINTEGERI_VPROC PAGL_GetIntegeri_v = NULL;
PFNGLGETINTEGERVPROC PAGL_GetIntegerv = NULL;
PFNGLGETINTERNALFORMATIVPROC PAGL_GetInternalformativ = NULL;
PFNGLGETOBJECTLABELKHRPROC PAGL_GetObjectLabelKHR = NULL;
PFNGLGETOBJECTPTRLABELKHRPROC PAGL_GetObjectPtrLabelKHR = NULL;
PFNGLGETPOINTERVKHRPROC PAGL_GetPointervKHR = NULL;
PFNGLGETPROGRAMBINARYPROC PAGL_GetProgramBinary = NULL;
PFNGLGETPROGRAMINFOLOGPROC PAGL_GetProgramInfoLog = NULL;
PFNGLGETPROGRAMIVPROC PAGL_GetProgramiv = NULL;
PFNGLGETQUERYOBJECTUIVPROC PAGL_GetQueryObjectuiv = NULL;
PFNGLGETQUERYIVPROC PAGL_GetQueryiv = NULL;
PFNGLGETRENDERBUFFERPARAMETERIVPROC PAGL_GetRenderbufferParameteriv = NULL;
PFNGLGETSAMPLERPARAMETERFVPROC PAGL_GetSamplerParameterfv = NULL;
PFNGLGETSAMPLERPARAMETERIVPROC PAGL_GetSamplerParameteriv = NULL;
PFNGLGETSHADERINFOLOGPROC PAGL_GetShaderInfoLog = NULL;
PFNGLGETSHADERPRECISIONFORMATPROC PAGL_GetShaderPrecisionFormat = NULL;
PFNGLGETSHADERSOURCEPROC PAGL_GetShaderSource = NULL;
PFNGLGETSHADERIVPROC PAGL_GetShaderiv = NULL;
PFNGLGETSTRINGPROC PAGL_GetString = NULL;
PFNGLGETSTRINGIPROC PAGL_GetStringi = NULL;
PFNGLGETSYNCIVPROC PAGL_GetSynciv = NULL;
PFNGLGETTEXPARAMETERFVPROC PAGL_GetTexParameterfv = NULL;
PFNGLGETTEXPARAMETERIVPROC PAGL_GetTexParameteriv = NULL;
PFNGLGETTRANSFORMFEEDBACKVARYINGPROC PAGL_GetTransformFeedbackVarying = NULL;
PFNGLGETUNIFORMBLOCKINDEXPROC PAGL_GetUniformBlockIndex = NULL;
PFNGLGETUNIFORMINDICESPROC PAGL_GetUniformIndices = NULL;
PFNGLGETUNIFORMLOCATIONPROC PAGL_GetUniformLocation = NULL;
PFNGLGETUNIFORMFVPROC PAGL_GetUniformfv = NULL;
PFNGLGETUNIFORMIVPROC PAGL_GetUniformiv = NULL;
PFNGLGETUNIFORMUIVPROC PAGL_GetUniformuiv = NULL;
PFNGLGETVERTEXATTRIBIIVPROC PAGL_GetVertexAttribIiv = NULL;
PFNGLGETVERTEXATTRIBIUIVPROC PAGL_GetVertexAttribIuiv = NULL;
PFNGLGETVERTEXATTRIBPOINTERVPROC PAGL_GetVertexAttribPointerv = NULL;
PFNGLGETVERTEXATTRIBFVPROC PAGL_GetVertexAttribfv = NULL;
PFNGLGETVERTEXATTRIBIVPROC PAGL_GetVertexAttribiv = NULL;
PFNGLHINTPROC PAGL_Hint = NULL;
PFNGLINVALIDATEFRAMEBUFFERPROC PAGL_InvalidateFramebuffer = NULL;
PFNGLINVALIDATESUBFRAMEBUFFERPROC PAGL_InvalidateSubFramebuffer = NULL;
PFNGLISBUFFERPROC PAGL_IsBuffer = NULL;
PFNGLISENABLEDPROC PAGL_IsEnabled = NULL;
PFNGLISFRAMEBUFFERPROC PAGL_IsFramebuffer = NULL;
PFNGLISPROGRAMPROC PAGL_IsProgram = NULL;
PFNGLISQUERYPROC PAGL_IsQuery = NULL;
PFNGLISRENDERBUFFERPROC PAGL_IsRenderbuffer = NULL;
PFNGLISSAMPLERPROC PAGL_IsSampler = NULL;
PFNGLISSHADERPROC PAGL_IsShader = NULL;
PFNGLISSYNCPROC PAGL_IsSync = NULL;
PFNGLISTEXTUREPROC PAGL_IsTexture = NULL;
PFNGLISTRANSFORMFEEDBACKPROC PAGL_IsTransformFeedback = NULL;
PFNGLISVERTEXARRAYPROC PAGL_IsVertexArray = NULL;
PFNGLLINEWIDTHPROC PAGL_LineWidth = NULL;
PFNGLLINKPROGRAMPROC PAGL_LinkProgram = NULL;
PFNGLMAPBUFFERRANGEPROC PAGL_MapBufferRange = NULL;
PFNGLOBJECTLABELKHRPROC PAGL_ObjectLabelKHR = NULL;
PFNGLOBJECTPTRLABELKHRPROC PAGL_ObjectPtrLabelKHR = NULL;
PFNGLPAUSETRANSFORMFEEDBACKPROC PAGL_PauseTransformFeedback = NULL;
PFNGLPIXELSTOREIPROC PAGL_PixelStorei = NULL;
PFNGLPOLYGONOFFSETPROC PAGL_PolygonOffset = NULL;
PFNGLPOPDEBUGGROUPKHRPROC PAGL_PopDebugGroupKHR = NULL;
PFNGLPROGRAMBINARYPROC PAGL_ProgramBinary = NULL;
PFNGLPROGRAMPARAMETERIPROC PAGL_ProgramParameteri = NULL;
PFNGLPUSHDEBUGGROUPKHRPROC PAGL_PushDebugGroupKHR = NULL;
PFNGLPUSHDEBUGGROUPPROC PAGL_PushDebugGroup = NULL;
PFNGLREADBUFFERPROC PAGL_ReadBuffer = NULL;
PFNGLREADPIXELSPROC PAGL_ReadPixels = NULL;
PFNGLRELEASESHADERCOMPILERPROC PAGL_ReleaseShaderCompiler = NULL;
PFNGLRENDERBUFFERSTORAGEPROC PAGL_RenderbufferStorage = NULL;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC PAGL_RenderbufferStorageMultisample = NULL;
PFNGLRESUMETRANSFORMFEEDBACKPROC PAGL_ResumeTransformFeedback = NULL;
PFNGLSAMPLECOVERAGEPROC PAGL_SampleCoverage = NULL;
PFNGLSAMPLERPARAMETERFPROC PAGL_SamplerParameterf = NULL;
PFNGLSAMPLERPARAMETERFVPROC PAGL_SamplerParameterfv = NULL;
PFNGLSAMPLERPARAMETERIPROC PAGL_SamplerParameteri = NULL;
PFNGLSAMPLERPARAMETERIVPROC PAGL_SamplerParameteriv = NULL;
PFNGLSCISSORPROC PAGL_Scissor = NULL;
PFNGLSHADERBINARYPROC PAGL_ShaderBinary = NULL;
PFNGLSHADERSOURCEPROC PAGL_ShaderSource = NULL;
PFNGLSTENCILFUNCPROC PAGL_StencilFunc = NULL;
PFNGLSTENCILFUNCSEPARATEPROC PAGL_StencilFuncSeparate = NULL;
PFNGLSTENCILMASKPROC PAGL_StencilMask = NULL;
PFNGLSTENCILMASKSEPARATEPROC PAGL_StencilMaskSeparate = NULL;
PFNGLSTENCILOPPROC PAGL_StencilOp = NULL;
PFNGLSTENCILOPSEPARATEPROC PAGL_StencilOpSeparate = NULL;
PFNGLTEXIMAGE2DPROC PAGL_TexImage2D = NULL;
PFNGLTEXIMAGE3DPROC PAGL_TexImage3D = NULL;
PFNGLTEXPARAMETERFPROC PAGL_TexParameterf = NULL;
PFNGLTEXPARAMETERFVPROC PAGL_TexParameterfv = NULL;
PFNGLTEXPARAMETERIPROC PAGL_TexParameteri = NULL;
PFNGLTEXPARAMETERIVPROC PAGL_TexParameteriv = NULL;
PFNGLTEXSTORAGE2DPROC PAGL_TexStorage2D = NULL;
PFNGLTEXSTORAGE3DPROC PAGL_TexStorage3D = NULL;
PFNGLTEXSUBIMAGE2DPROC PAGL_TexSubImage2D = NULL;
PFNGLTEXSUBIMAGE3DPROC PAGL_TexSubImage3D = NULL;
PFNGLTRANSFORMFEEDBACKVARYINGSPROC PAGL_TransformFeedbackVaryings = NULL;
PFNGLUNIFORM1FPROC PAGL_Uniform1f = NULL;
PFNGLUNIFORM1FVPROC PAGL_Uniform1fv = NULL;
PFNGLUNIFORM1IPROC PAGL_Uniform1i = NULL;
PFNGLUNIFORM1IVPROC PAGL_Uniform1iv = NULL;
PFNGLUNIFORM1UIPROC PAGL_Uniform1ui = NULL;
PFNGLUNIFORM1UIVPROC PAGL_Uniform1uiv = NULL;
PFNGLUNIFORM2FPROC PAGL_Uniform2f = NULL;
PFNGLUNIFORM2FVPROC PAGL_Uniform2fv = NULL;
PFNGLUNIFORM2IPROC PAGL_Uniform2i = NULL;
PFNGLUNIFORM2IVPROC PAGL_Uniform2iv = NULL;
PFNGLUNIFORM2UIPROC PAGL_Uniform2ui = NULL;
PFNGLUNIFORM2UIVPROC PAGL_Uniform2uiv = NULL;
PFNGLUNIFORM3FPROC PAGL_Uniform3f = NULL;
PFNGLUNIFORM3FVPROC PAGL_Uniform3fv = NULL;
PFNGLUNIFORM3IPROC PAGL_Uniform3i = NULL;
PFNGLUNIFORM3IVPROC PAGL_Uniform3iv = NULL;
PFNGLUNIFORM3UIPROC PAGL_Uniform3ui = NULL;
PFNGLUNIFORM3UIVPROC PAGL_Uniform3uiv = NULL;
PFNGLUNIFORM4FPROC PAGL_Uniform4f = NULL;
PFNGLUNIFORM4FVPROC PAGL_Uniform4fv = NULL;
PFNGLUNIFORM4IPROC PAGL_Uniform4i = NULL;
PFNGLUNIFORM4IVPROC PAGL_Uniform4iv = NULL;
PFNGLUNIFORM4UIPROC PAGL_Uniform4ui = NULL;
PFNGLUNIFORM4UIVPROC PAGL_Uniform4uiv = NULL;
PFNGLUNIFORMBLOCKBINDINGPROC PAGL_UniformBlockBinding = NULL;
PFNGLUNIFORMMATRIX2FVPROC PAGL_UniformMatrix2fv = NULL;
PFNGLUNIFORMMATRIX2X3FVPROC PAGL_UniformMatrix2x3fv = NULL;
PFNGLUNIFORMMATRIX2X4FVPROC PAGL_UniformMatrix2x4fv = NULL;
PFNGLUNIFORMMATRIX3FVPROC PAGL_UniformMatrix3fv = NULL;
PFNGLUNIFORMMATRIX3X2FVPROC PAGL_UniformMatrix3x2fv = NULL;
PFNGLUNIFORMMATRIX3X4FVPROC PAGL_UniformMatrix3x4fv = NULL;
PFNGLUNIFORMMATRIX4FVPROC PAGL_UniformMatrix4fv = NULL;
PFNGLUNIFORMMATRIX4X2FVPROC PAGL_UniformMatrix4x2fv = NULL;
PFNGLUNIFORMMATRIX4X3FVPROC PAGL_UniformMatrix4x3fv = NULL;
PFNGLUNMAPBUFFERPROC PAGL_UnmapBuffer = NULL;
PFNGLUSEPROGRAMPROC PAGL_UseProgram = NULL;
PFNGLVALIDATEPROGRAMPROC PAGL_ValidateProgram = NULL;
PFNGLVERTEXATTRIB1FPROC PAGL_VertexAttrib1f = NULL;
PFNGLVERTEXATTRIB1FVPROC PAGL_VertexAttrib1fv = NULL;
PFNGLVERTEXATTRIB2FPROC PAGL_VertexAttrib2f = NULL;
PFNGLVERTEXATTRIB2FVPROC PAGL_VertexAttrib2fv = NULL;
PFNGLVERTEXATTRIB3FPROC PAGL_VertexAttrib3f = NULL;
PFNGLVERTEXATTRIB3FVPROC PAGL_VertexAttrib3fv = NULL;
PFNGLVERTEXATTRIB4FPROC PAGL_VertexAttrib4f = NULL;
PFNGLVERTEXATTRIB4FVPROC PAGL_VertexAttrib4fv = NULL;
PFNGLVERTEXATTRIBDIVISORPROC PAGL_VertexAttribDivisor = NULL;
PFNGLVERTEXATTRIBI4IPROC PAGL_VertexAttribI4i = NULL;
PFNGLVERTEXATTRIBI4IVPROC PAGL_VertexAttribI4iv = NULL;
PFNGLVERTEXATTRIBI4UIPROC PAGL_VertexAttribI4ui = NULL;
PFNGLVERTEXATTRIBI4UIVPROC PAGL_VertexAttribI4uiv = NULL;
PFNGLVERTEXATTRIBIPOINTERPROC PAGL_VertexAttribIPointer = NULL;
PFNGLVERTEXATTRIBPOINTERPROC PAGL_VertexAttribPointer = NULL;
PFNGLVIEWPORTPROC PAGL_Viewport = NULL;

PFNGLPOLYGONMODEPROC PAGL_PolygonMode = NULL;
PFNGLDEBUGMESSAGECALLBACKKHRPROC PAGL_DebugMessageCallbackKHR = NULL;
PFNGLDEBUGMESSAGECONTROLKHRPROC PAGL_DebugMessageControlKHR = NULL;
PFNGLDEBUGMESSAGEINSERTKHRPROC PAGL_DebugMessageInsertKHR = NULL;
PFNGLDEBUGMESSAGECALLBACKPROC PAGL_DebugMessageCallback = NULL;
PFNGLDEBUGMESSAGECONTROLPROC PAGL_DebugMessageControl = NULL;
PFNGLDEBUGMESSAGEINSERTPROC PAGL_DebugMessageInsert = NULL;

#if (defined(GL_OS_WINDOWS) || defined(GL_OS_LINUX))
static void loadGLES20( PAGLuserptrloadfunc load, void* userptr) {
    if(!PAGL_GL_ES_VERSION_2_0) return;
    PAGL_ActiveTexture = (PFNGLACTIVETEXTUREPROC) load(userptr, "glActiveTexture");
    PAGL_AttachShader = (PFNGLATTACHSHADERPROC) load(userptr, "glAttachShader");
    PAGL_BindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC) load(userptr, "glBindAttribLocation");
    PAGL_BindBuffer = (PFNGLBINDBUFFERPROC) load(userptr, "glBindBuffer");
    PAGL_BindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) load(userptr, "glBindFramebuffer");
    PAGL_BindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) load(userptr, "glBindRenderbuffer");
    PAGL_BindTexture = (PFNGLBINDTEXTUREPROC) load(userptr, "glBindTexture");
    PAGL_BlendColor = (PFNGLBLENDCOLORPROC) load(userptr, "glBlendColor");
    PAGL_BlendEquation = (PFNGLBLENDEQUATIONPROC) load(userptr, "glBlendEquation");
    PAGL_BlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC) load(userptr, "glBlendEquationSeparate");
    PAGL_BlendFunc = (PFNGLBLENDFUNCPROC) load(userptr, "glBlendFunc");
    PAGL_BlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC) load(userptr, "glBlendFuncSeparate");
    PAGL_BufferData = (PFNGLBUFFERDATAPROC) load(userptr, "glBufferData");
    PAGL_BufferSubData = (PFNGLBUFFERSUBDATAPROC) load(userptr, "glBufferSubData");
    PAGL_CheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) load(userptr, "glCheckFramebufferStatus");
    PAGL_Clear = (PFNGLCLEARPROC) load(userptr, "glClear");
    PAGL_ClearColor = (PFNGLCLEARCOLORPROC) load(userptr, "glClearColor");
    PAGL_ClearDepthf = (PFNGLCLEARDEPTHFPROC) load(userptr, "glClearDepthf");
    PAGL_ClearStencil = (PFNGLCLEARSTENCILPROC) load(userptr, "glClearStencil");
    PAGL_ColorMask = (PFNGLCOLORMASKPROC) load(userptr, "glColorMask");
    PAGL_CompileShader = (PFNGLCOMPILESHADERPROC) load(userptr, "glCompileShader");
    PAGL_CompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC) load(userptr, "glCompressedTexImage2D");
    PAGL_CompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC) load(userptr, "glCompressedTexSubImage2D");
    PAGL_CopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC) load(userptr, "glCopyTexImage2D");
    PAGL_CopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC) load(userptr, "glCopyTexSubImage2D");
    PAGL_CreateProgram = (PFNGLCREATEPROGRAMPROC) load(userptr, "glCreateProgram");
    PAGL_CreateShader = (PFNGLCREATESHADERPROC) load(userptr, "glCreateShader");
    PAGL_CullFace = (PFNGLCULLFACEPROC) load(userptr, "glCullFace");
    PAGL_DeleteBuffers = (PFNGLDELETEBUFFERSPROC) load(userptr, "glDeleteBuffers");
    PAGL_DeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) load(userptr, "glDeleteFramebuffers");
    PAGL_DeleteProgram = (PFNGLDELETEPROGRAMPROC) load(userptr, "glDeleteProgram");
    PAGL_DeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) load(userptr, "glDeleteRenderbuffers");
    PAGL_DeleteShader = (PFNGLDELETESHADERPROC) load(userptr, "glDeleteShader");
    PAGL_DeleteTextures = (PFNGLDELETETEXTURESPROC) load(userptr, "glDeleteTextures");
    PAGL_DepthFunc = (PFNGLDEPTHFUNCPROC) load(userptr, "glDepthFunc");
    PAGL_DepthMask = (PFNGLDEPTHMASKPROC) load(userptr, "glDepthMask");
    PAGL_DepthRangef = (PFNGLDEPTHRANGEFPROC) load(userptr, "glDepthRangef");
    PAGL_DetachShader = (PFNGLDETACHSHADERPROC) load(userptr, "glDetachShader");
    PAGL_Disable = (PFNGLDISABLEPROC) load(userptr, "glDisable");
    PAGL_DisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC) load(userptr, "glDisableVertexAttribArray");
    PAGL_DrawArrays = (PFNGLDRAWARRAYSPROC) load(userptr, "glDrawArrays");
    PAGL_DrawElements = (PFNGLDRAWELEMENTSPROC) load(userptr, "glDrawElements");
    PAGL_Enable = (PFNGLENABLEPROC) load(userptr, "glEnable");
    PAGL_EnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) load(userptr, "glEnableVertexAttribArray");
    PAGL_Finish = (PFNGLFINISHPROC) load(userptr, "glFinish");
    PAGL_Flush = (PFNGLFLUSHPROC) load(userptr, "glFlush");
    PAGL_FramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) load(userptr, "glFramebufferRenderbuffer");
    PAGL_FramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) load(userptr, "glFramebufferTexture2D");
    PAGL_FrontFace = (PFNGLFRONTFACEPROC) load(userptr, "glFrontFace");
    PAGL_GenBuffers = (PFNGLGENBUFFERSPROC) load(userptr, "glGenBuffers");
    PAGL_GenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) load(userptr, "glGenFramebuffers");
    PAGL_GenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) load(userptr, "glGenRenderbuffers");
    PAGL_GenTextures = (PFNGLGENTEXTURESPROC) load(userptr, "glGenTextures");
    PAGL_GenerateMipmap = (PFNGLGENERATEMIPMAPPROC) load(userptr, "glGenerateMipmap");
    PAGL_GetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC) load(userptr, "glGetActiveAttrib");
    PAGL_GetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC) load(userptr, "glGetActiveUniform");
    PAGL_GetAttachedShaders = (PFNGLGETATTACHEDSHADERSPROC) load(userptr, "glGetAttachedShaders");
    PAGL_GetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC) load(userptr, "glGetAttribLocation");
    PAGL_GetBooleanv = (PFNGLGETBOOLEANVPROC) load(userptr, "glGetBooleanv");
    PAGL_GetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC) load(userptr, "glGetBufferParameteriv");
    PAGL_GetError = (PFNGLGETERRORPROC) load(userptr, "glGetError");
    PAGL_GetFloatv = (PFNGLGETFLOATVPROC) load(userptr, "glGetFloatv");
    PAGL_GetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) load(userptr, "glGetFramebufferAttachmentParameteriv");
    PAGL_GetIntegerv = (PFNGLGETINTEGERVPROC) load(userptr, "glGetIntegerv");
    PAGL_GetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC) load(userptr, "glGetProgramInfoLog");
    PAGL_GetProgramiv = (PFNGLGETPROGRAMIVPROC) load(userptr, "glGetProgramiv");
    PAGL_GetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) load(userptr, "glGetRenderbufferParameteriv");
    PAGL_GetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) load(userptr, "glGetShaderInfoLog");
    PAGL_GetShaderPrecisionFormat = (PFNGLGETSHADERPRECISIONFORMATPROC) load(userptr, "glGetShaderPrecisionFormat");
    PAGL_GetShaderSource = (PFNGLGETSHADERSOURCEPROC) load(userptr, "glGetShaderSource");
    PAGL_GetShaderiv = (PFNGLGETSHADERIVPROC) load(userptr, "glGetShaderiv");
    PAGL_GetString = (PFNGLGETSTRINGPROC) load(userptr, "glGetString");
    PAGL_GetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC) load(userptr, "glGetTexParameterfv");
    PAGL_GetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC) load(userptr, "glGetTexParameteriv");
    PAGL_GetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) load(userptr, "glGetUniformLocation");
    PAGL_GetUniformfv = (PFNGLGETUNIFORMFVPROC) load(userptr, "glGetUniformfv");
    PAGL_GetUniformiv = (PFNGLGETUNIFORMIVPROC) load(userptr, "glGetUniformiv");
    PAGL_GetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC) load(userptr, "glGetVertexAttribPointerv");
    PAGL_GetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC) load(userptr, "glGetVertexAttribfv");
    PAGL_GetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC) load(userptr, "glGetVertexAttribiv");
    PAGL_Hint = (PFNGLHINTPROC) load(userptr, "glHint");
    PAGL_IsBuffer = (PFNGLISBUFFERPROC) load(userptr, "glIsBuffer");
    PAGL_IsEnabled = (PFNGLISENABLEDPROC) load(userptr, "glIsEnabled");
    PAGL_IsFramebuffer = (PFNGLISFRAMEBUFFERPROC) load(userptr, "glIsFramebuffer");
    PAGL_IsProgram = (PFNGLISPROGRAMPROC) load(userptr, "glIsProgram");
    PAGL_IsRenderbuffer = (PFNGLISRENDERBUFFERPROC) load(userptr, "glIsRenderbuffer");
    PAGL_IsShader = (PFNGLISSHADERPROC) load(userptr, "glIsShader");
    PAGL_IsTexture = (PFNGLISTEXTUREPROC) load(userptr, "glIsTexture");
    PAGL_LineWidth = (PFNGLLINEWIDTHPROC) load(userptr, "glLineWidth");
    PAGL_LinkProgram = (PFNGLLINKPROGRAMPROC) load(userptr, "glLinkProgram");
    PAGL_PixelStorei = (PFNGLPIXELSTOREIPROC) load(userptr, "glPixelStorei");
    PAGL_PolygonOffset = (PFNGLPOLYGONOFFSETPROC) load(userptr, "glPolygonOffset");
    PAGL_ReadPixels = (PFNGLREADPIXELSPROC) load(userptr, "glReadPixels");
    PAGL_ReleaseShaderCompiler = (PFNGLRELEASESHADERCOMPILERPROC) load(userptr, "glReleaseShaderCompiler");
    PAGL_RenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) load(userptr, "glRenderbufferStorage");
    PAGL_SampleCoverage = (PFNGLSAMPLECOVERAGEPROC) load(userptr, "glSampleCoverage");
    PAGL_Scissor = (PFNGLSCISSORPROC) load(userptr, "glScissor");
    PAGL_ShaderBinary = (PFNGLSHADERBINARYPROC) load(userptr, "glShaderBinary");
    PAGL_ShaderSource = (PFNGLSHADERSOURCEPROC) load(userptr, "glShaderSource");
    PAGL_StencilFunc = (PFNGLSTENCILFUNCPROC) load(userptr, "glStencilFunc");
    PAGL_StencilFuncSeparate = (PFNGLSTENCILFUNCSEPARATEPROC) load(userptr, "glStencilFuncSeparate");
    PAGL_StencilMask = (PFNGLSTENCILMASKPROC) load(userptr, "glStencilMask");
    PAGL_StencilMaskSeparate = (PFNGLSTENCILMASKSEPARATEPROC) load(userptr, "glStencilMaskSeparate");
    PAGL_StencilOp = (PFNGLSTENCILOPPROC) load(userptr, "glStencilOp");
    PAGL_StencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC) load(userptr, "glStencilOpSeparate");
    PAGL_TexImage2D = (PFNGLTEXIMAGE2DPROC) load(userptr, "glTexImage2D");
    PAGL_TexParameterf = (PFNGLTEXPARAMETERFPROC) load(userptr, "glTexParameterf");
    PAGL_TexParameterfv = (PFNGLTEXPARAMETERFVPROC) load(userptr, "glTexParameterfv");
    PAGL_TexParameteri = (PFNGLTEXPARAMETERIPROC) load(userptr, "glTexParameteri");
    PAGL_TexParameteriv = (PFNGLTEXPARAMETERIVPROC) load(userptr, "glTexParameteriv");
    PAGL_TexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC) load(userptr, "glTexSubImage2D");
    PAGL_Uniform1f = (PFNGLUNIFORM1FPROC) load(userptr, "glUniform1f");
    PAGL_Uniform1fv = (PFNGLUNIFORM1FVPROC) load(userptr, "glUniform1fv");
    PAGL_Uniform1i = (PFNGLUNIFORM1IPROC) load(userptr, "glUniform1i");
    PAGL_Uniform1iv = (PFNGLUNIFORM1IVPROC) load(userptr, "glUniform1iv");
    PAGL_Uniform2f = (PFNGLUNIFORM2FPROC) load(userptr, "glUniform2f");
    PAGL_Uniform2fv = (PFNGLUNIFORM2FVPROC) load(userptr, "glUniform2fv");
    PAGL_Uniform2i = (PFNGLUNIFORM2IPROC) load(userptr, "glUniform2i");
    PAGL_Uniform2iv = (PFNGLUNIFORM2IVPROC) load(userptr, "glUniform2iv");
    PAGL_Uniform3f = (PFNGLUNIFORM3FPROC) load(userptr, "glUniform3f");
    PAGL_Uniform3fv = (PFNGLUNIFORM3FVPROC) load(userptr, "glUniform3fv");
    PAGL_Uniform3i = (PFNGLUNIFORM3IPROC) load(userptr, "glUniform3i");
    PAGL_Uniform3iv = (PFNGLUNIFORM3IVPROC) load(userptr, "glUniform3iv");
    PAGL_Uniform4f = (PFNGLUNIFORM4FPROC) load(userptr, "glUniform4f");
    PAGL_Uniform4fv = (PFNGLUNIFORM4FVPROC) load(userptr, "glUniform4fv");
    PAGL_Uniform4i = (PFNGLUNIFORM4IPROC) load(userptr, "glUniform4i");
    PAGL_Uniform4iv = (PFNGLUNIFORM4IVPROC) load(userptr, "glUniform4iv");
    PAGL_UniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC) load(userptr, "glUniformMatrix2fv");
    PAGL_UniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC) load(userptr, "glUniformMatrix3fv");
    PAGL_UniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC) load(userptr, "glUniformMatrix4fv");
    PAGL_UseProgram = (PFNGLUSEPROGRAMPROC) load(userptr, "glUseProgram");
    PAGL_ValidateProgram = (PFNGLVALIDATEPROGRAMPROC) load(userptr, "glValidateProgram");
    PAGL_VertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC) load(userptr, "glVertexAttrib1f");
    PAGL_VertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC) load(userptr, "glVertexAttrib1fv");
    PAGL_VertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC) load(userptr, "glVertexAttrib2f");
    PAGL_VertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC) load(userptr, "glVertexAttrib2fv");
    PAGL_VertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC) load(userptr, "glVertexAttrib3f");
    PAGL_VertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC) load(userptr, "glVertexAttrib3fv");
    PAGL_VertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC) load(userptr, "glVertexAttrib4f");
    PAGL_VertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC) load(userptr, "glVertexAttrib4fv");
    PAGL_VertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC) load(userptr, "glVertexAttribPointer");
    PAGL_Viewport = (PFNGLVIEWPORTPROC) load(userptr, "glViewport");
}
static void loadGL( PAGLuserptrloadfunc load, void* userptr) {
    #if (!defined(GL_OS_WINDOWS) && !defined(GL_DEBUG))
        return;
    #endif
    PAGL_PolygonMode = (PFNGLPOLYGONMODEPROC) load(userptr, "glPolygonMode");
}

static void loadGLES30( PAGLuserptrloadfunc load, void* userptr) {
    if(!PAGL_GL_ES_VERSION_3_0) return;
    PAGL_BeginQuery = (PFNGLBEGINQUERYPROC) load(userptr, "glBeginQuery");
    PAGL_BeginTransformFeedback = (PFNGLBEGINTRANSFORMFEEDBACKPROC) load(userptr, "glBeginTransformFeedback");
    PAGL_BindBufferBase = (PFNGLBINDBUFFERBASEPROC) load(userptr, "glBindBufferBase");
    PAGL_BindBufferRange = (PFNGLBINDBUFFERRANGEPROC) load(userptr, "glBindBufferRange");
    PAGL_BindSampler = (PFNGLBINDSAMPLERPROC) load(userptr, "glBindSampler");
    PAGL_BindTransformFeedback = (PFNGLBINDTRANSFORMFEEDBACKPROC) load(userptr, "glBindTransformFeedback");
    PAGL_BindVertexArray = (PFNGLBINDVERTEXARRAYPROC) load(userptr, "glBindVertexArray");
    PAGL_BlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC) load(userptr, "glBlitFramebuffer");
    PAGL_ClearBufferfi = (PFNGLCLEARBUFFERFIPROC) load(userptr, "glClearBufferfi");
    PAGL_ClearBufferfv = (PFNGLCLEARBUFFERFVPROC) load(userptr, "glClearBufferfv");
    PAGL_ClearBufferiv = (PFNGLCLEARBUFFERIVPROC) load(userptr, "glClearBufferiv");
    PAGL_ClearBufferuiv = (PFNGLCLEARBUFFERUIVPROC) load(userptr, "glClearBufferuiv");
    PAGL_CompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC) load(userptr, "glCompressedTexImage3D");
    PAGL_CompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC) load(userptr, "glCompressedTexSubImage3D");
    PAGL_CopyBufferSubData = (PFNGLCOPYBUFFERSUBDATAPROC) load(userptr, "glCopyBufferSubData");
    PAGL_CopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC) load(userptr, "glCopyTexSubImage3D");
    PAGL_DeleteQueries = (PFNGLDELETEQUERIESPROC) load(userptr, "glDeleteQueries");
    PAGL_DeleteSamplers = (PFNGLDELETESAMPLERSPROC) load(userptr, "glDeleteSamplers");
    PAGL_DeleteSync = (PFNGLDELETESYNCPROC) load(userptr, "glDeleteSync");
    PAGL_DeleteTransformFeedbacks = (PFNGLDELETETRANSFORMFEEDBACKSPROC) load(userptr, "glDeleteTransformFeedbacks");
    PAGL_DeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) load(userptr, "glDeleteVertexArrays");
    PAGL_DrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC) load(userptr, "glDrawArraysInstanced");
    PAGL_DrawBuffers = (PFNGLDRAWBUFFERSPROC) load(userptr, "glDrawBuffers");
    PAGL_DrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC) load(userptr, "glDrawElementsInstanced");
    PAGL_DrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC) load(userptr, "glDrawRangeElements");
    PAGL_EndQuery = (PFNGLENDQUERYPROC) load(userptr, "glEndQuery");
    PAGL_EndTransformFeedback = (PFNGLENDTRANSFORMFEEDBACKPROC) load(userptr, "glEndTransformFeedback");
    PAGL_FenceSync = (PFNGLFENCESYNCPROC) load(userptr, "glFenceSync");
    PAGL_FlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC) load(userptr, "glFlushMappedBufferRange");
    PAGL_FramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC) load(userptr, "glFramebufferTextureLayer");
    PAGL_GenQueries = (PFNGLGENQUERIESPROC) load(userptr, "glGenQueries");
    PAGL_GenSamplers = (PFNGLGENSAMPLERSPROC) load(userptr, "glGenSamplers");
    PAGL_GenTransformFeedbacks = (PFNGLGENTRANSFORMFEEDBACKSPROC) load(userptr, "glGenTransformFeedbacks");
    PAGL_GenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) load(userptr, "glGenVertexArrays");
    PAGL_GetActiveUniformBlockName = (PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC) load(userptr, "glGetActiveUniformBlockName");
    PAGL_GetActiveUniformBlockiv = (PFNGLGETACTIVEUNIFORMBLOCKIVPROC) load(userptr, "glGetActiveUniformBlockiv");
    PAGL_GetActiveUniformsiv = (PFNGLGETACTIVEUNIFORMSIVPROC) load(userptr, "glGetActiveUniformsiv");
    PAGL_GetBufferPointerv = (PFNGLGETBUFFERPOINTERVPROC) load(userptr, "glGetBufferPointerv");
    PAGL_GetFragDataLocation = (PFNGLGETFRAGDATALOCATIONPROC) load(userptr, "glGetFragDataLocation");
    PAGL_GetIntegeri_v = (PFNGLGETINTEGERI_VPROC) load(userptr, "glGetIntegeri_v");
    PAGL_GetInternalformativ = (PFNGLGETINTERNALFORMATIVPROC) load(userptr, "glGetInternalformativ");
    PAGL_GetProgramBinary = (PFNGLGETPROGRAMBINARYPROC) load(userptr, "glGetProgramBinary");
    PAGL_GetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC) load(userptr, "glGetQueryObjectuiv");
    PAGL_GetQueryiv = (PFNGLGETQUERYIVPROC) load(userptr, "glGetQueryiv");
    PAGL_GetSamplerParameterfv = (PFNGLGETSAMPLERPARAMETERFVPROC) load(userptr, "glGetSamplerParameterfv");
    PAGL_GetSamplerParameteriv = (PFNGLGETSAMPLERPARAMETERIVPROC) load(userptr, "glGetSamplerParameteriv");
    PAGL_GetStringi = (PFNGLGETSTRINGIPROC) load(userptr, "glGetStringi");
    PAGL_GetSynciv = (PFNGLGETSYNCIVPROC) load(userptr, "glGetSynciv");
    PAGL_GetTransformFeedbackVarying = (PFNGLGETTRANSFORMFEEDBACKVARYINGPROC) load(userptr, "glGetTransformFeedbackVarying");
    PAGL_GetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC) load(userptr, "glGetUniformBlockIndex");
    PAGL_GetUniformIndices = (PFNGLGETUNIFORMINDICESPROC) load(userptr, "glGetUniformIndices");
    PAGL_GetUniformuiv = (PFNGLGETUNIFORMUIVPROC) load(userptr, "glGetUniformuiv");
    PAGL_GetVertexAttribIiv = (PFNGLGETVERTEXATTRIBIIVPROC) load(userptr, "glGetVertexAttribIiv");
    PAGL_GetVertexAttribIuiv = (PFNGLGETVERTEXATTRIBIUIVPROC) load(userptr, "glGetVertexAttribIuiv");
    PAGL_InvalidateFramebuffer = (PFNGLINVALIDATEFRAMEBUFFERPROC) load(userptr, "glInvalidateFramebuffer");
    PAGL_InvalidateSubFramebuffer = (PFNGLINVALIDATESUBFRAMEBUFFERPROC) load(userptr, "glInvalidateSubFramebuffer");
    PAGL_IsQuery = (PFNGLISQUERYPROC) load(userptr, "glIsQuery");
    PAGL_IsSampler = (PFNGLISSAMPLERPROC) load(userptr, "glIsSampler");
    PAGL_IsSync = (PFNGLISSYNCPROC) load(userptr, "glIsSync");
    PAGL_IsTransformFeedback = (PFNGLISTRANSFORMFEEDBACKPROC) load(userptr, "glIsTransformFeedback");
    PAGL_IsVertexArray = (PFNGLISVERTEXARRAYPROC) load(userptr, "glIsVertexArray");
    PAGL_MapBufferRange = (PFNGLMAPBUFFERRANGEPROC) load(userptr, "glMapBufferRange");
    PAGL_PauseTransformFeedback = (PFNGLPAUSETRANSFORMFEEDBACKPROC) load(userptr, "glPauseTransformFeedback");
    PAGL_ProgramBinary = (PFNGLPROGRAMBINARYPROC) load(userptr, "glProgramBinary");
    PAGL_ProgramParameteri = (PFNGLPROGRAMPARAMETERIPROC) load(userptr, "glProgramParameteri");
    PAGL_ReadBuffer = (PFNGLREADBUFFERPROC) load(userptr, "glReadBuffer");
    PAGL_RenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) load(userptr, "glRenderbufferStorageMultisample");
    PAGL_ResumeTransformFeedback = (PFNGLRESUMETRANSFORMFEEDBACKPROC) load(userptr, "glResumeTransformFeedback");
    PAGL_SamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC) load(userptr, "glSamplerParameterf");
    PAGL_SamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC) load(userptr, "glSamplerParameterfv");
    PAGL_SamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC) load(userptr, "glSamplerParameteri");
    PAGL_SamplerParameteriv = (PFNGLSAMPLERPARAMETERIVPROC) load(userptr, "glSamplerParameteriv");
    PAGL_TexImage3D = (PFNGLTEXIMAGE3DPROC) load(userptr, "glTexImage3D");
    PAGL_TexStorage2D = (PFNGLTEXSTORAGE2DPROC) load(userptr, "glTexStorage2D");
    PAGL_TexStorage3D = (PFNGLTEXSTORAGE3DPROC) load(userptr, "glTexStorage3D");
    PAGL_TexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC) load(userptr, "glTexSubImage3D");
    PAGL_TransformFeedbackVaryings = (PFNGLTRANSFORMFEEDBACKVARYINGSPROC) load(userptr, "glTransformFeedbackVaryings");
    PAGL_Uniform1ui = (PFNGLUNIFORM1UIPROC) load(userptr, "glUniform1ui");
    PAGL_Uniform1uiv = (PFNGLUNIFORM1UIVPROC) load(userptr, "glUniform1uiv");
    PAGL_Uniform2ui = (PFNGLUNIFORM2UIPROC) load(userptr, "glUniform2ui");
    PAGL_Uniform2uiv = (PFNGLUNIFORM2UIVPROC) load(userptr, "glUniform2uiv");
    PAGL_Uniform3ui = (PFNGLUNIFORM3UIPROC) load(userptr, "glUniform3ui");
    PAGL_Uniform3uiv = (PFNGLUNIFORM3UIVPROC) load(userptr, "glUniform3uiv");
    PAGL_Uniform4ui = (PFNGLUNIFORM4UIPROC) load(userptr, "glUniform4ui");
    PAGL_Uniform4uiv = (PFNGLUNIFORM4UIVPROC) load(userptr, "glUniform4uiv");
    PAGL_UniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC) load(userptr, "glUniformBlockBinding");
    PAGL_UniformMatrix2x3fv = (PFNGLUNIFORMMATRIX2X3FVPROC) load(userptr, "glUniformMatrix2x3fv");
    PAGL_UniformMatrix2x4fv = (PFNGLUNIFORMMATRIX2X4FVPROC) load(userptr, "glUniformMatrix2x4fv");
    PAGL_UniformMatrix3x2fv = (PFNGLUNIFORMMATRIX3X2FVPROC) load(userptr, "glUniformMatrix3x2fv");
    PAGL_UniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC) load(userptr, "glUniformMatrix3x4fv");
    PAGL_UniformMatrix4x2fv = (PFNGLUNIFORMMATRIX4X2FVPROC) load(userptr, "glUniformMatrix4x2fv");
    PAGL_UniformMatrix4x3fv = (PFNGLUNIFORMMATRIX4X3FVPROC) load(userptr, "glUniformMatrix4x3fv");
    PAGL_UnmapBuffer = (PFNGLUNMAPBUFFERPROC) load(userptr, "glUnmapBuffer");
    PAGL_VertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC) load(userptr, "glVertexAttribDivisor");
    PAGL_VertexAttribI4i = (PFNGLVERTEXATTRIBI4IPROC) load(userptr, "glVertexAttribI4i");
    PAGL_VertexAttribI4iv = (PFNGLVERTEXATTRIBI4IVPROC) load(userptr, "glVertexAttribI4iv");
    PAGL_VertexAttribI4ui = (PFNGLVERTEXATTRIBI4UIPROC) load(userptr, "glVertexAttribI4ui");
    PAGL_VertexAttribI4uiv = (PFNGLVERTEXATTRIBI4UIVPROC) load(userptr, "glVertexAttribI4uiv");
    PAGL_VertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC) load(userptr, "glVertexAttribIPointer");
}
static void glad_gl_load_GL_KHR_debug( PAGLuserptrloadfunc load, void* userptr) {
    if(!PAGL_GL_KHR_debug) return;
    PAGL_DebugMessageCallbackKHR = (PFNGLDEBUGMESSAGECALLBACKKHRPROC) load(userptr, "glDebugMessageCallbackKHR");
    PAGL_DebugMessageControlKHR = (PFNGLDEBUGMESSAGECONTROLKHRPROC) load(userptr, "glDebugMessageControlKHR");
    PAGL_DebugMessageInsertKHR = (PFNGLDEBUGMESSAGEINSERTKHRPROC) load(userptr, "glDebugMessageInsertKHR");
    PAGL_GetDebugMessageLogKHR = (PFNGLGETDEBUGMESSAGELOGKHRPROC) load(userptr, "glGetDebugMessageLogKHR");
    PAGL_GetObjectLabelKHR = (PFNGLGETOBJECTLABELKHRPROC) load(userptr, "glGetObjectLabelKHR");
    PAGL_GetObjectPtrLabelKHR = (PFNGLGETOBJECTPTRLABELKHRPROC) load(userptr, "glGetObjectPtrLabelKHR");
    PAGL_GetPointervKHR = (PFNGLGETPOINTERVKHRPROC) load(userptr, "glGetPointervKHR");
    PAGL_ObjectLabelKHR = (PFNGLOBJECTLABELKHRPROC) load(userptr, "glObjectLabelKHR");
    PAGL_ObjectPtrLabelKHR = (PFNGLOBJECTPTRLABELKHRPROC) load(userptr, "glObjectPtrLabelKHR");
    PAGL_PopDebugGroupKHR = (PFNGLPOPDEBUGGROUPKHRPROC) load(userptr, "glPopDebugGroupKHR");
    PAGL_PushDebugGroupKHR = (PFNGLPUSHDEBUGGROUPKHRPROC) load(userptr, "glPushDebugGroupKHR");

    PAGL_DebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC) load(userptr, "glDebugMessageCallback");
    PAGL_DebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC) load(userptr, "glDebugMessageControl");
    PAGL_DebugMessageInsert = (PFNGLDEBUGMESSAGEINSERTPROC) load(userptr, "glDebugMessageInsert");
    PAGL_GetDebugMessageLog = (PFNGLGETDEBUGMESSAGELOGPROC) load(userptr, "glGetDebugMessageLog");
}



#if defined(GL_ES_VERSION_3_0) || defined(GL_VERSION_3_0)
#define PAGL_GL_IS_SOME_NEW_VERSION 1
#else
#define PAGL_GL_IS_SOME_NEW_VERSION 0
#endif

static int glad_gl_get_extensions( int version, const char **out_exts, unsigned int *out_num_exts_i, char ***out_exts_i) {
#if PAGL_GL_IS_SOME_NEW_VERSION
    if(PAGL_VERSION_MAJOR(version) < 3) {
#else
    PAGL_UNUSED(version);
    PAGL_UNUSED(out_num_exts_i);
    PAGL_UNUSED(out_exts_i);
#endif
        *out_exts = (const char *)glGetString(GL_EXTENSIONS);
#if PAGL_GL_IS_SOME_NEW_VERSION
    } else {
        unsigned int index = 0;
        unsigned int num_exts_i = 0;
        char **exts_i = NULL;
        if (PAGL_GetStringi == NULL || PAGL_GetIntegerv == NULL) {
            return 0;
        }
        PAGL_GetIntegerv(GL_NUM_EXTENSIONS, (int*) &num_exts_i);
        if (num_exts_i > 0) {
            exts_i = (char **) malloc(num_exts_i * (sizeof *exts_i));
        }
        if (exts_i == NULL) {
            return 0;
        }
        for(index = 0; index < num_exts_i; index++) {
            const char *gl_str_tmp = (const char*) PAGL_GetStringi(GL_EXTENSIONS, index);
            size_t len = strlen(gl_str_tmp) + 1;

            char *local_str = (char*) malloc(len * sizeof(char));
            if(local_str != NULL) {
                memcpy(local_str, gl_str_tmp, len * sizeof(char));
            }

            exts_i[index] = local_str;
        }

        *out_num_exts_i = num_exts_i;
        *out_exts_i = exts_i;
    }
#endif
    return 1;
}
static void glad_gl_free_extensions(char **exts_i, unsigned int num_exts_i) {
    if (exts_i != NULL) {
        unsigned int index;
        for(index = 0; index < num_exts_i; index++) {
            free((void *) (exts_i[index]));
        }
        free((void *)exts_i);
        exts_i = NULL;
    }
}
static int glad_gl_has_extension(int version, const char *exts, unsigned int num_exts_i, char **exts_i, const char *ext) {
    if(PAGL_VERSION_MAJOR(version) < 3 || !PAGL_GL_IS_SOME_NEW_VERSION) {
        const char *extensions;
        const char *loc;
        const char *terminator;
        extensions = exts;
        if(extensions == NULL || ext == NULL) {
            return 0;
        }
        while(1) {
            loc = strstr(extensions, ext);
            if(loc == NULL) {
                return 0;
            }
            terminator = loc + strlen(ext);
            if((loc == extensions || *(loc - 1) == ' ') &&
                (*terminator == ' ' || *terminator == '\0')) {
                return 1;
            }
            extensions = terminator;
        }
    } else {
        unsigned int index;
        for(index = 0; index < num_exts_i; index++) {
            const char *e = exts_i[index];
            if(strcmp(e, ext) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static PAGLapiproc glad_gl_get_proc_from_userptr(void *userptr, const char* name) {
    return (PAGL_GNUC_EXTENSION (PAGLapiproc (*)(const char *name)) userptr)(name);
}

static int glad_gl_find_extensions_gles2( int version) {
    const char *exts = NULL;
    unsigned int num_exts_i = 0;
    char **exts_i = NULL;
    if (!glad_gl_get_extensions(version, &exts, &num_exts_i, &exts_i)) return 0;

    PAGL_GL_KHR_debug = glad_gl_has_extension(version, exts, num_exts_i, exts_i, "GL_KHR_debug");

    glad_gl_free_extensions(exts_i, num_exts_i);

    return 1;
}

static int glad_gl_find_core_gles2(void) {
    int i;
    const char* version;
    const char* prefixes[] = {
        "OpenGL ES-CM ",
        "OpenGL ES-CL ",
        "OpenGL ES ",
        "OpenGL SC ",
        NULL
    };
    int major = 0;
    int minor = 0;
    version = (const char*) PAGL_GetString(GL_VERSION);
    if (!version) return 0;
    for (i = 0;  prefixes[i];  i++) {
        const size_t length = strlen(prefixes[i]);
        if (strncmp(version, prefixes[i], length) == 0) {
            version += length;
            break;
        }
    }

    PAGL_IMPL_UTIL_SSCANF(version, "%d.%d", &major, &minor);

    PAGL_GL_ES_VERSION_2_0 = (major == 2 && minor >= 0) || major > 2;
    PAGL_GL_ES_VERSION_3_0 = (major == 3 && minor >= 0) || major > 3;

    return PAGL_MAKE_VERSION(major, minor);
}



#if (defined(GL_OS_WINDOWS) )//||
#include <windows.h>
HMODULE wglInstance = NULL;
typedef PROC (WINAPI *PFNWGLGETPROCADDRESSPROC)(LPCSTR);
PFNWGLGETPROCADDRESSPROC my_wglGetProcAddress = NULL;
static PAGLapiproc glad_get_proc(void *vuserptr, const char* name) {
    PAGL_UNUSED(vuserptr);

    if(wglInstance == NULL)
        wglInstance = LoadLibraryA("opengl32.dll");
    if(wglInstance == NULL) return 0;

    // 获取 wglGetProcAddress 函数指针
    my_wglGetProcAddress =
            (PFNWGLGETPROCADDRESSPROC)GetProcAddress(wglInstance, "wglGetProcAddress");
    if (!my_wglGetProcAddress) {
        return 0;
    }

    PAGLapiproc ret = PAGL_GNUC_EXTENSION (PAGLapiproc) my_wglGetProcAddress(name);
    if(ret == NULL) {
        ret = PAGL_GNUC_EXTENSION (PAGLapiproc)GetProcAddress(wglInstance, name);
        if(ret == NULL) {
            printf("ERROR:GPU not support opengl3 error function name:%s\n", name);
            fflush(stdout);
        }
    }
    return ret;
}
int gladLoadGLES2Loader(GLADloadproc a){
    return gladLoadGLLoader(a);
}
int gladLoadGLLoader(GLADloadproc a){

    int version;
    void * ud = NULL;
    PAGL_GetString = (PFNGLGETSTRINGPROC) glad_get_proc(ud, "glGetString");
    if(PAGL_GetString == NULL) return 0;
    if(PAGL_GetString(GL_VERSION) == NULL) return 0;
    version = glad_gl_find_core_gles2();

    loadGLES20(glad_get_proc, ud);
    loadGLES30(glad_get_proc, ud);
    loadGL(glad_get_proc, ud);
    if (!glad_gl_find_extensions_gles2(version)) return 0;
    glad_gl_load_GL_KHR_debug(glad_get_proc, ud);

    return version;
}
#elif defined(GL_OS_LINUX)
#include <dlfcn.h>
static void * libeglHandle = NULL;
static void * libglesHandle = NULL;
PAGL_API_PTR void (*handle_eglGetProcAddress)(char const*procname);
static PAGLapiproc glad_get_proc(void *vuserptr, const char* name) {
    PAGL_UNUSED(vuserptr);
    if(libeglHandle == NULL){

        libeglHandle = dlopen("libEGL.so.1", RTLD_LAZY|RTLD_LOCAL);//PAGLapiproc ret = PAGL_GNUC_EXTENSION (PAGLapiproc)
        if(libeglHandle == NULL) {
            printf("ERROR:GPU not support opengl3 error function name:%s\n", "libEGL.so.1");
            exit(0);
        }
        if(handle_eglGetProcAddress == NULL){
                handle_eglGetProcAddress = dlsym(libeglHandle, "eglGetProcAddress");
            if(handle_eglGetProcAddress == NULL) {
                printf("ERROR:GPU not support opengl3 error function name:%s\n", "libGLESv2.so.2 eglGetProcAddress");
                exit(0);
            }
        }

    }
    if(libglesHandle == NULL){

        libglesHandle = dlopen("libGLESv2.so.2", RTLD_LAZY|RTLD_LOCAL);//PAGLapiproc ret = PAGL_GNUC_EXTENSION (PAGLapiproc)
        if(libglesHandle == NULL) {
            printf("ERROR:GPU not support opengl3 error function name:%s\n", "libEGL.so.1");
            exit(0);
        }
    }
    PAGLapiproc ret = PAGL_GNUC_EXTENSION (PAGLapiproc) dlsym(libglesHandle, name);
    return ret;
}

int LoadGLES(){
    gladLoadGLES2UserPtr( glad_get_proc, PAGL_GNUC_EXTENSION (void*) 0);
    return 0;
}
#endif
#else

static void other_gl_load_GL_ES_VERSION_2_0() {
    if(!PAGL_GL_ES_VERSION_2_0) return;
    PAGL_ActiveTexture = (PFNGLACTIVETEXTUREPROC) glActiveTexture;
    PAGL_AttachShader = (PFNGLATTACHSHADERPROC) glAttachShader;
    PAGL_BindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC) glBindAttribLocation;
    PAGL_BindBuffer = (PFNGLBINDBUFFERPROC) glBindBuffer;
    PAGL_BindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) glBindFramebuffer;
    PAGL_BindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) glBindRenderbuffer;
    PAGL_BindTexture = (PFNGLBINDTEXTUREPROC) glBindTexture;
    PAGL_BlendColor = (PFNGLBLENDCOLORPROC) glBlendColor;
    PAGL_BlendEquation = (PFNGLBLENDEQUATIONPROC) glBlendEquation;
    PAGL_BlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC) glBlendEquationSeparate;
    PAGL_BlendFunc = (PFNGLBLENDFUNCPROC) glBlendFunc;
    PAGL_BlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC) glBlendFuncSeparate;
    PAGL_BufferData = (PFNGLBUFFERDATAPROC) glBufferData;
    PAGL_BufferSubData = (PFNGLBUFFERSUBDATAPROC) glBufferSubData;
    PAGL_CheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) glCheckFramebufferStatus;
    PAGL_Clear = (PFNGLCLEARPROC) glClear;
    PAGL_ClearColor = (PFNGLCLEARCOLORPROC) glClearColor;
    PAGL_ClearDepthf = (PFNGLCLEARDEPTHFPROC) glClearDepthf;
    PAGL_ClearStencil = (PFNGLCLEARSTENCILPROC) glClearStencil;
    PAGL_ColorMask = (PFNGLCOLORMASKPROC) glColorMask;
    PAGL_CompileShader = (PFNGLCOMPILESHADERPROC) glCompileShader;
    PAGL_CompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC) glCompressedTexImage2D;
    PAGL_CompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC) glCompressedTexSubImage2D;
    PAGL_CopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC) glCopyTexImage2D;
    PAGL_CopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC) glCopyTexSubImage2D;
    PAGL_CreateProgram = (PFNGLCREATEPROGRAMPROC) glCreateProgram;
    PAGL_CreateShader = (PFNGLCREATESHADERPROC) glCreateShader;
    PAGL_CullFace = (PFNGLCULLFACEPROC) glCullFace;
    PAGL_DeleteBuffers = (PFNGLDELETEBUFFERSPROC) glDeleteBuffers;
    PAGL_DeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) glDeleteFramebuffers;
    PAGL_DeleteProgram = (PFNGLDELETEPROGRAMPROC) glDeleteProgram;
    PAGL_DeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) glDeleteRenderbuffers;
    PAGL_DeleteShader = (PFNGLDELETESHADERPROC) glDeleteShader;
    PAGL_DeleteTextures = (PFNGLDELETETEXTURESPROC) glDeleteTextures;
    PAGL_DepthFunc = (PFNGLDEPTHFUNCPROC) glDepthFunc;
    PAGL_DepthMask = (PFNGLDEPTHMASKPROC) glDepthMask;
    PAGL_DepthRangef = (PFNGLDEPTHRANGEFPROC) glDepthRangef;
    PAGL_DetachShader = (PFNGLDETACHSHADERPROC) glDetachShader;
    PAGL_Disable = (PFNGLDISABLEPROC) glDisable;
    PAGL_DisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC) glDisableVertexAttribArray;
    PAGL_DrawArrays = (PFNGLDRAWARRAYSPROC) glDrawArrays;
    PAGL_DrawElements = (PFNGLDRAWELEMENTSPROC) glDrawElements;
    PAGL_Enable = (PFNGLENABLEPROC) glEnable;
    PAGL_EnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) glEnableVertexAttribArray;
    PAGL_Finish = (PFNGLFINISHPROC) glFinish;
    PAGL_Flush = (PFNGLFLUSHPROC) glFlush;
    PAGL_FramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) glFramebufferRenderbuffer;
    PAGL_FramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) glFramebufferTexture2D;
    PAGL_FrontFace = (PFNGLFRONTFACEPROC) glFrontFace;
    PAGL_GenBuffers = (PFNGLGENBUFFERSPROC) glGenBuffers;
    PAGL_GenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) glGenFramebuffers;
    PAGL_GenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) glGenRenderbuffers;
    PAGL_GenTextures = (PFNGLGENTEXTURESPROC) glGenTextures;
    PAGL_GenerateMipmap = (PFNGLGENERATEMIPMAPPROC) glGenerateMipmap;
    PAGL_GetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC) glGetActiveAttrib;
    PAGL_GetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC) glGetActiveUniform;
    PAGL_GetAttachedShaders = (PFNGLGETATTACHEDSHADERSPROC) glGetAttachedShaders;
    PAGL_GetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC) glGetAttribLocation;
    PAGL_GetBooleanv = (PFNGLGETBOOLEANVPROC) glGetBooleanv;
    PAGL_GetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC) glGetBufferParameteriv;
    PAGL_GetError = (PFNGLGETERRORPROC) glGetError;
    PAGL_GetFloatv = (PFNGLGETFLOATVPROC) glGetFloatv;
    PAGL_GetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) glGetFramebufferAttachmentParameteriv;
    PAGL_GetIntegerv = (PFNGLGETINTEGERVPROC) glGetIntegerv;
    PAGL_GetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC) glGetProgramInfoLog;
    PAGL_GetProgramiv = (PFNGLGETPROGRAMIVPROC) glGetProgramiv;
    PAGL_GetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) glGetRenderbufferParameteriv;
    PAGL_GetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) glGetShaderInfoLog;
    PAGL_GetShaderPrecisionFormat = (PFNGLGETSHADERPRECISIONFORMATPROC) glGetShaderPrecisionFormat;
    PAGL_GetShaderSource = (PFNGLGETSHADERSOURCEPROC) glGetShaderSource;
    PAGL_GetShaderiv = (PFNGLGETSHADERIVPROC) glGetShaderiv;
    PAGL_GetString = (PFNGLGETSTRINGPROC) glGetString;
    PAGL_GetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC) glGetTexParameterfv;
    PAGL_GetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC) glGetTexParameteriv;
    PAGL_GetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) glGetUniformLocation;
    PAGL_GetUniformfv = (PFNGLGETUNIFORMFVPROC) glGetUniformfv;
    PAGL_GetUniformiv = (PFNGLGETUNIFORMIVPROC) glGetUniformiv;
    PAGL_GetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC) glGetVertexAttribPointerv;
    PAGL_GetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC) glGetVertexAttribfv;
    PAGL_GetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC) glGetVertexAttribiv;
    PAGL_Hint = (PFNGLHINTPROC) glHint;
    PAGL_IsBuffer = (PFNGLISBUFFERPROC) glIsBuffer;
    PAGL_IsEnabled = (PFNGLISENABLEDPROC) glIsEnabled;
    PAGL_IsFramebuffer = (PFNGLISFRAMEBUFFERPROC) glIsFramebuffer;
    PAGL_IsProgram = (PFNGLISPROGRAMPROC) glIsProgram;
    PAGL_IsRenderbuffer = (PFNGLISRENDERBUFFERPROC) glIsRenderbuffer;
    PAGL_IsShader = (PFNGLISSHADERPROC) glIsShader;
    PAGL_IsTexture = (PFNGLISTEXTUREPROC) glIsTexture;
    PAGL_LineWidth = (PFNGLLINEWIDTHPROC) glLineWidth;
    PAGL_LinkProgram = (PFNGLLINKPROGRAMPROC) glLinkProgram;
    PAGL_PixelStorei = (PFNGLPIXELSTOREIPROC) glPixelStorei;
    PAGL_PolygonOffset = (PFNGLPOLYGONOFFSETPROC) glPolygonOffset;
    PAGL_ReadPixels = (PFNGLREADPIXELSPROC) glReadPixels;
    PAGL_ReleaseShaderCompiler = (PFNGLRELEASESHADERCOMPILERPROC) glReleaseShaderCompiler;
    PAGL_RenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) glRenderbufferStorage;
    PAGL_SampleCoverage = (PFNGLSAMPLECOVERAGEPROC) glSampleCoverage;
    PAGL_Scissor = (PFNGLSCISSORPROC) glScissor;
    PAGL_ShaderBinary = (PFNGLSHADERBINARYPROC) glShaderBinary;
    PAGL_ShaderSource = (PFNGLSHADERSOURCEPROC) glShaderSource;
    PAGL_StencilFunc = (PFNGLSTENCILFUNCPROC) glStencilFunc;
    PAGL_StencilFuncSeparate = (PFNGLSTENCILFUNCSEPARATEPROC) glStencilFuncSeparate;
    PAGL_StencilMask = (PFNGLSTENCILMASKPROC) glStencilMask;
    PAGL_StencilMaskSeparate = (PFNGLSTENCILMASKSEPARATEPROC) glStencilMaskSeparate;
    PAGL_StencilOp = (PFNGLSTENCILOPPROC) glStencilOp;
    PAGL_StencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC) glStencilOpSeparate;
    PAGL_TexImage2D = (PFNGLTEXIMAGE2DPROC) glTexImage2D;
    PAGL_TexParameterf = (PFNGLTEXPARAMETERFPROC) glTexParameterf;
    PAGL_TexParameterfv = (PFNGLTEXPARAMETERFVPROC) glTexParameterfv;
    PAGL_TexParameteri = (PFNGLTEXPARAMETERIPROC) glTexParameteri;
    PAGL_TexParameteriv = (PFNGLTEXPARAMETERIVPROC) glTexParameteriv;
    PAGL_TexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC) glTexSubImage2D;
    PAGL_Uniform1f = (PFNGLUNIFORM1FPROC) glUniform1f;
    PAGL_Uniform1fv = (PFNGLUNIFORM1FVPROC) glUniform1fv;
    PAGL_Uniform1i = (PFNGLUNIFORM1IPROC) glUniform1i;
    PAGL_Uniform1iv = (PFNGLUNIFORM1IVPROC) glUniform1iv;
    PAGL_Uniform2f = (PFNGLUNIFORM2FPROC) glUniform2f;
    PAGL_Uniform2fv = (PFNGLUNIFORM2FVPROC) glUniform2fv;
    PAGL_Uniform2i = (PFNGLUNIFORM2IPROC) glUniform2i;
    PAGL_Uniform2iv = (PFNGLUNIFORM2IVPROC) glUniform2iv;
    PAGL_Uniform3f = (PFNGLUNIFORM3FPROC) glUniform3f;
    PAGL_Uniform3fv = (PFNGLUNIFORM3FVPROC) glUniform3fv;
    PAGL_Uniform3i = (PFNGLUNIFORM3IPROC) glUniform3i;
    PAGL_Uniform3iv = (PFNGLUNIFORM3IVPROC) glUniform3iv;
    PAGL_Uniform4f = (PFNGLUNIFORM4FPROC) glUniform4f;
    PAGL_Uniform4fv = (PFNGLUNIFORM4FVPROC) glUniform4fv;
    PAGL_Uniform4i = (PFNGLUNIFORM4IPROC) glUniform4i;
    PAGL_Uniform4iv = (PFNGLUNIFORM4IVPROC) glUniform4iv;
    PAGL_UniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC) glUniformMatrix2fv;
    PAGL_UniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC) glUniformMatrix3fv;
    PAGL_UniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC) glUniformMatrix4fv;
    PAGL_UseProgram = (PFNGLUSEPROGRAMPROC) glUseProgram;
    PAGL_ValidateProgram = (PFNGLVALIDATEPROGRAMPROC) glValidateProgram;
    PAGL_VertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC) glVertexAttrib1f;
    PAGL_VertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC) glVertexAttrib1fv;
    PAGL_VertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC) glVertexAttrib2f;
    PAGL_VertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC) glVertexAttrib2fv;
    PAGL_VertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC) glVertexAttrib3f;
    PAGL_VertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC) glVertexAttrib3fv;
    PAGL_VertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC) glVertexAttrib4f;
    PAGL_VertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC) glVertexAttrib4fv;
    PAGL_VertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC) glVertexAttribPointer;
    PAGL_Viewport = (PFNGLVIEWPORTPROC) glViewport;
}
static void other_gl_load_GL_ES_VERSION_3_0() {
    if(!PAGL_GL_ES_VERSION_3_0) return;
    PAGL_BeginQuery = (PFNGLBEGINQUERYPROC) glBeginQuery;
    PAGL_BeginTransformFeedback = (PFNGLBEGINTRANSFORMFEEDBACKPROC) glBeginTransformFeedback;
    PAGL_BindBufferBase = (PFNGLBINDBUFFERBASEPROC) glBindBufferBase;
    PAGL_BindBufferRange = (PFNGLBINDBUFFERRANGEPROC) glBindBufferRange;
    PAGL_BindSampler = (PFNGLBINDSAMPLERPROC) glBindSampler;
    PAGL_BindTransformFeedback = (PFNGLBINDTRANSFORMFEEDBACKPROC) glBindTransformFeedback;
    PAGL_BindVertexArray = (PFNGLBINDVERTEXARRAYPROC) glBindVertexArray;
    PAGL_BlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC) glBlitFramebuffer;
    PAGL_ClearBufferfi = (PFNGLCLEARBUFFERFIPROC) glClearBufferfi;
    PAGL_ClearBufferfv = (PFNGLCLEARBUFFERFVPROC) glClearBufferfv;
    PAGL_ClearBufferiv = (PFNGLCLEARBUFFERIVPROC) glClearBufferiv;
    PAGL_ClearBufferuiv = (PFNGLCLEARBUFFERUIVPROC) glClearBufferuiv;
    PAGL_ClientWaitSync = (PFNGLCLIENTWAITSYNCPROC) glClientWaitSync;
    PAGL_CompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC) glCompressedTexImage3D;
    PAGL_CompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC) glCompressedTexSubImage3D;
    PAGL_CopyBufferSubData = (PFNGLCOPYBUFFERSUBDATAPROC) glCopyBufferSubData;
    PAGL_CopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC) glCopyTexSubImage3D;
    PAGL_DeleteQueries = (PFNGLDELETEQUERIESPROC) glDeleteQueries;
    PAGL_DeleteSamplers = (PFNGLDELETESAMPLERSPROC) glDeleteSamplers;
    PAGL_DeleteSync = (PFNGLDELETESYNCPROC) glDeleteSync;
    PAGL_DeleteTransformFeedbacks = (PFNGLDELETETRANSFORMFEEDBACKSPROC) glDeleteTransformFeedbacks;
    PAGL_DeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) glDeleteVertexArrays;
    PAGL_DrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC) glDrawArraysInstanced;
    PAGL_DrawBuffers = (PFNGLDRAWBUFFERSPROC) glDrawBuffers;
    PAGL_DrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC) glDrawElementsInstanced;
    PAGL_DrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC) glDrawRangeElements;
    PAGL_EndQuery = (PFNGLENDQUERYPROC) glEndQuery;
    PAGL_EndTransformFeedback = (PFNGLENDTRANSFORMFEEDBACKPROC) glEndTransformFeedback;
    PAGL_FenceSync = (PFNGLFENCESYNCPROC) glFenceSync;
    PAGL_FlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC) glFlushMappedBufferRange;
    PAGL_FramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC) glFramebufferTextureLayer;
    PAGL_GenQueries = (PFNGLGENQUERIESPROC) glGenQueries;
    PAGL_GenSamplers = (PFNGLGENSAMPLERSPROC) glGenSamplers;
    PAGL_GenTransformFeedbacks = (PFNGLGENTRANSFORMFEEDBACKSPROC) glGenTransformFeedbacks;
    PAGL_GenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) glGenVertexArrays;
    PAGL_GetActiveUniformBlockName = (PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC) glGetActiveUniformBlockName;
    PAGL_GetActiveUniformBlockiv = (PFNGLGETACTIVEUNIFORMBLOCKIVPROC) glGetActiveUniformBlockiv;
    PAGL_GetActiveUniformsiv = (PFNGLGETACTIVEUNIFORMSIVPROC) glGetActiveUniformsiv;
    PAGL_GetBufferParameteri64v = (PFNGLGETBUFFERPARAMETERI64VPROC) glGetBufferParameteri64v;
    PAGL_GetBufferPointerv = (PFNGLGETBUFFERPOINTERVPROC) glGetBufferPointerv;
    PAGL_GetFragDataLocation = (PFNGLGETFRAGDATALOCATIONPROC) glGetFragDataLocation;
    PAGL_GetInteger64i_v = (PFNGLGETINTEGER64I_VPROC) glGetInteger64i_v;
    PAGL_GetInteger64v = (PFNGLGETINTEGER64VPROC) glGetInteger64v;
    PAGL_GetIntegeri_v = (PFNGLGETINTEGERI_VPROC) glGetIntegeri_v;
    PAGL_GetInternalformativ = (PFNGLGETINTERNALFORMATIVPROC) NULL;//glGetInternalformativ;
    PAGL_GetProgramBinary = (PFNGLGETPROGRAMBINARYPROC) glGetProgramBinary;
    PAGL_GetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC) glGetQueryObjectuiv;
    PAGL_GetQueryiv = (PFNGLGETQUERYIVPROC) glGetQueryiv;
    PAGL_GetSamplerParameterfv = (PFNGLGETSAMPLERPARAMETERFVPROC) glGetSamplerParameterfv;
    PAGL_GetSamplerParameteriv = (PFNGLGETSAMPLERPARAMETERIVPROC) glGetSamplerParameteriv;
    PAGL_GetStringi = (PFNGLGETSTRINGIPROC) glGetStringi;
    PAGL_GetSynciv = (PFNGLGETSYNCIVPROC) glGetSynciv;
    PAGL_GetTransformFeedbackVarying = (PFNGLGETTRANSFORMFEEDBACKVARYINGPROC) glGetTransformFeedbackVarying;
    PAGL_GetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC) glGetUniformBlockIndex;
    PAGL_GetUniformIndices = (PFNGLGETUNIFORMINDICESPROC) glGetUniformIndices;
    PAGL_GetUniformuiv = (PFNGLGETUNIFORMUIVPROC) glGetUniformuiv;
    PAGL_GetVertexAttribIiv = (PFNGLGETVERTEXATTRIBIIVPROC) glGetVertexAttribIiv;
    PAGL_GetVertexAttribIuiv = (PFNGLGETVERTEXATTRIBIUIVPROC) glGetVertexAttribIuiv;
    PAGL_InvalidateFramebuffer = (PFNGLINVALIDATEFRAMEBUFFERPROC)  NULL;//glInvalidateFramebuffer;//opengl 4.3, opengl ES 3.0
    PAGL_InvalidateSubFramebuffer = (PFNGLINVALIDATESUBFRAMEBUFFERPROC)  NULL;//glInvalidateSubFramebuffer;//opengl 4.3, opengl ES 3.0
    PAGL_IsQuery = (PFNGLISQUERYPROC) glIsQuery;
    PAGL_IsSampler = (PFNGLISSAMPLERPROC) glIsSampler;
    PAGL_IsSync = (PFNGLISSYNCPROC) glIsSync;
    PAGL_IsTransformFeedback = (PFNGLISTRANSFORMFEEDBACKPROC) glIsTransformFeedback;
    PAGL_IsVertexArray = (PFNGLISVERTEXARRAYPROC) glIsVertexArray;
    PAGL_MapBufferRange = (PFNGLMAPBUFFERRANGEPROC) glMapBufferRange;
    PAGL_PauseTransformFeedback = (PFNGLPAUSETRANSFORMFEEDBACKPROC) glPauseTransformFeedback;
    PAGL_ProgramBinary = (PFNGLPROGRAMBINARYPROC) glProgramBinary;
    PAGL_ProgramParameteri = (PFNGLPROGRAMPARAMETERIPROC) glProgramParameteri;
    PAGL_ReadBuffer = (PFNGLREADBUFFERPROC) glReadBuffer;
    PAGL_RenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) glRenderbufferStorageMultisample;
    PAGL_ResumeTransformFeedback = (PFNGLRESUMETRANSFORMFEEDBACKPROC) glResumeTransformFeedback;
    PAGL_SamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC) glSamplerParameterf;
    PAGL_SamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC) glSamplerParameterfv;
    PAGL_SamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC) glSamplerParameteri;
    PAGL_SamplerParameteriv = (PFNGLSAMPLERPARAMETERIVPROC) glSamplerParameteriv;
    PAGL_TexImage3D = (PFNGLTEXIMAGE3DPROC) glTexImage3D;
    PAGL_TexStorage2D = (PFNGLTEXSTORAGE2DPROC) NULL;//glTexStorage2D;//opengl 4.2, opengl ES 3.0
    PAGL_TexStorage3D = (PFNGLTEXSTORAGE3DPROC) NULL;//glTexStorage3D;//opengl 4.2, opengl ES 3.0
    PAGL_TexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC) glTexSubImage3D;
    PAGL_TransformFeedbackVaryings = (PFNGLTRANSFORMFEEDBACKVARYINGSPROC) glTransformFeedbackVaryings;
    PAGL_Uniform1ui = (PFNGLUNIFORM1UIPROC) glUniform1ui;
    PAGL_Uniform1uiv = (PFNGLUNIFORM1UIVPROC) glUniform1uiv;
    PAGL_Uniform2ui = (PFNGLUNIFORM2UIPROC) glUniform2ui;
    PAGL_Uniform2uiv = (PFNGLUNIFORM2UIVPROC) glUniform2uiv;
    PAGL_Uniform3ui = (PFNGLUNIFORM3UIPROC) glUniform3ui;
    PAGL_Uniform3uiv = (PFNGLUNIFORM3UIVPROC) glUniform3uiv;
    PAGL_Uniform4ui = (PFNGLUNIFORM4UIPROC) glUniform4ui;
    PAGL_Uniform4uiv = (PFNGLUNIFORM4UIVPROC) glUniform4uiv;
    PAGL_UniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC) glUniformBlockBinding;
    PAGL_UniformMatrix2x3fv = (PFNGLUNIFORMMATRIX2X3FVPROC) glUniformMatrix2x3fv;
    PAGL_UniformMatrix2x4fv = (PFNGLUNIFORMMATRIX2X4FVPROC) glUniformMatrix2x4fv;
    PAGL_UniformMatrix3x2fv = (PFNGLUNIFORMMATRIX3X2FVPROC) glUniformMatrix3x2fv;
    PAGL_UniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC) glUniformMatrix3x4fv;
    PAGL_UniformMatrix4x2fv = (PFNGLUNIFORMMATRIX4X2FVPROC) glUniformMatrix4x2fv;
    PAGL_UniformMatrix4x3fv = (PFNGLUNIFORMMATRIX4X3FVPROC) glUniformMatrix4x3fv;
    PAGL_UnmapBuffer = (PFNGLUNMAPBUFFERPROC) glUnmapBuffer;
    PAGL_VertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC) glVertexAttribDivisor;
    PAGL_VertexAttribI4i = (PFNGLVERTEXATTRIBI4IPROC) glVertexAttribI4i;
    PAGL_VertexAttribI4iv = (PFNGLVERTEXATTRIBI4IVPROC) glVertexAttribI4iv;
    PAGL_VertexAttribI4ui = (PFNGLVERTEXATTRIBI4UIPROC) glVertexAttribI4ui;
    PAGL_VertexAttribI4uiv = (PFNGLVERTEXATTRIBI4UIVPROC) glVertexAttribI4uiv;
    PAGL_VertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC) glVertexAttribIPointer;
    PAGL_WaitSync = (PFNGLWAITSYNCPROC) glWaitSync;
}
static void other_gl_load_GL_KHR_debug() {
    if(!PAGL_GL_KHR_debug) return;
    #ifdef GL_OS_WINDOWS
    PAGL_DebugMessageCallbackKHR = (PFNGLDEBUGMESSAGECALLBACKKHRPROC) glDebugMessageCallbackKHR;
    PAGL_DebugMessageControlKHR = (PFNGLDEBUGMESSAGECONTROLKHRPROC) glDebugMessageControlKHR;
    PAGL_DebugMessageInsertKHR = (PFNGLDEBUGMESSAGEINSERTKHRPROC) glDebugMessageInsertKHR;
    PAGL_GetDebugMessageLogKHR = (PFNGLGETDEBUGMESSAGELOGKHRPROC) glGetDebugMessageLogKHR;
    PAGL_GetObjectLabelKHR = (PFNGLGETOBJECTLABELKHRPROC) glGetObjectLabelKHR;
    PAGL_GetObjectPtrLabelKHR = (PFNGLGETOBJECTPTRLABELKHRPROC) glGetObjectPtrLabelKHR;
    PAGL_GetPointervKHR = (PFNGLGETPOINTERVKHRPROC) glGetPointervKHR;
    PAGL_ObjectLabelKHR = (PFNGLOBJECTLABELKHRPROC) glObjectLabelKHR;
    PAGL_ObjectPtrLabelKHR = (PFNGLOBJECTPTRLABELKHRPROC) glObjectPtrLabelKHR;
    PAGL_PopDebugGroupKHR = (PFNGLPOPDEBUGGROUPKHRPROC) glPopDebugGroupKHR;
    PAGL_PushDebugGroupKHR = (PFNGLPUSHDEBUGGROUPKHRPROC) glPushDebugGroupKHR;
    #endif
}

int LoadGLES(){
    other_gl_load_GL_ES_VERSION_2_0();
    other_gl_load_GL_ES_VERSION_3_0();
    other_gl_load_GL_KHR_debug();
    return 0;
}
// #elif defined(GL_OS_MACOS)

// #elif defined(GL_OS_LINUX)

// #elif defined(GL_OS_IPHONE)

// #elif defined(GL_OS_ANDROID)

#endif

#if __OS_WINDOWS || __OS_LINUX
void glActiveTexture(GLenum texture) {
    PAGL_ActiveTexture(texture);
}
void glAttachShader(GLuint program, GLuint shader) {
    PAGL_AttachShader(program, shader);
}
void glBeginQuery(GLenum target, GLuint id) {
    PAGL_BeginQuery(target, id);
}
void glBeginTransformFeedback(GLenum primitiveMode) {
    PAGL_BeginTransformFeedback(primitiveMode);
}
void glBindAttribLocation(GLuint program, GLuint index, const GLchar * name) {
    PAGL_BindAttribLocation(program, index, name);
}
void glBindBuffer(GLenum target, GLuint buffer) {
    PAGL_BindBuffer(target, buffer);
}
void glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    PAGL_BindBufferBase(target, index, buffer);
}
void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    PAGL_BindBufferRange(target, index, buffer, offset, size);
}
void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    PAGL_BindFramebuffer(target, framebuffer);
}
void glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    PAGL_BindRenderbuffer(target, renderbuffer);
}
void glBindSampler(GLuint unit, GLuint sampler) {
    PAGL_BindSampler(unit, sampler);
}
void glBindTexture(GLenum target, GLuint texture) {
    PAGL_BindTexture(target, texture);
}
void glBindTransformFeedback(GLenum target, GLuint id) {
    PAGL_BindTransformFeedback(target, id);
}
void glBindVertexArray(GLuint array) {
    PAGL_BindVertexArray(array);
}
void glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    PAGL_BlendColor(red, green, blue, alpha);
}
void glBlendEquation(GLenum mode) {
    PAGL_BlendEquation(mode);
}
void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    PAGL_BlendEquationSeparate(modeRGB, modeAlpha);
}
void glBlendFunc(GLenum sfactor, GLenum dfactor) {
    PAGL_BlendFunc(sfactor, dfactor);
}
void glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    PAGL_BlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}
void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    PAGL_BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}
void glBufferData(GLenum target, GLsizeiptr size, const void * data, GLenum usage) {
    PAGL_BufferData(target, size, data, usage);
}
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void * data) {
    PAGL_BufferSubData(target, offset, size, data);
}
GLenum glCheckFramebufferStatus(GLenum target) {
    return PAGL_CheckFramebufferStatus(target);
}
void glClear(GLbitfield mask) {
    PAGL_Clear(mask);
}
void glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    PAGL_ClearBufferfi(buffer, drawbuffer, depth, stencil);
}
void glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat * value) {
    PAGL_ClearBufferfv(buffer, drawbuffer, value);
}
void glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint * value) {
    PAGL_ClearBufferiv(buffer, drawbuffer, value);
}
void glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint * value) {
    PAGL_ClearBufferuiv(buffer, drawbuffer, value);
}
void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    PAGL_ClearColor(red, green, blue, alpha);
}
void glClearDepthf(GLfloat d) {
    PAGL_ClearDepthf(d);
}
void glClearStencil(GLint s) {
    PAGL_ClearStencil(s);
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    PAGL_ColorMask(red, green, blue, alpha);
}
void glCompileShader(GLuint shader) {
    PAGL_CompileShader(shader);
}
void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void * data) {
    PAGL_CompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}
void glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void * data) {
    PAGL_CompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data);
}
void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void * data) {
    PAGL_CompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
void glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void * data) {
    PAGL_CompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}
void glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    PAGL_CopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}
void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    PAGL_CopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}
void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    PAGL_CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}
void glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    PAGL_CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}
GLuint glCreateProgram(void) {
    return PAGL_CreateProgram();
}
GLuint glCreateShader(GLenum type) {
    return PAGL_CreateShader(type);
}
void glCullFace(GLenum mode) {
    PAGL_CullFace(mode);
}

void glDebugMessageCallbackKHR(GLDEBUGPROCKHR callback, const void * userParam) {
    PAGL_DebugMessageCallbackKHR(callback, userParam);
}
void glDebugMessageControlKHR(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint * ids, GLboolean enabled) {
    PAGL_DebugMessageControlKHR(source, type, severity, count, ids, enabled);
}
void glDebugMessageInsertKHR(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * buf) {
    PAGL_DebugMessageInsertKHR(source, type, id, severity, length, buf);
}

void glDebugMessageCallback(GLDEBUGPROCKHR callback, const void * userParam) {
    PAGL_DebugMessageCallback(callback, userParam);
}
void glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint * ids, GLboolean enabled) {
    PAGL_DebugMessageControl(source, type, severity, count, ids, enabled);
}
void glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * buf) {
    PAGL_DebugMessageInsert(source, type, id, severity, length, buf);
}

void glDeleteBuffers(GLsizei n, const GLuint * buffers) {
    PAGL_DeleteBuffers(n, buffers);
}
void glDeleteFramebuffers(GLsizei n, const GLuint * framebuffers) {
    PAGL_DeleteFramebuffers(n, framebuffers);
}
void glDeleteProgram(GLuint program) {
    PAGL_DeleteProgram(program);
}
void glDeleteQueries(GLsizei n, const GLuint * ids) {
    PAGL_DeleteQueries(n, ids);
}
void glDeleteRenderbuffers(GLsizei n, const GLuint * renderbuffers) {
    PAGL_DeleteRenderbuffers(n, renderbuffers);
}
void glDeleteSamplers(GLsizei count, const GLuint * samplers) {
    PAGL_DeleteSamplers(count, samplers);
}
void glDeleteShader(GLuint shader) {
    PAGL_DeleteShader(shader);
}
void glDeleteSync(GLsync sync) {
    PAGL_DeleteSync(sync);
}
void glDeleteTextures(GLsizei n, const GLuint * textures) {
    PAGL_DeleteTextures(n, textures);
}
void glDeleteTransformFeedbacks(GLsizei n, const GLuint * ids) {
    PAGL_DeleteTransformFeedbacks(n, ids);
}
void glDeleteVertexArrays(GLsizei n, const GLuint * arrays) {
    PAGL_DeleteVertexArrays(n, arrays);
}
void glDepthFunc(GLenum func) {
    PAGL_DepthFunc(func);
}
void glDepthMask(GLboolean flag) {
    PAGL_DepthMask(flag);
}
void glDepthRangef(GLfloat n, GLfloat f) {
    PAGL_DepthRangef(n, f);
}
void glDetachShader(GLuint program, GLuint shader) {
    PAGL_DetachShader(program, shader);
}
void glDisable(GLenum cap) {
    PAGL_Disable(cap);
}
void glDisableVertexAttribArray(GLuint index) {
    PAGL_DisableVertexAttribArray(index);
}
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    PAGL_DrawArrays(mode, first, count);
}
void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) {
    PAGL_DrawArraysInstanced(mode, first, count, instancecount);
}
void glDrawBuffers(GLsizei n, const GLenum * bufs) {
    PAGL_DrawBuffers(n, bufs);
}
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void * indices) {
    PAGL_DrawElements(mode, count, type, indices);
}
void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount) {
    PAGL_DrawElementsInstanced(mode, count, type, indices, instancecount);
}
void glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void * indices) {
    PAGL_DrawRangeElements(mode, start, end, count, type, indices);
}
void glEnable(GLenum cap) {
    PAGL_Enable(cap);
}
void glEnableVertexAttribArray(GLuint index) {
    PAGL_EnableVertexAttribArray(index);
}
void glEndQuery(GLenum target) {
    PAGL_EndQuery(target);
}
void glEndTransformFeedback(void) {
    PAGL_EndTransformFeedback();
}
GLsync glFenceSync(GLenum condition, GLbitfield flags) {
    return PAGL_FenceSync(condition, flags);
}
void glFinish(void) {
    PAGL_Finish();
}
void glFlush(void) {
    PAGL_Flush();
}
void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    PAGL_FlushMappedBufferRange(target, offset, length);
}
void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    PAGL_FramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}
void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    PAGL_FramebufferTexture2D(target, attachment, textarget, texture, level);
}
void glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    PAGL_FramebufferTextureLayer(target, attachment, texture, level, layer);
}
void glFrontFace(GLenum mode) {
    PAGL_FrontFace(mode);
}
void glGenBuffers(GLsizei n, GLuint * buffers) {
    PAGL_GenBuffers(n, buffers);
}
void glGenFramebuffers(GLsizei n, GLuint * framebuffers) {
    PAGL_GenFramebuffers(n, framebuffers);
}
void glGenQueries(GLsizei n, GLuint * ids) {
    PAGL_GenQueries(n, ids);
}
void glGenRenderbuffers(GLsizei n, GLuint * renderbuffers) {
    PAGL_GenRenderbuffers(n, renderbuffers);
}
void glGenSamplers(GLsizei count, GLuint * samplers) {
    PAGL_GenSamplers(count, samplers);
}
void glGenTextures(GLsizei n, GLuint * textures) {
    PAGL_GenTextures(n, textures);
}
void glGenTransformFeedbacks(GLsizei n, GLuint * ids) {
    PAGL_GenTransformFeedbacks(n, ids);
}
void glGenVertexArrays(GLsizei n, GLuint * arrays) {
    PAGL_GenVertexArrays(n, arrays);
}
void glGenerateMipmap(GLenum target) {
    PAGL_GenerateMipmap(target);
}
void glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name) {
    PAGL_GetActiveAttrib(program, index, bufSize, length, size, type, name);
}
void glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name) {
    PAGL_GetActiveUniform(program, index, bufSize, length, size, type, name);
}
void glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei * length, GLchar * uniformBlockName) {
    PAGL_GetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}
void glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint * params) {
    PAGL_GetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}
void glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint * uniformIndices, GLenum pname, GLint * params) {
    PAGL_GetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
}
void glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei * count, GLuint * shaders) {
    PAGL_GetAttachedShaders(program, maxCount, count, shaders);
}
GLint glGetAttribLocation(GLuint program, const GLchar * name) {
    return PAGL_GetAttribLocation(program, name);
}
void glGetBooleanv(GLenum pname, GLboolean * data) {
    PAGL_GetBooleanv(pname, data);
}

void glGetBufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    PAGL_GetBufferParameteriv(target, pname, params);
}
void glGetBufferPointerv(GLenum target, GLenum pname, void ** params) {
    PAGL_GetBufferPointerv(target, pname, params);
}
GLuint glGetDebugMessageLogKHR(GLuint count, GLsizei bufSize, GLenum * sources, GLenum * types, GLuint * ids, GLenum * severities, GLsizei * lengths, GLchar * messageLog) {
    return PAGL_GetDebugMessageLogKHR(count, bufSize, sources, types, ids, severities, lengths, messageLog);
}
GLenum glGetError(void) {
    return PAGL_GetError();
}
void glGetFloatv(GLenum pname, GLfloat * data) {
    PAGL_GetFloatv(pname, data);
}
GLint glGetFragDataLocation(GLuint program, const GLchar * name) {
    return PAGL_GetFragDataLocation(program, name);
}
void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint * params) {
    PAGL_GetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}
void glGetIntegeri_v(GLenum target, GLuint index, GLint * data) {
    PAGL_GetIntegeri_v(target, index, data);
}
void glGetIntegerv(GLenum pname, GLint * data) {
    PAGL_GetIntegerv(pname, data);
}
void glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint * params) {
    PAGL_GetInternalformativ(target, internalformat, pname, count, params);
}
void glGetObjectLabelKHR(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei * length, GLchar * label) {
    PAGL_GetObjectLabelKHR(identifier, name, bufSize, length, label);
}
void glGetObjectPtrLabelKHR(const void * ptr, GLsizei bufSize, GLsizei * length, GLchar * label) {
    PAGL_GetObjectPtrLabelKHR(ptr, bufSize, length, label);
}
void glGetPointervKHR(GLenum pname, void ** params) {
    PAGL_GetPointervKHR(pname, params);
}
void glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei * length, GLenum * binaryFormat, void * binary) {
    PAGL_GetProgramBinary(program, bufSize, length, binaryFormat, binary);
}
void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog) {
    PAGL_GetProgramInfoLog(program, bufSize, length, infoLog);
}
void glGetProgramiv(GLuint program, GLenum pname, GLint * params) {
    PAGL_GetProgramiv(program, pname, params);
}
void glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint * params) {
    PAGL_GetQueryObjectuiv(id, pname, params);
}
void glGetQueryiv(GLenum target, GLenum pname, GLint * params) {
    PAGL_GetQueryiv(target, pname, params);
}
void glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    PAGL_GetRenderbufferParameteriv(target, pname, params);
}
void glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat * params) {
    PAGL_GetSamplerParameterfv(sampler, pname, params);
}
void glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint * params) {
    PAGL_GetSamplerParameteriv(sampler, pname, params);
}
void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog) {
    PAGL_GetShaderInfoLog(shader, bufSize, length, infoLog);
}
void glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint * range, GLint * precision) {
    PAGL_GetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
}
void glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * source) {
    PAGL_GetShaderSource(shader, bufSize, length, source);
}
void glGetShaderiv(GLuint shader, GLenum pname, GLint * params) {
    PAGL_GetShaderiv(shader, pname, params);
}
const GLubyte * glGetString(GLenum name) {
    return PAGL_GetString(name);
}
const GLubyte * glGetStringi(GLenum name, GLuint index) {
    return PAGL_GetStringi(name, index);
}
void glGetSynciv(GLsync sync, GLenum pname, GLsizei count, GLsizei * length, GLint * values) {
    PAGL_GetSynciv(sync, pname, count, length, values);
}
void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat * params) {
    PAGL_GetTexParameterfv(target, pname, params);
}
void glGetTexParameteriv(GLenum target, GLenum pname, GLint * params) {
    PAGL_GetTexParameteriv(target, pname, params);
}
void glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLsizei * size, GLenum * type, GLchar * name) {
    PAGL_GetTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
}
GLuint glGetUniformBlockIndex(GLuint program, const GLchar * uniformBlockName) {
    return PAGL_GetUniformBlockIndex(program, uniformBlockName);
}
void GetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const* uniformNames, GLuint * uniformIndices)
{
    PAGL_GetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
}
GLint glGetUniformLocation(GLuint program, const GLchar * name) {
    return PAGL_GetUniformLocation(program, name);
}
void glGetUniformfv(GLuint program, GLint location, GLfloat * params) {
    PAGL_GetUniformfv(program, location, params);
}
void glGetUniformiv(GLuint program, GLint location, GLint * params) {
    PAGL_GetUniformiv(program, location, params);
}
void glGetUniformuiv(GLuint program, GLint location, GLuint * params) {
    PAGL_GetUniformuiv(program, location, params);
}
void glGetVertexAttribIiv(GLuint index, GLenum pname, GLint * params) {
    PAGL_GetVertexAttribIiv(index, pname, params);
}
void glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint * params) {
    PAGL_GetVertexAttribIuiv(index, pname, params);
}
void glGetVertexAttribPointerv(GLuint index, GLenum pname, void ** pointer) {
    PAGL_GetVertexAttribPointerv(index, pname, pointer);
}
void glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat * params) {
    PAGL_GetVertexAttribfv(index, pname, params);
}
void glGetVertexAttribiv(GLuint index, GLenum pname, GLint * params) {
    PAGL_GetVertexAttribiv(index, pname, params);
}
void glHint(GLenum target, GLenum mode) {
    PAGL_Hint(target, mode);
}
void glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum * attachments) {
    PAGL_InvalidateFramebuffer(target, numAttachments, attachments);
}
void glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum * attachments, GLint x, GLint y, GLsizei width, GLsizei height) {
    PAGL_InvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
}
GLboolean glIsBuffer(GLuint buffer) {
    return PAGL_IsBuffer(buffer);
}
GLboolean glIsEnabled(GLenum cap) {
    return PAGL_IsEnabled(cap);
}
GLboolean glIsFramebuffer(GLuint framebuffer) {
    return PAGL_IsFramebuffer(framebuffer);
}
GLboolean glIsProgram(GLuint program) {
    return PAGL_IsProgram(program);
}
GLboolean glIsQuery(GLuint id) {
    return PAGL_IsQuery(id);
}
GLboolean glIsRenderbuffer(GLuint renderbuffer) {
    return PAGL_IsRenderbuffer(renderbuffer);
}
GLboolean glIsSampler(GLuint sampler) {
    return PAGL_IsSampler(sampler);
}
GLboolean glIsShader(GLuint shader) {
    return PAGL_IsShader(shader);
}
GLboolean glIsSync(GLsync sync) {
    return PAGL_IsSync(sync);
}
GLboolean glIsTexture(GLuint texture) {
    return PAGL_IsTexture(texture);
}
GLboolean glIsTransformFeedback(GLuint id) {
    return PAGL_IsTransformFeedback(id);
}
GLboolean glIsVertexArray(GLuint array) {
    return PAGL_IsVertexArray(array);
}
void glLineWidth(GLfloat width) {
    PAGL_LineWidth(width);
}
void glLinkProgram(GLuint program) {
    PAGL_LinkProgram(program);
}
void * glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
return PAGL_MapBufferRange(target, offset, length, access);
}
void glObjectLabelKHR(GLenum identifier, GLuint name, GLsizei length, const GLchar * label) {
    PAGL_ObjectLabelKHR(identifier, name, length, label);
}
void glObjectPtrLabelKHR(const void * ptr, GLsizei length, const GLchar * label) {
    PAGL_ObjectPtrLabelKHR(ptr, length, label);
}
void glPauseTransformFeedback(void) {
    PAGL_PauseTransformFeedback();
}
void glPixelStorei(GLenum pname, GLint param) {
    PAGL_PixelStorei(pname, param);
}
void glPolygonOffset(GLfloat factor, GLfloat units) {
    PAGL_PolygonOffset(factor, units);
}
void glPopDebugGroupKHR(void) {
    PAGL_PopDebugGroupKHR();
}
void glProgramBinary(GLuint program, GLenum binaryFormat, const void * binary, GLsizei length) {
    PAGL_ProgramBinary(program, binaryFormat, binary, length);
}
void glProgramParameteri(GLuint program, GLenum pname, GLint value) {
    PAGL_ProgramParameteri(program, pname, value);
}
void glPushDebugGroupKHR(GLenum source, GLuint id, GLsizei length, const GLchar * message) {
    PAGL_PushDebugGroupKHR(source, id, length, message);
}
void glReadBuffer(GLenum src) {
    PAGL_ReadBuffer(src);
}
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void * pixels) {
    PAGL_ReadPixels(x, y, width, height, format, type, pixels);
}
void glReleaseShaderCompiler(void) {
    PAGL_ReleaseShaderCompiler();
}
void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    PAGL_RenderbufferStorage(target, internalformat, width, height);
}
void glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    PAGL_RenderbufferStorageMultisample(target, samples, internalformat, width, height);
}
void glResumeTransformFeedback(void) {
    PAGL_ResumeTransformFeedback();
}
void glSampleCoverage(GLfloat value, GLboolean invert) {
    PAGL_SampleCoverage(value, invert);
}
void glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
    PAGL_SamplerParameterf(sampler, pname, param);
}
void glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat * param) {
    PAGL_SamplerParameterfv(sampler, pname, param);
}
void glSamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
    PAGL_SamplerParameteri(sampler, pname, param);
}
void glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint * param) {
    PAGL_SamplerParameteriv(sampler, pname, param);
}
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    PAGL_Scissor(x, y, width, height);
}
void glShaderBinary(GLsizei count, const GLuint * shaders, GLenum binaryFormat, const void * binary, GLsizei length) {
    PAGL_ShaderBinary(count, shaders, binaryFormat, binary, length);
}
void glShaderSource(GLuint shader, GLsizei count, const GLchar *const* string, const GLint * length) {
    PAGL_ShaderSource(shader, count, string, length);
}
void glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    PAGL_StencilFunc(func, ref, mask);
}
void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    PAGL_StencilFuncSeparate(face, func, ref, mask);
}
void glStencilMask(GLuint mask) {
    PAGL_StencilMask(mask);
}
void glStencilMaskSeparate(GLenum face, GLuint mask) {
    PAGL_StencilMaskSeparate(face, mask);
}
void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    PAGL_StencilOp(fail, zfail, zpass);
}
void glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    PAGL_StencilOpSeparate(face, sfail, dpfail, dppass);
}
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void * pixels) {
    PAGL_TexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}
void glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void * pixels) {
    PAGL_TexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
}
void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    PAGL_TexParameterf(target, pname, param);
}
void glTexParameterfv(GLenum target, GLenum pname, const GLfloat * params) {
    PAGL_TexParameterfv(target, pname, params);
}
void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    PAGL_TexParameteri(target, pname, param);
}
void glTexParameteriv(GLenum target, GLenum pname, const GLint * params) {
    PAGL_TexParameteriv(target, pname, params);
}
void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    PAGL_TexStorage2D(target, levels, internalformat, width, height);
}
void glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    PAGL_TexStorage3D(target, levels, internalformat, width, height, depth);
}
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void * pixels) {
    PAGL_TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}
void glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void * pixels) {
    PAGL_TexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}
void glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const* varyings, GLenum bufferMode) {
    PAGL_TransformFeedbackVaryings(program, count, varyings, bufferMode);
}
void glUniform1f(GLint location, GLfloat v0) {
    PAGL_Uniform1f(location, v0);
}
void glUniform1fv(GLint location, GLsizei count, const GLfloat * value) {
    PAGL_Uniform1fv(location, count, value);
}
void glUniform1i(GLint location, GLint v0) {
    PAGL_Uniform1i(location, v0);
}
void glUniform1iv(GLint location, GLsizei count, const GLint * value) {
    PAGL_Uniform1iv(location, count, value);
}
void glUniform1ui(GLint location, GLuint v0) {
    PAGL_Uniform1ui(location, v0);
}
void glUniform1uiv(GLint location, GLsizei count, const GLuint * value) {
    PAGL_Uniform1uiv(location, count, value);
}
void glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    PAGL_Uniform2f(location, v0, v1);
}
void glUniform2fv(GLint location, GLsizei count, const GLfloat * value) {
    PAGL_Uniform2fv(location, count, value);
}
void glUniform2i(GLint location, GLint v0, GLint v1) {
    PAGL_Uniform2i(location, v0, v1);
}
void glUniform2iv(GLint location, GLsizei count, const GLint * value) {
    PAGL_Uniform2iv(location, count, value);
}
void glUniform2ui(GLint location, GLuint v0, GLuint v1) {
    PAGL_Uniform2ui(location, v0, v1);
}
void glUniform2uiv(GLint location, GLsizei count, const GLuint * value) {
    PAGL_Uniform2uiv(location, count, value);
}
void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    PAGL_Uniform3f(location, v0, v1, v2);
}
void glUniform3fv(GLint location, GLsizei count, const GLfloat * value) {
    PAGL_Uniform3fv(location, count, value);
}
void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    PAGL_Uniform3i(location, v0, v1, v2);
}
void glUniform3iv(GLint location, GLsizei count, const GLint * value) {
    PAGL_Uniform3iv(location, count, value);
}
void glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    PAGL_Uniform3ui(location, v0, v1, v2);
}
void glUniform3uiv(GLint location, GLsizei count, const GLuint * value) {
    PAGL_Uniform3uiv(location, count, value);
}
void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    PAGL_Uniform4f(location, v0, v1, v2, v3);
}
void glUniform4fv(GLint location, GLsizei count, const GLfloat * value) {
    PAGL_Uniform4fv(location, count, value);
}
void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    PAGL_Uniform4i(location, v0, v1, v2, v3);
}
void glUniform4iv(GLint location, GLsizei count, const GLint * value) {
    PAGL_Uniform4iv(location, count, value);
}
void glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    PAGL_Uniform4ui(location, v0, v1, v2, v3);
}
void glUniform4uiv(GLint location, GLsizei count, const GLuint * value) {
    PAGL_Uniform4uiv(location, count, value);
}
void glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
    PAGL_UniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}
void glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    PAGL_UniformMatrix2fv(location, count, transpose, value);
}
void glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    PAGL_UniformMatrix2x3fv(location, count, transpose, value);
}
void glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    PAGL_UniformMatrix2x4fv(location, count, transpose, value);
}
void glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    PAGL_UniformMatrix3fv(location, count, transpose, value);
}
void glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    PAGL_UniformMatrix3x2fv(location, count, transpose, value);
}
void glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    PAGL_UniformMatrix3x4fv(location, count, transpose, value);
}
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    PAGL_UniformMatrix4fv(location, count, transpose, value);
}
void glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    PAGL_UniformMatrix4x2fv(location, count, transpose, value);
}
void glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    PAGL_UniformMatrix4x3fv(location, count, transpose, value);
}
GLboolean glUnmapBuffer(GLenum target) {
    return PAGL_UnmapBuffer(target);
}
void glUseProgram(GLuint program) {
    PAGL_UseProgram(program);
}
void glValidateProgram(GLuint program) {
    PAGL_ValidateProgram(program);
}
void glVertexAttrib1f(GLuint index, GLfloat x) {
    PAGL_VertexAttrib1f(index, x);
}
void glVertexAttrib1fv(GLuint index, const GLfloat * v) {
    PAGL_VertexAttrib1fv(index, v);
}
void glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
    PAGL_VertexAttrib2f(index, x, y);
}
void glVertexAttrib2fv(GLuint index, const GLfloat * v) {
    PAGL_VertexAttrib2fv(index, v);
}
void glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    PAGL_VertexAttrib3f(index, x, y, z);
}
void glVertexAttrib3fv(GLuint index, const GLfloat * v) {
    PAGL_VertexAttrib3fv(index, v);
}
void glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    PAGL_VertexAttrib4f(index, x, y, z, w);
}
void glVertexAttrib4fv(GLuint index, const GLfloat * v) {
    PAGL_VertexAttrib4fv(index, v);
}
void glVertexAttribDivisor(GLuint index, GLuint divisor) {
    PAGL_VertexAttribDivisor(index, divisor);
}
void glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    PAGL_VertexAttribI4i(index, x, y, z, w);
}
void glVertexAttribI4iv(GLuint index, const GLint * v) {
    PAGL_VertexAttribI4iv(index, v);
}
void glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    PAGL_VertexAttribI4ui(index, x, y, z, w);
}
void glVertexAttribI4uiv(GLuint index, const GLuint * v) {
    PAGL_VertexAttribI4uiv(index, v);
}
void glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void * pointer) {
    PAGL_VertexAttribIPointer(index, size, type, stride, pointer);
}
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void * pointer) {
    PAGL_VertexAttribPointer(index, size, type, normalized, stride, pointer);
}
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    PAGL_Viewport(x, y, width, height);
}


void glDebugMessageCallback(GLDEBUGPROC callback, const void * userParam);
void glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint * ids, GLboolean enabled);
void glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * buf);

#endif

#if GL_DEBUG
void glPolygonMode(GLenum face, GLenum mode){
    if(PAGL_PolygonMode == NULL)
        return ;
    PAGL_PolygonMode(face, mode);
}

#endif

