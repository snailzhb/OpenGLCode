/** MeshCraft.exe — mesh 地形建造玩法 */
#include "app/MeshCraftApplication.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    MeshCraftApplication app;
    return app.run();
}
