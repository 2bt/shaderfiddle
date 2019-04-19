#pragma once

union SDL_Event;

namespace fx {

    struct App {
        virtual void init() {}
        virtual void free() {}
        virtual void key(int code) {}
        virtual void update() {}
        virtual void resized() {}
        virtual void process_event(SDL_Event const& e) {}
    };

    int run(App& App);
    void exit(int result = 0);

    struct Input {
        int  x;
        int  y;
        bool a;
        bool b;
    };

    Input const& input();

    int screen_width();
    int screen_height();
}
