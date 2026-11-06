# OpenGLCode

C++17 + OpenGL + GLFW 游戏引擎，两个可执行程序：

| 程序 | 启动 | 说明 |
|------|------|------|
| GameEigen | `bin/运行体素游戏.bat` | 体素世界 |
| MeshCraft | `bin/运行MeshCraft.bat` | 网格地形建造 |

## 目录

```
OpenGLCode/
├── src/          源码
├── bin/          可执行文件（CMake 直接输出到这里）
├── includes/     头文件（GLFW、GLM 等）
├── lib/          静态库
├── dlls/         运行时 DLL
├── resources/    资源
├── docs/         文档（ENGINE.md、GAMEPLAY.md）
└── out/          VS CMake 构建缓存（可删，重新编译会重建）
```

## 编译（Visual Studio）

打开文件夹 → 选择 `x64-Release` → 生成。exe 自动输出到 `bin/`。

## 文档

- [引擎架构](docs/ENGINE.md)
- [玩法说明](docs/GAMEPLAY.md)
- [MeshCraft](docs/MESHCRAFT.md)
