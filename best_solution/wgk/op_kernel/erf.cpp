// Kernel侧核函数实现
#include "kernel_operator.h"

#include "erf_tiling.h"
#include "tiling_key_erf.h"

constexpr uint32_t TILE_LENGTH = 8192;
constexpr uint32_t TILE_LENGTH_SMALL = 8192;

constexpr uint32_t BUFFER_NUM = 2;
constexpr uint32_t BLOCK_ELEMENT_NUM = 8;

constexpr float ERF_CLIP_BOUND = 2.79f;
constexpr uint32_t SMALL_LENGTH_THRESHOLD = 8192;

// ============================================================
// Small NoQueue Kernel: length <= 8192
// 使用 TBuf + 手动 MTE2_V / V_MTE3 同步
// ============================================================
template <class DT_X>
class KernelErfSmallNoQueue {
public:
    __aicore__ inline KernelErfSmallNoQueue() {}

    __aicore__ inline void Init(GM_ADDR x, GM_ADDR y, uint32_t length, AscendC::TPipe* pipeIn)
    {
        pipe = pipeIn;
        this->length = length;

        xGm.SetGlobalBuffer((__gm__ DT_X*)x, length);
        yGm.SetGlobalBuffer((__gm__ DT_X*)y, length);

        // NoQueue：直接从 TBuf 取 LocalTensor，不走 Alloc/EnQue/DeQue
        pipe->InitBuffer(xBuf, TILE_LENGTH_SMALL * sizeof(DT_X));
        pipe->InitBuffer(yBuf, TILE_LENGTH_SMALL * sizeof(DT_X));
        pipe->InitBuffer(tmpBuf1, TILE_LENGTH_SMALL * sizeof(float));
        pipe->InitBuffer(tmpBuf2, TILE_LENGTH_SMALL * sizeof(float));
    }

    __aicore__ inline void Process()
    {
        const uint32_t len = this->length;

        AscendC::LocalTensor<DT_X> xLocal = xBuf.Get<DT_X>();
        AscendC::LocalTensor<DT_X> yLocal = yBuf.Get<DT_X>();
        AscendC::LocalTensor<float> tmp1 = tmpBuf1.Get<float>();
        AscendC::LocalTensor<float> tmp2 = tmpBuf2.Get<float>();

        // ====================================================
        // 1. GM -> UB，走 MTE2
        // ====================================================
        if ((len % BLOCK_ELEMENT_NUM) == 0) {
            AscendC::DataCopy(xLocal, xGm[0], len);
        } else {
            AscendC::DataCopyExtParams copyParams = {
                1,
                static_cast<uint32_t>(len * sizeof(DT_X)),
                0,
                0,
                0
            };
            AscendC::DataCopyPadExtParams<DT_X> padParams = {false, 0, 0, 0};
            AscendC::DataCopyPad(xLocal, xGm[0], copyParams, padParams);
        }

        // MTE2 -> V：保证搬入完成后再进行 Vector 计算
        AscendC::TEventID eventIdMTE2ToV =
            pipe->FetchEventID<AscendC::HardEvent::MTE2_V>();
        AscendC::SetFlag<AscendC::HardEvent::MTE2_V>(eventIdMTE2ToV);
        AscendC::WaitFlag<AscendC::HardEvent::MTE2_V>(eventIdMTE2ToV);

        // ====================================================
        // 2. Vector 计算
        // erf(x) ~= clip(x) * P7(clip(x)^2)
        // ====================================================
        AscendC::Mins(yLocal, xLocal, ERF_CLIP_BOUND, len);
        AscendC::Maxs(yLocal, yLocal, -ERF_CLIP_BOUND, len);

        AscendC::Mul(tmp1, yLocal, yLocal, len);

        AscendC::Muls(tmp2, tmp1, -6.41006838e-07f, len);
        AscendC::Adds(tmp2, tmp2, 2.41356576e-05f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, -3.97179653e-04f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, 3.81664835e-03f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, -2.42977635e-02f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, 1.10419527e-01f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, -3.75276983e-01f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, 1.12837917f, len);

        AscendC::Mul(yLocal, yLocal, tmp2, len);

        // V -> MTE3：保证 Vector 写 yLocal 完成后再搬出
        AscendC::TEventID eventIdVToMTE3 =
            pipe->FetchEventID<AscendC::HardEvent::V_MTE3>();
        AscendC::SetFlag<AscendC::HardEvent::V_MTE3>(eventIdVToMTE3);
        AscendC::WaitFlag<AscendC::HardEvent::V_MTE3>(eventIdVToMTE3);

        // ====================================================
        // 3. UB -> GM，走 MTE3
        // ====================================================
        if ((len % BLOCK_ELEMENT_NUM) == 0) {
            AscendC::DataCopy(yGm[0], yLocal, len);
        } else {
            AscendC::DataCopyExtParams copyParams = {
                1,
                static_cast<uint32_t>(len * sizeof(DT_X)),
                0,
                0,
                0
            };
            AscendC::DataCopyPad(yGm[0], yLocal, copyParams);
        }
    }

private:
    AscendC::TPipe* pipe;

