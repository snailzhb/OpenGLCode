#pragma once

#include "voxel/BlockTypes.h"
#include "voxel/Chunk.h"
#include "voxel/VoxelConstants.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace engine::lighting {
class LightingSystem;
}

namespace voxel {

/** 动态旗帜实例：由 decorateChunk 登记，由 SceneryRenderer 绘制 */
struct SceneryFlag {
    float poleX = 0.0f;
    float poleZ = 0.0f;
    float clothAnchorY = 0.0f;
    float yawDeg = 0.0f;
    int colorSeed = 0;
};

/** 炊烟粒子：实例化 billboard，由 GPU 绘制 */
struct ScenerySmokePuff {
    glm::vec4 posScale{}; // xyz, halfSize
    glm::vec4 tintPhase{}; // rgb 偏色, phase
};

    /** 与主网格解耦的装饰体素：世界中心 xyz、边长 scale、图集 tileId（1-28） */
struct SceneryScaledVoxel {
    glm::vec4 posScale{}; // xyz = 立方体中心, w = 边长
    glm::vec4 tilePad{};  // x = tileId, y = 绕 Y 轴旋转（度）
};

class VoxelWorld {
public:
    VoxelWorld();

    BlockId getBlock(int wx, int wy, int wz) const;
    void setBlock(int wx, int wy, int wz, BlockId b);

    /** 射线与方块相交：命中方块整数坐标 outIx..，面法线 outN；未命中返回 false */
    bool raycastBlock(glm::vec3 origin, glm::vec3 dir, float maxDist, glm::ivec3& outBlock,
                      glm::ivec3& outNormal) const;

    void ensureChunk(int cx, int cy, int cz);
    /** 在水平方向预生成/加载 chunk（含全部 Y 层），halfExtent 为 chunk 半径 */
    void streamAround(int wx, int wz, int halfExtentChunks);

    /** 删除水平方向远离相机的 chunk（及 pending 队列中对应项），降低常驻内存 */
    void pruneChunksAwayFromCamera(const glm::vec3& camWorld, int streamHalfExtentChunks);
    void markNeighborsDirty(int wx, int wy, int wz);

    /**
     * @param sortCam 非空时优先处理离相机近的 chunk（观感上近处先“长出来”）
     * @param maxCpuSec 单帧 CPU 时间上限（秒）；<=0 表示仅用 maxChunkRebuilds 计数限制
     */
    void rebuildDirtyMeshes(int maxChunkRebuilds = 256, const glm::vec3* sortCam = nullptr, float maxCpuSec = -1.0f);
    void uploadMeshes(int maxChunkUploads = 256, const glm::vec3* sortCam = nullptr, float maxCpuSec = -1.0f);

    /** 仍保留：调试或导入后需强制刷满时可调用（会卡顿） */
    void flushAllPendingMeshes();

    /** streamAround 登记、尚未实例化的 chunk 数量 */
    size_t pendingChunkCreateCount() const { return pendingCreate_.size(); }

    /** 按距离优先创建 chunk，减轻单帧尖峰（与 streamAround 配合） */
    void pumpPendingChunkCreations(int maxCreates, const glm::vec3& camWorld);

    /**
     * 从 tools/voxelize_obj_to_voxdat.py 生成的稀疏体素文件放置方块。
     * 格式: "HVX1" + uint32 little-endian 数量 n + n×(uint8 ix,iy,iz)，局部坐标 0..47。
     * 世界坐标 = (originWx + ix, originWy + iy, originWz + iz)。
     * @return 是否成功打开并解析（无文件或魔数错误返回 false）
     */
    bool placeFromVoxDat(const char* path, int originWx, int originWy, int originWz, BlockId material);

    const std::unordered_map<uint64_t, std::unique_ptr<Chunk>>& chunks() const { return chunks_; }

    const std::vector<SceneryFlag>& sceneryFlags() const { return sceneryFlags_; }
    const std::vector<ScenerySmokePuff>& smokePuffs() const { return smokePuffs_; }
    const std::vector<SceneryScaledVoxel>& scaledVoxels() const { return scaledVoxels_; }

    static void worldToChunk(int wx, int wy, int wz, int& cx, int& cy, int& cz, int& lx, int& ly, int& lz);
    static uint64_t packChunk(int cx, int cy, int cz);

    /** 程序化地表高度：平原为主、小丘陵、曲流河谷下切（与未缓存 chunk 的 getBlock 地形一致） */
    static int terrainHeight(int wx, int wz);

    bool hasAnyGlassMesh() const;

    /** 收集相机附近路灯世界坐标与颜色，供体素着色器点光近似（每帧调用） */
    void gatherNearbyLamps(const glm::vec3& cam, int maxLamps, float searchRadius, std::vector<glm::vec4>& outPacked,
                           int& outCount) const;

    /** 场景编排 / Game API：多尺寸装饰体素 */
    void addScaledProp(float worldX, float worldZ, float side, int tileId, float yawDeg = 0.0f, int hubWx = 0,
                       int hubWz = 0, float hubAvoidRadius = 0.0f);
    void addScaledPropAt(float worldX, float worldY, float worldZ, float side, int tileId, float yawDeg = 0.0f);

    /** 场景光照由引擎 LightingSystem 接管（村落扩建时注册火把等） */
    void setSceneLighting(engine::lighting::LightingSystem* lighting) { sceneLighting_ = lighting; }

private:
    Chunk* getChunkMutable(int cx, int cy, int cz);
    const Chunk* getChunkConst(int cx, int cy, int cz) const;
    void generateChunk(Chunk& ch);
    void decorateChunk(Chunk& ch);
    /** 每个网格小区至多一次：在包含地表锚点的 chunk 首次加载时放置村落 */
    void tryPlaceRegionalVillagesForChunk(int cx, int cy, int cz);
    void buildRegionalVillageCluster(int anchorWx, int anchorWz, uint32_t seed, bool hub);
    void placeCompositePrefabs(int anchorWx, int anchorWz, uint32_t seed, bool hub);

    std::unordered_map<uint64_t, std::unique_ptr<Chunk>> chunks_;
    std::vector<SceneryFlag> sceneryFlags_;
    std::vector<ScenerySmokePuff> smokePuffs_;
    std::vector<SceneryScaledVoxel> scaledVoxels_;
    std::unordered_set<uint64_t> regionalVillagePlaced_;
    /** 流式加载：已登记但未 generate+decorate 的 chunk */
    std::unordered_set<uint64_t> pendingCreate_;
    engine::lighting::LightingSystem* sceneLighting_ = nullptr;
};

} // namespace voxel
