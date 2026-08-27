#include "include/GameApp.h"

#include <emscripten/emscripten.h>

namespace
{
    GameApp app;

    void MainLoopTick()
    {
        app.Tick();
    }
}

int main()
{
    app.Initialize();

    // The browser owns the frame loop; a blocking while() would freeze the tab.
    // Passing fps=0 defers to requestAnimationFrame for vsync-matched pacing.
    emscripten_set_main_loop(MainLoopTick, 0, 1);

    app.Shutdown();
    return 0;
}