    AscendC::TBuf<AscendC::QuePosition::VECIN> xBuf;
    AscendC::TBuf<AscendC::QuePosition::VECOUT> yBuf;
    AscendC::TBuf<AscendC::QuePosition::VECCALC> tmpBuf1;
    AscendC::TBuf<AscendC::QuePosition::VECCALC> tmpBuf2;

    AscendC::GlobalTensor<DT_X> xGm;
    AscendC::GlobalTensor<DT_X> yGm;

    uint32_t length;
};

// ============================================================
// Large Kernel: length > 4096
// 保留 TQue + BUFFER_NUM=2 大核
// ============================================================
template <class DT_X>
class KernelErfLarge {
public:
    __aicore__ inline KernelErfLarge() {}

    __aicore__ inline void Init(GM_ADDR x, GM_ADDR y, uint32_t length, AscendC::TPipe* pipeIn)
    {
        pipe = pipeIn;

        const uint32_t blockNum = AscendC::GetBlockNum();
        const uint32_t blockIdx = AscendC::GetBlockIdx();

        const uint32_t totalDataBlockNum = (length + BLOCK_ELEMENT_NUM - 1) / BLOCK_ELEMENT_NUM;
        const uint32_t baseBlockNum = totalDataBlockNum / blockNum;
        const uint32_t tailBlockNum = totalDataBlockNum % blockNum;

        const uint32_t coreBlockNum = baseBlockNum + (blockIdx < tailBlockNum ? 1U : 0U);
        const uint32_t coreBlockOffset =
            blockIdx * baseBlockNum + (blockIdx < tailBlockNum ? blockIdx : tailBlockNum);

        coreOffset = coreBlockOffset * BLOCK_ELEMENT_NUM;

        const uint32_t paddedCoreLength = coreBlockNum * BLOCK_ELEMENT_NUM;
        coreLength = coreOffset >= length
            ? 0U
            : (paddedCoreLength < length - coreOffset ? paddedCoreLength : length - coreOffset);

        tileNum = (coreLength + TILE_LENGTH - 1) / TILE_LENGTH;

        xGm.SetGlobalBuffer((__gm__ DT_X*)x + coreOffset, coreLength);
        yGm.SetGlobalBuffer((__gm__ DT_X*)y + coreOffset, coreLength);

        pipe->InitBuffer(inQueueX, BUFFER_NUM, TILE_LENGTH * sizeof(DT_X));
        pipe->InitBuffer(outQueueY, BUFFER_NUM, TILE_LENGTH * sizeof(DT_X));
        pipe->InitBuffer(tmpBuf1, TILE_LENGTH * sizeof(float));
        pipe->InitBuffer(tmpBuf2, TILE_LENGTH * sizeof(float));
    }

