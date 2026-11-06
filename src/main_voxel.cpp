/** GameEigen.exe — 体素村落玩法 */
#include "app/VoxelApplication.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    VoxelApplication app;
    return app.run();
}
