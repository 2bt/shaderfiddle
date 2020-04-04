#include "fx.hpp"
#include "gfx.hpp"
#include "gui.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <SDL2/SDL.h>
#include <algorithm>
#include <uv.h>


struct Variable {
    std::string name;
    float       min;
    float       max;
    float       val;
    bool        rendered;
};


class App : public fx::App {
public:
    App(char* const path) : m_path(path) {}

    void init() override;

    void free() override {
        uv_loop_close(m_loop);
        gui::free();
        delete m_va;
        delete m_vb;
        for (gfx::Shader* s : m_shaders) delete s;
        for (gfx::Texture2D* c : m_channels) delete c;

        delete m_framebuffer;
        delete m_scale_shader;
        delete m_overlay_tex;
        delete m_overlay_shader;
    }

    void process_event(SDL_Event const& e) override {
        gui::process_event(e);
    }

    void resized() override {
        init_channels();
    }

    void key(int code) override {
        if (code == SDL_SCANCODE_RETURN) {
            printf("%f, %f, %f | %f, %f\n", m_pos.x, m_pos.y, m_pos.z, m_ang.x, m_ang.y);
        }
        int old_scale = m_scale;
        if (code == SDL_SCANCODE_EQUALS) ++m_scale;
        if (code == SDL_SCANCODE_MINUS) m_scale = std::max(1, m_scale - 1);
        if (m_scale != old_scale) init_channels();
    }

    void update() override;

private:

    void load_shader();
    void init_channels();
    void update_view();

    static void event_callback(uv_fs_event_t* handle, const char* path, int events, int) {
        App* a = (App*) handle->data;
        if (events & UV_CHANGE) {
            uv_fs_event_stop(handle);
            a->load_shader();
            uv_fs_event_start(handle, &event_callback, a->m_path, 0);
        }
    }

    glm::vec3 m_pos = { 65.829933, 8.649950, -136.384216 };
    glm::vec2 m_ang = { 0.040000, -0.400000 };
    glm::mat3 m_eye;
    char const*           m_path;
    uv_loop_t*            m_loop;
    uv_fs_event_t         m_handle;

    std::vector<Variable> m_variables;
    uint32_t              m_start_time = SDL_GetTicks();
    uint32_t              m_frame      = 0;

    std::array<gfx::Shader*, 4>     m_shaders  = {};
    std::array<gfx::Texture2D*, 4>  m_channels = {};
    bool                            m_clear_channels = false;

    gfx::RenderState   m_rs;
    gfx::VertexArray*  m_va = nullptr;
    gfx::VertexBuffer* m_vb = nullptr;

    int                m_scale        = 1;
    gfx::Framebuffer*  m_framebuffer  = nullptr;
    gfx::Shader*       m_scale_shader = nullptr;

    gfx::Texture2D*    m_overlay_tex    = nullptr;
    gfx::Shader*       m_overlay_shader = nullptr;
};


void App::init() {
    gui::init();

//    m_overlay_tex    = gfx::Texture2D::create("overlay.png");
//    m_overlay_shader = gfx::Shader::create(R"(#version 130
//void main() { gl_Position = gl_Vertex; }
//)", R"(#version 130
//uniform sampler2D tex;
//uniform vec2 res;
//void main() {
//    ivec2 ts = textureSize(tex, 0);
//    ivec2 tc = ivec2(gl_FragCoord.xy + (ts - res) * 0.5);
//    vec4 d = texelFetch(tex, ivec2(tc.x, ts.y - tc.y), 0);
//    gl_FragColor = vec4(d.rgb, 0.5);
//}
//)");
//    m_overlay_shader->set_uniform("tex", m_overlay_tex);

    m_framebuffer = gfx::Framebuffer::create();
    m_scale_shader = gfx::Shader::create(R"(#version 130
void main() { gl_Position = gl_Vertex; }
)", R"(#version 130
uniform sampler2D tex;
uniform vec2 scale;
void main() {
    gl_FragColor = texture2D(tex, gl_FragCoord.xy * scale);
}
)");
    init_channels();

    m_vb = gfx::VertexBuffer::create(gfx::BufferHint::StaticDraw);
    m_va = gfx::VertexArray::create();
    m_va->set_primitive_type(gfx::PrimitiveType::TriangleStrip);
    m_va->set_attribute(0, m_vb, gfx::ComponentType::Float, 2, false, 0, sizeof(glm::vec2));

    std::vector<glm::vec2> v = {
       { -1, -1 },
       {  1, -1 },
       { -1,  1 },
       {  1,  1 },
    };
    m_vb->init_data(v);
    m_va->set_count(v.size());

    load_shader();
    m_handle.data = this;

    m_loop = uv_default_loop();
    uv_fs_event_init(m_loop, &m_handle);
    uv_fs_event_start(&m_handle, &event_callback, m_path, 0);
}

