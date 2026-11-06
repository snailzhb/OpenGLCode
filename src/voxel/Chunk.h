#pragma once

#include "voxel/BlockTypes.h"
#include "voxel/VoxelConstants.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace voxel {

class Chunk {
public:
    int cx = 0;
    int cy = 0;
    int cz = 0;

    Chunk(int ix, int iy, int iz);
    ~Chunk();

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;

    static size_t index(int lx, int ly, int lz);

    BlockId getLocal(int lx, int ly, int lz) const;
    void setLocal(int lx, int ly, int lz, BlockId b);
    /** 批量地形生成用：只写 data，不反复置 dirty；最后需 markMeshDirty() */
    void setLocalBulk(int lx, int ly, int lz, BlockId b);
    void markMeshDirty() { meshRebuildPending_ = true; }

    void markDirty() { meshRebuildPending_ = true; }
    /** 体素相对 mesh 已过期，需 CPU 重建 */
    bool meshRebuildPending() const { return meshRebuildPending_; }
    /** CPU 顶点已就绪，需推送到 GPU（可与重建分开排队，避免单帧卡顿） */
    bool gpuUploadPending() const { return gpuUploadPending_; }

    /** world-space block query for neighbors */
    void rebuildMesh(const std::function<BlockId(int, int, int)>& worldGet);

    void uploadIfDirty();
    void drawOpaque() const;
    void drawGlass() const;
    void drawWater() const;

    bool hasOpaque() const { return opaqueCount_ > 0; }
    bool hasGlass() const { return glassCount_ > 0; }
    bool hasWater() const { return waterCount_ > 0; }

private:
    void shutdownGpu();
    void clearCpuMesh();

    std::vector<uint8_t> data_;
    bool meshRebuildPending_ = true;
    bool gpuUploadPending_ = false;

    unsigned int opaqueVao_ = 0;
    unsigned int opaqueVbo_ = 0;
    int opaqueCount_ = 0;
    std::vector<float> opaqueVerts_;

    unsigned int glassVao_ = 0;
    unsigned int glassVbo_ = 0;
    int glassCount_ = 0;
    std::vector<float> glassVerts_;

    unsigned int waterVao_ = 0;
    unsigned int waterVbo_ = 0;
    int waterCount_ = 0;
    std::vector<float> waterVerts_;
};

} // namespace voxel
