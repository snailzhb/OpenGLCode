#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace meshcraft {

/** 程序化高度场地形：河流、湿润带、独立水面网格 */
class TerrainMesh {
public:
    static constexpr int kDefaultResolution = 256;
    static constexpr float kCellSize = 1.0f;

    bool build(int resolution = kDefaultResolution, uint32_t seed = 42);
    void uploadGpu();
    void shutdownGpu();

    void draw() const;
    void drawWater() const;

    int resolution() const { return res_; }
    float cellSize() const { return kCellSize; }
    glm::vec2 originXZ() const { return origin_; }
    float sizeXZ() const { return static_cast<float>(res_) * kCellSize; }
    float waterLevel() const { return waterLevel_; }

    float sampleHeight(float worldX, float worldZ) const;
    float sampleWetness(float worldX, float worldZ) const;
    /** 河道核心掩码 [0,1]，不含远距离河岸湿润 */
    float sampleRiverMask(float worldX, float worldZ) const;
    bool sampleIsWater(float worldX, float worldZ) const;
    glm::vec3 sampleNormal(float worldX, float worldZ) const;

    bool raycast(const glm::vec3& origin, const glm::vec3& dir, float& maxDist, glm::vec3& hitWorld,
                 glm::vec3& hitNormal) const;

private:
    int heightIndex(int ix, int iz) const;
    float heightAtGrid(int ix, int iz) const;
    float wetnessAtGrid(int ix, int iz) const;
    float riverMaskAtGrid(int ix, int iz) const;
    void generateHeights(uint32_t seed);
    void carveRivers(uint32_t seed);
    void buildTerrainMesh();
    void buildWaterMesh();

    int res_ = 0;
    glm::vec2 origin_{0.0f};
    float waterLevel_ = 5.2f;
    std::vector<float> heights_;
    std::vector<float> wetness_;
    std::vector<float> riverMask_;
    std::vector<float> vertices_;
    int indexCount_ = 0;

    std::vector<float> waterVertices_;
    int waterIndexCount_ = 0;

    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    unsigned int waterVao_ = 0;
    unsigned int waterVbo_ = 0;
    unsigned int waterEbo_ = 0;
};

} // namespace meshcraft
