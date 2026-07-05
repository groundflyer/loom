// -*- C++ -*-
// viewport.cpp

#include "viewport.hpp"

#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <stb_easy_font.h>

namespace loom::gl
{
const char*
GLException::what() const noexcept
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

void
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

void
check_shader_status(GLuint shader, GLenum pname)
{
    CHECK_STATUS(glGetShaderiv, shader, pname)
}

/// @brief Check program compilation status.
/// @throw std::runtime_error if linkage fails.
void
check_program_status(GLuint program,
                     GLenum pname){CHECK_STATUS(glGetProgramiv, program, pname)}

#undef CHECK_STATUS

Shader::Shader(const GLchar* src, GLenum shader_type) :
    m_id(glCreateShader(shader_type))
{
    glShaderSource(m_id, 1, &src, nullptr);
    glCompileShader(m_id);
    check_shader_status(m_id, GL_COMPILE_STATUS);
}

Shader::~Shader() { glDeleteShader(m_id); }

Program::Program(Shader a, Shader b) : m_id(glCreateProgram())
{
    glAttachShader(m_id, a.id());
    glAttachShader(m_id, b.id());
    glLinkProgram(m_id);
    check_program_status(m_id, GL_LINK_STATUS);
}

void
Program::use() const noexcept
{
    glUseProgram(m_id);
}

Program::~Program() { glDeleteProgram(m_id); }

const char*
System::Handle::get_string(GLenum name) const noexcept
{
    return reinterpret_cast<const char*>(glGetString(name));
}

void
System::Handle::poll() const noexcept
{
    glfwPollEvents();
}

System::Handle::~Handle() { glfwTerminate(); }

void
System::error_callback(int /* error */, const char* desc)
{
    throw std::runtime_error(desc);
}

const System::Handle&
System::init()
{
    glfwSetErrorCallback(error_callback);

    if(!initialized)
    {
        initialized = glfwInit();
        if(!initialized) throw std::runtime_error("Failed to initialize GLFW");
    }

    return handle;
}

void
post_size(int width, int height) noexcept
{
    glViewport(0, 0, width, height);
}

Window::Window(int                    resx,
               int                    resy,
               GLFWframebuffersizefun resize_callback,
               GLFWkeyfun             key_callback,
               const char*            title)
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "Loom");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, title);

    window = glfwCreateWindow(resx, resy, title, nullptr, nullptr);

    if(!window) throw std::runtime_error("Failed to create window");

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, resize_callback);
    glfwSetKeyCallback(window, key_callback);

    glfwSetWindowSizeLimits(window, 640, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
    gl_check_errors();

    glfwGetFramebufferSize(window, &resx, &resy);
    glViewport(0, 0, resx, resy);
}

std::pair<int, int>
Window::get_res() noexcept
{
    int resx, resy;
    glfwGetFramebufferSize(window, &resx, &resy);
    return {resx, resy};
}

bool
Window::active() noexcept
{
    return !glfwWindowShouldClose(window);
}

void
Window::close() noexcept
{
    glfwSetWindowShouldClose(window, true);
}

void
Window::swap_buffers() noexcept
{
    glfwSwapBuffers(window);
}

Window::~Window()
{
    if(window) glfwDestroyWindow(window);
}

ImageViewport::ImageViewport() :
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
    glGenTextures(1, &m_texture);
    // assume no other textures - bind the texture once
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void
ImageViewport::load_img(const float* img_data, int width, int height)
{
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 width,
                 height,
                 0,
                 GL_RGBA,
                 GL_FLOAT,
                 img_data);

    gl_check_errors();
}

void
ImageViewport::reload_img(const float* img_data, int width, int height)
{
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    width,
                    height,
                    GL_RGBA,
                    GL_FLOAT,
                    img_data);

    gl_check_errors();
}

void
ImageViewport::draw() noexcept
{
    m_program.use();
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

ImageViewport::~ImageViewport()
{
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteBuffers(1, &m_ebo);
    glDeleteTextures(1, &m_texture);
}

/// @brief Create a matrix for projecting two-dimensional coordinates onto
/// screen.
template <std::floating_point T>
static Matrix<T, 4, 4, std::array>
ortho(T left, T right, T bottom, T top) noexcept
{
    Matrix<T, 4, 4, std::array> ret = Matrix<T, 4, 4, std::array>::identity();

    ret[0][0] = static_cast<T>(2) / (right - left);
    ret[1][1] = static_cast<T>(2) / (top - bottom);
    ret[2][2] = -1;
    ret[3][0] = -(right + left) / (right - left);
    ret[3][1] = -(top + bottom) / (top - bottom);
    return ret;
}

void
OSD::upload_projection(const Matrix<float, 4, 4, std::array>& proj) noexcept
{
    glUseProgram(m_program.id());
    glUniformMatrix4fv(m_proj_loc, 1, GL_FALSE, proj.data().data());
}

void
OSD::update_vbo(const char* buffer) noexcept
{
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, static_cast<void*>(0));
    glEnableVertexAttribArray(0);

    glBufferData(GL_ARRAY_BUFFER, m_quads * 4 * 16, buffer, GL_STATIC_DRAW);
}

void
OSD::update_ebo() noexcept
{
    std::vector<std::uint32_t> indices;

    indices.reserve(m_quads * 6);

    for(int q = 0; q < m_quads; ++q)
    {
        const std::uint32_t v = q * 4;

        indices.push_back(v);
        indices.push_back(v + 1);
        indices.push_back(v + 2);

        indices.push_back(v);
        indices.push_back(v + 2);
        indices.push_back(v + 3);
    }

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size() * sizeof(std::uint32_t),
                 indices.data(),
                 GL_STATIC_DRAW);
}

OSD::OSD(const float x,
         const float y,
         const char* text,
         int         width,
         int         height) :
    m_x(x), m_y(y), m_program(Shader(vertex_shader_src, GL_VERTEX_SHADER),
                              Shader(fragment_shader_src, GL_FRAGMENT_SHADER)),
    m_proj_loc(glGetUniformLocation(m_program.id(), "projection"))
{
    if(m_proj_loc == -1) throw std::runtime_error("project location is wrong");

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    upload_projection(
        ortho(0.f, static_cast<float>(width), static_cast<float>(height), 0.f));

    update_text(text);

    gl_check_errors();

    glBindVertexArray(0);
}

void
OSD::update_text(const char* text) noexcept
{
    constexpr std::size_t buffer_size = 40000;

    std::vector<char> buffer(buffer_size);
    m_quads = stb_easy_font_print(m_x,
                                  m_y,
                                  const_cast<char*>(text),
                                  nullptr,
                                  buffer.data(),
                                  buffer_size);

    update_vbo(buffer.data());
    update_ebo();
}

void
OSD::resize(int width, int height) noexcept
{
    upload_projection(
        ortho(0.f, static_cast<float>(width), static_cast<float>(height), 0.f));
}

void
OSD::draw() noexcept
{
    m_program.use();
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_quads * 6, GL_UNSIGNED_INT, 0);
}

OSD::~OSD()
{
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteBuffers(1, &m_ebo);
}
} // namespace loom::gl