    __aicore__ inline void Process()
    {
        for (uint32_t i = 0; i < tileNum; ++i) {
            uint32_t len = TILE_LENGTH;
            if (i == tileNum - 1) {
                len = coreLength - i * TILE_LENGTH;
            }

            CopyIn(i, len);
            Compute(len);
            CopyOut(i, len);
        }
    }

private:
    __aicore__ inline void CopyIn(uint32_t progress, uint32_t len)
    {
        AscendC::LocalTensor<DT_X> xLocal = inQueueX.AllocTensor<DT_X>();

        if ((len % BLOCK_ELEMENT_NUM) == 0) {
            AscendC::DataCopy(xLocal, xGm[progress * TILE_LENGTH], len);
        } else {
            AscendC::DataCopyExtParams copyParams = {
                1,
                static_cast<uint32_t>(len * sizeof(DT_X)),
                0,
                0,
                0
            };
            AscendC::DataCopyPadExtParams<DT_X> padParams = {false, 0, 0, 0};
            AscendC::DataCopyPad(xLocal, xGm[progress * TILE_LENGTH], copyParams, padParams);
        }

        inQueueX.EnQue(xLocal);
    }

    __aicore__ inline void Compute(uint32_t len)
    {
        AscendC::LocalTensor<DT_X> xLocal = inQueueX.DeQue<DT_X>();
        AscendC::LocalTensor<DT_X> yLocal = outQueueY.AllocTensor<DT_X>();

        AscendC::LocalTensor<float> tmp1 = tmpBuf1.Get<float>();
        AscendC::LocalTensor<float> tmp2 = tmpBuf2.Get<float>();

        AscendC::Mins(yLocal, xLocal, ERF_CLIP_BOUND, len);
        AscendC::Maxs(yLocal, yLocal, -ERF_CLIP_BOUND, len);

        AscendC::Mul(tmp1, yLocal, yLocal, len);

        AscendC::Muls(tmp2, tmp1, -6.41006838e-07f, len);
        AscendC::Adds(tmp2, tmp2, 2.41356576e-05f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, -3.97179653e-04f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, 3.81664835e-03f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, -2.42977635e-02f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, 1.10419527e-01f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, -3.75276983e-01f, len);

        AscendC::Mul(tmp2, tmp2, tmp1, len);
        AscendC::Adds(tmp2, tmp2, 1.12837917f, len);

        AscendC::Mul(yLocal, yLocal, tmp2, len);

        outQueueY.EnQue(yLocal);
        inQueueX.FreeTensor(xLocal);
    }

    __aicore__ inline void CopyOut(uint32_t progress, uint32_t len)
    {
        AscendC::LocalTensor<DT_X> yLocal = outQueueY.DeQue<DT_X>();

        if ((len % BLOCK_ELEMENT_NUM) == 0) {
            AscendC::DataCopy(yGm[progress * TILE_LENGTH], yLocal, len);
        } else {
            AscendC::DataCopyExtParams copyParams = {
                1,
                static_cast<uint32_t>(len * sizeof(DT_X)),
                0,
                0,
                0
            };
            AscendC::DataCopyPad(yGm[progress * TILE_LENGTH], yLocal, copyParams);
        }

        outQueueY.FreeTensor(yLocal);
    }

private:
    AscendC::TPipe* pipe;

    AscendC::TQue<AscendC::QuePosition::VECIN, BUFFER_NUM> inQueueX;
    AscendC::TQue<AscendC::QuePosition::VECOUT, BUFFER_NUM> outQueueY;

    AscendC::TBuf<AscendC::QuePosition::VECCALC> tmpBuf1;
    AscendC::TBuf<AscendC::QuePosition::VECCALC> tmpBuf2;

    AscendC::GlobalTensor<DT_X> xGm;
    AscendC::GlobalTensor<DT_X> yGm;

    uint32_t coreOffset;
    uint32_t coreLength;
    uint32_t tileNum;
};

// ============================================================
// Kernel Entry
// ============================================================
template <typename DT_X>
__global__ __aicore__ void erf(GM_ADDR x, GM_ADDR y, GM_ADDR workspace, GM_ADDR tiling)
{
    REGISTER_TILING_DEFAULT(ErfTilingData);
    GET_TILING_DATA_WITH_STRUCT(ErfTilingData, tiling_data, tiling);

    AscendC::TPipe pipe;

    if (tiling_data.length <= SMALL_LENGTH_THRESHOLD) {
        KernelErfSmallNoQueue<DT_X> op;
        op.Init(x, y, tiling_data.length, &pipe);
        op.Process();
    } else {
        KernelErfLarge<DT_X> op;
        op.Init(x, y, tiling_data.length, &pipe);
        op.Process();
    }
}