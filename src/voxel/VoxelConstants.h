#pragma once

namespace voxel {

inline constexpr int kChunkShift = 4;
inline constexpr int kChunkSize = 1 << kChunkShift; // 16
inline constexpr int kChunkMask = kChunkSize - 1;

/** 世界 Y 方向 chunk 索引范围 [kChunkYMin, kChunkYMax]（含） */
inline constexpr int kChunkYMin = -4;
inline constexpr int kChunkYMax = 5;

inline constexpr float kBlockSize = 1.0f;
inline constexpr float kSeaLevel = 35.0f;

/** 主相机绘制 chunk 的最大距离（世界单位，到 chunk AABB 中心） */
inline constexpr float kChunkDrawMaxDist = 380.0f;

inline constexpr int kWindowW = 1280;
inline constexpr int kWindowH = 720;

/** 运行时每帧 mesh 重建 / GPU 上传的 chunk 数上限（另受 CPU 时间预算约束） */
inline constexpr int kMeshRebuildBudgetPerFrame = 32;
inline constexpr int kMeshUploadBudgetPerFrame = 40;

/** 每帧用于 CPU 体素 mesh 重建的最长时间（秒），避免单帧尖峰 */
inline constexpr float kMeshRebuildCpuBudgetSec = 0.0028f;
/** 每帧用于 glBufferData 上传的最长时间（秒） */
inline constexpr float kMeshUploadCpuBudgetSec = 0.0022f;

/** 流式区域每帧实际创建的 chunk 数下限；过远队列积压时会自适应提高 */
inline constexpr int kChunkCreateBudgetPerFrame = 5;

/** 水平方向卸载：保留相机 chunk 周围「流式半径 + 该边距」的 Chebyshev 距离内 chunk，以外整列删除 */
inline constexpr int kChunkPruneMarginChunks = 2;

} // namespace voxel
