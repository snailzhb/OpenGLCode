# GameEigen 引擎架构

本项目已从「单文件体素演示」升级为 **引擎 + 游戏 API + 体素玩法** 的分层结构。

## 目录结构

```
src/
  engine/           # 运行时与子系统
    Engine.*        # 帧循环、子系统调度
    physics/        # 体素 AABB 物理、角色控制器
    lighting/       # 方向光 + 点光 + 环境光
  game/             # 游戏层 API 与场景编排
    GameAPI.*       # 对外统一门面
    SceneComposer.* # 市场、码头、城墙等扩建
  voxel/            # 方块世界、流式 chunk
  rendering/        # 图集、缩放体素、旗帜、水面
  app/              # GLFW 应用与渲染管线（VoxelApplication / MeshCraftApplication）
  game/meshcraft/   # MeshCraft：高度场地形 mesh + 网格建筑
```

## 引擎子系统

| 子系统 | 职责 |
|--------|------|
| **PhysicsWorld** | 动态刚体（AABB）、重力、地面碰撞 |
| **CharacterController** | 第一人称行走 / 跳跃 / 水中 / 飞行 |
| **VoxelCollider** | 将 `VoxelWorld` 封装为碰撞查询 |
| **LightingSystem** | 太阳方向光、天空半球环境、最多 24 盏点光 |

每帧顺序（在 `VoxelApplication::update` 中）：

1. `engine_.beginFrame(dt)` → 物理、光照子系统 tick  
2. `CharacterController::tick` → 更新相机位置  
3. 体素流式 / 网格重建 / 玩法逻辑  
4. `render` 中 `syncLightingFromScene` → 收集路灯 + 动态火把并上传着色器  

## 游戏 API（`game::GameAPI`）

通过 `GameAPI::get()` 访问当前会话：

```cpp
auto& api = game::GameAPI::get();
api.world().streamAround(wx, wz, 4);
api.lighting().addFlickerTorch(pos, color, intensity, radius, phase);
api.toast("Hello");
api.player().eyePosition();
```

绑定在 `VoxelApplication::init` 中完成；关闭时解绑。

## 光照模型

- **方向光**：昼夜循环驱动的太阳/月光（阴影贴图仍用 `uSunDir`）  
- **环境光**：`uSkyZenith` / `uSkyHorizon` + `uHemiStrength`  
- **点光**：`uPointLightCount` + `uPointLightBuf[24×2]`  
  - 体素路灯（`gatherNearbyLamps`）  
  - `LightingSystem::addPointLight` / `addFlickerTorch`（市场火把、篝火等）  

体素与缩放体素着色器均已接入同一套 uniform。

## 物理

- 玩家使用 **CharacterController**（非刚体列表），与体素网格 AABB 解析碰撞。  
- **PhysicsWorld** 可挂载 **Dynamic** 刚体，供将来掉落物、可推动方块等扩展。  
- 飞行模式（`F`）绕过碰撞，由 `CharacterInput::flyMode` 控制。

## 场景复杂度（`SceneComposer`）

主村（hub）在 `buildRegionalVillageCluster` 之后自动调用：

- **东门市场**：平台 + 8 摊位缩放体素 + 火把点光  
- **南码头**：栈桥、水面、路灯  
- **外圈城墙**：半径 ~58 的矮墙 + 间歇路灯  
- **西北仓储**：多座仓房缩放体素  

副村调用 `enrichRegionalCluster`（营地 + 火把）。

流式半径由 `kStreamHalf_=4`、绘制距离 `kChunkDrawMaxDist=380` 支撑更大视野。

## 扩展建议

1. **脚本**：在 Lua/Python 中调用 `GameAPI` 的 C 绑定。  
2. **ECS**：在 `game/` 下增加 `Entity` / `Component` 管理动态物体。  
3. **高级光照**：SSAO、体积雾、Cascaded Shadow Maps。  
4. **物理**：接入 Bullet；`VoxelCollider` 保留为静态世界形状提供者。

## MeshCraft（mesh 地形建造）

独立玩法模块，复用引擎物理与光照，见 [MESHCRAFT.md](MESHCRAFT.md)。

```bash
MeshCraft.exe
```

体素玩法：`GameEigen.exe`。

## 构建

与原先相同，CMake 目标仍为 `GameEigen`；新增源文件已写入 `src/CMakeLists.txt`。
