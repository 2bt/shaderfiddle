#include "fx.hpp"
#include "gfx.hpp"
#include "gui.hpp"
#include <fstream>
#include <sstream>
#include <SDL2/SDL.h>
#include <algorithm>
#include <uv.h>


class App : public fx::App {
public:
    App(char* const path) : m_path(path) {}

    void init() override {
        gui::init();

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

    void free() override {
        uv_loop_close(m_loop);
        gui::free();
        delete m_va;
        delete m_vb;
        delete m_shader;
    }

    void process_event(SDL_Event const& e) override {
        gui::process_event(e);
    }


    void update() override {
        uv_run(m_loop, UV_RUN_NOWAIT);
        if (!m_shader) return;
        ++m_frame;

        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        m_y_ang += (ks[SDL_SCANCODE_RIGHT] - ks[SDL_SCANCODE_LEFT]) * 0.02f;
        m_x_ang += (ks[SDL_SCANCODE_DOWN] - ks[SDL_SCANCODE_UP]) * 0.02f;

        float cy = cosf(m_y_ang);
        float sy = sinf(m_y_ang);
        float cx = cosf(m_x_ang);
        float sx = sinf(m_x_ang);

        glm::mat3 eye = glm::mat3{
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
        m_pos += eye * mov * 0.1f;

        if (mov != glm::vec3()) {
            printf("%f, %f, %f | %f, %f\n",
                    m_pos.x, m_pos.y, m_pos.z, m_x_ang, m_y_ang);
        }

        if (m_shader->has_uniform("iPos")) {
            m_shader->set_uniform("iPos", m_pos);
        }
        if (m_shader->has_uniform("iEye")) {
            m_shader->set_uniform("iEye", eye);
        }
        if (m_shader->has_uniform("iResolution")) {
            m_shader->set_uniform("iResolution", glm::vec2(fx::screen_width(), fx::screen_height()));
        }
        if (m_shader->has_uniform("iFrame")) {
            m_shader->set_uniform("iFrame", float(m_frame));
        }
        if (m_shader->has_uniform("iTime")) {
            m_shader->set_uniform("iTime", (SDL_GetTicks() - m_start_time) * 0.001f);
        }


        gui::new_frame();
        gui::set_next_window_pos({5, 5});
        gui::begin_window("Variables");

        for (Variable& v : m_variables) {
            std::string u = "_" + v.name;
            if (m_shader->has_uniform(u)) {
                gui::drag_float(v.name.c_str(), v.val, 1, v.min, v.max);
                m_shader->set_uniform(u, v.val);
            }
        }

        gfx::clear({0, 0, 0, 1});
        gfx::draw(m_rs, m_shader, m_va);
        gui::render();
    }


private:

    void load_shader();

    static void event_callback(uv_fs_event_t* handle, const char* path, int events, int) {
        App* a = (App*) handle->data;
        if (events & UV_CHANGE) {
            uv_fs_event_stop(handle);
            a->load_shader();
            uv_fs_event_start(handle, &event_callback, a->m_path, 0);
        }
    }

    glm::vec3 m_pos = { 2.285664, 3.737782, -8.859721 };
    float m_x_ang = 0.3;
    float m_y_ang = -0.34;

    struct Variable {
        std::string name;
        float       min;
        float       max;
        float       val;
    };

    char const*           m_path;
    uv_loop_t*            m_loop;
    uv_fs_event_t         m_handle;
    std::vector<Variable> m_variables;
    gfx::Shader*          m_shader = nullptr;

    uint32_t m_start_time = SDL_GetTicks();
    uint32_t m_frame      = 0;

    gfx::RenderState   m_rs;
    gfx::VertexArray*  m_va = nullptr;
    gfx::VertexBuffer* m_vb = nullptr;
};


void App::load_shader() {
    printf("loading shader...\n");

    std::ifstream file(m_path);
    if (!file.is_open()) {
        printf("cannot open shader file\n");
        return;
    }

    if (m_shader) delete m_shader;
    m_shader = nullptr;

    std::stringstream buf;
    char c;

    // helper functions for parsing
    auto expect = [&c, &file](char e){
        if (c != e) throw std::logic_error("invalid token");
        file.get(c);
    };
    auto parse_number = [&c, &file](){
        std::string s;
        while (isspace(c)) file.get(c);
        if (c == '-') {
            s += '-';
            file.get(c);
        }
        while (isalnum(c)) {
            s += c;
            file.get(c);
        }
        if (c == '.') {
            s += '.';
            file.get(c);
            while (isalnum(c)) {
                s += c;
                file.get(c);
            }
        }
        while (isspace(c)) file.get(c);
        if (s.empty()) throw std::logic_error("cannot parse number");
        return std::stof(s);
    };



    while (file.get(c)) {
        // find $variables
        if (c == '$') {
            Variable var { "", 0, 1, 0.5 };
            while (file.get(c) && (isalnum(c) || c == '_')) var.name += c;
            if (var.name.empty()) {
                printf("variable error: name empty\n");
                return;
            }

            // check for optional (min,max)
            if (c == '(') {
                file.get(c);
                try {
                    var.min = parse_number();
                    expect(',');
                    var.max = parse_number();
                    var.val = (var.min + var.max) * 0.5;
                    if (c == '.') {
                        file.get(c);
                        var.val = parse_number();
                    }
                    expect(')');
                }
                catch (std::logic_error const& e){
                    printf("variable error: %s\n", e.what());
                    return;
                }
            }

            // allocate variable
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

            buf << "_" << var.name;
        }

        buf << c;
    }

    int prelines = 6;
    std::stringstream ss;
    ss << "#version 130\n"
            "uniform vec3 iPos;\n"
            "uniform mat3 iEye;\n"
            "uniform float iTime;\n"
            "uniform float iFrame;\n"
            "uniform vec2 iResolution;\n";
    for (Variable const& v : m_variables) {
        ss << "uniform float _" << v.name << ";\n";
        ++prelines;
    }
    ss << buf.str();
    std::string code = ss.str();
    try {
        m_shader = gfx::Shader::create(nullptr, code.c_str());
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


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("usage: %s shader_file\n", argv[0]);
        return 0;
    }
    App a(argv[1]);
    return fx::run(a);
}
