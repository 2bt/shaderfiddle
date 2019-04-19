#include "fx.hpp"
#include "gfx.hpp"
#include "gui.hpp"
#include <fstream>
#include <sstream>
#include <SDL2/SDL.h>
#include <algorithm>
#include <uv.h>


struct Variable {
    std::string name;
    float       min;
    float       max;
    float       val;
};

std::vector<Variable> m_variables;
gfx::Shader*          m_shader = nullptr;
char const*           m_path;




void load_shader() {
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
            if (std::none_of(m_variables.begin(), m_variables.end(), [&var](auto& v){
                return v.name == var.name;
            })) {
                m_variables.emplace_back(var);
            }

            buf << "_" << var.name;
        }

        buf << c;
    }

    std::stringstream code;
    code << "#version 130\n"
            "uniform float iTime;\n"
            "uniform float iFrame;\n"
            "uniform vec2 iResolution;\n";
    for (Variable const& v : m_variables) {
        code << "uniform float _" << v.name << ";\n";
    }

    code << buf.str();
    m_shader = gfx::Shader::create(nullptr, code.str().c_str());

    printf("done\n");
}


void event_callback(uv_fs_event_t* handle, const char*, int events, int) {
    if (events & UV_CHANGE) {
        uv_fs_event_stop(handle);
        load_shader();
        uv_fs_event_start(handle, &event_callback, m_path, 0);
    }
}


class App : public fx::App {
public:

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
        ++m_frame;

        if (!m_shader) return;

        gui::new_frame();
        gui::set_next_window_pos({5, 5});
        gui::begin_window("Variables");


        for (Variable& v : m_variables) {
            gui::drag_float(v.name.c_str(), v.val, 1, v.min, v.max);
            std::string u = "_" + v.name;
            if (m_shader->has_uniform(u)) m_shader->set_uniform(u, v.val);
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

        gfx::clear({0, 0, 0, 1});
        gfx::draw(m_rs, m_shader, m_va);
        gui::render();
    }


private:

    uint32_t m_start_time = SDL_GetTicks();
    uint32_t m_frame = 0;

    uv_loop_t*    m_loop;
    uv_fs_event_t m_handle;

    gfx::RenderState   m_rs;
    gfx::VertexArray*  m_va = nullptr;
    gfx::VertexBuffer* m_vb = nullptr;
};


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("usage: %s shader_file\n", argv[0]);
        return 0;
    }
    m_path = argv[1];
    App a;
    return fx::run(a);
}