void App::init_channels() {
    m_clear_channels = true;
    for (gfx::Texture2D*& c : m_channels) {
        delete c;
        c = gfx::Texture2D::create(gfx::TextureFormat::RGBA32F,
                                   fx::screen_width() / m_scale,
                                   fx::screen_height() / m_scale,
                                   nullptr,
                                   gfx::FilterMode::Linear);
    }
}


void App::update_view() {
    glm::vec3 old_pos = m_pos;
    glm::vec2 old_ang = m_ang;

    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    m_ang.x += (ks[SDL_SCANCODE_DOWN]  - ks[SDL_SCANCODE_UP]) * 0.02f;
    m_ang.y += (ks[SDL_SCANCODE_RIGHT] - ks[SDL_SCANCODE_LEFT]) * 0.02f;

    float cy = cosf(m_ang.y);
    float sy = sinf(m_ang.y);
    float cx = cosf(m_ang.x);
    float sx = sinf(m_ang.x);

    m_eye = glm::mat3{
        cy, 0, -sy,
        0, 1, 0,
        sy, 0, cy,
    } * glm::mat3{
        1, 0, 0,
        0, cx, sx,
        0, -sx, cx,
    };

    glm::vec3 mov = {
        ks[SDL_SCANCODE_D]     - ks[SDL_SCANCODE_A],
        ks[SDL_SCANCODE_SPACE] - ks[SDL_SCANCODE_LSHIFT],
        ks[SDL_SCANCODE_W]     - ks[SDL_SCANCODE_S],
    };
    m_pos += m_eye * mov * 1.0f;

    m_clear_channels |= m_pos != old_pos || m_ang != old_ang || ks[SDL_SCANCODE_RETURN];
}

void App::update() {
    uv_run(m_loop, UV_RUN_NOWAIT);
    ++m_frame;

    update_view();

    for (Variable& v : m_variables) v.rendered = false;

    gui::new_frame();
    gui::set_next_window_pos({5, 5});
    gui::begin_window("Variables");
    for (gfx::Shader* shader : m_shaders) {
        if (!shader) break;
        if (shader->has_uniform("iPos")) shader->set_uniform("iPos", m_pos);
        if (shader->has_uniform("iEye")) shader->set_uniform("iEye", m_eye);
        if (shader->has_uniform("iResolution")) {
            shader->set_uniform("iResolution", glm::vec2(m_channels[0]->get_width(),
                                                         m_channels[0]->get_height()));
        }
        if (shader->has_uniform("iFrame")) shader->set_uniform("iFrame", float(m_frame));
        if (shader->has_uniform("iTime")) {
            shader->set_uniform("iTime", (SDL_GetTicks() - m_start_time) * 0.001f);
        }
        if (shader->has_uniform("iChannel0")) shader->set_uniform("iChannel0", m_channels[0]);
        if (shader->has_uniform("iChannel1")) shader->set_uniform("iChannel1", m_channels[1]);
        if (shader->has_uniform("iChannel2")) shader->set_uniform("iChannel2", m_channels[2]);
        if (shader->has_uniform("iChannel3")) shader->set_uniform("iChannel3", m_channels[3]);
        for (Variable& v : m_variables) {
            std::string u = "_" + v.name;
            if (shader->has_uniform(u)) {
                shader->set_uniform(u, v.val);
                if (!v.rendered) {
                    if (gui::drag_float(v.name.c_str(), v.val, 1, v.min, v.max)) {
                        m_clear_channels = true;
                    }
                    v.rendered = true;
                }
            }
        }
    }

    int index = -1;
    for (gfx::Shader* shader : m_shaders) {
        if (!shader) break;
        ++index;
        m_framebuffer->attach_color(m_channels[index]);
        if (m_clear_channels) gfx::clear({}, m_framebuffer);
        gfx::draw(m_rs, shader, m_va, m_framebuffer);
    }
    m_clear_channels = false;

    if (index >= 0) {
        gfx::clear({});
        m_scale_shader->set_uniform("tex", m_channels[index]);
        m_scale_shader->set_uniform("scale", 1.0f / glm::vec2(fx::screen_width(), fx::screen_height()));
        gfx::draw(m_rs, m_scale_shader, m_va);
    }

    // overlay
//    gfx::RenderState rs;
//    rs.blend_enabled      = true;
//    rs.blend_func_src_rgb = gfx::BlendFunc::SrcAlpha;
//    rs.blend_func_dst_rgb = gfx::BlendFunc::OneMinusSrcAlpha;
//    m_overlay_shader->set_uniform("res", glm::vec2(fx::screen_width(), fx::screen_height()));
//    gfx::draw(rs, m_overlay_shader, m_va);

    gui::render();
}


