#include "voxel/Chunk.h"

#include <glad/glad.h>

#include <cmath>
#include <cstring>

namespace voxel {

namespace {


const int S = kChunkSize;

void pushFace(std::vector<float>& out, const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
              const glm::vec3& p3, const glm::vec3& n, float u0, float v0, float u1, float v1, float tid) {
    /* 顶点顺序为 CCW（从立方体外部看），与 glFrontFace(GL_CCW) + glCullFace(GL_BACK) 一致 */
    const float verts[6][9] = {
        {p0.x, p0.y, p0.z, n.x, n.y, n.z, u0, v0, tid}, {p2.x, p2.y, p2.z, n.x, n.y, n.z, u1, v1, tid},
        {p1.x, p1.y, p1.z, n.x, n.y, n.z, u1, v0, tid}, {p0.x, p0.y, p0.z, n.x, n.y, n.z, u0, v0, tid},
        {p3.x, p3.y, p3.z, n.x, n.y, n.z, u0, v1, tid}, {p2.x, p2.y, p2.z, n.x, n.y, n.z, u1, v1, tid},
    };
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 9; ++j)
            out.push_back(verts[i][j]);
}

float tileId(BlockId b) {
    return static_cast<float>(static_cast<int>(b));
}

bool faceVisible(BlockId self, BlockId neighbor) {
    if (self == BlockId::Air)
        return false;
    /* 相邻同为水时不画面：避免水体内部海量半透明叠层（主因卡顿与发蓝） */
    if (self == BlockId::Water)
        return neighbor != BlockId::Water;
    /* 玻璃族：仅与「同种玻璃」相邻时剔除内面；不同玻璃/固体仍画面 */
    if (isGlassFamily(self))
        return !isGlassFamily(neighbor) || neighbor != self;
    /* 路灯：与木柱/地等实体相邻仍画面（自发光）；两灯块相邻不画内面，避免重片 */
    if (isLamp(self)) {
        if (isLamp(neighbor))
            return false;
        return neighbor == BlockId::Air || neighbor == BlockId::Water || isGlassFamily(neighbor) || isOpaque(neighbor);
    }
    /* 普通不透明：与空气/水/玻璃相邻画面；与路灯相邻不画（由灯块画与木柱的共享面） */
    return neighbor == BlockId::Air || neighbor == BlockId::Water || isGlassFamily(neighbor);
}

} // namespace

Chunk::Chunk(int ix, int iy, int iz) : cx(ix), cy(iy), cz(iz) {
    data_.assign(static_cast<size_t>(S * S * S), static_cast<uint8_t>(BlockId::Air));
}

Chunk::~Chunk() {
    shutdownGpu();
}

size_t Chunk::index(int lx, int ly, int lz) {
    return static_cast<size_t>(lx) + static_cast<size_t>(lz) * S + static_cast<size_t>(ly) * S * S;
}

BlockId Chunk::getLocal(int lx, int ly, int lz) const {
    if (lx < 0 || ly < 0 || lz < 0 || lx >= S || ly >= S || lz >= S)
        return BlockId::Air;
    return static_cast<BlockId>(data_[index(lx, ly, lz)]);
}

void Chunk::setLocal(int lx, int ly, int lz, BlockId b) {
    if (lx < 0 || ly < 0 || lz < 0 || lx >= S || ly >= S || lz >= S)
        return;
    data_[index(lx, ly, lz)] = static_cast<uint8_t>(b);
    meshRebuildPending_ = true;
}

void Chunk::setLocalBulk(int lx, int ly, int lz, BlockId b) {
    if (lx < 0 || ly < 0 || lz < 0 || lx >= S || ly >= S || lz >= S)
        return;
    data_[index(lx, ly, lz)] = static_cast<uint8_t>(b);
}

void Chunk::shutdownGpu() {
    if (opaqueVbo_) {
        glDeleteBuffers(1, &opaqueVbo_);
        opaqueVbo_ = 0;
    }
    if (opaqueVao_) {
        glDeleteVertexArrays(1, &opaqueVao_);
        opaqueVao_ = 0;
    }
    if (glassVbo_) {
        glDeleteBuffers(1, &glassVbo_);
        glassVbo_ = 0;
    }
    if (glassVao_) {
        glDeleteVertexArrays(1, &glassVao_);
        glassVao_ = 0;
    }
    if (waterVbo_) {
        glDeleteBuffers(1, &waterVbo_);
        waterVbo_ = 0;
    }
    if (waterVao_) {
        glDeleteVertexArrays(1, &waterVao_);
        waterVao_ = 0;
    }
    opaqueCount_ = glassCount_ = waterCount_ = 0;
}

void Chunk::clearCpuMesh() {
    opaqueVerts_.clear();
    glassVerts_.clear();
    waterVerts_.clear();
}

