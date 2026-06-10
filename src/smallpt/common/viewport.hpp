// -*- C++ -*-
/// @file viewport.hpp
/// @brief OpenGL viewport for images.

#pragma once

#include <common/forbid.hpp>

#include <GLFW/glfw3.h>

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace loom
{
/// @brief Exception replacing OpenGL error codes.
struct GLException
{
    GLenum c;

    const char*
    what() const noexcept
    {
        switch(c)
        {
        case(GL_NO_ERROR): return "no error";
        case(GL_INVALID_ENUM): return "invalid enum";
        case(GL_INVALID_VALUE): return "invalid value";
        case(GL_INVALID_OPERATION): return "invalid operation";
        case(GL_INVALID_FRAMEBUFFER_OPERATION):
            return "invalid framebuffer operation";
        case(GL_OUT_OF_MEMORY): return "out of memory";
        case(GL_STACK_UNDERFLOW): return "stack underflow";
        case(GL_STACK_OVERFLOW): return "stack overflow";
        }
        return "";
    }
};

/// @brief Check for OpenGL errors.
/// @throw GLException if got an error.
static void
gl_check_errors()
{
    const GLenum glerror = glGetError();
    if(glerror) { throw GLException{glerror}; }
}

#define CHECK_STATUS(CALL, ID, PNAME)                   \
    int status;                                         \
    CALL(ID, PNAME, &status);                           \
    if(!status)                                         \
    {                                                   \
        char info_log[512];                             \
        glGetShaderInfoLog(ID, 512, nullptr, info_log); \
        glfwTerminate();                                \
        throw std::runtime_error(info_log);             \
    }

/// @brief Check shader compilation status.
/// @throw std::runtime_error if compilation fails.
static void
check_shader_status(GLuint shader, GLenum pname = GL_COMPILE_STATUS)
{
    CHECK_STATUS(glGetShaderiv, shader, pname)
}

/// @brief Check program compilation status.
/// @throw std::runtime_error if linkage fails.
static void
check_program_status(GLuint program, GLenum pname = GL_LINK_STATUS)
{
    CHECK_STATUS(glGetProgramiv, program, pname)
}

#undef CHECK_STATUS

/// @brief Create and compile shader.
/// @throw std::runtime_error if shader compilation fails.
static GLuint
make_shader(const GLchar* src, GLenum shader_type)
{
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    check_shader_status(shader, GL_COMPILE_STATUS);
    return shader;
}

/// @brief Simple OpengGL 3.3 viewport for an image.
template <typename ResizeReaction, typename KeyReaction, typename MouseReaction>
class ImageViewport
{
    static inline int gl_initialized = false;

    static constexpr GLchar vertex_shader_src[] =
        "#version 330 core\n"
        "layout (location = 0) in vec3 inP;"
        "layout (location = 1) in vec2 inUV;"
        "out vec2 UV;"
        "void main(){"
        "gl_Position = vec4(inP, 1.0);"
        "UV = inUV;}";

    static constexpr GLchar fragment_shader_src[] =
        "#version 330 core\n"
        "out vec4 Cf;"
        "in vec2 UV;"
        "uniform sampler2D img;"
        "void main(){Cf = texture(img, UV);}";

    static constexpr GLfloat vertex_data[] = {
        // positions    // texture coordinates
        -1.f, 1.f,  0, 0,   0,   1.f,  1.f,  0, 1.f, 0,
        1.f,  -1.f, 0, 1.f, 1.f, -1.f, -1.f, 0, 0,   1.f};

    static constexpr GLuint indices[] = {
        0,
        1,
        2, // first triangle
        2,
        3,
        0 // second triangle
    };

    static inline GLFWwindow* window         = nullptr;
    static inline GLuint      shader_program = -1u;
    static inline GLuint      VBO            = -1u;
    static inline GLuint      VAO            = -1u;
    static inline GLuint      EBO            = -1u;
    static inline GLuint      texture        = -1u;
    static inline int         resx           = 640;
    static inline int         resy           = 480;

    static inline ResizeReaction resize_reaction;
    static inline KeyReaction    key_reaction;
    static inline MouseReaction  mouse_reaction;

    static void
    resize_callback(GLFWwindow* window, int width, int height) noexcept
    {
        resx = width;
        resy = height;
        glViewport(0, 0, width, height);
        resize_reaction(window, width, height);
    }

    static void
    key_callback(GLFWwindow* window,
                 int         key,
                 int /*scancode*/,
                 int action,
                 int mods) noexcept
    {
        key_reaction(window, key, action, mods);
    }

    static void
    mouse_button_callback(GLFWwindow* window,
                          int         button,
                          int         action,
                          int         mods) noexcept
    {
        mouse_reaction(window, button, action, mods);
    }

    static void
    cursor_position_callback(GLFWwindow* window,
                             double      xpos,
                             double      ypos) noexcept
    {
        mouse_reaction(window, xpos, ypos);
    }

    static void
    init(int            _resx,
         int            _resy,
         ResizeReaction _resize_reaction,
         KeyReaction    _key_reaction,
         MouseReaction  _mouse_reaction)
    {
        resx = _resx;
        resy = _resy;

        window = glfwCreateWindow(resx, resy, "Loom", nullptr, nullptr);

        if(!window)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create window");
        }

        resize_reaction = _resize_reaction;
        mouse_reaction  = _mouse_reaction;
        key_reaction    = _key_reaction;

        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, resize_callback);
        glfwSetKeyCallback(window, key_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetCursorPosCallback(window, cursor_position_callback);

        glfwSetWindowSizeLimits(window,
                                640,
                                480,
                                GLFW_DONT_CARE,
                                GLFW_DONT_CARE);
        gl_check_errors();

        glfwGetFramebufferSize(window, &resx, &resy);
        glViewport(0, 0, resx, resy);

        GLuint vertex_shader = make_shader(vertex_shader_src, GL_VERTEX_SHADER);
        GLuint fragment_shader =
            make_shader(fragment_shader_src, GL_FRAGMENT_SHADER);

        shader_program = glCreateProgram();
        glAttachShader(shader_program, vertex_shader);
        glAttachShader(shader_program, fragment_shader);
        glLinkProgram(shader_program);
        check_program_status(shader_program, GL_LINK_STATUS);

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(vertex_data),
                     vertex_data,
                     GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     sizeof(indices),
                     indices,
                     GL_STATIC_DRAW);

        // position
        glVertexAttribPointer(0,
                              3,
                              GL_FLOAT,
                              GL_FALSE,
                              5 * sizeof(float),
                              static_cast<void*>(0));
        glEnableVertexAttribArray(0);

        // uv
        glVertexAttribPointer(1,
                              2,
                              GL_FLOAT,
                              GL_FALSE,
                              5 * sizeof(float),
                              reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // texture
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glUseProgram(shader_program);
    }

    static std::pair<int, int>
    get_res() noexcept
    {
        return {resx, resy};
    }

    template <typename T>
    static void
    load_img(const T* img_data, int width, int height)
    {
        constexpr auto type_flag =
            std::is_same_v<T, unsigned char> ?
                GL_UNSIGNED_BYTE :
                (std::is_same_v<T, float> ? GL_FLOAT : GL_UNSIGNED_INT);

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA,
                     width,
                     height,
                     0,
                     GL_RGBA,
                     type_flag,
                     img_data);

        gl_check_errors();
    }

    template <typename T>
    static void
    reload_img(const T* img_data, int width, int height)
    {
        constexpr auto type_flag =
            std::is_same_v<T, unsigned char> ?
                GL_UNSIGNED_BYTE :
                (std::is_same_v<T, float> ? GL_FLOAT : GL_UNSIGNED_INT);

        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        width,
                        height,
                        GL_RGBA,
                        type_flag,
                        img_data);

        gl_check_errors();
    }

    static bool
    active() noexcept
    {
        return !glfwWindowShouldClose(window);
    }

    static void
    close() noexcept
    {
        glfwSetWindowShouldClose(window, true);
    }

    static void
    draw() noexcept
    {
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
    }

    static void
    cleanup() noexcept
    {
        if(window) glfwDestroyWindow(window);

        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);

        glfwTerminate();
    }

  public:
    FORBID_ALL(ImageViewport);

    /// @brief Object interface to viewport.
    struct Handler
    {
        FORBID_ALL(Handler);

        Handler(int            resx,
                int            resy,
                ResizeReaction ra,
                KeyReaction    ka,
                MouseReaction  ma)
        {
            ImageViewport::init(resx, resy, ra, ka, ma);
        }
        ~Handler() { ImageViewport::cleanup(); }

        /// @brief Viewport resolution.
        std::pair<int, int>
        get_res() const noexcept
        {
            return ImageViewport::get_res();
        }

        template <typename T>
        void
        load_img(const T* img_data, int resx, int resy) const
        {
            ImageViewport::load_img(img_data, resx, resy);
        }

        template <typename T>
        void
        reload_img(const T* img_data, int resx, int resy) const
        {
            ImageViewport::reload_img(img_data, resx, resy);
        }

        bool
        active() const noexcept
        {
            return ImageViewport::active();
        }

        void
        draw() const noexcept
        {
            ImageViewport::draw();
        }

        void
        close() const noexcept
        {
            ImageViewport::close();
        }
    };

    template <typename RR, typename KR, typename MR>
    friend ImageViewport<RR, KR, MR>::Handler
    make_viewport(int _resx,
                  int _resy,
                  RR  rr,
                  KR  kr,
                  MR  mr,
                  const char*,
                  const char*);
};

/// @brief Factory function.
template <typename ResizeReaction, typename KeyReaction, typename MouseReaction>
ImageViewport<ResizeReaction, KeyReaction, MouseReaction>::Handler
make_viewport(int            _resx,
              int            _resy,
              ResizeReaction rr,
              KeyReaction    kr,
              MouseReaction  mr,
              const char*    x11_class_name    = "Loom",
              const char*    x11_instance_name = "Viewport")
{
    if(!ImageViewport<ResizeReaction, KeyReaction, MouseReaction>::
           gl_initialized)
    {
        ImageViewport<ResizeReaction, KeyReaction, MouseReaction>::
            gl_initialized = glfwInit();
        if(!ImageViewport<ResizeReaction, KeyReaction, MouseReaction>::
               gl_initialized)
            throw std::runtime_error("Failed to initialize GLFW");

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHintString(GLFW_X11_CLASS_NAME, x11_class_name);
        glfwWindowHintString(GLFW_X11_INSTANCE_NAME, x11_instance_name);
    }

    return typename ImageViewport<ResizeReaction, KeyReaction, MouseReaction>::
        Handler(_resx, _resy, rr, kr, mr);
}
} // namespace loom