class Parser {
public:
    Parser(std::istream& input) : m_input(input) {}

    template <class Func>
    std::string parse_shader(Func const& func) {
        std::stringstream ss;
        std::string line;
        while (std::getline(m_input, line)) {
            ++m_line_count;
            if (line == "---") break;
            static const std::regex var_reg(R"(\$(\w+)(\(([^,]+),([^)]+)\))?)");
            std::smatch match;
            while (std::regex_search(line, match, var_reg)) {
                ss << match.prefix();
                ss << '_' << match[1];
                Variable var { match[1], 0, 1, 0.5f };
                if (match[2].length() > 0) {
                    var.min = std::stof(match[3]);
                    var.max = std::stof(match[4]);
                    var.val = (var.min + var.max) * 0.5f;
                }
                func(var);
                line = match.suffix();
            }
            ss << line << '\n';
        }
        return ss.str();
    }
private:
    int           m_line_count = 0;
    std::istream& m_input;
};


void App::load_shader() {
    printf("loading shader...\n");
    m_clear_channels = true;

    std::ifstream file(m_path);
    if (!file.is_open()) {
        printf("cannot open shader file\n");
        return;
    }

    for (gfx::Shader*& s : m_shaders) {
        delete s;
        s = nullptr;
    }

    try {
        Parser parser(file);
        for (int i = 0; i < 4; ++i) {
            std::string code = parser.parse_shader([this](Variable var) {
                auto it = std::find_if(m_variables.begin(), m_variables.end(), [&var](auto& v) {
                    return v.name == var.name;
                });
                if (it != m_variables.end()) {
                    if (var.min != 0 || var.max != 1) {
                        var.val = it->val;
                        *it = var;
                    }
                }
                else m_variables.emplace_back(var);
            });
            if (code.empty()) break;

            int prelines = 10;
            std::stringstream ss;
            ss << R"(#version 130
uniform vec3 iPos;
uniform mat3 iEye;
uniform float iTime;
uniform float iFrame;
uniform vec2 iResolution;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;
)";
            for (Variable const& v : m_variables) {
                ss << "uniform float _" << v.name << ";\n";
                ++prelines;
            }
            ss << code;
            code = ss.str();

            try {
                m_shaders[i] = gfx::Shader::create(nullptr, code.c_str());
                printf("done.\n");
            }
            catch (std::runtime_error const& e) {
                const char* msg = e.what();
                while (*msg) {
                    const char* c = msg;
                    while (*c && *c != '0') ++c;
                    while (*c && *c == '0') ++c;
                    while (*c && (*c < '0' || *c > '9')) ++c;
                    char* d;
                    int line = strtol(c, &d, 10);
                    c = d;
                    while (*d && *d != '\n') ++d;
                    printf("%d%.*s\n", line - prelines, int(d - c), c);
                    if (*d == '\n') ++d;
                    msg = d;
                }
            }
        }
    }
    catch (std::logic_error const& e) {
        printf("ERROR: %s\n", e.what());
        for (gfx::Shader*& s : m_shaders) {
            delete s;
            s = nullptr;
        }
        return;
    }

}


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("usage: %s shader_file\n", argv[0]);
        return 0;
    }
    App a(argv[1]);
    return fx::run(a);
}
