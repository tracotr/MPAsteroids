#include "include/GameApp.h"

int main()
{
    GameApp app;
    
    app.Initialize();
    app.RunLoop();
    app.Shutdown();

    return 0;
}