# MeshCraft — Mesh 地形建造游戏

基于 **GameEigen 引擎**（物理、光照子系统）的独立玩法：程序化 **mesh 地形** + **网格对齐建筑**（非体素）。

## 启动

双击或在命令行运行 **独立 exe**（与体素版分开）：

```bash
MeshCraft.exe
```

也可双击 `bin/运行MeshCraft.bat`。

体素村落请运行 **`GameEigen.exe`**（不要用 `GameEngine.exe`，该名称不存在）。

## 操作

与体素版 **GameEigen** 一致（底部快捷栏、左拆右放、10m 交互距离）：

| 按键 | 功能 |
|------|------|
| W A S D | 移动（行走时在水平面移动） |
| 空格 | 跳跃 / 飞行上升 |
| Shift | 冲刺 / 飞行下降 |
| F | 行走 ↔ 飞行 |
| Tab | 鼠标捕获 |
| 鼠标左键 | 拆除建筑 |
| 鼠标右键 | 在击中面相邻格放置 |
| 1–6 | 快捷栏构件 |
| 滚轮 | 切换快捷栏 |
| Esc | 退出 |

## 世界内容

- **256×256** 高度场地形：草 / 土 / 石 / 沙 **三平面贴图** + 噪声混合，河岸湿润带丝滑过渡（无方块格接缝）。
- **河流**：程序化河道 carved + 半透明水面（波纹）。
- **桥梁**：主河道上的桁架桥面、桥墩与护栏。
- **大型庄园**：多层楼、塔楼与屋顶（出生点西北侧）。
- 出生点附近 **示例村**，可在 **2m 网格 × 2.5m 层高** 自由扩建（不可在河心放置）。

## 架构（重构后）

| 模块 | 职责 |
|------|------|
| `MeshCraftWorld` | 地形生成、场景播种、射线拾取、放置/拆除 |
| `TerrainMesh` | 高度场、河道雕刻、地形/水面网格 |
| `MeshBuildingWorld` | 2m 网格建筑、OBB 射线、碰撞 |
| `BuildingGeom` | 带 yaw 的 OBB 数学 |
| `MeshSceneRenderer` | 地形 + 实例化建筑 + 水面 |
| `MeshCraftApplication` | 窗口、输入、渲染循环 |

引擎复用：`Engine`、`CharacterController`、`MeshTerrainCollider`、`LightingSystem`。详见 [ENGINE.md](ENGINE.md)。