void Chunk::rebuildMesh(const std::function<BlockId(int, int, int)>& worldGet) {
    gpuUploadPending_ = false;
    clearCpuMesh();
    /* 预分配适中：过小会多次 realloc；过大会长期占满 vector capacity（上传后已 shrink） */
    opaqueVerts_.reserve(10240);
    glassVerts_.reserve(2048);
    waterVerts_.reserve(4096);
    const int wx0 = cx * S;
    const int wy0 = cy * S;
    const int wz0 = cz * S;

    auto worldAt = [&](int lx, int ly, int lz) -> BlockId {
        return worldGet(wx0 + lx, wy0 + ly, wz0 + lz);
    };

    for (int ly = 0; ly < S; ++ly) {
        for (int lz = 0; lz < S; ++lz) {
            for (int lx = 0; lx < S; ++lx) {
                const BlockId b = worldAt(lx, ly, lz);
                if (b == BlockId::Air)
                    continue;

                const float x0 = static_cast<float>(wx0 + lx);
                const float y0 = static_cast<float>(wy0 + ly);
                const float z0 = static_cast<float>(wz0 + lz);
                const float x1 = x0 + 1.0f;
                const float y1 = y0 + 1.0f;
                const float z1 = z0 + 1.0f;

                std::vector<float>* target = nullptr;
                if (b == BlockId::Water)
                    target = &waterVerts_;
                else if (isGlassFamily(b))
                    target = &glassVerts_;
                else if (isOpaque(b))
                    target = &opaqueVerts_;
                else
                    target = &opaqueVerts_;

                const float tid = tileId(b);

                /* +X */
                if (faceVisible(b, worldAt(lx + 1, ly, lz))) {
                    glm::vec3 p000(x1, y0, z0), p001(x1, y0, z1), p011(x1, y1, z1), p010(x1, y1, z0);
                    pushFace(*target, p000, p001, p011, p010, glm::vec3(1, 0, 0), 0, 0, 1, 1, tid);
                }
                /* -X */
                if (faceVisible(b, worldAt(lx - 1, ly, lz))) {
                    glm::vec3 p100(x0, y0, z1), p101(x0, y0, z0), p111(x0, y1, z0), p110(x0, y1, z1);
                    pushFace(*target, p100, p101, p111, p110, glm::vec3(-1, 0, 0), 0, 0, 1, 1, tid);
                }
                /* +Y top */
                if (faceVisible(b, worldAt(lx, ly + 1, lz))) {
                    glm::vec3 p010(x0, y1, z0), p011(x1, y1, z0), p111(x1, y1, z1), p110(x0, y1, z1);
                    pushFace(*target, p010, p011, p111, p110, glm::vec3(0, 1, 0), 0, 0, 1, 1, tid);
                }
                /* -Y bottom */
                if (faceVisible(b, worldAt(lx, ly - 1, lz))) {
                    glm::vec3 p000(x0, y0, z0), p001(x1, y0, z0), p011(x1, y0, z1), p010(x0, y0, z1);
                    pushFace(*target, p000, p001, p011, p010, glm::vec3(0, -1, 0), 0, 0, 1, 1, tid);
                }
                /* +Z */
                if (faceVisible(b, worldAt(lx, ly, lz + 1))) {
                    glm::vec3 p001(x1, y0, z1), p000(x0, y0, z1), p010(x0, y1, z1), p011(x1, y1, z1);
                    pushFace(*target, p001, p000, p010, p011, glm::vec3(0, 0, 1), 0, 0, 1, 1, tid);
                }
                /* -Z */
                if (faceVisible(b, worldAt(lx, ly, lz - 1))) {
                    glm::vec3 p100(x0, y0, z0), p101(x1, y0, z0), p111(x1, y1, z0), p110(x0, y1, z0);
                    pushFace(*target, p100, p101, p111, p110, glm::vec3(0, 0, -1), 0, 0, 1, 1, tid);
                }
            }
        }
    }
    meshRebuildPending_ = false;
    gpuUploadPending_ = true;
}

void Chunk::uploadIfDirty() {
    if (!gpuUploadPending_)
        return;

    auto uploadStream = [](unsigned int& vao, unsigned int& vbo, const std::vector<float>& v, int& count) {
        count = static_cast<int>(v.size() / 9);
        if (count <= 0) {
            if (vbo) {
                glDeleteBuffers(1, &vbo);
                vbo = 0;
            }
            if (vao) {
                glDeleteVertexArrays(1, &vao);
                vao = 0;
            }
            return;
        }
        const GLsizeiptr bytes = static_cast<GLsizeiptr>(v.size() * sizeof(float));
        if (!vao) {
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, bytes, v.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(8 * sizeof(float)));
        } else {
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, bytes, v.data(), GL_STATIC_DRAW);
        }
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    };

    uploadStream(opaqueVao_, opaqueVbo_, opaqueVerts_, opaqueCount_);
    uploadStream(glassVao_, glassVbo_, glassVerts_, glassCount_);
    uploadStream(waterVao_, waterVbo_, waterVerts_, waterCount_);

    gpuUploadPending_ = false;
    /* 顶点已在 GPU，释放 CPU 侧副本以节省进程内存（下次 rebuild 会再 reserve） */
    clearCpuMesh();
    opaqueVerts_.shrink_to_fit();
    glassVerts_.shrink_to_fit();
    waterVerts_.shrink_to_fit();
}

void Chunk::drawOpaque() const {
    if (opaqueCount_ <= 0 || opaqueVao_ == 0)
        return;
    glBindVertexArray(opaqueVao_);
    glDrawArrays(GL_TRIANGLES, 0, opaqueCount_);
    glBindVertexArray(0);
}

void Chunk::drawGlass() const {
    if (glassCount_ <= 0 || glassVao_ == 0)
        return;
    glBindVertexArray(glassVao_);
    glDrawArrays(GL_TRIANGLES, 0, glassCount_);
    glBindVertexArray(0);
}

void Chunk::drawWater() const {
    if (waterCount_ <= 0 || waterVao_ == 0)
        return;
    glBindVertexArray(waterVao_);
    glDrawArrays(GL_TRIANGLES, 0, waterCount_);
    glBindVertexArray(0);
}

} // namespace voxel
