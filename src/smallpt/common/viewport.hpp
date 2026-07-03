// -*- C++ -*-
/// @file viewport.hpp
/// @brief OpenGL viewport for images.

#pragma once

#include <common/forbid.hpp>
#include <common/linal.hpp>

#include <GLFW/glfw3.h>

#include <utility>

namespace loom::gl
{
/// @brief Exception replacing OpenGL error codes.
struct GLException
{
    GLenum c;

    const char*
    what() const noexcept;
};

void
gl_check_errors();

/// @brief Check shader compilation status.
/// @throw std::runtime_error if compilation fails.
void
check_shader_status(GLuint shader, GLenum pname = GL_COMPILE_STATUS);

/// @brief Check program compilation status.
/// @throw std::runtime_error if linkage fails.
void
check_program_status(GLuint program, GLenum pname = GL_LINK_STATUS);

/// @brief Shader wrapper.
class Shader
{
    GLuint m_id;

  public:
    FORBID_DEFAULT(Shader);

    Shader(const GLchar* src, GLenum shader_type);

    GLuint
    id() const noexcept
    {
        return m_id;
    }

    ~Shader();
};

/// @brief Shader program wrapper.
class Program
{
    GLuint m_id;

  public:
    FORBID_DEFAULT(Program);

    Program(Shader a, Shader b);

    GLuint
    id() const noexcept
    {
        return m_id;
    }

    void
    use() const noexcept;

    ~Program();
};

/// @brief Responsible for GLFW initialization and termination.
class System
{
    struct Handle
    {
        FORBID_COPY(Handle);
        FORBID_MOVE(Handle);

        const char*
        get_string(GLenum name) const noexcept;

        void
        poll() const noexcept;

        ~Handle();

      private:
        Handle() noexcept {}

        friend System;
    };

    static void
    error_callback(int /* error */, const char* desc);

    static inline bool   initialized = false;
    static inline Handle handle;

  public:
    FORBID_ALL(System);

    using Handle = Handle;

    [[nodiscard]] static const Handle&
    init();
};

void
post_size(int width, int height) noexcept;

/// @brief GLFW and OpenGL window initialization.
struct Window
{
    GLFWwindow* window = nullptr;

    FORBID_COPY(Window);

    Window(int                    _resx,
           int                    _resy,
           GLFWframebuffersizefun resize_callback,
           GLFWkeyfun             key_callback,
           const char*            title = "Loom");

    std::pair<int, int>
    get_res() noexcept;

    bool
    active() noexcept;

    void
    close() noexcept;

    void
    swap_buffers() noexcept;

    ~Window();
};

/// @brief Simple OpengGL viewport for displaying image.
class ImageViewport
{
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

    Program m_program;
    GLuint  m_vbo;
    GLuint  m_vao;
    GLuint  m_ebo;
    GLuint  m_texture;

  public:
    ImageViewport();

    void
    load_img(const float* img_data, int width, int height);

    void
    reload_img(const float* img_data, int width, int height);

    void
    draw() noexcept;

    ~ImageViewport();
};

/// @brief Simple on-screen-display.
class OSD
{
    static constexpr char vertex_shader_src[] =
        "#version 330 core\n"
        "layout (location = 0) in vec2 pos;"
        "uniform mat4 projection;"
        "void main(){"
        "gl_Position = projection * vec4(pos, 0.0, 1.0);}";
    static constexpr char fragment_shader_src[] =
        "#version 330 core\n"
        "out vec4 Cf;"
        "void main(){"
        "Cf = vec4(1.0, 1.0, 1.0, 1.0);}";

    char m_buffer[20000];
    int  m_quads = 0;

    GLuint m_vao;
    GLuint m_vbo;
    GLuint m_ebo;

    Program m_program;

    GLint m_proj_loc;

    void
    upload_projection(const Matrix<float, 4, 4, std::array>& proj) noexcept;

    void
    update_vbo() noexcept;

    void
    update_ebo() noexcept;

  public:
    FORBID_DEFAULT(OSD);
    FORBID_COPY(OSD);

    OSD(const float x, const float y, const char* text, int width, int height);

    void
    resize(int width, int height) noexcept;

    void
    draw() noexcept;

    ~OSD();
};
} // namespace loom::gl
