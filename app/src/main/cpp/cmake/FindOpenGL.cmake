find_library(OPENGL_EGL_LIBRARY EGL)
find_library(OPENGL_GLES3_LIBRARY GLESv3)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenGL
    REQUIRED_VARS OPENGL_EGL_LIBRARY OPENGL_GLES3_LIBRARY
)

if(OpenGL_FOUND AND NOT TARGET OpenGL::EGL)
    add_library(OpenGL::EGL INTERFACE IMPORTED)
    set_property(TARGET OpenGL::EGL PROPERTY INTERFACE_LINK_LIBRARIES
        "${OPENGL_EGL_LIBRARY};${OPENGL_GLES3_LIBRARY}"
    )
endif()

if(OpenGL_FOUND AND NOT TARGET OpenGL::GL)
    add_library(OpenGL::GL INTERFACE IMPORTED)
    set_property(TARGET OpenGL::GL PROPERTY INTERFACE_LINK_LIBRARIES
        "${OPENGL_GLES3_LIBRARY}"
    )
endif()
