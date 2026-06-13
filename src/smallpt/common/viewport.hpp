// -*- C++ -*-
/// @file viewport.hpp
/// @brief OpenGL viewport for images.

#pragma once

#include <common/forbid.hpp>

#include <GLFW/glfw3.h>

#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace loom::gl
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

/// @brief Shader wrapper.
class Shader
{
    GLuint m_id;

  public:
    FORBID_DEFAULT(Shader);

    Shader(const GLchar* src, GLenum shader_type)
    {
        m_id = glCreateShader(shader_type);
        glShaderSource(m_id, 1, &src, nullptr);
        glCompileShader(m_id);
        check_shader_status(m_id, GL_COMPILE_STATUS);
    }

    GLuint
    id() const noexcept
    {
        return m_id;
    }

    ~Shader() { glDeleteShader(m_id); }
};

/// @brief Shader program wrapper.
class Program
{
    GLuint m_id;

  public:
    FORBID_DEFAULT(Program);

    template <std::same_as<Shader>... Shaders>
    Program(Shaders&&... shaders)
    {
        m_id = glCreateProgram();

        (..., glAttachShader(m_id, shaders.id()));
        glLinkProgram(m_id);
        check_program_status(m_id, GL_LINK_STATUS);
    }

    GLuint
    id() const noexcept
    {
        return m_id;
    }

    void
    use() const noexcept
    {
        glUseProgram(m_id);
    }

    ~Program() { glDeleteProgram(m_id); }
};

/// @brief Responsible for GLFW initialization and termination.
class System
{
    struct Handle
    {
        FORBID_COPY(Handle);
        FORBID_MOVE(Handle);

        const char*
        get_string(GLenum name) const noexcept
        {
            return reinterpret_cast<const char*>(glGetString(name));
        }

        void
        poll() const noexcept
        {
            glfwPollEvents();
        }

        ~Handle() { glfwTerminate(); }

      private:
        Handle() noexcept {}

        friend System;
    };

    static inline bool   initialized = false;
    static inline Handle handle;

  public:
    FORBID_ALL(System);

    using Handle = Handle;

    [[nodiscard]] static const Handle&
    init()
    {
        if(!initialized)
            if(!(initialized = glfwInit()))
                throw std::runtime_error("Failed to initialize GLFW");

        return handle;
    }
};

/// @brief GLFW and OpenGL window initialization.
template <typename ResizeReaction, typename KeyReaction>
class Window
{
    static inline GLFWwindow* window = nullptr;

    static inline ResizeReaction resize_reaction;
    static inline KeyReaction    key_reaction;

    static inline int resx;
    static inline int resy;

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
    init(int            _resx,
         int            _resy,
         ResizeReaction _resize_reaction,
         KeyReaction    _key_reaction,
         const char*    title = "Loom")
    {
        resx = _resx;
        resy = _resy;

        window = glfwCreateWindow(resx, resy, title, nullptr, nullptr);

        if(!window)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create window");
        }

        resize_reaction = _resize_reaction;
        key_reaction    = _key_reaction;

        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, resize_callback);
        glfwSetKeyCallback(window, key_callback);

        glfwSetWindowSizeLimits(window,
                                640,
                                480,
                                GLFW_DONT_CARE,
                                GLFW_DONT_CARE);
        gl_check_errors();

        glfwGetFramebufferSize(window, &resx, &resy);
        glViewport(0, 0, resx, resy);
    }

    static std::pair<int, int>
    get_res() noexcept
    {
        return {resx, resy};
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
    cleanup() noexcept
    {
        if(window) glfwDestroyWindow(window);
    }

    static void
    swap_buffers() noexcept
    {
        glfwSwapBuffers(window);
    }

    /// @brief Object interface to viewport.
    struct Handle
    {
        FORBID_ALL(Handle);

        Handle(int            resx,
               int            resy,
               ResizeReaction ra,
               KeyReaction    ka,
               const char*    title)
        {
            init(resx, resy, ra, ka, title);
        }
        ~Handle() { cleanup(); }

        /// @brief Viewport resolution.
        std::pair<int, int>
        get_res() const noexcept
        {
            return Window::get_res();
        }

        bool
        active() const noexcept
        {
            return Window::active();
        }

        void
        close() const noexcept
        {
            Window::close();
        }

        void
        swap_buffers() const noexcept
        {
            Window::swap_buffers();
        }
    };

  public:
    FORBID_ALL(Window);

    template <typename RR, typename KR>
    friend Window<RR, KR>::Handle
    create_window(const System::Handle&,
                  int _resx,
                  int _resy,
                  RR  rr,
                  KR  kr,
                  const char*,
                  const char*,
                  const char*);
};

/// @brief Factory function for creating OpenGL windows.
template <typename ResizeReaction, typename KeyReaction>
Window<ResizeReaction, KeyReaction>::Handle
create_window(const System::Handle&,
              int            _resx,
              int            _resy,
              ResizeReaction rr,
              KeyReaction    kr,
              const char*    title,
              const char*    x11_class_name    = "Loom",
              const char*    x11_instance_name = "Viewport")
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHintString(GLFW_X11_CLASS_NAME, x11_class_name);
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, x11_instance_name);

    return {_resx, _resy, rr, kr, title};
}

/// @brief Simple OpengGL viewport for displaying image.
class ImageViewport
{
    static constexpr GLchar vertex_shader_src[] =
        "#version 460 core\n"
        "layout (location = 0) in vec3 inP;"
        "layout (location = 1) in vec2 inUV;"
        "out vec2 UV;"
        "void main(){"
        "gl_Position = vec4(inP, 1.0);"
        "UV = inUV;}";

    static constexpr GLchar fragment_shader_src[] =
        "#version 460 core\n"
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

    Program m_program;
    GLuint  m_vbo;
    GLuint  m_vao;
    GLuint  m_ebo;
    GLuint  m_textrue;

  public:
    ImageViewport() :
        m_program(Shader(vertex_shader_src, GL_VERTEX_SHADER),
                  Shader(fragment_shader_src, GL_FRAGMENT_SHADER))
    {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(vertex_data),
                     vertex_data,
                     GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
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
        glGenTextures(1, &m_textrue);
        // assume no other textures - bind the texture once
        glBindTexture(GL_TEXTURE_2D, m_textrue);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    template <typename T>
    void
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
    void
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

    void
    draw() noexcept
    {
        m_program.use();
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    ~ImageViewport()
    {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_vbo);
        glDeleteBuffers(1, &m_ebo);
    }
};
} // namespace loom::gl
