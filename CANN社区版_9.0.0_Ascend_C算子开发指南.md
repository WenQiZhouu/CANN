# 3 算子实践参考

本文档组织结构

异构计算

SIMD算子实现

SIMT算子实现

SIMD与SIMT混合算子实现

功能调试

性能分析

SIMD算子性能优化

SIMD与SIMT混合算子性能优化

优秀实践

# 3.1 本文档组织结构

# 需要具备的知识

本文档主要用于指导开发者基于Ascend C编程语言在昇腾AI处理器上开发高性能的算子。在阅读此文档时，需要开发者具备如下能力：

熟练使用C++编程语言。

了解计算机架构。

理解昇腾AI处理器硬件架构。

完成Ascend C编程相关文档、课程学习。

可以完成Ascend C开发、调试环境搭建。

可以独立完成Ascend C算子的开发。

熟练使用性能分析工具获取性能数据。

您可以通过LINK获取如上内容的学习资料。

# 可以解决的问题

开发者完成Ascend C算子（本文后续提到的算子均指使用Ascend C开发的算子）的开发后，如果需要对算子性能进行进一步优化，那么通过阅读本文档可以得到有效帮助。

本文档首先介绍异构计算的硬件特点及运行的数据交互，然后介绍使用Ascend C编程时调试调优的思路以及各种性能优化手段，最后介绍具体的性能优化案例。

优化算子性能是一个持续迭代的流程，将如下步骤不断循环迭代，直至达成性能目标。在优秀实践章节，开发者可以进一步了解基于以下四个步骤的具体实践。


图 3-1 算子性能优化流程


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/fc88a8349a94b8a2c5998765bb0172fd400649773377268bfd273e47323b45c9.jpg)


本文档分为如下几个章节，各章的内容以及目标如下：

异构计算：介绍算子在硬件上的部署以及运行的数据流。目的是让开发者在宏观上了解硬件架构上可能影响算子执行性能的流程。

SIMD算子实现：介绍矢量编程、矩阵编程、融合算子编程三种典型场景下的算子Tiling、Kernel实现，是对Ascend C编程范式的具体应用。

SIMT算子实现：介绍使用SIMT API进行算子开发的参考案例。

SIMD与SIMT混合算子实现：介绍使用Reg矢量计算API和SIMT API进行SIMD与SIMT混合编程的算子开发参考案例。

功能调试：介绍部分比较常见的影响算子功能的场景。目的是让开发者可以快速解决功能问题，便于进行性能优化，以及快速解决在性能优化过程中可能出现的功能问题。

性能分析：介绍了算子性能数据分析的方向。目的是让开发者能够通过分析性能数据，识别性能的优化方向。算子性能数据的测试方法，以及性能工具使用方式的详细说明请参考《性能调优工具》，本章不做赘述。

SIMD算子性能优化：介绍性能优化的手段。目的是让开发者结合算子性能瓶颈点，开展性能优化工作。将主要的优化建议分成搬运优化、内存优化、API使用优化、流水优化以及Tiling优化。有一些优化建议是对上述分类的综合体现，则写在了相关性比较大的章节。这些建议按优先级进行分类，优先级是综合考虑性能效果及其范围来设定：为大多数Ascend C算子带来性能收益的建议具有最高优先

级，而仅影响特定情况的手段被给予较低优先级。开发者不必熟悉所有优化手段，可以根据分析得到的算子性能瓶颈，获取对应的优化手段，逐渐了解优化策略全貌。

SIMD与SIMT混合算子性能优化：介绍SIMD与SIMT混合编程场景的性能优化建议。包括内存访问优化、计算优化等。

优秀实践：介绍算子性能优化的优秀实践案例。目的是让开发者结合实例更深入的理解上文内容，同时参考其中的优化手段和优化思路，完成算子的性能优化，实现从理论到实践的过渡。

# 3.2 异构计算

使用Ascend C进行编程时，会涉及到在两个不同的平台（Host、Device）上开发代码。本章简单介绍Host、Device之间的差异，便于开发者宏观地了解这个异构系统；同时给出算子相关的数据流，结合异构架构的特点，开发者可以进一步了解应该如何合理安排算子代码的执行位置，便于得到更好的性能。

# Host 侧 CPU 和 Device 侧 NPU 的主要区别

# 不同的硬件资源

CPU是为了执行通用计算任务而设计的，但在处理大量的并行计算（如矩阵乘、批数据处理）时效率不高。NPU是为了加速机器学习和深度学习任务而设计的，它擅长执行大量的并行计算。NPU包含了大量的专用硬件，比如：支持矩阵计算的Cube单元，NPU中一个核可以支持一个时钟周期内完成数据量为16 * 16 * 16、数据类型为float16的乘累加运算；支持向量计算的Vector单元，NPU中一个核可以支持一个时钟周期内完成数据量为128 + 128、数据类型为float16的加法运算。

# 不同的物理内存空间

Host和Device的物理内存是分离的，通常需要在Host侧内存和Device侧内存之间进行数据交换。

# 如何合理安排算子代码

开发者进行Ascend C算子开发时，可以将Host和Device视为一个协同的异构系统，为每个处理单元分配其擅长的工作。在Host侧推荐执行非计算密集型任务，一般为标量计算任务。在Device侧推荐进行计算密集型任务，利用Device侧NPU的SIMD(SingleInstruction Multiple Data)指令可以高效的实现批量数据的矩阵运算、向量运算等。

Ascend C算子的实现主要包含两个部分：

# Host侧Tiling实现

由于NPU中AI Core内部存储无法完全容纳算子输入输出的所有数据，需要每次搬运一部分输入数据进行计算然后搬出，再搬运下一部分输入数据进行计算，这个过程就称之为Tiling。切分数据的算法称为Tiling算法或者Tiling策略。根据算子的shape等信息来确定数据切分算法相关参数（比如每次搬运的块大小，循环的总次数）的计算程序，称之为Tiling实现，也叫Tiling函数（Tiling Function）。由于Tiling实现中均为标量计算，AI Core并不擅长，所以我们将其独立出来放在Host侧CPU上执行。

# Device侧Kernel实现

Kernel实现即算子核函数实现，在Kernel函数内部通过解析Host侧传入的Tiling结构体获取Tiling信息，根据Tiling信息控制数据搬入搬出Local Memory的流程；通过调用计算、数据搬运、内存管理、任务同步等API实现算子逻辑。其核心逻辑基本上都为计算密集型任务，适合在Device侧NPU上执行。

# 算子数据流

算子执行过程中涉及到Host和Device的数据交换。这里仅针对Tiling参数的传递，给出具体的数据流：Host侧Tiling算法根据算子具体输入输出的信息，完成Tiling参数的计算，并存放在Tiling结构体中；将Host侧的Tiling结构体发送到Device侧，Device侧的算子获取并解析Tiling结构体，基于该信息执行后续的算子计算逻辑。


图 3-2 算子 Tiling 传递数据流


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/0df055897a654c1c14678077ba2112da4bd38b974717ae09e47edf4abf8e1ff1.jpg)


# 3.3 SIMD 算子实现

# 3.3.1 概述

Ascend C的算子实现主要包含两个部分：

Host侧Tiling实现

由于NPU中AI Core内部存储无法完全容纳算子输入输出的所有数据，需要每次搬运一部分输入数据进行计算然后搬出，再搬运下一部分输入数据进行计算，这个过程就称之为Tiling。切分数据的算法称为Tiling算法或者Tiling策略。根据算子的shape等信息来确定数据切分算法相关参数（比如每次搬运的块大小，以及总共循环多少次）的计算程序，称之为Tiling实现，也叫Tiling函数（Tiling

Function）。由于Tiling实现中完成的均为标量计算，AI Core并不擅长，所以我们将其独立出来放在Host侧CPU上执行。

Device侧Kernel实现

Kernel实现即算子核函数实现，在Kernel函数内部通过解析Host侧传入的Tiling结构体获取Tiling信息，根据Tiling信息控制数据搬入搬出Local Memory的流程；通过调用计算、数据搬运、内存管理、任务同步API，实现算子逻辑。其核心逻辑基本上都为计算密集型任务，需要在NPU上执行。

本章介绍矢量编程、矩阵编程、融合算子编程三种典型场景下的算子Tiling、Kernel实现，是对上文中典型编程范式的具体应用，同时也介绍了编程的更多细节、API的使用方法等。

# 3.3.2 矢量编程

# 3.3.2.1 概述

本节将以Add算子为例，带您快速构建Ascend C矢量算子程序，并学习矢量算子开发的典型场景以及处理方式。涉及的场景包括：

基础矢量算子：开发一个简单的Add矢量算子。

. TBuf的使用：在算子计算过程中使用临时空间存储运算的中间结果。

多核Tiling：算子在AI处理器的多个核上运行，所有核的计算数据量相等且32字节对齐。

尾块Tiling：算子在AI处理器的多个核上运行，所有核的计算数据量相等，每个核上除最后一个数据块（尾块）外，其余数据块的数据量相等，每个核都需要处理尾块数据的计算。

尾核Tiling：算子在AI处理器的多个核上运行，数据无法平均分配到每个核。将所有核分为多个整核和多个尾核，整核的计算数据量相等，尾核的计算数据量相等。

尾核&尾块：算子在AI处理器的多个核上运行，数据无法平均分配到每个核，同时每个核内的数据无法均分，除最后一个数据块（尾块）外，其余数据块的数据量相等，每个核都需要单独处理尾块数据的计算。

DoubleBuffer场景：使能double buffer，算子中的多条流水并行执行。

Broadcast场景：算子中两个输入的shape（形状）不相等，需要将一个输入的shape进行Broadcast（广播）后，再执行计算。

非对齐场景：更多数据非32字节对齐场景的处理方案。

# 须知

进行数据搬运和Vector计算时，对于搬运的数据长度和操作数的起始地址有如下的对齐要求：

● 使用DataCopy接口进行数据搬运，搬运的数据长度和操作数的起始地址（UnifiedBuffer上）必须保证32字节对齐。

● 通常情况下，进行Vector计算时，操作数的起始地址必须保证32字节对齐，执行计算的基本单位为32字节。

# 3.3.2.2 基础矢量算子

基于Ascend C方式实现基础矢量算子核函数的流程如下图所示。


图3-3 矢量算子核函数实现流程


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/750e13e189920b32591d3e5ee1e49fb8632cc5f94aaef5fa165da7a0a6c112ce.jpg)


算子分析：分析算子的数学表达式、输入、输出以及计算逻辑的实现，明确需要调用的Ascend C接口。

核函数定义：定义Ascend C算子入口函数。

根据矢量编程范式实现算子类：完成核函数的内部实现，包括3个基本任务：CopyIn，Compute，CopyOut。

下文以输入为half数据类型且shape的最后一维为32Bytes对齐、在单核上运行的、一次完成计算的Add算子为例，对上述步骤进行详细介绍。本样例中介绍的算子完整代码请参见基础Add算子样例。

# 算子分析

算子分析具体步骤如下：

步骤1 明确算子的数学表达式及计算逻辑。

Add算子的数学表达式为：

$$
z = x + y
$$

计算逻辑是：Ascend C提供的矢量计算接口的操作元素都为LocalTensor，输入数据需要先从外部存储（Global Memory）搬运进片上存储（Unified Buffer），然后使用计算接口完成两个输入参数相加，得到最终结果，再搬出到外部存储上。Ascend CAdd算子的计算逻辑如下图所示。


图 3-4 算子计算逻辑


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/9877a924ab3458c5684ecd7eee93416283d8e6486654d244782b8d5e017b4c98.jpg)


步骤2 明确输入和输出。

Add算子有两个输入：x与y；输出为z。

本样例中算子的输入支持的数据类型为half（float16），算子输出的数据类型与输入的数据类型相同。

. 算子输入支持的shape为（1，2048），输出shape与输入shape相同。

算子输入支持的format为：ND。

步骤3 确定核函数名称和参数。

● 您可以自定义核函数名称，本样例中核函数命名为vec_add_custom。

根据对算子输入输出的分析，确定核函数有3个参数x，y，z；x，y为输入在Global Memory上的内存地址，z为输出在Global Memory上的内存地址。

步骤4 确定算子实现所需接口。

实现涉及外部存储和内部存储间的数据搬运，查看Ascend C API参考中的数据搬运接口，需要使用DataCopy来实现数据搬运。

本样例只涉及矢量计算的加法操作，查看Ascend C API参考中的矢量计算接口，初步分析可使用基础算术Add接口Add实现x+y。

使用Queue队列管理计算中使用的Tensor数据结构，具体使用EnQue、DeQue等接口。

----结束

通过以上分析，得到Ascend C Add算子的设计规格如下：

算子类型（OpType）：Add

算子输入输出：


表 3-1 Add算子输入输出规格


<table><tr><td>name</td><td>shape</td><td>data type</td><td>format</td></tr><tr><td>x(输入)</td><td>(1, 2048)</td><td>half</td><td>ND</td></tr><tr><td>y(输入)</td><td>(1, 2048)</td><td>half</td><td>ND</td></tr><tr><td>z(输出)</td><td>(1, 2048)</td><td>half</td><td>ND</td></tr></table>

核函数名称：vec_add_custom

使用的主要接口：

DataCopy：数据搬移接口

Add：矢量基础算术接口

EnQue、DeQue等接口：Queue队列管理接口

算子实现文件名称：vector_add.asc

# 核函数定义

根据核函数中介绍的规则进行核函数的定义。

# 步骤1 函数原型定义

本样例中，函数名为vector_add_custom（核函数名称可自定义），根据算子分析中对算子输入输出的分析，确定有3个参数x，y，z，其中x，y为输入内存，z为输出内存。根据核函数的规则介绍，函数原型定义如下所示：使用__global__函数类型限定符标识它是一个核函数，可以被<<<>>>调用；使用__vector__函数类型限定符标识该核函数在设备端aicore的Vector Core上执行；为方便起见，统一使用GM_ADDR宏修饰入参，GM_ADDR宏定义请参考核函数。

```txt
__global__ __vector__ void vector_add_custom(GM_ADDR x, GM_ADDR y, GM_ADDR z)
{
} 
```

步骤2 调用算子类的Init和Process函数。

算子类的Init函数，完成内存初始化相关工作，Process函数完成算子实现的核心逻辑，具体介绍参见算子类实现。

```cpp
__global__ __vector__ void vector_add_custom(GM_ADDR x, GM_ADDR y, GM_ADDR z)
{
    AscendC::TPipe pipe;
    KernelAdd op;
    op.Init(x, y, z, &pipe);
    op.Process();
} 
```

步骤3 根据核函数定义和调用章节，调用核函数时，除了需要传入参数x，y，z，还需要传入numBlocks（核函数执行的核数），nullptr（保留参数，设置为nullptr），stream（应用程序中维护异步操作执行顺序的stream）来规定核函数的执行配置。

```txt
vector_add_custom<<<numBlocks, nullptr, stream>>>(xDevice, yDevice, zDevice); 
```

----结束

# 算子类实现

根据上一节介绍，核函数中会调用算子类的Init和Process函数，本节具体讲解如何基于编程范式实现算子类。

根据矢量编程范式对Add算子的实现流程进行设计的思路如下，矢量编程范式请参考矢量编程范式，设计完成后得到的Add算子实现流程图参见图3 Add算子实现流程：

Add算子的实现流程分为3个基本任务：CopyIn，Compute，CopyOut。CopyIn任务负责将Global Memory上的输入Tensor xGm和yGm搬运至Local Memory，分别存储在xLocal，yLocal，Compute任务负责对xLocal，yLocal执行加法操作，计算结果存储在zLocal中，CopyOut任务负责将输出数据从zLocal搬运至GlobalMemory上的输出Tensor zGm中。

CopyIn，Compute任务间通过VECIN队列inQueueX，inQueueY进行同步，Compute，CopyOut任务间通过VECOUT队列outQueueZ进行同步。

任务间交互使用到的内存、临时变量的内存统一使用Pipe内存管理对象进行管理。


图 3-5 Add 算子实现流程


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/0c7101a04af3fb9d4f0269c7834c023b65b6a58918056e047c3e91b91e98d270.jpg)


算子类中主要实现上述流程，包含对外开放的初始化Init函数和核心处理函数Process，Process函数中会对上图中的三个基本任务进行调用；同时包括一些算子实现中会用到的私有成员，比如上图中的GlobalTensor（xGm、yGm、zGm）和VECIN、VECOUT队列等。KernelAdd算子类具体成员如下：

```cpp
class KernelAdd {
public:
    __aicore__ inline KernelAdd() {}
    // 初始化函数，完成内存初始化相关操作
    __aicore__ inline void Init(GM_ADDR x, GM_ADDR y, GM_ADDR z, AscendC::TPipe* pipeIn) {}
    // 核心处理函数，实现算子逻辑，调用私有成员函数CopyIn、Compute、CopyOut完成矢量算子的三级流水操作
    __aicore__ inline void Process() {}

private:
    // 搬入函数，完成CopyIn阶段的处理，被核心Process函数调用
    __aicore__ inline void CopyIn() {}
    // 计算函数，完成Compute阶段的处理，被核心Process函数调用
    __aicore__ inline void Compute() {}
    // 搬出函数，完成CopyOut阶段的处理，被核心Process函数调用
    __aicore__ inline void CopyOut() {}

private:
    AscendC::TPipe* pipe; // Pipe内存管理对象
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQueueX; // 输入数据Queue队列管理对象，TPosition为VECIN
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQueueY; // 输入数据Queue队列管理对象，TPosition为VECIN
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQueueZ; // 输出数据Queue队列管理对象，TPosition为VECOUT
    AscendC::GlobalTensor<half> xGm; // 管理输入输出Global Memory内存地址的对象，其中xGm, yGm为输入，zGm为输出
    AscendC::GlobalTensor<half> yGm;
```

```cpp
AscendC::GlobalTensor<half> zGm;
}; 
```

初始化函数主要完成以下内容：

设置输入输出Global Tensor的Global Memory内存地址。本样例中的分配方案是：数据整体长度TOTAL_LENGTH为1 * 2048，使用GlobalTensor类的SetGlobalBuffer接口设定该核上Global Memory的起始地址以及长度。

```txt
xGm.SetGlobalBuffer((__gm__ half *)x, TOTAL_LENGTH); 
```

通过Pipe内存管理对象为输入输出Queue分配内存。比如，为输入x的Queue分配内存，可以通过如下代码段实现：

```txt
pipe->InitBuffer(inQueueX, 1, TOTAL_LENGTH * sizeof(half)) 
```

具体的初始化函数代码如下：

```cpp
constexpr int32_t TOTAL_LENGTH = 1 * 2048; // 数据总长
__aicore__ inline void Init(GM_ADDR x, GM_ADDR y, GM_ADDR z, AscendC::TPipe* pipeIn)
{
    pipe = pipeIn;
    // 设置Global Memory的起始地址以及长度
    xGm.SetGlobalBuffer((__gm__ half*)x, TOTAL_LENGTH);
    yGm.SetGlobalBuffer((__gm__ half*)y, TOTAL_LENGTH);
    zGm.SetGlobalBuffer((__gm__ half*)z, TOTAL_LENGTH);

    // 通过Pipe内存管理对象为输入输出Queue分配内存
    pipe->InitBuffer(inQueueX, 1, TOTAL_LENGTH * sizeof(half));
    pipe->InitBuffer(inQueueY, 1, TOTAL_LENGTH * sizeof(half));
    pipe->InitBuffer(outQueueZ, 1, TOTAL_LENGTH * sizeof(half));
}
```

基于矢量编程范式，将核函数的实现分为3个基本任务：CopyIn，Compute，CopyOut。Process函数中通过如下方式调用这三个函数。

```cpp
__aicore__ inline void Process()
{
    CopyIn();
    Compute();
    CopyOut();
} 
```

根据编程范式上面的算法分析，将整个计算拆分成三个Stage，用户单独编写每个Stage的代码，三阶段流程示意图参见图3-5，具体流程如下：

步骤1 Stage1：CopyIn函数实现。

1. 使用DataCopy接口将GlobalTensor数据拷贝到LocalTensor。

2. 使用EnQue将LocalTensor放入VECIN的Queue中。

```cpp
__aicore__ inline void CopyIn()
{
    // 从Que中为LocalTensor分配内存
    AscendC::LocalTensor<half> xLocal = inQueueX.AllocTensor<half>();
    AscendC::LocalTensor<half> yLocal = inQueueY.AllocTensor<half>();
    // 将GlobalTensor数据拷贝到LocalTensor
    AscendC::DataCopy(xLocal, xGm, TOTAL_LENGTH);
    AscendC::DataCopy(yLocal, yGm, TOTAL_LENGTH);
    // LocalTensor放入VECIN的Queue中
    inQueueX.EnQue(xLocal);
    inQueueY.EnQue(yLocal);
}
```

步骤2 Stage2：Compute函数实现。

1. 使用DeQue从VECIN中取出LocalTensor。

2. 使用Ascend C接口Add完成矢量计算。

3. 使用EnQue将计算结果LocalTensor放入到VECOUT的Queue中。

4. 使用FreeTensor释放不再使用的LocalTensor。

```cpp
__aicore__ inline void Compute()
{
    // 将Input从VECIN的Queue中取出
    AscendC::LocalTensor<half> xLocal = inQueueX.DeQue<half>();
    AscendC::LocalTensor<half> yLocal = inQueueY.DeQue<half>();
    AscendC::LocalTensor<half> zLocal = outQueueZ.AllocTensor<half>();
    // 调用Add算子进行计算
    AscendC::Add(zLocal, xLocal, yLocal, TOTAL_LENGTH);
    // 将计算结果LocalTensor放入到VECOUT的Queue中
    outQueueZ.EnQue<half>(zLocal);
    // 释放LocalTensor
    inQueueX.FreeTensor(xLocal);
    inQueueY.FreeTensor(yLocal);
}
```

步骤3 Stage3：CopyOut函数实现。

1. 使用DeQue接口从VECOUT的Queue中取出LocalTensor。

2. 使用DataCopy接口将LocalTensor拷贝到GlobalTensor上。

3. 使用FreeTensor将不再使用的LocalTensor进行回收。

```cpp
__aicore__ inline void CopyOut()
{
    // 将计算结果从VECOUT的Queue中取出
    AscendC::LocalTensor<half> zLocal = outQueueZ.DeQue<half>();
    // 将计算结果从LocalTensor数据拷贝到GlobalTensor
    AscendC::DataCopy(zGm, zLocal, TOTAL_LENGTH);
    // 释放LocalTensor
    outQueueZ.FreeTensor(zLocal);
}
```

----结束

# 3.3.2.3 TBuf 的使用

在大多数算子开发时，核函数计算过程需要使用临时内存来存储运算的中间结果，这些中间结果以临时变量表示，临时变量占用的内存可以使用TBuf数据结构来管理，具体介绍请参考TBuf。下文将以输入的数据类型为bfloat16_t、在单核上运行的Add算子为例，介绍TBuf的使用方式。本样例中介绍的算子完整代码请参见使用临时内存的Add算子样例。

在Atlas A2 训练系列产品/Atlas 800I A2 推理产品上，Add接口不支持对数据类型bfloat16_t的源操作数进行求和计算。因此，需要先将算子输入的数据类型转换成Add接口支持的数据类型，再进行计算。为保证计算精度，调用Cast接口将输入bfloat16_t类型转换为float类型，再进行Add计算，并在计算结束后将float类型转换回bfloat16_t类型。

通过以上分析，得到Ascend C Add算子的设计规格如下：

算子类型（OpType）：Add

算子输入输出：


表3-2 Add算子输入输出规格


<table><tr><td>name</td><td>shape</td><td>data type</td><td>format</td></tr><tr><td>x(输入)</td><td>(1, 2048)</td><td>bfloat16_t</td><td>ND</td></tr><tr><td>y(输入)</td><td>(1, 2048)</td><td>bfloat16_t</td><td>ND</td></tr><tr><td>z(输出)</td><td>(1, 2048)</td><td>bfloat16_t</td><td>ND</td></tr></table>

核函数名称：tmp_buffer_custom

使用的主要接口：

DataCopy：数据搬移接口

Cast：矢量精度转换接口

Add：矢量基础算术接口

EnQue、DeQue等接口：Queue队列管理接口

算子实现文件名称：tmp_buffer.asc

# 算子类实现

该样例的CopyIn，CopyOut任务与基础矢量算子相同，Compute任务的具体流程如下图所示。


图 3-6 输入为 bfloat16_t 类型的 Add 计算流程


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/05cc633417a0fde10a131db10e42d5a69b72c704dc87d6bc1fbfac8828582221.jpg)


在Compute任务中，表示Cast转换结果、Add计算结果的临时变量均需要使用临时内存存储。与基础矢量算子实现的KernelAdd算子类相比，本样例新增两个TBuf类型的成员变量tmpBuf0、tmpBuf1，用于管理计算过程中使用的临时内存，代码如下。

```cpp
class KernelAdd {
public:
    __aicore__ inline KernelAdd() {}
    __aicore__ inline void Init(GM_ADDR x, GM_ADDR y, GM_ADDR z, AscendC::TPipe* pipeIn) {}
    __aicore__ inline void Process() {}
private:
    __aicore__ inline void CopyIn() {}
    __aicore__ inline void Compute() {}
    __aicore__ inline void CopyOut() {}
private:
    AscendC::TPipe* pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQueueX;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQueueY;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQueueZ;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmpBuf0;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmpBuf1;
    AscendC::GlobalTensor<bfloat16_t> xGm; 
```

```txt
AscendC::GlobalTensor<bfloat16_t> yGm;
AscendC::GlobalTensor<bfloat16_t> zGm;
}; 
```

初始化函数阶段除原有步骤外，需要调用InitBuffer接口为TBuf变量分配内存，具体的初始化函数代码如下：

```cpp
__aicore__ inline void Init(GM_ADDR x, GM_ADDR y, GM_ADDR z, AscendC::TPipe* pipeIn)
{
    pipe = pipeIn;
    xGm.SetGlobalBuffer((__gm__ bfloat16_t*)x, TOTAL_LENGTH);
    yGm.SetGlobalBuffer((__gm__ bfloat16_t*)y, TOTAL_LENGTH);
    zGm.SetGlobalBuffer((__gm__ bfloat16_t*)z, TOTAL_LENGTH);

    pipe->InitBuffer(inQueueX, 1, TOTAL_LENGTH * sizeof(bfloat16_t));
    pipe->InitBuffer(inQueueY, 1, TOTAL_LENGTH * sizeof(bfloat16_t));
    pipe->InitBuffer(outQueueZ, 1, TOTAL_LENGTH * sizeof(bfloat16_t));

    pipe->InitBuffer(tmpBuf0, TOTAL_LENGTH * sizeof(float));
    pipe->InitBuffer(tmpBuf1, TOTAL_LENGTH * sizeof(float));
} 
```

基于矢量编程范式，核函数需要实现3个基本任务：CopyIn，Compute，CopyOut。与基础矢量算子实现相同，Process函数按顺序调用CopyIn函数，Compute函数，CopyOut函数。其中，CopyIn函数，CopyOut函数与基础矢量算子的CopyIn函数、基础矢量算子的CopyOut函数的实现没有差异，此处不过多赘述。Compute函数的实现步骤如下：

1. 使用DeQue从VECIN的Queue中取出LocalTensor。

2. 使用TBuf.Get从TBuf上获取全部长度的Tensor作为临时内存。

3. 使用Cast接口将LocalTensor转换为float类型，并存入临时内存。

4. 使用Add接口完成矢量计算，将计算结果存入临时内存。

5. 使用Cast接口将临时内存中的计算结果转换为bfloat16_t类型。

6. 使用EnQue将bfloat16_t类型的结果LocalTensor放入VECOUT的Queue中。

7. 使用FreeTensor释放不再使用的LocalTensor。

```cpp
__aicore__ inline void Compute()
{
    AscendC::LocalTensor<bfloat16_t> xLocal = inQueueX.DeQue<bfloat16_t> ();
    AscendC::LocalTensor<bfloat16_t> yLocal = inQueueY.DeQue<bfloat16_t> ();
    AscendC::LocalTensor<bfloat16_t> zLocal = outQueueZ.AllocTensor<bfloat16_t> ();
    AscendC::LocalTensor<float> tmpTensor0 = tmpBuf0.Get<float>();
    AscendC::LocalTensor<float> tmpTensor1 = tmpBuf1.Get<float>();

    AscendC::Cast(tmpTensor0, xLocal, AscendC::RoundMode::CAST_NONE, TOTAL_LENGTH);
    AscendC::Cast(tmpTensor1, yLocal, AscendC::RoundMode::CAST_NONE, TOTAL_LENGTH);

    AscendC::Add(tmpTensor0, tmpTensor0, tmpTensor1, TOTAL_LENGTH);
    AscendC::Cast(zLocal, tmpTensor0, AscendC::RoundMode::CAST_RINT, TOTAL_LENGTH);

    outQueueZ.EnQue<bfloat16_t>(zLocal);
    inQueueX.FreeTensor(xLocal);
    inQueueY.FreeTensor(yLocal);
} 
```

# 3.3.2.4 多核&Tiling 切分

# 3.3.2.4.1 概述

Ascend C核函数是运行在一个核上的处理函数，上述介绍的基础矢量算子与TBuf的使用样例均为在单核上运行的算子，不涉及Host侧Tiling实现。矢量算子实现的组成如下图所示。

为了提高算子的执行效率，通常在算子中实现多核并行计算，即对输入数据进行切分，并将不同的数据块分配到不同的核上处理。此外，由于单个核上内部存储LocalMemory大小有限，存在无法一次完整地容纳算子的输入和输出数据的场景，因此需要每次搬运一部分输入进行计算然后搬出，再搬运下一部分输入进行计算，直到获得最终的完整结果，这个数据切分、分块计算的过程称之为Tiling。切分数据的算法称为Tiling算法或者Tiling策略。根据算子的shape等信息来确定数据切分算法相关参数（比如每次搬运的块大小，以及总共循环多少次）的计算程序，称之为Tiling实现，也叫Tiling函数（Tiling Function）。由于Tiling实现中完成的均为标量计算，AI Core并不擅长，所以我们将其独立出来放在Host侧CPU上执行。核函数内部通过解析Host侧传入的Tiling结构体获取Tiling信息，根据Tiling信息控制数据搬入、搬出Local Memory的流程；通过调用计算、数据搬运、内存管理、任务同步API，实现算子逻辑。


图 3-7 算子实现组成


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/256abd920eb3907e8f0de11574d23b4c24975e063901ca3c3589017707797724.jpg)


由于硬件限制，在对输入数据进行数据切分时应遵循以下几个原则：

1. 由于AI Core中Unified Buffer上的物理限制，要求Unified Buffer上的数据存储空间必须保持32字节对齐。

输入数据不满足32字节对齐时，需要取输入数据长度向上对齐到32字节的长度作为输入数据总长度。

进行Tiling有关计算时，以32字节为最小单位进行计算。

2. 尽可能最大利用Unified Buffer空间。

AI Core与外部存储交互时会产生性能开销，频繁的进行数据搬运会导致性能瓶颈，因此应尽可能充分利用Unified Buffer空间，减少从Global Memory上搬运数据的次数。

3. AI处理器包含多个AI Core，应该充分均衡利用多核计算能力，将计算均衡分配到多个AI Core上。

本章将基于以上原则对几种典型场景进行说明，完整代码请参见多核Add算子样例。


图 3-8 多核及 Tiling 示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/6541a6db967409292541c1059b1521fc6bc33eeb12d6f13c9ef542e3df69438e.jpg)


数据切分示意如上图所示，将长度为TOTAL_LENGTH的算子输入分配到多个核上进行计算，每个核上计算的数据长度为BLOCK_LENGTH。对于每个核的计算数据，基于Local Memory的大小进一步切分，切分数据块的个数为TILE_NUM，得到的每个数据块的长度为TILE_LENGTH。

根据每个核计算的数据量是否相同、核内每个数据块的数据量是否相同，切分策略可能会存在以下几种场景：

1. 核间均分，核内均分：每个核处理的数据量相同，核内每个数据块的数据量相同。在此场景中，通过多核Tiling将数据均匀分配到各个核上执行，每个核上每次计算的数据长度相同。

2. 核间均分，核内不均分：每个核处理的数据量相同，核内各数据块的数据量不完全相同。此场景基于多核Tiling，核内数据不能切分为多个数据量相同且32字节对齐的数据块，需要通过尾块Tiling处理尾块数据的计算。

3. 核间不均分，核内均分：每个核处理的数据量不同，核内每个数据块的数据量相同。在此场景中，通过尾核Tiling的处理解决数据无法在各核间均匀分配的问题。

4. 核间不均分，核内不均分：每个核处理的数据量不同，核内各数据块的数据量不完全相同。该场景下需要同时考虑尾核&尾块，处理多核间及核内数据的合理切分。

# 3.3.2.4.2 多核 Tiling

基于Ascend C方式实现带有Tiling的算子的开发流程如下图所示。


图 3-9 算子开发流程


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/629d23773fd87689d8140e70aa275718ef3d1eed3fcff743b3df8a4406f930dd.jpg)


# 算子分析

本样例为输入数据在核间均分、核内均分场景。本样例的Tiling策略为：数据整体长度TOTAL_LENGTH为8 * 2048，数据平均分配到8个核上运行，每个核上计算的数据长度BLOCK_LENGTH为2048，将单核上的数据切分成16块（此处切分成16块仅用来作为Tiling的样例，并不代表性能最佳，仅供参考），每块数据的长度TILE_LENGTH为128。数据切分示意如下图所示：


图3-10 数据切分示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/8505972d6db19c872f55039ae566f540b3ab1803404b5cb4629e595d0c817150.jpg)


通过以上分析，得到Ascend C Add算子的设计规格如下：

算子类型（OpType）：Add

算子输入输出：


表3-3 Add算子输入输出规格


<table><tr><td>name</td><td>shape</td><td>data type</td><td>format</td></tr><tr><td>x(输入)</td><td>(8, 2048)</td><td>half</td><td>ND</td></tr><tr><td>y(输入)</td><td>(8, 2048)</td><td>half</td><td>ND</td></tr><tr><td>z(输出)</td><td>(8, 2048)</td><td>half</td><td>ND</td></tr></table>

核函数名称：tiling_strategy_custom

使用的主要接口：

DataCopy：数据搬移接口

Add：矢量基础算术接口

EnQue、DeQue等接口：Queue队列管理接口

算子实现文件名称：tiling_strategy.asc

# Tiling 实现

前述场景中算子的输入和输出均为固定shape，然而在实际的算子开发场景中，这些信息是支持动态变化的，场景会更加灵活和复杂。动态shape场景下，输入的shape是未知的。一些与输入shape相关的变量（比如每次搬运的块大小等），需要通过Tiling计算出来，然后传递到kernel侧，kernel侧使用该参数进行后续的计算。

具体实现方式为：分析设计Tiling参数、定义Tiling结构体，在Host侧通过上下文获取输入输出的shape信息，根据shape信息，计算Tiling参数并设置到对应的Tiling结构体中；通过核函数入口参数将Tiling信息传入核函数，在核函数内通过解析Tiling结构体，获取并使用相关参数来实现核函数内部逻辑，详细介绍请参考Host侧tiling实现。本节将以上述分析中的切分策略为例，说明如何实现Tiling。

基于本节的切分策略，Tiling需要定义如下参数：

blockLength：每个核的计算数据长度；

tileNum：每个核需要计算的数据块个数；

tileLength：每个核内每个数据块的长度。

根据确定的Tiling参数，使用C++语法定义TilingData结构体，代码如下。

```txt
struct AddCustomTilingData {
    uint32_t blockLength;
    uint32_t tileNum;
    uint32_t tileLength;
    ...
} 
```

接下来完成Tiling参数的计算。由于每个核内数据被切分为16块，根据使用的核数和核内切分数，计算Tiling参数，并写入到Tiling结构体内。代码示例如下：

```c
constexpr int32_t NUM_BLOCKS = 8;    // 使用的核数
constexpr int32_t TILE_NUM = 16;    // 核内切分数量
void GenerateTilingData(uint8_t* tilingBuf, uint32_t numBlocks)
{
    uint32_t totalLength;
    // 此处省略如何获取数据总长TOTAL_LENGTH，可以根据具体情况实现。本章节仅介绍Tiling相关内容。
    AddCustomTilingData* tiling = reinterpret_cast<AddCustomTilingData *>(tilingBuf);
    uint32_t blockLength = TOTAL_LENGTH / numBlocks;
    uint32_t tileNum = TILE_NUM;
    uint32_t tileLength = blockLength / tileNum;

    tiling->blockLength = blockLength;
    tiling->tileNum = tileNum;
    tiling->tileLength = tileLength;
}
```

最后，在Host侧调用程序中，调用上述Tiling参数计算函数，计算出相关参数，然后传递到Kernel侧核函数。

```txt
constexpr int32_t NUM_BLOCKS = 8;
...
uint8_t *tiling = nullptr;
size_t tilingSize = sizeof(AddCustomTilingData);
GenerateTilingData(tiling, NUM_BLOCKS); // 调用tiling参数计算函数
...
tiling_strategy_custom<<<NUM_BLOCKS, nullptr, stream>>>(xDevice, yDevice, zDevice, *reinterpret_cast<AddCustomTilingData*>(tiling));
...
```

# 算子类实现

Kernel侧算子实现仍遵循矢量算子核函数实现流程，接下来重点介绍本场景中算子类实现的不同点。

设置输入输出Global Tensor的Global Memory内存地址。

由于本样例中将数据分配到了多个核上进行处理，每个核处理不同的数据，因此不同核要处理的数据在Global Memory上的地址不同，在初始化函数Init中，需要获取单核所需处理的输入输出在Global Memory上的内存偏移地址，并将该偏移地址设置到GlobalTensor中。

以获取输入x在Global Memory上的内存偏移地址为例，数据整体长度TOTAL_LENGTH为8 * 2048，平均分配到8个核上运行，每个核上处理的数据长度blockLength为2048，调用GetBlockIdx接口获取当前核的index，x +blockLength * GetBlockIdx()即为单核处理程序中x在Global Memory上的内存偏移地址，获取偏移地址后，使用GlobalTensor类的SetGlobalBuffer接口设定该核上Global Memory的起始地址以及长度，具体示意图请参考图3-11。代码如下所示：

```cpp
xGm.SetGlobalBuffer((__gm__ half *)x + this->blockLength * AscendC::GetBlockIdx(), this->blockLength); 
```


图 3-11 多核并行处理示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/8376ea344ae87a41e4076f67ffc9da097926c35bd860913e36b54dfa38786874.jpg)


通过Pipe内存管理对象为输入输出Queue分配内存。

对于单核上的处理数据，可以进行数据切块（Tiling），在本示例中，仅作为参考，将单核上的数据（2048个数）切分成16块（并不意味着16块就是性能最优），每块tileLength（128）个数据。数据切分示意图如图3-12所示。


图 3-12 单核数据切分示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/ed64c817ebb69200ff30d3728067f381d75b811c961898fb37597ab0d8cb1ca1.jpg)


与基础矢量算子相比，在通过Pipe内存管理对象为输入输出Queue分配内存时，需使用单核内每个数据块的长度tileLength作为分配内存的长度。比如，为输入x的Queue分配内存，可以通过如下代码段实现，Pipe为inQueueX分配了一块大小为tileLength * sizeof(half)个字节的内存块，每个内存块能容纳tileLength（128）个half类型数据。

```txt
pipe->InitBuffer(inQueueX, 1, this->tileLength * sizeof(half)) 
```


具体的初始化函数代码如下：


```cpp
__aicore__ inline void Init(GM_ADDR x, GM_ADDR y, GM_ADDR z, AddCustomTilingData tiling, AscendC::TPipe* pipeIn)
{
    pipe = pipeIn;
    this->blockLength = tiling.blockLength;
    this->tileNum = tiling.tileNum;
    this->tileLength = tiling.tileLength;
    // 计算每个核上的地址偏移
    xGm.SetGlobalBuffer((__gm__ half *)x + this->blockLength * AscendC::GetBlockIdx(), this->blockLength);
    yGm.SetGlobalBuffer((__gm__ half *)y + this->blockLength * AscendC::GetBlockIdx(), this->blockLength);
    zGm.SetGlobalBuffer((__gm__ half *)z + this->blockLength * AscendC::GetBlockIdx(), this->blockLength);
    // pipe alloc memory to queue, the unit is Bytes
    pipe->InitBuffer(inQueueX, 1, this->tileLength * sizeof(half));
    pipe->InitBuffer(inQueueY, 1, this->tileLength * sizeof(half));
    pipe->InitBuffer(outQueueZ, 1, this->tileLength * sizeof(half));
}
```

每个核需要对tileNum个数据块分别进行搬入、计算、搬出处理，因此Process函数内将tileNum作为循环上限。

```cpp
__aicore__ inline void Process()
{
    int32_t loopCount = this->tileNum;
    // tiling strategy, pipeline parallel
    for (int32_t i = 0; i < loopCount; i++) {
    CopyIn(i, this->tileLength);
    Compute(i, this->tileLength);
    CopyOut(i, this->tileLength);
    }
} 
```

对应的，每个核内搬入、搬出每个数据块时，需定位到每个数据块所在GlobalMemory上的内存偏移地址，因此在CopyIn和CopyOut函数内部使用DataCopy接口时，需增加每个数据块的地址偏移。Compute函数没有变化，与基础矢量算子相同。


CopyIn函数实现代码如下：


```cpp
__aicore__ inline void CopyIn(int32_t progress, uint32_t tileLength)
{
    ...
    // copy progress_th tile from global tensor to local tensor
    AscendC::DataCopy(xLocal, xGm[progress * this->tileLength], tileLength);
    AscendC::DataCopy(yLocal, yGm[progress * this->tileLength], tileLength);
    ...
} 
```


CopyOut函数实现代码如下：


```cpp
__aicore__ inline void CopyOut(int32_t progress, uint32_t tileLength)
{
    ...
    // copy progress_th tile from local tensor to global tensor
    AscendC::DataCopy(zGm[progress * this->tileLength], zLocal, tileLength);
    ...
} 
```

# 3.3.2.4.3 尾块 Tiling

如下图中的示例，算子的输入shape为（1，2048），支持的数据类型为half类型，输入数据可以对齐到一个datablock的大小（32字节），输入数据为2048 * 2 / 32 = 128个datablock，因此可以平均分配到每个核上（假设使用8个核），每个核上处理256个数，16个datablock。此时不需要进行尾块处理。


图 3-13 shape 对齐场景


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/529cc9cf879286b36a2cd19cf3a4c1be841c49f049125ee6bf3f5619b76d3463.jpg)


针对一些shape，比如算子的输入shape为（1，1904），支持的数据类型为half类型，输入数据可以对齐到一个datablock的大小（32字节），可以平均分配到每个核上（假设使用8个核），每个核上处理238个数，238个数无法均分到datablock上，分满14个datablock后，剩余14个数（28字节），多核切分后需要进行尾块处理。

对于不同shape的输入进行数据切分时，可能会发生Tiling后的数据平均分配到多核上，但每个核内的数据无法均分的情况。针对此种场景，在Tiling参数中增加变量lastTileLength，用来表示最后一个分块，即尾块的大小。因此，在定义算子的Tiling结构体时包含以下四个成员：

blockLength：每个核上计算的数据长度；

tileNum：每个核上切分的主块数据块的个数；

tileLength：每个核上主块数据块的长度；

lastTileLength：每个核上尾块的长度。


图 3-14 多核 Tiling 尾块示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/5babf2a4fff64d1bde4d04f16fc21f870209bbe0003bd12aed30409eb5bb12bb.jpg)


# Tiling 实现


算子的Tiling结构体定义如下：


```c
struct AddCustomTilingData {
    uint32_t blockLength;
    uint32_t tileNum;
    uint32_t tileLength; 
```

```txt
uint32_t lastTileLength;
}; 
```

Host侧Tiling实现的主要内容为计算以上四个成员变量。步骤如下：

步骤1 判断数据总长度totalLength是否满足32字节对齐，如不满足，则计算totalLength向上32字节对齐后的长度totalLengthAligned。

```txt
constexpr uint32_t BLOCK_SIZE = 32;
// 为方便计算，这里根据数据类型定义变量alignNum作为对齐数
uint32_t alignNum = BLOCK_SIZE / dataTypeSize;
// totalLength为数据总量
totalLengthAligned = (totalLength % alignNum == 0U) ?
    static_cast<uint32_t>(totalLength) :
    ((static_cast<uint32_t>(totalLength) + alignNum - 1) / alignNum) * alignNum;
```

步骤2 判断totalLengthAligned是否能被使用的核数NumBlocks均分，如果可以，则计算每个核上计算数据长度blockLength。

```c
constexpr uint32_t NUM_BLOCKS = 8;
constexpr uint32_t UB_BLOCK_NUM = 100; // 此处为方便验证，使用UB_BLOCK_NUM作为Unified Buffer可用的Block数量，因此可得出可用UB空间的大小为UB_BLOCK_NUM * BLOCK_SIZE
uint32_t blockLength, tileNum;
if ((totalLengthAligned / alignNum) % NUM_BLOCKS == 0U) {
    blockLength = totalLengthAligned / NUM_BLOCKS;
}
```

步骤3 计算tileNum。为了减少数据搬运开销，应尽量使用核内的Unified Buffer空间。基于每个核上的计算量以及可用Unified Buffer空间的大小，计算tileNum。

```javascript
tileNum = blockLength / (alignNum * UB_BLOCK_NUM); 
```

步骤4 根据计算出的tileNum，计算tileLength和lastTileLength。

如果每个核的计算量能够被当前可用Unified Buffer空间均分，则按照无尾块场景处理。

```c
if (static_cast<uint32_t>(blockLength / alignNum) % UB_BLOCK_NUM == 0U) {
    // 单核的计算量能被当前可用UB空间均分，仅有主块，无尾块
    tileLength = UB_BLOCK_NUM * alignNum;
    lastTileLength = 0U;
}
```

反之，按照尾块场景处理，尾块长度为单核计算数据长度 - tileNum * tileLength。

```cpp
if (tileNum == 0U) {
    // 单核需要计算的长度小于UB可用空间，按照仅有尾块处理
    tileLength = 0U;
    lastTileLength = static_cast<uint32_t>(((blockLength + alignNum - 1) / alignNum) * alignNum);
} else {
    // 同时有主块和尾块
    tileLength = UB_BLOCK_NUM * alignNum;
    lastTileLength = static_cast<uint32_t>(blockLength - tileNum * tileLength);
}
```

# ----结束

Host侧Tiling实现的代码如下：

```txt
constexpr uint32_t BLOCK_SIZE = 32;
constexpr uint32_t NUM_BLOCKS = 8;
constexpr uint32_t UB_BLOCK_NUM = 100; // 此处为方便验证，使用UB_BLOCK_NUM作为UB可用的Block数量，因此可得出可用UB空间的大小为UB_BLOCK_NUM * BLOCK_SIZE
...
uint32_t alignNum = BLOCK_SIZE / dataTypeSize; // 为方便计算，这里根据数据类型定义变量alignNum作为对齐数，dataTypeSize为运算数据的数据类型对应的字节数
// totalLength为数据总量
totalLengthAligned = (totalLength % alignNum == 0U) ?
```

```txt
static_cast<uint32_t>(totalLength) :
((static_cast<uint32_t>(totalLength) + alignNum - 1) / alignNum) * alignNum;
uint32_t blockLength, tileNum;
if ((totalLengthAligned / alignNum) % NUM_BLOCKS == 0U) {
    blockLength = totalLengthAligned / NUM_BLOCKS;
    tileNum = blockLength / alignNum / UB_BLOCK_NUM;

    if (tileNum == 0) {
    // 单核需要计算的长度小于UB可用空间，按照仅有尾块处理
    tileLength = 0;
    lastTileLength = ((blockLength + alignNum - 1) / alignNum) * alignNum;
    } else if ((blockLength / alignNum) % UB_BLOCK_NUM == 0) {
    // 单核的计算量能被当前可用UB空间均分，仅有主块，无尾块
    tileLength = UB_BLOCK_NUM * alignNum;
    lastTileLength = 0;
    } else {
    // 同时有主块和尾块
    tileLength = UB_BLOCK_NUM * alignNum;
    lastTileLength = blockLength - tileNum * tileLength;
    }
    ...
}
```


(1，1904)形状的输入数据计算后，tiling结构体内各个变量的值如下：


```c
struct AddCustomTilingData {
    uint32_t blockLength = 238; // 每个核计算238个half，8个核共计算1904个half
    uint32_t tileNum = 0; // 可用的UB空间足够，为仅有尾块的场景
    uint32_t tileLength = 0; // 没有主块，主块长度为0
    uint32_t lastTileLength = 240; // 238个half未32B对齐，对齐到240个half搬运
};
```

# 算子类实现

与多核Tiling相比，在Init函数中通过Pipe内存管理对象为输入输出Queue分配内存时，取tileLength与lastTileLength中的最大值作为分配内存的长度。例如，当单核需要计算的长度小于UB可用空间时，按照仅有尾块处理，此时tileLength为0，而lastTileLength为数据块长度。因此，需要取两者中的较大值来分配内存。

```cpp
uint32_t initBufferLength = AscendC::Std::max(this->tileLength, this->lastTileLength);
pipe->InitBuffer(inQueueX, 1, this->initBufferLength * sizeof(dataType)); 
```

由于尾块长度为lastTileLength，与主块数据块的长度不同，因此在CopyIn函数、Compute函数、CopyOut函数中传入本次循环待处理的数据块长度参数tileLength，即待处理的主块或尾块的数据长度。


Process函数实现代码如下：


```c
__aicore__ inline void Process()
{
    // 计算主块数据，对应数据块长度为tileLength
    for (uint32_t i = 0; i < this->tileNum; i++) {
    CopyIn(i, this->tileLength);
    Compute(i, this->tileLength);
    CopyOut(i, this->tileLength);
    }
    // 计算尾块数据，对应数据块长度为lastTileLength
    if (this->lastTileLength > 0) {
    CopyIn(this->tileNum, this->lastTileLength);
    Compute(this->tileNum, this->lastTileLength);
    CopyOut(this->tileNum, this->lastTileLength);
    }
}
```


CopyIn函数实现代码如下：


```c
__aicore__ inline void CopyIn(int32_t progress, uint32_t tileLength)
{ 
```

```cpp
AscendC::LocalTensor<T> xLocal = inQueueX.AllocTensor<T>();
AscendC::LocalTensor<T> yLocal = inQueueY.AllocTensor<T>();
AscendC::DataCopy(xLocal, xGm[progress * this->tileLength], tileLength);
AscendC::DataCopy(yLocal, yGm[progress * this->tileLength], tileLength);
inQueueX.EnQue(xLocal);
inQueueY.EnQue(yLocal);
} 
```


Compute函数实现代码如下：


```cpp
__aicore__ inline void Compute(int32_t progress, uint32_t tileLength)
{
    AscendC::LocalTensor<T> xLocal = inQueueX.DeQue<T>();
    AscendC::LocalTensor<T> yLocal = inQueueY.DeQue<T>();
    AscendC::LocalTensor<T> zLocal = outQueueZ.AllocTensor<T>();
    AscendC::Add(zLocal, xLocal, yLocal, tileLength);
    outQueueZ.EnQue<T>(zLocal);
    inQueueX.FreeTensor(xLocal);
    inQueueY.FreeTensor(yLocal);
} 
```


CopyOut函数实现代码如下：


```cpp
__aicore__ inline void CopyOut(int32_t progress, uint32_t tileLength)
{
    AscendC::LocalTensor<T> zLocal = outQueueZ.DeQue<T>();
    AscendC::DataCopy(zGm[progress * this->tileLength], zLocal, tileLength);
    outQueueZ.FreeTensor(zLocal);
} 
```

# 3.3.2.4.4 尾核 Tiling

对于不同shape的输入进行数据切分时，可能会发生数据无法平均分配到多个核的情况。例如当算子的输入shape为[1, 1999]，使用核数为8，数据类型为half时，需要计算的数据总量为1 * 1999 * sizeof(half) = 3998字节，3998字节既不满足32字节对齐，也无法平均分配到8个核上。因此该场景下，对数据进行多核切分后，每个核的计算数据量不同。此种情况下，应该尽可能均匀的分配数据，所有核上的计算数据量有两种情况，将计算量较多的核称为整核，计算量较少的核称为尾核。


图3-15 数据对齐示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/becf0ca4bd14b98a74481b18f0014395ba2179a9e504b5cf85ec53ee5246e4c5.jpg)


# Tiling 实现

因为AI处理器在进行数据搬运和Vector计算时，对于搬运的数据长度和UnifiedBuffer首地址都有必须32字节对齐的要求，首先待处理数据需要先保证向上对齐到32字节的大小。该场景下后续搬运和计算的处理细节请参考3.3.2.7 非对齐场景。如下代码片段展示了将数据对齐到datablock大小的示例：

```c
constexpr uint32_t SIZE_OF_HALF = 2;
constexpr uint32_t BLOCK_SIZE = 32;
constexpr uint32_t NUM_BLOCKS = 8;
constexpr uint32_t ALIGN_NUM = BLOCK_SIZE / SIZE_OF_HALF;
// shape需要对齐到的32字节，假设原totalLength为1999，向上满足32字节对齐后为2000
uint32_t totalLengthAligned = (totalLength % ALIGN_NUM == 0U) ?
```

```c
static_cast<uint32_t>(totalLength):
((static_cast<uint32_t>(totalLength) + ALIGN_NUM - 1) / ALIGN_NUM) * ALIGN_NUM; 
```

满足32字节对齐后的数据，应尽可能的均分到每个核上。如果无法均分，那么先将可以均分的部分平均分配，剩余的部分分配给部分核，会有部分核多算一个datablock。为了保证切分后的数据仍是满足32字节对齐的，以ALIGN_NUM（ALIGN_NUM个数据为32字节）为粒度，将数据分配到所有核上。在本样例中，数据类型为half，ALIGN_NUM = BLOCK_SIZE / sizeof(half) = 16。将对齐后的数据总量按ALIGN_NUM为粒度分成x个数据块，x = 2000 / 16 = 125。

AI处理器的核数NUM_BLOCKS为8，无法将125个数据块均分到8个核上。按照以下步骤将数据块尽可能的均分到每个核上：

a. 计算x / NUM_BLOCKS = 15；

b. 计算x % NUM_BLOCKS = 5。

根据上述步骤得出，如果每个核上分配15个数据块，那么将有5个数据块剩余。将这5个剩余的数据块分配到5个核上，这样可以得到5个计算16个数据块的整核和3个计算15个数据块的尾核。下图展示了数据无法均分时多核切分的示例。


图 3-16 无法均分到每个核上的示例


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/822b57036cd6a90c32824431c54c824b1ce1785df71a95071d0762e7f897f4a5.jpg)


基于上文，设计如下的算子Tiling结构体成员：

formerNum：分配到数据量较多的核数，即整核的核数。

tailNum：分配到数据量较少的核数，即尾核的核数。

formerLength：整核计算的数据长度。

tailLength：尾核计算的数据长度。

Tiling参数的计算代码如下：

```c
constexpr uint32_t NUM_BLOCKS = 8;
constexpr uint32_t SIZE_OF_HALF = 2;
constexpr uint32_t BLOCK_SIZE = 32;
// shape需要对齐到的最小单位
constexpr uint32_t ALIGN_NUM = BLOCK_SIZE / SIZE_OF_HALF;
...
void GenerateTilingData(uint8_t* tilingBuf, uint32_t numBlocks)
{
    // shape需要对齐到的datablock,假设原totalLength为1999，向上满足32字节对齐后为2000
    uint32_t totalLengthAligned = (totalLength % ALIGN_NUM == 0U) ?
    static_cast<uint32_t>(totalLength) :
```

```lisp
((static_cast<uint32_t>(totalLength) + ALIGN_NUM - 1) / ALIGN_NUM) * ALIGN_NUM;
// 核心数为8，一个datablock包含16个数，那么：datablock的总数：2000 / 16 = 125
// 有5个核会分到16个datablock：125 % 8 = 5，可以称之为整核
// 有3个核会分到15个datablock：8 - 5 = 3，可以称之为尾核
uint32_t formerNum = (totalLengthAligned / ALIGN_NUM) % numBlocks;
uint32_t tailNum = numBlocks - formerNum;
// 整核计算的数据长度：totalLengthAligned / NUM_BLOCKS为每个核上计算的元素个数，formerLength为上述元素个数向上32字节对齐的结果
uint32_t formerLength =
    static_cast<uint32_t>(((totalLengthAligned + numBlocks - 1) / numBlocks + alignNum - 1) / alignNum) * alignNum;
// 尾核计算的数据长度：totalLengthAligned / NUM_BLOCKS为每个核上计算的元素个数，tailLength 为上述元素个数向下32字节对齐的结果
uint32_t tailLength = (totalLengthAligned / numBlocks / ALIGN_NUM) * ALIGN_NUM;
...
```

# 算子类实现

在Kernel侧的Init函数中，计算输入在Global Memory上的内存偏移地址时，应对整核和尾核加以区分。

整核上，输入的内存偏移地址计算代码如下：

```javascript
xGm.SetGlobalBuffer((__gm__ T*)x + formerLength * AscendC::GetBlockIdx(), formerLength); 
```

尾核上，计算输入的内存偏移地址时，需在全部整核的数据长度基础上加上尾核的偏移量，代码如下：

```txt
xGm.SetGlobalBuffer((__gm__ T*)x + formerLength * formerNum + tailLength * (AscendC::GetBlockIdx() - formerNum), tailLength); 
```

完整的Init函数实现代码如下：

```cpp
__aicore__ inline void Init(GM_ADDR x, GM_ADDR y, GM_ADDR z, AddCustomTilingData tiling, AscendC::TPipe* pipeIn)
{
    pipe = pipeIn;
    if (AscendC::GetBlockIdx() < tiling.formerNum) {
    this->tileLength = tiling.formerLength;
    uint64_t offset = tiling.formerLength * AscendC::GetBlockIdx();
    xGm.SetGlobalBuffer((__gm_ half *)x + offset, tiling.formerLength);
    yGm.SetGlobalBuffer((__gm_ half *)y + offset, tiling.formerLength);
    zGm.SetGlobalBuffer((__gm_ half *)z + offset, tiling.formerLength);
    } else {
    this->tileLength = tiling.tailLength;
    uint64_t offset = tiling.formerLength * tiling.formerNum
    + tiling.tailLength * (AscendC::GetBlockIdx() - tiling.formerNum);
    xGm.SetGlobalBuffer((__gm_ half *)x + offset, tiling.tailLength);
    yGm.SetGlobalBuffer((__gm_ half *)y + offset, tiling.tailLength);
    zGm.SetGlobalBuffer((__gm_ half *)z + offset, tiling.tailLength);
    }
    pipe->InitBuffer(inQueueX, 1, this->tileLength * sizeof(half));
    pipe->InitBuffer(inQueueY, 1, this->tileLength * sizeof(half));
    pipe->InitBuffer(outQueueZ, 1, this->tileLength * sizeof(half));
} 
```

其余实现与多核Tiling中的实现一致，这里不重复进行说明。

# 3.3.2.4.5 尾核&尾块

对于不同shape的输入进行数据切分时，可能会发生数据无法平均分配到多个核、同时每个核内的数据无法均分的情况。参考核间均分场景下的尾块处理与核间不均分场景下的尾核处理的处理方式，将两者结合起来考虑整核的尾块、尾核的尾块的处理方式。

# Tiling 实现

由于本场景中核间、核内的数据均无法均分，在核间不均分场景下的尾核处理定义的Tiling结构体的基础上增加两个成员变量：

formerLastTileLength：数据量多的核最后一个分块大小，即整核的尾块大小。计算时，先按3.3.2.4.4 尾核Tiling中提到的分核策略，切分数据量多的核。

```c
// shape需要对齐到的datablock
uint32_t totalLengthAligned = (totalLength % alignNum == 0U) ?
    static_cast<uint32_t>(totalLength) :
    ((static_cast<uint32_t>(totalLength) + alignNum - 1) / alignNum) * alignNum;
// 计算整核数量
uint32_t formerNum = (totalLengthAligned / alignNum) % numBlocks;
// 计算整核的数据量
uint32_t formerLength = static_cast<uint32_t>(((totalLengthAligned + numBlocks - 1) / numBlocks + alignNum - 1) / alignNum) * alignNum;
```


再按3.3.2.4.3 尾块Tiling中的切分策略，计算尾块长度。


```c
TilingParamsCalc(formerLength, alignNum, formerTileNum, formerTileLength, formerLastTileLength);

void TilingParamsCalc(uint32_t length, uint32_t alignNum, uint32_t& tileNum, uint32_t& tileLength, uint32_t& lastTileLength)
{
    tileNum = length / (alignNum * UB_BLOCK_NUM);
    if (tileNum == 0U) {
    tileLength = 0U;
    lastTileLength = static_cast<uint32_t>(((length + alignNum - 1) / alignNum) * alignNum);
    } else if (static_cast<uint32_t>(length / alignNum) % UB_BLOCK_NUM == 0U) {
    tileLength = UB_BLOCK_NUM * alignNum;
    lastTileLength = 0U;
    } else {
    tileLength = UB_BLOCK_NUM * alignNum;
    lastTileLength = static_cast<uint32_t>(length - tileNum * tileLength);
    }
} 
```

tailLastTileLength：数据量少的核最后一个分块大小，即尾核的尾块大小。计算时，先按3.3.2.4.4 尾核Tiling中提到的分核策略，切分数据量少的核。

```c
// 计算尾核数量
uint32_t tailNum = numBlocks - formerNum;
// 计算尾核的数据量
uint32_t tailLength = (totalLengthAligned / numBlocks / alignNum) * alignNum;
```


再按3.3.2.4.3 尾块Tiling中的切分策略，计算尾块长度。


```c
TilingParamsCalc(tailLength, alignNum, tailTileNum, tailTileLength, tailLastTileLength);

void TilingParamsCalc(uint32_t length, uint32_t alignNum, uint32_t& tileNum, uint32_t& tileLength, uint32_t& lastTileLength)
{
    tileNum = length / (alignNum * UB_BLOCK_NUM);
    if (tileNum == 0U) {
    tileLength = 0U;
    lastTileLength = static_cast<uint32_t>(((length + alignNum - 1) / alignNum) * alignNum);
    } else if (static_cast<uint32_t>(length / alignNum) % UB_BLOCK_NUM == 0U) {
    tileLength = UB_BLOCK_NUM * alignNum;
    lastTileLength = 0U;
    } else {
    tileLength = UB_BLOCK_NUM * alignNum;
    lastTileLength = static_cast<uint32_t>(length - tileNum * tileLength);
    }
} 
```

# 算子类实现

Kernel侧Init函数和Process函数的实现需将核间均分场景下的尾块处理与核间不均分场景下的尾核处理的实现结合起来。

Init函数中由于整核和尾核对应的tileLength和lastTileLength不同。因此需按照核间不均分场景下的尾核处理中提到的分别处理整核和尾核。后续对主块和尾块的CopyIn、Compute、CopyOut函数的处理方式与核间均分场景下的处理方式相同。


Init函数实现代码如下：


```cpp
__aicore__ inline void Init(GM_ADDR x, GM_ADDR y, GM_ADDR z, AddCustomTilingData tiling, AscendC::TPipe* pipeIn)
{
    pipe = pipeIn;
    if (AscendC::GetBlockIdx() < tiling.formerNum) {
    this->tileNum = tiling.formerTileNum;
    this->tileLength = tiling.formerTileLength;
    this->lastTileLength = tiling.formerLastTileLength;
    uint64_t offset = tiling.formerLength * AscendC::GetBlockIdx();
    xGm.SetGlobalBuffer((__gm__ half *)x + offset, tiling.formerLength);
    yGm.SetGlobalBuffer((__gm__ half *)y + offset, tiling.formerLength);
    zGm.SetGlobalBuffer((__gm__ half *)z + offset, tiling.formerLength);
    } else {
    this->tileNum = tiling.tailTileNum;
    this->tileLength = tiling.tailTileLength;
    this->lastTileLength = tiling.tailLastTileLength;
    uint64_t offset = tiling.formerLength * tiling.formerNum
    + tiling.tailLength * (AscendC::GetBlockIdx() - tiling.formerNum);
    xGm.SetGlobalBuffer((__gm__ half *)x + offset, tiling.tailLength);
    yGm.SetGlobalBuffer((__gm__ half *)y + offset, tiling.tailLength);
    zGm.SetGlobalBuffer((__gm__ half *)z + offset, tiling.tailLength);
    }

    // 只有尾块的场景下，tileLength为0，因此取tileLength和lastTileLength的最大值来初始化
    uint32_t initBufferLength = AscendC::Std::max(this->tileLength, this->lastTileLength);
    pipe->InitBuffer(inQueueX, 1, this->initBufferLength * sizeof(half));
    pipe->InitBuffer(inQueueY, 1, this->initBufferLength * sizeof(half));
    pipe->InitBuffer(outQueueZ, 1, this->initBufferLength * sizeof(half));
}
```

# 3.3.2.5 DoubleBuffer 场景

因存在算子中多次搬入搬出数据的场景，为充分利用硬件资源，实现多流水并行，引入DoubleBuffer机制。DoubleBuffer是通过将输入数据分成大小相等的两块，充分利用AI Core的硬件资源，实现数据搬入、计算、数据搬出的并行执行方式。下面以“核间不均分，核内不均分”的样例为例，介绍算子中DoubleBuffer的实现，完整样例代码请参见使用DoubleBuffer的Add算子样例。


图 3-17 DoubleBuffer 数据切分示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/8858ef10e0a95075517515c04a7269d6cd856878fe972193aec4449c3bf9a8be.jpg)


# Tiling 实现

使能DoubleBuffer后，每一个数据块会分成大小相等的两块，因此，若要使能DoubleBuffer，要求数据总量应该能够均分。为了简化处理，将可用的Unified Buffer空间以32字节为粒度，分成n块dataBlock，如果n不是偶数，则减1，这样就可以保证一套代码兼容开启或不开启DoubleBuffer功能。对应步骤如下：

步骤1 判断数据总长度totalLength是否满足32字节对齐，如不满足，则计算totalLength向上32字节对齐后的长度totalLengthAligned。

```c
constexpr uint32_t BLOCK_SIZE = 32;
// 为方便计算，这里根据数据类型定义变量alignNum作为对齐数
uint32_t alignNum = BLOCK_SIZE / dataTypeSize;
// totalLength为数据总量
uint32_t totalLengthAligned = (totalLength % alignNum == 0)?
    totalLength : ((totalLength + alignNum - 1) / alignNum) * alignNum;
```

步骤2 根据totalLengthAligned，计算每个核的计算数据长度blockLength，分核策略可参照3.3.2.4.4 尾核Tiling。

步骤3 计算其余Tiling参数。

对当前Unified Buffer可用空间以32字节为粒度，进行切分，计算出数据块个数UB_BLOCK_NUM。根据是否开启DoubleBuffer计算出当前可用的最大数据块个数，记作MAX_AVAILABLE_UB_BLOCK_NUM。最后，以MAX_AVAILABLE_UB_BLOCK_NUM为粒度，对blockLength进行切分。为方便演示，如下代码直接给出UB_BLOCK_NUM，作为当前Unified Buffer可用空间包含的block（32字节）数。

```c
constexpr uint32_t BUFFER_NUM = 2;
constexpr uint32_t UB_BLOCK_NUM = 21; // UB最大可以使用的block数量
constexpr uint32_t MAX_AVAILABLE_UB_BLOCK_NUM = UB_BLOCK_NUM / BUFFER_NUM * BUFFER_NUM;
```

```proto
tileNum = blockLength / (alignNum * MAX_AVAILABLE_UB_BLOCK_NUM);
if (tileNum == 0) {
    // 单核需要计算的长度小于UB可用空间，按照仅有尾块处理
    tileLength = 0;
    lastTileLength = (blockLength + alignNum - 1) / alignNum * alignNum;
} else if ((blockLength / alignNum) % MAX_AVAILABLE_UB_BLOCK_NUM == 0) {
    // 单核的计算量能被当前可用UB空间均分，仅有主块，无尾块
    tileLength = MAX_AVAILABLE_UB_BLOCK_NUM * alignNum;
    lastTileLength = 0;
} else {
    // 同时有主块和尾块
    tileLength = MAX_AVAILABLE_UB_BLOCK_NUM * alignNum;
    lastTileLength = blockLength - tileNum * tileLength;
}
```

# ----结束

# 算子类实现

不开启DoubleBuffer时，只需要对每个核上最后一个分块的起始地址做处理；开启DoubleBuffer后，需要处理的数据块长度变成原来的一半，所以需要对最后两个数据块的起始地址做处理。

开启DoubleBuffer，参考InitBuffer接口函数原型，将num参数配置成2，即BUFFER_NUM。

```cpp
this->initBufferLength = AscendC::Std::max(this->tileLength, this->lastTileLength);
pipe.InitBuffer(inQueueX, BUFFER_NUM, this->initBufferLength * sizeof(dataType));
pipe.InitBuffer(inQueueY, BUFFER_NUM, this->initBufferLength * sizeof(dataType));
pipe.InitBuffer(outQueueZ, BUFFER_NUM, this->initBufferLength * sizeof(dataType)); 
```

同时在计算核内每个数据块的长度时，考虑DoubleBuffer场景，需要将Buffer数量，即BUFFER_NUM=2带入计算。

```txt
this->tileLength = tiling.tileLength / BUFFER_NUM; 
```

由于无法保证尾块满足DoubleBuffer的条件，因此不对尾块进行切分。

```javascript
this->lastTileLength = tiling.lastTileLength; 
```


Init函数实现代码如下：


```cpp
__aicore__ inline void Init(GM_ADDR x, GM_ADDR y, GM_ADDR z, AddCustomTilingData tiling)
{
    if (tiling.isEvenCore) {
    this->blockLength = tiling.blockLength;
    this->tileNum = tiling.tileNum;
    this->tileLength = tiling.tileLength / BUFFER_NUM;
    this->lastTileLength = tiling.lastTileLength;

    xGm.SetGlobalBuffer(__gm__ dataType *)x + this->blockLength * AscendC::GetBlockIdx(), this->blockLength);
    yGm.SetGlobalBuffer(__gm__ dataType *)y + this->blockLength * AscendC::GetBlockIdx(), this->blockLength);
    zGm.SetGlobalBuffer(__gm__ dataType *)z + this->blockLength * AscendC::GetBlockIdx(), this->blockLength);
    } else {
    if (AscendC::GetBlockIdx() < tiling.formerNum) {
    this->tileNum = tiling.formerTileNum;
    this->tileLength = tiling.formerTileLength / BUFFER_NUM;
    this->lastTileLength = tiling.formerLastTileLength;

    xGm.SetGlobalBuffer(__gm__ dataType *)x + tiling.formerLength * AscendC::GetBlockIdx(), tiling.formerLength);
    yGm.SetGlobalBuffer(__gm__ dataType *)y + tiling.formerLength * AscendC::GetBlockIdx(), tiling.formerLength);
    zGm.SetGlobalBuffer(__gm__ dataType *)z + tiling.formerLength * AscendC::GetBlockIdx(), tiling.formerLength);
    } else {
    this->tileNum = tiling.tailTileNum;
    this->tileLength = tiling.tailTileLength / BUFFER_NUM;
    this->lastTileLength = tiling.tailLastTileLength;

    xGm.SetGlobalBuffer(__gm__ dataType *)x + tiling.formerLength * tiling.formerNum + tiling.tailLength * (AscendC::GetBlockIdx() - tiling.formerNum), tiling.tailLength);
    yGm.SetGlobalBuffer(__gm__ dataType *)y + tiling.formerLength * tiling.formerNum + tiling.tailLength * (AscendC::GetBlockIdx() - tiling.formerNum), tiling.tailLength);
    zGm.SetGlobalBuffer(__gm__ dataType *)z + tiling.formerLength * tiling.formerNum + tiling.tailLength * (AscendC::GetBlockIdx() - tiling.formerNum), tiling.tailLength);
    }
}

uint32_t initBufferLength = AscendC::Std::max(this->tileLength, this->lastTileLength);
pipe.InitBuffer(inQueueX, BUFFER_NUM, initBufferLength * sizeof(dataType));
pipe.InitBuffer(inQueueY, BUFFER_NUM, initBufferLength * sizeof(dataType));
pipe.InitBuffer(outQueueZ, BUFFER_NUM, initBufferLength * sizeof(dataType)); 
```

由于开启DoubleBuffer后，切分后的主块数据块个数翻倍，在Process函数中，需要将BUFFER_NUM带入计算循环次数；尾块独立计算，不开启DoubleBuffer。后续主尾块在CopyIn、Compute、CopyOut函数中的处理，与尾块tiling处理相同。

```c
__aicore__ inline void Process()
{
    // 主块进行DoubleBuffer计算，所以loopCount得乘以2
    uint32_t loopCount = this->tileNum * BUFFER_NUM;
    for (uint32_t i = 0; i < loopCount; i++) {
    CopyIn(i, this->tileLength);
    Compute(i, this->tileLength);
    CopyOut(i, this->tileLength);
```

```txt
}
// 尾块进行计算, 不做DoubleBuffer操作
if (this->lastTileLength > 0U) {
    CopyIn(loopCount, this->lastTileLength);
    Compute(loopCount, this->lastTileLength);
    CopyOut(loopCount, this->lastTileLength);
}
```

# 3.3.2.6 Broadcast 场景

在某些场景下，可能会存在两个输入shape不相同的情况。由于Add接口只支持对shape相同的输入进行计算，因此需要先对输入进行shape变换，再进行Add计算。本节将对满足Broadcast条件的输入在算子实现中的Broadcast处理进行介绍，其他场景可以参考本章节中提供的思路。

# 须知

Broadcast机制通过扩展较小维度的数据，使得不同shape的输入能够进行运算，从而避免了显式的复制操作，提高了计算效率。数据进行Broadcast需满足：两个输入的维度个数相同，并且仅在某一个维度上的长度不同，某一个输入在此维度的长度为1。比如：shape为(32, 8) 和 (32, 1) 的两个输入可以进行Broadcast，因为它们都是二维，且第一个维度大小相等，而不相等的维度中第二个输入的维度为1，满足条件。

本节中将使用Broadcast接口，因此输入需满足该API相关约束。同时，由于硬件限制，该API的输入地址需满足32字节对齐。本节以输入维度为2、第二个轴（axis = 1）需要Broadcast为例进行说明。完整的样例代码请参见输入Broadcast的Add算子样例。

# Tiling 实现

与输入shape相同的场景相比，在Tiling结构体中增加相应的成员变量，表示是否需要对输入进行Broadcast、需要对哪个维度进行Broadcast、Broadcast的轴需要扩充的倍数。因此新增四个Tiling结构体成员：

xLen和yLen：表示两个输入的数据长度。

axis：表示对输入的哪个维度进行Broadcast。

coef：表示Broadcast的输入需要扩维的倍数。例如，x shape为(m, 1)，y shape为(m, n)，则coef = n。如下图所示，图中相同颜色部分为单次计算的数据块。


图 3-18 axis=1 时 coef 示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/5e6dbafaf8d1eb65a0cb098fb0ffa47d1db25b663e2683a7bc147fc62bef6493.jpg)



Tiling结构体定义代码如下所示：


```c
struct AddCustomTilingData {
    uint32_t xLen;
    uint32_t yLen;
    uint32_t coef;
    uint32_t axis;
}; 
```

设需要进行Broadcast的输入长度为shorterAxisLen；不需要进行Broadcast的输入长度为totalLength。

```c
constexpr uint32_t BLOCK_SIZE = 32;
... // 读入数据
uint32_t totalLength = (xLen > yLen)? xLen : yLen;
uint32_t shorterAxisLen = (xLen < yLen)? xLen : yLen;
```

使用shorterAxisLen进行分核计算，并使用分核后的长度与coef相乘作为totalLength的分核长度。

```lisp
constexpr uint32_t BLOCK_SIZE = 32;
uint32_t alignCoef = (tiling->axis == 0U) ? shorterAxisLen : totalLength / shorterAxisLen;
uint32_t divDimCoef = (tiling->axis == 0U) ? totalLength / shorterAxisLen : shorterAxisLen;
if (divDimCoef % blockDim == 0U) {
    uint32_t blockLength = divDimCoef / blockDim * alignCoef;
    ...
} else {
    uint32_t formerNum = (divDimCoef / BUFFER_NUM) % blockDim;
    uint32_t tailNum = blockDim - formerNum;

    uint32_t formerLength = ((divDimCoef / BUFFER_NUM) / blockDim + 1U) * BUFFER_NUM * alignCoef;
    uint32_t tailLength = ((divDimCoef / BUFFER_NUM) / blockDim) * BUFFER_NUM * alignCoef;
    ....
} 
```

进行核内数据切分时，需要计算Unified Buffer数据块的数量向coef和BUFFER_NUM对齐之后的数量ubBlockAligned。

```c
uint32_t ubBlockAligned =
(MAX_AVAILABLE_UB_BLOCK_NUM * alignNum / (alignCoef * BUFFER_NUM) * (alignCoef * BUFFER_NUM) == 0U) ?
MAX_AVAILABLE_UB_BLOCK_NUM :
MAX_AVAILABLE_UB_BLOCK_NUM * alignNum / (alignCoef * BUFFER_NUM) * (alignCoef * BUFFER_NUM);
...
tileNum = length / ubBlockAligned; 
```

```txt
if (length % ubBlockAligned == 0U || tileNum == 0U) {
    if (tileNum == 0U) {
    tileNum = 1U;
    }
    if (length < ubBlockAligned) {
    tileLength = length;
    lastTileLength = tileLength;
    } else {
    tileLength = ubBlockAligned;
    lastTileLength = tileLength;
    }
} else {
    tileNum++;
    tileLength = ubBlockNum;
    lastTileLength = (uint32_t)(length - (tileNum - 1) * tileLength);
} 
```

# 算子类实现

在核函数初始化阶段，根据Tiling结构体传入的参数确定对哪个输入进行Broadcast。由于针对输入的第二个轴（axis = 1）进行Broadcast，可以计算出，对于需要进行Broadcast的输入，每个核搬入数据长度为blockLength / coef。


初始化函数代码如下：


```cpp
__aicore__ inline void Init(GM_ADDR x, GM_ADDR y, GM_ADDR z, AddCustomTilingData tiling, AscendC::TPipe* pipeIn)
{
    pipe = pipeIn;
    GM_ADDR longerInputPtr;
    GM_ADDR shorterInputPtr;
    if (tiling.xLen > tiling.yLen) {
    longerInputPtr = x;
    shorterInputPtr = y;
    } else {
    longerInputPtr = y;
    shorterInputPtr = x;
    }
    this->coef = tiling.coef;
    if (tiling.isEvenCore) {
    this->tileNum = tiling.tileNum;
    this->tileLength = tiling.tileLength / BUFFER_NUM;
    this->lastTileLength = tiling.lastTileLength;
    xGm.SetGlobalBuffer((__gm__ T*)longerInputPtr + tiling.blockLength * AscendC::GetBlockIdx(),
    tiling.blockLength);
    yGm.SetGlobalBuffer((__gm__ T*)shorterInputPtr + tiling.blockLength * AscendC::GetBlockIdx() / this->coef, tiling.blockLength / this->coef);
    zGm.SetGlobalBuffer((__gm__ T*)z + tiling.blockLength * AscendC::GetBlockIdx(), tiling.blockLength);
    } else {
    if (AscendC::GetBlockIdx() < tiling.formerNum) {
    this->tileNum = tiling.formerTileNum;
    this->tileLength = tiling.formerTileLength / BUFFER_NUM;
    this->lastTileLength = tiling.formerLastTileLength;
    xGm.SetGlobalBuffer((__gm__ T*)longerInputPtr + tiling.formerLength * AscendC::GetBlockIdx(),
    tiling.formerLength);
    yGm.SetGlobalBuffer((__gm__ T*)shorterInputPtr + tiling.formerLength * AscendC::GetBlockIdx() / this->coef, tiling.formerLength / this->coef);
    zGm.SetGlobalBuffer((__gm__ T*)z + tiling.formerLength * AscendC::GetBlockIdx(),
    tiling.formerLength);
    } else {
    this->tileNum = tiling.tailTileNum;
    this->tileLength = tiling.tailTileLength / BUFFER_NUM;
    this->lastTileLength = tiling.tailLastTileLength;
    xGm.SetGlobalBuffer((__gm__ T*)longerInputPtr + tiling.formerLength * tiling.formerNum + tiling.tailLength * (AscendC::GetBlockIdx() - tiling.formerNum), tiling.tailLength);
    yGm.SetGlobalBuffer((__gm__ T*)shorterInputPtr + tiling.formerLength * tiling.formerNum / this->coef + tiling.tailLength * (AscendC::GetBlockIdx() - tiling.formerNum) / this->coef, tiling.tailLength / this-
```

```cpp
>coef);
    zGm.SetGlobalBuffer((__gm__ T*)z + tiling.formerLength * tiling.formerNum + tiling.tailLength * (AscendC::GetBlockIdx() - tiling.formerNum), tiling.tailLength);
}
pipe->InitBuffer(inQueueX, BUFFER_NUM, this->tileLength * sizeof(T));
pipe->InitBuffer(inQueueY, BUFFER_NUM, this->coef * sizeof(T));
pipe->InitBuffer(outQueueZ, BUFFER_NUM, this->tileLength * sizeof(T));
pipe->InitBuffer(tmpBuf2, this->tileLength * sizeof(dataType)); 
```

由于数据是向coef对齐的，在数据拷贝的过程中可能会出现地址不满足32字节对齐的场景，因此CopyIn函数、CopyOut函数中使用DataCopyPad进行数据拷贝。


CopyIn函数实现代码如下：


```cpp
__aicore__ inline void CopyIn(int32_t progress)
{
    AscendC::LocalTensor<T> xLocal = inQueueX.AllocTensor<T>();
    AscendC::LocalTensor<T> yLocal = inQueueY.AllocTensor<T>();
    AscendC::DataCopyExtParams copyXParams = {1, (uint32_t)(this->tileLength * sizeof(T)), 0, 0, 0};
    AscendC::DataCopyExtParams copyYParams = {1, (uint32_t)(this->tileLength * sizeof(T) / this->coef), 0, 0};
    AscendC::DataCopyPadExtParams<T> padParams = {false, 0, 0, 0};
    if (progress == (this->tileNum * BUFFER_NUM - 1)) {
    AscendC::DataCopyPad<T>(xLocal, xGm[(progress - LAST_TWO_TILE) * this->tileLength + this->lastTileLength],
    copyXParams, padParams);
    AscendC::DataCopyPad<T>(yLocal, yGm[((progress - LAST_TWO_TILE) * this->tileLength + this->lastTileLength) / this->coef],
    copyYParams, padParams);
    } else {
    AscendC::DataCopyPad<T>(xLocal, xGm[progress * this->tileLength], copyXParams, padParams);
    AscendC::DataCopyPad<T>(yLocal, yGm[progress * this->tileLength / this->coef], copyYParams, padParams);
    }
    inQueueX.EnQue(xLocal);
    inQueueY.EnQue(yLocal);
} 
```


CopyOut函数实现代码如下：


```cpp
__aicore__ inline void CopyOut(int32_t progress)
{
    AscendC::LocalTensor<T> zLocal = outQueueZ.DeQue<T>();
    AscendC::DataCopyExtParams copyParams = {1, (uint32_t)(this->tileLength * sizeof(T)), 0, 0, 0};
    if (progress == (this->tileNum * BUFFER_NUM - 1)) {
    AscendC::DataCopyPad<T>(zGm[(progress - LAST_TWO_TILE) * this->tileLength + this->lastTileLength], zLocal, copyParams);
    } else {
    AscendC::DataCopyPad<T>(zGm[progress * this->tileLength], zLocal, copyParams);
    }
    outQueueZ.FreeTensor(zLocal);
} 
```

在Compute函数中，调用Add接口前需要先对输入进行Broadcast。这里需要计算Broadcast前后的shape。基于前文提到的数据关系，可以计算得出Broadcast前后的shape分别为{tileLength / broadcastCoef, 1}和{tileLength / broadcastCoef,broadcastCoef}。在此基础上对输入进行Broadcast，并将计算结果存入临时空间中，然后进行Add计算。实现代码示例如下所示：

```cpp
__aicore__ inline void Compute(int32_t progress)
{
    AscendC::LocalTensor<T> xLocal = inQueueX.DeQue<T>(); 
    AscendC::LocalTensor<T> yLocal = inQueueY.DeQue<T>(); 
    AscendC::LocalTensor<T> zLocal = outQueueZ.AllocTensor<T>(); 
    AscendC::LocalTensor<T> broadcastTmpTensor = tmpBuf2.Get<T>(); 
    uint32_t dstShape[] = {this->tileLength / this->coef, this->coef};
} 
```

```cpp
uint32_t srcShape[] = {this->tileLength / this->coef, 1};
AscendC::Broadcast<T, 2, 1>(broadcastTmpTensor, yLocal, dstShape, srcShape);
... 
```

# 3.3.2.7 非对齐场景

本节介绍非32字节对齐数据的更多处理方式，包括数据搬入、计算和搬出的处理。用户在实际算子开发中，可以参考如下方案介绍和算子样例灵活地处理非对齐场景。

# 数据搬运和 Vector 计算的对齐要求

# 须知

进行数据搬运和Vector计算时，对于搬运的数据长度和操作数的起始地址有如下的对齐要求：

● 使用DataCopy接口进行数据搬运，搬运的数据长度和操作数的起始地址（UB上）必须保证32字节对齐。

● 通常情况下，进行Vector计算时，操作数的起始地址必须保证32字节对齐。具体对齐要求需要查阅对应的API参考进行确认。

下文描述中的Global指Global Memory上的tensor，Local指Local Memory上的tensor。

下面是一些非对齐搬运和计算的例子。

# 非对齐搬入

当需要从Global拷贝11个half数值到Local时，为保证搬运的数据长度32字节对齐，使用DataCopy拷贝16个half（32B）数据到Local上，Local[11]~Local[15]被写成无效数据-1。


图3-19 非对齐搬入


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/b6385e01ac091bdd3f4681fc3599fe92e137ef8bdc1a4d53aaf276a657228138.jpg)


# 非对齐搬出

当需要从Local拷贝11个half数值到Global时，为保证搬运的数据长度32字节对齐，使用DataCopy拷贝16个half（32B）数据到Global上，Global[11]~Global[15]被写成无效数据-1。


图3-20 非对齐搬出


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/c6fab073c24dad6987f593d5993db75effbea238bf0345c744807e8f45eb4c70.jpg)


# 矢量计算起始地址非32字节对齐的错误示例

矢量计算时需要保证起始地址32字节对齐，如下的示例中，从Local1[7]，即LocalTensor的第8个数开始计算，起始地址不满足32字节对齐，是错误示例。


图 3-21 矢量计算起始地址非 32 字节对齐的错误示例


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/bbafb31c8f9630fdb2a6ced2df10619ff653607bc21c362009935628323505bd.jpg)


# 非对齐处理方案

DataCopyPad接口提供非对齐搬运的功能，如果基于该接口支持的产品开发算子（参见产品支持情况），则可以直接使用该接口解决非对齐场景下的搬运问题。使用DataCopyPad的完整示例请参考DataCopyPad样例。

部分型号不支持DataCopyPad接口，需要参考如下的方案处理。


图 3-22 非对齐处理方案示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/42454a1d19ad68d82b5199b36ab4701497419e37ff95bd811784183d13ce6444.jpg)


由于搬入时搬运的数据长度必须保证32字节对齐。数据长度非对齐的情况下，从Global逐行搬运Tensor数据到Local中，Local中每行都存在冗余数据。

搬入后，进行矢量计算时对冗余数据的处理方式有以下几种：

冗余数据参与计算。一般用于elewise计算场景。

通过mask参数掩掉冗余数据。一般用于轴归约计算等场景。

通过Duplicate逐行清零。计算前，针对每一行数据，调用基础API Duplicate对冗余数据位置填充0值。

通过Pad一次性清零。计算前，针对多行数据，可以采用高阶API Pad接口对冗余数据一次性清零。

由于搬出时搬运的数据长度和操作数的起始地址（UB上）必须保证32字节对齐，搬出时可以选择去除冗余数据或者带着冗余数据搬出的方式。

使用UnPad接口去除冗余数据后搬出。待搬出的有效数据总长度满足32字节对齐时，可使用高阶API UnPad接口去除冗余数据并完整搬出。

使用GatherMask收集有效数据后搬出。待搬出的有效数据总长度大于等于32字节时，可使用GatherMask重新收集有效数据，保证搬出的有效数据起始地址和数据长度32字节对齐。

带冗余数据搬出。注意多核处理时开启原子加（使用SetAtomicAdd接口），防止数据踩踏。

下面分别对上述几种处理方案做详细说明。

冗余数据参与计算

如下图所示，对前11个half数据进行Abs计算，冗余数据可以参与计算，不影响最终结果。步骤为：

a. 使用DataCopy从Global搬运16个half数据到Local1中，包含冗余数据-11~-15；

b. 直接使用Abs做整块计算，不用计算尾块大小，冗余数据参与计算。


图3-23 冗余数据参与计算


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/a8e71d0006794b035c022c077cb169384d53bc3706b83f594aa194c1fbf4e48b.jpg)


使用mask掩掉冗余数据

如下图所示，假设输入数据的shape为16 * 4，将输入数据搬入到UB后每行数据前4个half数据为有效数据，其余为冗余数据。为只对前4个half数据进行ReduceMin计算，可以通过设置mask参数的方法掩掉冗余数据。针对每行数据的处理步骤为：

a. 使用DataCopy从Global搬运16个half数据到Local1中；

b. 对归约计算的目的操作数Local2清零，如使用Duplicate等；

c. 进行归约操作，将ReduceMin的mask模式设置为前4个数据有效，从而掩掉冗余数据。


图 3-24 使用 mask 掩掉脏数据


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/d2b309d04d0d4d97588eea98099e7e7aa0c316d3b429a247232f797982faf782.jpg)


通过Duplicate逐行清零。

如下图所示，对于搬入后的非对齐数据，逐行进行Duplicate清零处理，步骤为：

a. 使用DataCopy从Global搬运16个half数据到Local中；

b. 使用基础API Duplicate，按照如下方式设置mask值，控制仅后5个元素位置有效，将冗余数据填充为0。

uint64_t mask0 = ((uint64_t)1 << 16) - ((uint64_t)1 << 11); 

uint64_t mask[2] = {mask0, 0}; 


图 3-25 通过 Duplicate 逐行清零


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/06f752d000e8f8a2e058f3f19ab89167c4621dd6f7697d8d0b42bc2202cee8ca.jpg)


通过Pad一次性清零。

如下图所示，假设输入数据的shape为16 * 6，搬入Local后大小为16 * 16，每行都包含冗余数据，逐行清零性能较差，可以使用Pad一次性清零，步骤为：

a. 将16 * 6的数据从GM上逐行搬入UB后，每行有6个有效数据；

b. 使用Pad接口将冗余数据位置填充为0。（对应Pad接口使用场景为：tensor的width已32B对齐，但是有部分冗余数据）。


图 3-26 通过 Pad 一次性清零


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/eadea3ab764ca0522074686e77794b03c8a6447a49d1cb6731127f13ff10689f.jpg)


使用UnPad接口去除冗余数据后搬出。

如下图所示，Local内存大小为16*16，每行中只有前6个数为有效数据， 要搬出的有效数据16 * 6满足32B对齐，可以使用UnPad接口去除冗余数据并完整搬出。步骤如下：

a. 使用UnPad高阶API去除冗余值；

b. 使用DataCopy搬运出连续的16 * 6个half数据到Global中。


图 3-27 使用 UnPad 接口去除冗余数据后搬出


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/afa5feb770f5b763768f234ac67c71430c35015405ec9dbc0b5c65eedd883fce.jpg)


使用GatherMask收集有效数据后搬出。

如下图所示，为搬出19个half数据到Global中，有16-18这3个数据的搬运无法满足对齐要求，使用GatherMask对有效数据进行重新收集，收集3-18这16个数据并搬出。步骤如下：

a. 完整拷贝前16个half（32B）数据到Global中；

b. 使用GatherMask接口，将Local1[3]~[18]的数Gather到Local2中，Local2从对齐地址开始；

c. 从Local2中搬运Gather的数据（32B整数倍）到Global中。


图 3-28 使用 GatherMask 收集有效数据后搬出。


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/6a380d1c38787032e476a88ffbd69665335057aa7b9fb3e37bceecf0ec3e1c51.jpg)


# 带冗余数据搬出

如下图所示，有4个核参与计算，每个核拷贝出4个数，每个核上拷贝的数据长度不满足32字节对齐，采用将冗余数据一起搬出的方式，步骤如下：

a. 将目标Global完整清零，可以通过在host清零或者在kernel侧用UB覆盖的方式处理；

b. 将本核内的Local数据，除了要搬出的4个有效数据，其余冗余部分清零（使用Duplicate接口）；

c. 使用原子累加的方式拷贝到Global，原子累加结合冗余数据清零，确保不会出现数据踩踏。


图3-29 带冗余数据搬出


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/3f6028a0af0222e4bf305d2f4bde0db3188b9dcc5f0be2716de39928c686851f.jpg)


# 样例介绍

# . 样例一：冗余数据参与计算+使用GatherMask收集有效数据后搬出。

本样例中展示了shape为128 * 18的tensor进行Abs计算的算子实现。针对每行数据的处理方案如下：

搬入后，每行数据的后14个数为冗余数据。Abs接口入参BLOCKLEN_CEIL为32个数，是18个数进行32字节对齐后的结果，有14个冗余数据参与计算。

AscendC::Abs(outputLocal, inputLocal, BLOCKLEN_CEIL); // main calculation 

计算完成后，通过GatherMask的bufPattern入参控制收集18个数中的后16个数。

```cpp
uint16_t tmpValue = 0;
AscendC::Duplicate<uint16_t>(bufPattern, tmpValue, 16);
bufPattern.SetValue(0, 0b1111111111111100); // select the last 14 elements of the first 1
bufPattern.SetValue(1, 0b0000000000000011); // select the first 2 elements of the next 16
uint32_t mask = 32;
uint64_t rsvdCnt = 0;
AscendC::LocalTensor<half> tailLocal = outQueueTail.AllocTensor<half>();
AscendC::GatherMask(tailLocal, outputLocal, bufPattern, true, mask, {1, 1, 8, 8}, rsvdCnt); 
```

首先使用DataCopy搬运前16个数，然后搬运后16个数，中间的14个数存在重复搬运。注意：因为DataCopy的目的地址存在重叠所以需要通过PipeBarrier添加流水同步。

```c
uint32_t copyLenMain = TILE_LENGTH * sizeof(half) / 32 * 32 / sizeof(half);
uint32_t offsetMain = progress * TILE_LENGTH;
AscendC::DataCopy(dstGlobal[offsetMain], outputLocal, copyLenMain);
AscendC::PipeBarrier<PIPE_MTE3>();
uint32_t tailLen = 32 / sizeof(half);
uint32_t offsetTail = offsetMain + (TILE_LENGTH - tailLen);
AscendC::DataCopy(dstGlobal[offsetTail], tailLocal, tailLen); 
```

搬入时要保证32字节对齐，所以要将输入的最后一行补齐到32字节对齐，防止访问非法数据，main.cpp中对GM上输入的长度的定义如下：

```c
size_t inputByteSize = 2318 * sizeof(int16_t); // 2318 = 2304 + 32 - 18
size_t outputByteSize = 2304 * sizeof(int16_t); 
```

样例二：通过Duplicate逐行清零+带冗余数据搬出。

本样例中展示了shape为64 * 11的tensor进行Abs计算的算子实现。 共使用4个核，每个核处理16 * 11个数据。

搬入后，每行数据的后5个数为冗余数据。通过Duplicate接口对每行数据中的后5个数据进行清零。

```cpp
// mask mode controls only the last 5 elements doing Duplicate
uint64_t mask0 = (1ul << 16) - (1ul << BLOCK_ELEMENT_NUM);
uint64_t mask[2] = {mask0, 0};
for (int32_t i = 0; i < BLOCK_GROUP_NUM; i++) {
    AscendC::Duplicate<half>(inputLocal[i * BLOCKLEN_CEIL], 0, mask, 1, 1, 1); // clear dummy data on inputLocal
}
AscendC::Abs(outputLocal, inputLocal, BLOCKLEN_CEIL * BLOCK_GROUP_NUM); 
```

搬出时，带冗余数据搬出并开启原子累加，BLOCKLEN_CEIL中包含冗余数据。

```cpp
AscendC::SetAtomicAdd<half>();
for (int32_t i = 0; i < BLOCK_GROUP_NUM; i++) {
    AscendC::DataCopy<half>(dstGlobal[i * BLOCK_ELEMENT_NUM], outputLocal[i * BLOCKLEN_CEIL], BLOCKLEN_CEIL);
}
AscendC::DisableDmaAtomic(); 
```

所以在初始化时，需要对GM数据进行清零，清零代码如下，示例中多个核都调用Fill接口进行清零，需要调用SyncAll进行核间同步。

```cpp
AscendC::Fill<half>(dstGlobal, blockLength, 0);
pipe.InitBuffer(inQueue, BUFFER_NUM, BLOCK_GROUP_NUM * BLOCKLEN_CEIL * sizeof(half));
pipe.InitBuffer(outQueue, BUFFER_NUM, BLOCK_GROUP_NUM * BLOCKLEN_CEIL * sizeof(half));
pipe.InitBuffer(syncLocalTbuf, USE_CORE_NUM * DEFAULT_SYNCALL_NEED_SIZE * sizeof(int32_t));
AscendC::LocalTensor<int32_t> SyncLocal = syncLocalTbuf.Get<int32_t>();
AscendC::SyncAll(syncGlobal, SyncLocal, USE_CORE_NUM); 
```

搬入时要保证32字节对齐，需要将输入的最后一行补齐到32字节对齐，防止访问非法数据；搬出时带冗余数据搬出，输出的最后一行也需要补齐到32字节对齐。main.cpp中对GM上输入输出的长度的定义如下：

```c
uint32_t elemLength = 709; // TOTAL_LENGTH + (BLOCKLEN_CEIL - BLOCK_ELEMENT_NUM)  
// copy in borrow the next (BLOCKLEN_CEIL - BLOCK_ELEMENT_NUM) elements of srcGM size_t inputByteSize = elemLength * sizeof(int16_t);  
// copy out atomic add extra (BLOCKLEN_CEIL - BLOCK_ELEMENT_NUM) zeros to dstGM size_t outputByteSize = elemLength * sizeof(int16_t); 
```

样例三：冗余数据参与计算+使用UnPad接口去除冗余数据后搬出。

本样例中展示了shape为2048 * 14的tensor进行Abs计算的算子实现。 共使用8个核，每个核处理256 * 14个数据。

搬入后，每行数据的后2个数为冗余数据。Abs接口入参BLOCK_GROUP_NUM *BLOCKLEN_CEIL为连续的16行数据，每行16个数，每行的冗余数据参与计算。

AscendC::Abs(inputLocal, inputLocal, BLOCK_GROUP_NUM * BLOCKLEN_CEIL); // main calculation计算后，使用UnPad接口去除冗余数据后再搬出，通过unPadParams.rightPad参数控制去除每行最后的2个冗余数据。

unPadParams.rightPad = BLOCKLEN_CEIL - BLOCK_ELEMENT_NUM; // delete 2 dummy half each row AscendC::UnPad<half>(outputLocal, inputLocal, unPadParams, this->tiling); 

注意：UnPad接口需要传入tiling参数。abs_unpad_tiling.cpp中关键计算过程如下：

```cpp
AscendC::GetUnPadMaxMinTmpSize(*ascendcPlatform, srcShape, sizeof(int16_t), tmpMaxSize, tmpMinSize);
optiling::UnPadTiling tilingData;
AscendC::UnPadTilingFunc(srcShape, tmpMaxSize, sizeof(int16_t), tilingData); 
```

tiling参数需要通过核函数的入参传入到kernel侧，供UnPad高阶API使用。

abs_unpad_custom<<<numBlocks, nullptr, stream>>>(xDevice, zDevice, tilingDevice); 

搬入时要保证32字节对齐，所以要将输入的最后一行补齐到32字节对齐，防止访问非法数据，main.cpp中对GM上输入的长度的定义如下。

```c
// 28674 is TOTAL_LENGTH + (BLOCKLEN_CEIL - BLOCK_ELEMENT_NUM)
// 28672 is TOTAL_LENGTH
// copy in borrow the next (BLOCKLEN_CEIL - BLOCK_ELEMENT_NUM) elements of srcGM
uint32_t oriLength = 28672;
uint32_t colNum = 14;
uint32_t maxColNum = 32 / sizeof(uint16_t);
uint32_t padLength = oriLength + maxColNum - colNum;
size_t inputByteSize = padLength * sizeof(int16_t);
size_t outputByteSize = oriLength * sizeof(int16_t); 
```

示例四：通过Pad一次性清零+带冗余数据搬出。

本样例中展示了shape为2048 * 7的tensor进行Abs计算的算子实现。 共使用8个核，每个核处理256 * 7个数据。

搬入后，每行数据的后9个数为冗余数据。每个核上通过Pad接口将256 * 9的冗余数据块整体清零后参与Abs计算。

```cpp
AscendC::PadParams padParams = {0, BLOCKLEN_CEIL - BLOCK_ELEMENT_NUM, 0};
AscendC::Pad(outputLocal, inputLocal, padParams, this->tiling);
AscendC::Abs(outputLocal, outputLocal, BLOCK_GROUP_NUM * BLOCKLEN_CEIL); // main calculation 
```

计算后带冗余数据搬出的代码解释和样例二一致。

注意：Pad接口需要传入tiling参数。abs_pad_tiling.cpp中关键计算过程如下：

```txt
AscendC::GetPadMaxMinTmpSize(srcShape, sizeof(int16_t), tmpMaxSize, tmpMinSize);
optiling::PadTiling tilingData;
AscendC::PadTilingFunc(srcShape, oriSrcShape, tmpMaxSize, sizeof(int16_t), tilingData); 
```

tiling参数需要通过核函数的入参传入到kernel侧，供Pad高阶API使用。

abs_pad_custom<<<numBlocks, nullptr, stream>>>(xDevice, zDevice, tilingDevice); 

搬入时要保证32字节对齐，需要将输入的最后一行补齐到32字节对齐，防止访问非法数据；搬出时带冗余数据搬出，输出的最后一行也需要补齐到32字节对齐。main.cpp中对GM上输入输出的长度的定义如下：

```c
// 14336 is the length of input data
uint32_t oriLength = 14336;
// we must allocate more space to prevent invalid address access
uint32_t padLength = oriLength + shapePad[1] - shapeUsed[1];
size_t inputByteSize = padLength * sizeof(int16_t);
size_t outputByteSize = padLength * sizeof(int16_t);
// however, original length must be used when output to file
size_t outputFileSize = oriLength * sizeof(int16_t); 
```

样例五：使用mask掩掉冗余数据+带冗余数据搬出。

本样例中展示了shape为16 * 4的tensor每行数据进行ReduceMin计算的算子实现。 共使用4个核，每个核处理4 * 4个数据。

搬入后，每行数据的后12个数为冗余数据。通过ReduceMin的入参Mask控制只有前4个数参与计算。

```cpp
uint64_t Mask0 = ((uint64_t)1 << BLOCK_ELEMENT_NUM) - 1; // mask mode controls only the first 4 elements do ReduceMin calculation
uint64_t Mask[2] = {Mask0, 0};
// main calculation
for (int i = 0; i < BLOCK_GROUP_NUM; i++) {
    AscendC::ReduceMin<half>(outputLocal[i * BLOCKLEN_CEIL], inputLocal[i * BLOCKLEN_CEIL], workLocal, Mask, 1, 8, false);
}
outQueue.EnQue<half>(outputLocal);
inQueue.FreeTensor(inputLocal); 
```

计算后带冗余数据搬出的代码解释和样例二一致。

搬入时要保证32字节对齐，需要将输入的最后一行补齐到32字节对齐，防止访问非法数据；搬出时带冗余数据搬出，输出的最后一行也需要补齐到32字节对齐。main.cpp中对GM上输入输出的长度的定义如下：

```c
// copy in borrow the next (BLOCKLEN_CEIL - BLOCK_ELEMENT_NUM) elements of srcGM size_t inputByteSize = 76 * sizeof(int16_t);
// copy out atomic add extra (BLOCKLEN_CEIL - BLOCK_ELEMENT_NUM) zeros to dstGM size_t outputByteSize = 76 * sizeof(int16_t); 
```

# 3.3.3 矩阵编程（高阶 API）

# 3.3.3.1 基础知识

# 说明

本节内容为使用高阶API进行矩阵乘法的编程指导。使用高阶API进行实际的矩阵编程时，需要通过API参考确认接口支持的产品型号。

# 矩阵乘法概述

Matmul的计算公式为：C = A * B + bias，其示意图如下。

● A、B为源操作数，A为左矩阵，形状为[M, K]；B为右矩阵，形状为[K, N]。

C为目的操作数，存放矩阵乘结果的矩阵，形状为[M, N]。

bias为矩阵乘偏置，形状为[1, N]。对A*B结果矩阵的每一行都采用该bias进行偏置。


图 3-30 Matmul 矩阵乘示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/d9fdb2775630d2c49b822e17d9769f32bb26082eb62a5913b1601868e4601d14.jpg)


# 矩阵乘法数据流

在了解矩阵乘法数据流之前，需要先回顾一下几个重要的存储逻辑位置的概念：

搬入数据的存放位置：A1，用于存放整块A矩阵，可类比CPU多级缓存中的二级缓存；

搬入数据的存放位置：B1，用于存放整块B矩阵，可类比CPU多级缓存中的二级缓存；

搬入数据的存放位置：C1，用于存放整块的矩阵乘偏置Bias矩阵，可类比CPU多级缓存中的二级缓存；

搬入数据的存放位置：A2，用于存放切分后的小块A矩阵，可类比CPU多级缓存中的一级缓存；

搬入数据的存放位置：B2，用于存放切分后的小块B矩阵，可类比CPU多级缓存中的一级缓存；

搬入数据的存放位置：C2，用于存放切分后的小块矩阵乘偏置Bias矩阵，可类比CPU多级缓存中的一级缓存；

● 结果数据的存放位置：CO1，用于存放小块结果C矩阵，可理解为Cube Out；

● 结果数据的存放位置：CO2，用于存放整块结果C矩阵，可理解为Cube Out；

● 搬入数据的存放位置：VECCALC，一般在计算需要临时变量时使用此位置。

矩阵乘法数据流指矩阵乘的输入输出在各存储位置间的流向。逻辑位置的数据流向如下图所示（为了简化描述，没有列出bias）：

A矩阵从输入位置到A2的数据流如下（输入位置可以是GM或者VECOUT）：GM->A2，GM->A1->A2；VECOUT->A1->A2。

由于A1比A2的空间更大，数据从GM或VECOUT可以先搬入A1进行缓存，待该数据执行Cube计算前，数据直接从A1搬入A2，这样在搬运大量数据时可以减少计算前的等待时间，提升性能，只有在搬入数据较少的场景才可能使用GM->A2的数据流。

B矩阵从输入位置到B2的数据流如下（输入位置可以是GM或者VECOUT）：GM->B2，GM->B1->B2；VECOUT->B1->B2。

由于B1比B2的空间更大，数据从GM或VECOUT可以先搬入B1进行缓存，待该数据执行Cube计算前，数据直接从B1搬入B2，这样在搬运大量数据时可以减少计算前的等待时间，提升性能，只有在搬入数据较少的场景才可能使用GM->B2的数据流。

完成A2*B2=CO1计算。

CO1数据汇聚到CO2：CO1->CO2。

从CO2到输出位置（输出位置可以是GM或者VECIN）：CO2->GM/CO2->VECIN。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/cf7f87c0005aa7dfae701abdf9577c62d643aa6ca015b345aee2028264f92e00.jpg)


# 数据格式

在完成Matmul矩阵乘法时，主要涉及到两种分形格式ND和NZ。其它的数据格式请参考数据排布格式。

ND：普通格式，N维张量。

● NZ：为满足AI Core中Cube计算单元高性能计算的需要，引入该特殊格式。

ND –> NZ的变换过程为：

$$
\begin{array}{l} (\dots , N, H, W) \text {- - > pad- - > (\dots , N, H1*H0, W1*W0)- - > reshape- - > (\dots , N, H1, H0, W1, W0)- - > transpose- - > (\dots , N,} \\ W 1, H 1, H 0, W 0) \end{array}
$$

如下图所示 （W，H）大小的矩阵被分为（H1*W1）个分形，按照列优先排布，形状如N字形；每个分形内部有（H0*W0）个元素，按照行优先排布，形状如z字形。所以这种数据格式称为NZ（大N小Z）格式。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/114c61a7515d518623a1f261363d1ace9fafaf37be8acc482fc20043f0aa2900.jpg)


下面我们再通过一个具体的例子来深入理解ND和NZ格式的数据排布区别。假设分形格式为2*2，如下图所示4*4的矩阵，ND（1，4，4）和NZ（1，2，2，2，2）格式存储的情况下，数据在内存中的排布格式分别为：

ND: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 

NZ: 0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15 

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/2e97b9c870c3a6731d8b83874aa4e9242619b09e5e2d7b9b04bd6c50406cc1e5.jpg)


关于矩阵ND到NZ格式转换的样例请参考Matmul输入矩阵ND到NZ格式转换的算子样例。

# 数据分块（Tiling）

# 多核切分

为了实现多核并行，需要将矩阵数据进行切分，分配到不同的核上进行处理。切分策略如下图所示：

对于A矩阵，沿着M轴进行切分，切分成多份的singleCoreM，单核上处理SingleCoreM * K大小的数据。

对于B矩阵，沿着N轴进行切分，切分成多份的singleCoreN，单核上处理K *SingleCoreN大小的数据。

对于C矩阵，SingleCoreM * K大小的A矩阵和K * SingleCoreN大小的B矩阵相乘得到SingleCoreM * SingleCoreN大小的C矩阵，即为单核上输出的C矩阵大小。

比如，下图中共有8个核参与计算，将A矩阵沿着M轴划分为4块，将B矩阵沿着N轴切分为两块，单核上仅处理某一分块（比如图中绿色部分为core3上参与计算的数据）：SingleCoreM * K大小的A矩阵分块和SingleCoreN* K大小的B矩阵分块相乘得到SingleCoreM * SingleCoreN大小的C矩阵分块。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/86aecfc2c52f04f2dc35383245ccb81c1d8666d303a6d8f5b6b1900a07971aec.jpg)



矩阵A



×


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/d5f01e2d523e8349daab676d717e62f6641537f15599488dc0d7e860fe2793cf.jpg)



矩阵B


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/7c7910648e004f4a1a0b64599abdf19ee8c8978f1f1fc1be1c6db00d0047e6ac.jpg)



矩阵c


另外，单核上处理的K轴长度为SingleCoreK，对于K轴较大的场景，可以沿着K轴进行切分，切分成多份的singleCoreK，详细案例介绍请参考Matmul高阶API使能多核切K。

# 核内切分

大多数情况下，Local Memory的存储，无法完整的容纳算子的输入与输出，需要每次搬运一部分输入进行计算然后搬出，再搬运下一部分输入进行计算，直到得到完整的最终结果，也就是需要做核内的输入切分。切分的策略如下所示：

对于A矩阵，沿M轴进行切分，将singleCoreM切分成多份的baseM，切分成的份数对应图示的mIter；沿K轴进行切分，切分成多份的baseK。

对于B矩阵，沿N轴进行切分，将singleCoreN切分成多份的baseN，切分成的份数对应图示的nIter；沿K轴进行切分，切分成多份的baseK。

对于C矩阵，A矩阵中baseM*baseK大小的分块和B矩阵中baseK*baseN大小的分块相乘并累加，得到C矩阵中对应位置baseM*baseN大小的分块。比如，图中结果矩阵中的绿色矩阵块5是通过如下的累加过程得到的：a*a+b*b+c*c+d*d+e*e+f*f。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/30ec8f8211a27b7615635bda15556b16eb885c0d8fea3fb94e0a059a8caf8ce0.jpg)


除了基本块形状baseM, baseN, baseK外，还有一些常用的tiling参数，其含义如下：

iterateOrder：一次Iterate迭代计算出[baseM, baseN]大小的C矩阵分片。Iterate完成后，Matmul会自动偏移下一次Iterate输出的C矩阵位置，iterateOrder表示自动偏移的顺序。

0代表先往M轴方向偏移再往N轴方向偏移；

1代表先往N轴方向偏移再往M轴方向偏移。

在上图的示例中，iterateOrder取值为0。

depthA1，depthB1：A1、B1上存储的矩阵片全载A2、B2的份数，A2、B2存储大小分别是baseM * baseK、baseN * baseK，即depthA1是A1矩阵切片含有baseM * baseK块的个数，depthB1是B1矩阵切片含有baseN * baseK块的个数。

stepM，stepN：stepM为左矩阵在A1中缓存的buffer M方向上baseM的倍数，stepN为右矩阵在B1中缓存的buffer N方向上baseN的倍数。

stepKa，stepKb：stepKa为左矩阵在A1中缓存的buffer K方向上baseK的倍数，stepKb为右矩阵在B1中缓存的buffer K方向上baseK的倍数。

# 3.3.3.2 算子实现

# 实现流程

上文介绍了Matmul矩阵乘的数据切分方案和数据流。Ascend C提供一组Matmul高阶API，封装了这些常用的切分和数据搬运、计算的算法逻辑，方便用户快速实现Matmul矩阵乘法的运算操作。开发者在host侧通过调用API自动获取Tiling参数，该参数传递到kernel侧后，在初始化操作时传入，通过几个简单的API即可完成矩阵乘操作。完整样例请参考LINK。


图3-31 矩阵编程流程示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/66dfb6bdae1f6e110df11b3ddc2ab9aa61a4a23a4823a2f22d2a20938a7d847e.jpg)


host侧自动获取Tiling参数的关键步骤介绍如下：

步骤1 创建Tiling对象。

```cpp
auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance(); matmul_tiling::MultiCoreMatmulTiling tilingApi(*ascendcPlatform); 
```

传入硬件平台信息创建PlatformAscendC对象，然后创建Tiling对象，硬件平台信息可以通过GetPlatformInfo获取。

步骤2 设置参与Matmul运算的核数，A、B、Bias的内存逻辑位置、格式和数据类型。

```txt
tilingApi.SetDim(ascendcPlatform->GetCoreNumAic());
tilingApi.SetAType(AscendC::TPosition::GM, CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
tilingApi.SetBType(AscendC::TPosition::GM, CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
tilingApi.SetCType(AscendC::TPosition::GM, CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT);
tilingApi.SetBiasType(AscendC::TPosition::GM, CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT); 
```

步骤3 设置矩阵shape信息。

```txt
tilingApi.SetShape(M, N, K);
tilingApi.SetOrgShape(M, N, K); // 设置原始完整的形状M、N、K
```

步骤4 设置可用空间大小信息。

设置Matmul计算时可用的L1 Buffer/L0C Buffer/Unified Buffer空间大小，-1表示AI处理器对应Buffer的大小。

```javascript
tilingApi.SetBufferSpace(-1, -1, -1); 
```

步骤5 按需设置其他参数，比如设置bias参与计算。

```javascript
tilingApi.EnableBias(true); 
```

步骤6 获取Tiling参数。

```cpp
int64_t res = tilingApi.GetTiling(tilingData);
if (res == -1) {
    std::cout << "gen tiling failed" << std::endl;
} 
```

步骤7 Tiling参数的序列化保存等其他操作。

```javascript
uint32_t tcubeTilingSize = tilingData dimensionalSize();  
tilingData.SaveToBuffer(tilingBuf, tcubeTilingSize); 
```

# ----结束

kernel侧使用Matmul API矩阵乘运算的具体步骤如下：

# 步骤1 创建Matmul对象

创建Matmul对象的示例如下：

纯Cube模式（只有矩阵计算）场景下，建议在代码中定义ASCENDC_CUBE_ONLY宏，避免额外的性能开销。本节内容以纯Cube模式举例。

默认为MIX模式（包含矩阵计算和矢量计算），该场景下通常不定义ASCENDC_CUBE_ONLY宏，如果在程序中使用了ASCENDC_CUBE_ONLY宏，则必须使用ASCEND_IS_AIC宏和ASCEND_IS_AIV宏将Cube计算和Vector计算隔离开，更多内容请参考3.3.5 融合算子编程。

// 纯Cube模式（只有矩阵计算）场景下，需要设置该代码宏，并且必须在#include "lib/matmul_intf.h"之前设置#define ASCENDC_CUBE_ONLY

#include "lib/matmul_intf.h" 

typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, half> aType; 

typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, half> bType; 

typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, float> cType; 

typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, float> biasType; 

AscendC::Matmul<aType, bType, cType, biasType> mm; 

创建对象时需要传入A、B、C、Bias的参数类型信息， 类型信息通过MatmulType来定义，包括：内存逻辑位置、数据格式、数据类型。

# 步骤2 初始化操作。

REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), mm, &tiling); // 初始化

# 说明

Matmul高阶API内部实现时需要使用系统workspace（即对应本步骤中的GetSysWorkSpacePtr接口），开发者需要自行申请系统workspace的空间：

```txt
- 在host侧Tiling实现时，设置总的workspace的数值大小（包含用户workspace和系统workspace），workspace空间由框架来申请并管理。系统workspace的空间大小通过6.4.2.1.13 GetLibApiWorkSpaceSize获取。
size_t userWorkspaceSize = 0;
size_t systemWorkspaceSize = static_cast<size_t>(ascendcPlatform.GetLibApiWorkSpaceSize());
size_t *currentWorkspace = context->GetWorkspaceSizes(1);
currentWorkspace[0] = userWorkspaceSize + systemWorkspaceSize;
```

```txt
- 若算子工程不是自定义算子工程，也不是带有HAVE_WORKSPACE编译宏的Kernel直调算子工程，框架不会自动设置workspace，需要在kernel侧的Matmul初始化前，通过SetSysWorkSpace设置系统workspace。
// 使用Matmul时必须设置workspace空间
SetSysWorkspace(workspace);
if (GetSysWorkSpacePtr() == nullptr) {
    return;
}
```

# 步骤3 设置左矩阵A、右矩阵B、Bias。

```javascript
mm.SetTensorA(gm_a); // 设置左矩阵A
mm.SetTensorB(gm_b); // 设置右矩阵B
mm.SetBias(gm_bias); // 设置Bias
```

# 步骤4 完成矩阵乘操作。

调用Iterate完成单次迭代计算，叠加while循环完成单核全量数据的计算。Iterate方式，可以自行控制迭代次数，完成所需数据量的计算，方式比较灵活。

```javascript
while (mm.Iterate()) {
    mm.GetTensorC(gm_c);
} 
```

调用IterateAll完成单核上所有数据的计算。IterateAll方式，无需循环迭代，使用比较简单。mm.IterateAll(gm_c);

# 步骤5 结束矩阵乘操作。

```javascript
mm.End(); 
```

----结束

# 设置 Shape 信息

在实现Host Tiling时可以设置Shape信息，用于Tiling计算；kernel侧运行时也可以修改部分Shape信息，用于尾块设置、Matmul复用（多个Matmul计算复用一个Matmul对象）等场景。本节对涉及到的Shape概念进行介绍，并给出host侧和kernel侧设置Tiling信息的指导。

orgShape：M、N、K 

singleCoreShape：singleCoreM、singleCoreN、singleCoreK 

singleShape：singleM、singleN、singleK 

baseShape：baseM、baseN、baseK 

通过数据分块（Tiling）的介绍我们已经了解了orgShape(M、N、K)，singleCoreShape(singleCoreM、singleCoreN、singleCoreK)，baseShape(baseM、baseN、baseK)的概念，如下图所示：

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/f991b1bbed68bf81842d7383e05546bb27c1b12c3fb2b75804b42ae2e2f5b867.jpg)


除此之外，单核的Matmul Tiling时，实际参与Matmul计算的shape可以是原始shape中的一部分，singleM, singleN, singleK用于表达实际参与Matmul计算的shape，如下图所示。在单核的情况下，singleM, singleN, singleK会透传给singleCoreM,singleCoreN, singleCoreK。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/894417001e9f90fa38a030abda7c25fba8df0498295ff2529dc2f4870cd04f39.jpg)


Kernel运行时设置

SetTail、SetSingleShape都是运行时修改singleCoreM、singleCoreN、singleCoreK，处理尾块时使用SetTail，Matmul复用（多个Matmul计算复用一个Matmul对象）的场景可以使用SetSingleShape重新设置。

SetOrgShape是运行时修改M、N、K，Matmul复用的场景可以使用SetOrgShape重新设置。

单核Tiling时设置

SetOrgShape（必选）：设置M、N、K

SetShape（非必选）： 设置singleM、singleN、singleK，等同于设置singleCoreM、singleCoreN、singleCoreK

SetFixSplit（非必选）：设置baseM、baseN、baseK

多核Tiling时设置

SetOrgShape（必选）：设置M、N、K

SetShape（非必选）： 设置singleM、singleN、singleK

SetFixSplit（非必选）：设置baseM、baseN、baseK

SetSingleShape(非必选)： 设置singleCoreM、singleCoreN、singleCoreK

SetSingleRange(非必选) ：设置singleCoreM、singleCoreN、singleCoreK的范围

# 设置 format 格式

创建Matmul对象时需要传入A、B、C、Bias的参数类型信息， 类型信息通过MatmulType来定义，包括：内存逻辑位置、数据格式、数据类型。示例如下：

typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, half> aType; typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, half> bType; 

typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, float> cType; typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, float> biasType; AscendC::Matmul<aType, bType, cType, biasType> mm; 

针对数据格式，包括CubeFormat::ND, CubeFormat::NZ, CubeFormat::ND_ALIGN三种，ND和NZ格式在数据格式章节已经介绍，ND_ALIGN格式的介绍请参考数据排布格式。

# 3.3.3.3 特性场景

# 3.3.3.3.1 Matmul 特性介绍

除了前述基础知识和算子实现中介绍的基本计算能力外，Matmul矩阵编程还提供了适用于不同场景的处理能力及多种功能，具体场景和功能列于下表中，详细内容请见后续章节。


表 3-4 Matmul 功能特性表


<table><tr><td>特性描述</td><td>功能简介</td></tr><tr><td>多核对齐切分</td><td>在多核场景中,支持将矩阵数据沿M、N、K轴切分,满足M能被singleCoreM整除、N能被singleCoreN整除、K能被singleCoreK整除的对齐场景时的处理方式,从而实现多核并行计算矩阵乘。</td></tr><tr><td>多核非对齐切分</td><td>在多核场景中,支持将矩阵数据沿M、N、K轴切分。当出现M不能被singleCoreM整除、或N不能被singleCoreN整除、或K不能被singleCoreK整除的非对齐场景(即尾块场景)时的处理方式。</td></tr><tr><td>异步场景处理</td><td>MIX场景(包含矩阵计算和矢量计算)下不需要等待矩阵乘计算完成,先执行其它计算。</td></tr><tr><td>矩阵乘输出的量化/反量化</td><td>将矩阵乘的计算结果从CO1搬出到Global Memory时,对矩阵元素执行数据量化或反量化操作。</td></tr><tr><td>自定义数据搬入搬出</td><td>自定义矩阵乘计算前后的数据搬运函数。本功能支持用户实现左矩阵A、右矩阵B从Global Memory分别自定义搬入到A1、B1的过程,输出C矩阵从CO1自定义搬出到Global Memory的过程。</td></tr><tr><td>矩阵乘输出的Channel拆分</td><td>矩阵乘输出的Channel拆分,又称ChannelSplit。指float数据类型、NZ数据格式的输出C矩阵按照16*8的分形大小存储。</td></tr><tr><td>矩阵向量乘</td><td>矩阵向量乘即GEMV,指矩阵乘计算中M=1,K&gt;1的场景,即对形状为(1,K)的左矩阵A实现矩阵乘计算。</td></tr><tr><td>4:2稀疏矩阵乘</td><td>4:2稀疏矩阵乘,又称Sparse Matmul。指对稀疏左矩阵A和4:2稠密化的右矩阵B实现矩阵乘计算。</td></tr><tr><td>上/下三角矩阵乘</td><td>忽略位于矩阵中下三角或上三角位置的元素的计算,实现矩阵中上三角或下三角位置的元素的矩阵乘计算。</td></tr><tr><td>TSCM输入的矩阵乘</td><td>对内存逻辑位置为TSCM的左矩阵A或右矩阵B实现矩阵乘计算。</td></tr><tr><td>矩阵乘输出的N方向对齐</td><td>矩阵乘输出的N方向对齐,又称ND_ALIGN格式输出。指对数据格式为ND_ALIGN的输出C矩阵实现N方向32字节对齐的自动补齐及输出。</td></tr><tr><td>单次矩阵乘局部输出</td><td>单次矩阵乘局部输出,又称Partial Output,指矩阵乘计算时不对单核K方向的计算结果做累加,直接输出计算结果。</td></tr><tr><td>AIC和AIV独立运行机制</td><td>AIC和AIV独立运行机制,又称双主模式。MIX场景(包含矩阵计算和矢量计算)下AIC核和AIV核独立运行代码,不依赖消息驱动。</td></tr><tr><td>MxMatmul场景</td><td>带有量化系数的矩阵乘法,即左矩阵和右矩阵均有对应的量化系数矩阵,对左矩阵和右矩阵分别量化后再做矩阵乘计算。</td></tr></table>


表 3-5 BatchMatmu 功能 l 特性表


<table><tr><td>特性描述</td><td>功能简介</td></tr><tr><td>Batch Matmul基础功能</td><td>Batch Matmul基础功能,支持批量处理Matmul,调用一次IterateBatch接口,计算出多个singleCoreM * singleCoreN大小的C矩阵。</td></tr><tr><td>Batch Matmul复用Bias矩阵</td><td>每个Batch的Matmul计算复用同一个不带Batch轴的Bias矩阵。</td></tr></table>

# 3.3.3.3.2 多核对齐切分

# 功能介绍

为了实现多核并行，提升计算效率，需要将矩阵数据进行切分，分配到不同的核上进行处理。主要的切分策略有切分K轴和不切分K轴两种。

不切分K轴、仅切分M、N轴的策略如下：

对于A矩阵，沿着M轴进行切分，切分成多份的singleCoreM，单核上处理SingleCoreM * K大小的数据。

对于B矩阵，沿着N轴进行切分，切分成多份的singleCoreN，单核上处理K *SingleCoreN大小的数据。

对于C矩阵，SingleCoreM * K大小的A矩阵和K * SingleCoreN大小的B矩阵相乘得到SingleCoreM * SingleCoreN大小的C矩阵，即为单核上输出的C矩阵大小。

比如，下图中共有8个核参与计算，将A矩阵沿着M轴划分为4块，将B矩阵沿着N轴切分为两块，单核上仅处理某一分块（比如图中绿色部分为core5上参与计算的数据）：SingleCoreM * K大小的A矩阵分块和SingleCoreN * K大小的B矩阵分块相乘得到SingleCoreM * SingleCoreN大小的C矩阵分块。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/08b89bba58e9d6ca5d52448e59415f7994178c61a69beb27decdb0073266720c.jpg)



矩阵A



×


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/932a431680a5b00329a67bddf2d0b32648fc19e1d655c56e5488737d832e7ef0.jpg)



矩阵b



一


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/bc75b45f7d3958b80ebecd4a9688fbd0dd109a615c81aa03e151ea9b8d681026.jpg)



矩阵c


切分M、N、K轴的策略如下图所示：

对于A矩阵，沿着M轴进行切分，切分成多份的singleCoreM，沿着K轴切分，切分成多份的singleCoreK，单核上处理singleCoreM * singleCoreK大小的数据。

对于B矩阵，沿着K轴进行切分，切分成多份的singleCoreK，沿着N轴进行切分，切分成多份的singleCoreN，单核上处理singleCoreK * singleCoreN大小的数据。

对于C矩阵，singleCoreM * singleCoreK大小的A矩阵与singleCoreK *singleCoreN大小的B矩阵相乘并累加得到singleCoreM * singleCoreN大小的C矩阵分块。

比如下图中，C矩阵中的R矩阵块，是通过A1*B1+A2*B2+A3*B3累加得到的，其中，A1*B1、A2*B2、A3*B3可在多个核上并行计算。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/7d906e639c0c4fdd13de2cb6b632becfeb3935743156a9861c1aa4fb658313a4.jpg)



矩阵A


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/55a070c00f5f81bed57dcae3309cc48e09963108372515e810235846fc2ff87d.jpg)



矩阵B


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/a5d9d5e2e470f8e26f2e5b4b39a1a4947d6809325f45285a097c81d8b06b8db2.jpg)



矩阵c


上述的切分策略会在Tiling参数中体现，比如SingleCoreM、SingleCoreN、SingleCoreK，开发者在host侧通过调用API自动获取Tiling参数，与单核场景的不同的是，多核Tiling需要使用MultiCoreMatmulTiling构造多核Tiling对象，并通过SetDim接口设置Matmul计算所用的核数。注意：这里设置的核数为Matmul计算可用的核数，仅在多核场景下设置，用于计算tiling参数；SetBlockDim为整个算子计算所用核数，是实际会加载的核数，是必须设置的。SetBlockDim的设置规则请参考numBlocks的说明。SetDim的设置规则如下：

纯Cube模式（只有矩阵计算）场景，本节内容以纯Cube模式举例。

SetDim设置当前AI处理器可用的核数，通过Tiling计算得到执行Matmul计算实际使用的核数，实际使用的核数小于等于AI处理器可用的核数。SetBlockDim按照实际使用的核数由用户进行配置。

● MIX模式（包含矩阵计算和矢量计算）的设置规则请参考MIX场景核数设置规则。

# 使用场景

多核处理Matmul矩阵计算场景。

# 约束说明

无

# 调用示例

该场景的关键代码示例如下。Matmul多核对齐场景的完整样例请参考：多核切M、N的样例：Matmul多核Kernel直调样例；多核切K的样例：多核切K场景的算子样例。

```rust
// 构造多核Tiling对象
auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance(socVersion);
matmul_tiling::MultiCoreMatmulTiling cubeTiling(*ascendcPlatform);
// 仅包含Cube计算的算子，设置可参与矩阵乘运算的核数为当前AI处理器上的Cube核数
cubeTiling.SetDim(ascendcPlatform.GetCoreNumAic());
cubeTiling.SetAType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND,
matmul_tiling::DataType::DT_FLOAT16);
cubeTiling.SetBType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND,
matmul_tiling::DataType::DT_FLOAT16);
cubeTiling.SetCType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND,
matmul_tiling::DataType::DT_FLOAT);
cubeTiling.SetBiasType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND,
matmul_tiling::DataType::DT_FLOAT);
cubeTiling.SetOrgShape(M, N, K);
cubeTiling.SetShape(M, N, K);
cubeTiling.EnableBias(isBias);
optiling::TCubeTiling tilingData;
// 获取Tiling参数
int ret = cubeTiling.GetTiling(tilingData);    // if ret = -1, gen tiling failed
```

# 3.3.3.3.3 多核非对齐切分

# 功能介绍

多核场景，对矩阵进行切分时，若M、N、K无法整除singleCoreM 、singleCoreN、singleCoreK时，就会出现尾块，即多核非对齐场景。如下图矩阵A、B的最后一行和最后一列的矩阵块：

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/963a4ccc37df9e16c00127ce403a6e457a3664649d8aeb2b36a9454f6c120997.jpg)


此时，C矩阵中的R矩阵块，依然是通过A1*B1+A2*B2+A3*B3+A4*B4累加得到的，处理A1*B1、A2*B2、A3*B3、A4*B4等尾块时，需在kernel侧设置尾块大小，在不改变原有tiling的情况下，调用SetTail接口重新设置本次计算的singleCoreM/singleCoreN/singleCoreK，在处理尾块的时候按照设置的值也就是tailM/tailN/tailK进行搬运和计算。

# 使用场景

多核处理Matmul矩阵计算，存在尾块的场景。

# 约束说明

处理尾块调用的SetTail接口，需要在Iterate/IterateAll之前调用。

# 调用示例

Matmul多核非对齐场景的完整样例请参考Matmul多核非对齐切分算子样例。该场景的关键代码示例如下。

```javascript
// 处理尾块
int tailM = tiling.M - mCoreIndex * tiling.singleCoreM;
tailM = tailM < tiling.singleCoreM ? tailM : tiling.singleCoreM;
int tailN = tiling.N - nCoreIndex * tiling.singleCoreN;
tailN = tailN < tiling.singleCoreN ? tailN : tiling.singleCoreN;
// 当 tailM < singleCoreM 或 tailN < singleCoreN 时被认为需要处理尾块，此时可以调用 SetTail 接口进行设置
if (tailM < tiling.singleCoreM || tailN < tiling.singleCoreN) {
    matmulObj.SetTail(tailM, tailN);
}
```

# 3.3.3.3.4 异步场景处理

# 功能介绍

Matmul的Iterate和IterateAll接口在MIX场景（包含矩阵计算和矢量计算）下提供了同步和异步两种模式，纯Cube场景（只有矩阵计算）下，只支持同步模式。

同步模式指的是程序执行时，需要等待某个操作完成后才能继续执行下一步操作。 异步模式指的是程序执行时，不需要等待某个操作完成就可以继续执行下一步操作。

Iterate&GetTensorC的同步和异步

同步：执行完一次Iterate迭代计算后，执行GetTensorC搬运矩阵C分片，搬运完成后，才能进行下一次计算。如下图所示，C矩阵中，矩阵块1搬走后，才能计算矩阵块2，矩阵块2搬运完成后，才能计算矩阵块3。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/43024b722ad7d155d3be214186fe1ee72e2daceae71a29669670b2c438e32173.jpg)


Iterate&GetTensorC同步模式的关键代码示例如下：

```javascript
while (mm.Iterate()) {
    mm.GetTensorC(gm_c);
} 
```

异步：通过设置Iterate接口的模板参数开启异步模式。调用Iterate后，无需立即调用GetTensorC同步等待矩阵C分块搬运完成，可以先执行其它操作，待需要获取结果时再调用GetTensorC。异步模式可以减少同步等待，提高并行度，开发者对计算性能要求较高时，可以选用该方式。异步场景时，需要使用一块临时空间来缓存Iterate计算结果，否则会覆盖计算结果，调用GetTensorC时会在该临时空间中获取C的矩阵分片。临时空间通过SetWorkspace接口进行设置。SetWorkspace接口需要在Iterate接口之前调用。

Iterate&GetTensorC异步模式的关键代码示例如下：

```txt
mm.SetWorkspace(workspace, size); // 其中，workspace为临时空间的物理地址，size为singleCoreM * singleCoreN的矩阵C大小
// 异步模式
mm.template Iterate<false>();
…… // 执行其他操作
auto mIter = Ceil(singleCoreM, baseM);
auto nIter = Ceil(singleCoreN, baseN);
for (int i = 0; i < mIter * nIter ; ++i) {
    mm.GetTensorC<false> (gm_c);
}
```

IterateAll的同步和异步

同步：后续操作需要同步等待IterateAll执行结束。

IterateAll同步模式的关键代码示例如下：

```javascript
mm.SetTensorA(gm_a); // 设置左矩阵A
mm.SetTensorB(gm_b); // 设置右矩阵B
mm.SetBias(gm_bias); // 设置Bias
mm.IterateAll(gm_c);
// 后续操作
...
```

异步：后续操作不需要同步等待IterateAll执行结束，需要IterateAll的结果时，调用WaitIterateAll等待IterateAll异步接口返回。

IterateAll异步模式的关键代码示例如下：

```cpp
AscendC::Matmul<aType, bType, cType, biasType> mm;
mm.SetTensorA(queryGm[tensorACoreOffset]);
mm.SetTensorB(keyGm[tensorBCoreOffset + sInnerStart * singleProcessSInnerSize * tilingData->attentionScoreOffsetStrideParams.matmulHead], true);
mm.SetTail(singleProcessSOuterSize, mmNNum);
mm.template IterateAll<false>(workspaceGm[tmp_block_idx * mmResUbSize * sInnerLoopTimes], 0, false, true);
// 执行其他操作
mm.WaitIterateAll(); // 等待IterateAll完成
DataCopy(dstUB, GM); // 进行GM到UB的拷贝
```

# 使用场景

Iterate&GetTensorC的同步：MIX场景（包含矩阵计算和矢量计算）、纯Cube场景（只有矩阵计算）。

. Iterate&GetTensorC的异步：仅MIX场景（包含矩阵计算和矢量计算）。

IterateAll的同步：MIX场景（包含矩阵计算和矢量计算）、纯Cube场景（只有矩阵计算）。

IterateAll的异步：仅MIX场景（包含矩阵计算和矢量计算）。

# 约束说明

Iterate&GetTensorC的异步场景：

传入的C矩阵地址空间大小需要保证不小于baseM * baseN。

SetWorkspace接口需要在Iterate接口之前调用。

支持只输出到VECIN、只输出到Global Memory，同时输出到GlobalMemory和VECIN三种输出方式。

取出C矩阵到VECIN时，数据格式仅支持NZ；取出C矩阵到GM时，数据格式支持ND或NZ。

IterateAll的异步场景：

– 传入的C矩阵地址空间大小需要保证不小于singleCoreM * singleCoreN。

仅支持连续输出至Global Memory。

# 调用示例

Iterate&GetTensorC的异步场景的完整样例请参考异步场景样例、Iterate异步场景样例。

IterateAll的异步场景的完整样例请参考IterateAll异步场景样例。

# 3.3.3.3.5 矩阵乘输出的量化/反量化

# 功能介绍

对于特定输入输出数据类型，Matmul支持将计算结果从CO1搬出到Global Memory时，对输出C矩阵元素执行数据量化或反量化操作。

Matmul量化场景：Matmul计算时左矩阵A、右矩阵B为half或bfloat16_t数据类型，输出C矩阵为int8_t数据类型。该场景下，C矩阵的数据从CO1搬出到GlobalMemory时，会执行量化操作，将最终结果量化为int8_t类型，如下图所示。


图 3-32 Matmul 量化场景示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/165ba2dadfd12d8cda284fea716d146c02e033a29452557fc1cc9425b85f42cd.jpg)


Matmul反量化场景：Matmul计算时左矩阵A、右矩阵B为int8_t或int4b_t数据类型，输出C矩阵为half数据类型，或者左矩阵A、右矩阵B为int8_t数据类型，输出C矩阵为int8_t数据类型。该场景下，C矩阵的数据从CO1搬出到Global Memory时，会执行反量化操作，将最终结果反量化为对应的half类型或int8_t类型，如下图所示。


图 3-33 Matmul 反量化场景示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/b1683a9585d3eee3dd8f36394a8722f4d0abe92c419fb7111c27bb7f9326f84d.jpg)


Matmul量化/反量化包含两种模式：同一系数的量化/反量化模式、向量的量化/反量化模式，开发者在算子Tiling侧调用SetDequantType接口设置量化或反量化模式，这两种模式的具体区别为：

同一系数的量化/反量化模式（PER_TENSOR模式）：整个C矩阵对应一个量化参数，量化参数的shape为[1]。开发者在算子Kernel侧调用接口SetQuantScalar设置量化参数。

向量的量化/反量化模式（PER_CHANNEL模式）：C矩阵的shape为[m, n]，每个channel维度即C矩阵的每一列，对应一个量化参数，量化参数的shape为[n]。开发者在算子Kernel侧调用接口SetQuantVector设置量化参数。


表3-6 量化/反量化模式对应的接口配置


<table><tr><td>模式</td><td>Tiling侧接口</td><td>Kernel侧接口</td></tr><tr><td>同一系数的量化/反量化</td><td>SetDequantType(DequantType::SCALAR)</td><td>SetQuantScalar(gmScalar)</td></tr><tr><td>向量的量化/反量化</td><td>SetDequantType(DequantType::TENSOR)</td><td>SetQuantVector(gmTensor)</td></tr></table>

# 使用场景

需要对矩阵计算结果进行量化/反量化操作的场景，当前该场景下，Matmul输入输出矩阵支持的数据类型如下表所示。


表3-7 Matmul 量化/反量化支持的数据类型


<table><tr><td>A矩阵</td><td>B矩阵</td><td>C矩阵</td><td>支持平台</td></tr><tr><td>half</td><td>half</td><td>int8_t</td><td>Atlas 350 加速卡Atlas A3 训练系列产品/Atlas A3推理系列产品Atlas A2 训练系列产品/Atlas A2推理系列产品</td></tr><tr><td>bfloat16_t</td><td>bfloat16_t</td><td>int8_t</td><td>Atlas 350 加速卡Atlas A3 训练系列产品/Atlas A3推理系列产品Atlas A2 训练系列产品/Atlas A2推理系列产品</td></tr><tr><td>int8_t</td><td>int8_t</td><td>half</td><td>Atlas 350 加速卡Atlas A3 训练系列产品/Atlas A3推理系列产品Atlas A2 训练系列产品/Atlas A2推理系列产品</td></tr><tr><td>int4b_t</td><td>int4b_t</td><td>half</td><td>Atlas A3 训练系列产品/Atlas A3推理系列产品Atlas A2 训练系列产品/Atlas A2推理系列产品</td></tr><tr><td>int8_t</td><td>int8_t</td><td>int8_t</td><td>Atlas 350 加速卡Atlas A3 训练系列产品/Atlas A3推理系列产品Atlas A2 训练系列产品/Atlas A2推理系列产品</td></tr><tr><td>int8_t</td><td>int8_t</td><td>bfloat16_t</td><td>Atlas 350 加速卡</td></tr><tr><td>fp8_e4m3fn_t/fp8_e5m2_t</td><td>fp8_e4m3fn_t/fp8_e5m2_t</td><td>fp8_e4m3fn_t/half/bfloat16_t/ float</td><td>Atlas 350 加速卡</td></tr><tr><td>hifloat8_t</td><td>hifloat8_t</td><td>hifloat8_t/half/bfloat16_t/ float</td><td>Atlas 350 加速卡注意:输出为hifloat8_t时,采用Half to Away Round方式量化。量化场景的输出为float类型时,该量化模式精度无法达到双万分之一,可以达到双千分之一。如果有双万分之一的精度要求,建议使用AscendDeQuant高阶API。</td></tr></table>

# 约束说明

SetQuantScalar和SetQuantVector接口必须在Iterate或者IterateAll接口前调用。

在Kernel侧与Tiling侧设置的量化/反量化模式需要保持一致：

Kernel侧调用SetQuantScalar接口设置同一系数的量化/反量化模式，对应Tiling侧调用SetDequantType接口配置模式为DequantType::SCALAR。

Kernel侧调用SetQuantVector接口设置向量的量化/反量化模式，对应Tiling侧调用SetDequantType接口配置模式为DequantType::TENSOR。

当A、B矩阵为int8_t或int4b_t类型，C矩阵为half时，本节特性的输出结果不支持INF_NAN模式。若结果需要以INF_NAN输出，建议在调用Matmul API时将结果输出到TPosition::VECIN，同时将输出的数据类型设置为int32_t，再基于AIV核使用高阶API AscendDequant将该结果反量化为half类型。

# 调用示例

完整的算子样例请参考matmul_quant样例。

Tiling实现

调用SetDequantType接口设置量化或反量化模式，其他实现内容与基础场景相同。

```rust
auto ascendcPlatform = platform_ascendc::PlatformAscendC(context->GetPlatformInfo());
matmul_tiling::MatmulApiTiling tiling(ascendcPlatform);
tiling.SetAType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_INT8);
tiling.SetBType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_INT8);
tiling.SetCType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
tiling.SetBiasType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_INT32);
tiling.SetShape(M, N, K);
tiling.SetOrgShape(M, N, K);
tiling.EnableBias(true);
tiling.SetDequantType(DequantType::SCALAR); // 设置同一系数的量化/反量化模式
```

```cpp
// tiling.SetDequantType(DequantType::TENSOR); // 设置向量的量化/反量化模式
... // 执行其他配置
```

# Kernel实现

根据具体量化模式场景，调用SetQuantScalar或SetQuantVector接口设置量化参数。其他实现内容与基础场景相同。

# 同一系数的量化/反量化模式

```txt
REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), mm, &tiling);
float tmp = 0.1; // 输出gm时会乘以0.1
uint64_t ans = static_cast<uint64_t>(*reinterpret_cast<int32_t*>(&tmp)); // 浮点值量化系数转换为 uint64_t类型进行设置
mm.SetQuantScalar(ans);
mm.SetTensorA(gm_a);
mm.SetTensorB(gm_b);
mm.SetBias(gm_bias);
mm.IterateAll(gm_c);
```

# 向量的量化/反量化模式

```matlab
GlobalTensor gmQuant;
...
REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), mm, &tiling);
mm.SetQuantVector(gmQuant);
mm.SetTensorA(gm_a);
mm.SetTensorB(gm_b);
mm.SetBias(gm_bias);
mm.IterateAll(gm_c); 
```

# 3.3.3.3.6 矩阵乘输出的 Channel 拆分

# 功能介绍

矩阵乘输出的Channel拆分，又称ChannelSplit。指当Matmul计算结果C矩阵的格式为NZ时，C矩阵采用分形存储，关于NZ格式的详细内容请参考数据格式。当C矩阵的物理排布格式为NZ、数据类型为float时，默认情况下，每个分形内部包含16*16个元素，即分形的大小为16*16。ChannelSplit的功能为将此场景下C矩阵的每个16*16的分形切分为16*8的分形，使得C矩阵按照16*8的分形进行存储。

由于1个float类型数据的大小为4字节，16*8的分形在内轴满足32字节对齐，内轴上的数据量与一条NPU矢量计算指令处理的数据单元一致，这便于后续的其它计算。ChannelSplit功能默认不启用，用户需通过设置MatmulConfig中的isEnableChannelSplit参数为true来开启此功能。


图 3-34 ChannelSplit 功能示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/6f3c291be078304bd167b32a03927e0b89c0187c28ab56438a551bbdbf29ff77.jpg)


# 使用场景

对于NZ格式、float类型的C矩阵，需要按16*8的分形存储时，使用该功能。

# 约束说明

开启ChannelSplit功能需满足：

C矩阵的数据排布格式为CubeFormat::NZ。

C矩阵的数据类型为float。

C矩阵的内存逻辑位置为Global Memory。

矩阵乘结果CO1数据类型为float。

# 调用示例

完整的算子样例请参考matmul_channelsplit算子样例。

```cpp
// 指定获取和修改的MatmulConfig模板
constexpr static MatmulConfigMode configMode = MatmulConfigMode::CONFIG_NORM;
// 修改模板参数isEnableChannelSplit=true，开启该MatmulConfig模板的ChannelSplit功能
constexpr static MatmulFuncParams funcParamsChannelSplit{
    false, false, false, false, 0, IterateOrder::ORDER_M, ScheduleType::INNER_PRODUCT, true, false, false, false, true/*isEnableChannelSplit*/
};
constexpr static MatmulConfig MM_CFG = GetMMConfig<configMode>(funcParamsChannelSplit);
Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, MM_CFG> mm;

// 常规Matmul计算，最后输出分形为16*8
REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), mm);
mm.SetTensorA(gm_a);
mm.SetTensorB(gm_b);
mm.SetBias(gm_bias);
mm.IterateAll(gm_c);
```

# 3.3.3.3.7 矩阵向量乘

# 功能介绍

矩阵向量乘（General Matrix-Vector multiplication），即GEMV，是指Matmul计算中M=1，形状为(1, K)的左矩阵A与形状为(K, N)的右矩阵B进行矩阵乘运算的场景。

Matmul支持在Tiling侧与Kernel侧通过配置A矩阵的数据格式为VECTOR来开启GEMV模式，从而高效处理M=1的计算场景。若在M=1时未开启GEMV模式，Matmul计算则将M方向作为非对齐场景进行处理。GEMV模式相较于非对齐处理方式，搬运数据量更少，性能更好。

以M=1，K=256，N=32，左右矩阵数据类型为half的Matmul为具体示例，说明GEMV模式的Matmul API内部处理过程。

# GEMV模式

将A矩阵从A1搬运到A2时，1*256的向量被当作16*16的矩阵进行处理，调用LoadData接口一次完成16*16分形大小的矩阵搬运。B矩阵的搬运以及矩阵乘计算跟基础场景相同，如下图所示。


图 3-35 GEMV 模式 M=1 的矩阵乘计算示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/901e255ab78ce11112232074be7ce7cbd8d00ec596809e2306ba3022fd6b5e75.jpg)


# 非GEMV模式

将A矩阵从A1搬运到A2时，1*256的向量被当作非对齐矩阵数据进行处理，将M方向对齐到32字节后进行搬运。调用LoadData接口每次搬运16*16分形大小的矩阵，一共搬运K/16=16次，导致搬运数据量增加，性能相较于GEMV模式差，如下图所示。


图3-36 非GEMV 模式M=1的矩阵乘计算示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/31f8c0a517a059219cf97c9efd001622e2a83f4142c5391c51c42bb12b71c8a9.jpg)


# 使用场景

形状为(1, K)的A矩阵（M=1，K>1）做矩阵乘计算，即输入A矩阵的数据是向量数据。

# 约束说明

● 在Matmul计算中，若要开启GEMV模式，A矩阵的原始输入形状M必须等于1。

GEMV场景下，左矩阵A不支持转置。

● GEMV场景下，Global Memory上的左矩阵数据需要保证16字节对齐。

MxMatmul场景计算矩阵向量乘时，左矩阵A和左量化系数矩阵scaleA仅支持内存逻辑位置为TPosition::GM。

# 调用示例

完整的算子样例请参考matmul_gemv算子样例。

Tiling实现

调用SetAType接口，设置A矩阵的数据格式为CubeFormat::VECTOR，其它Tiling实现与基础场景相同。

```rust
auto ascendcPlatform = platform_ascendc::PlatformAscendC(context->GetPlatformInfo());
matmul_tiling::MatmulApiTiling tiling(ascendcPlatform);
// 调用设置A矩阵的格式为CubeFormat::VECTOR
tiling.SetAType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::VECTOR,
matmul_tiling::DataType::DT_FLOAT16);
tiling.SetBType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND,
matmul_tiling::DataType::DT_FLOAT16);
tiling.SetCType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND,
matmul_tiling::DataType::DT_FLOAT);
tiling.SetBiasType(AscendC::TPosition::GM, matmul_tiling::CubeFormat::ND,
matmul_tiling::DataType::DT_FLOAT);
... // 其他实现内容
optiling::TCubeTiling tilingData;
int ret = tiling.GetTiling(tilingData);
```

Kernel实现

相较于基础场景，GEMV场景在创建Matmul对象时，设置模板参数A_TYPE的数据格式为CubeFormat::VECTOR。

```cpp
#include "lib/matmul_intf.h"
using A_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::VECTOR, half>;
using B_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, half>; 
```

```cpp
using C_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, float>;
using BIAS_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, float>;
AscendC::Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE> mm; 
```

# 3.3.3.3.8 4:2 稀疏矩阵乘

# 功能介绍

4:2稀疏矩阵乘，又称Sparse Matmul。该场景下输入的原始左矩阵A、右矩阵B为稀疏矩阵，稀疏矩阵B中每4个元素中至少有2个为零元素；在进行Matmul计算前，用户需要自行对B矩阵进行4：2稠密化，即基于原始稀疏矩阵B在每4个元素中过滤掉2个零元素，使B矩阵稠密化为稠密矩阵；Sparse Matmul场景调用Matmul API完成A矩阵与4:2稠密化后的B矩阵的矩阵乘计算。Sparse Matmul可以跳过稀疏矩阵B中的零元素，仅对非零元素进行数据搬运存储和计算，从而减少矩阵乘计算时的内存占用和计算量，提升性能。

# 实现流程

# 步骤1 数据预处理

在计算前的数据准备阶段，用户自行对原始为稀疏矩阵的B矩阵完成稠密化，稠密过程请参考稠密算法说明。稠密化过程结束后，得到4:2稠密化后的右矩阵B和索引矩阵index，稠密化后的右矩阵B和索引矩阵index将作为Sparse Matmul场景的计算输入。


图 3-37 对原始稀疏矩阵B进行4:2 稠密化过程示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/a30b0ed2e57900694e8902574ea329231051a9c33d15f58978e2c72332f52f97.jpg)


稠密化过程对于稀疏矩阵B的每4个元素，在索引矩阵index中生成2个2位索引，每个索引分别指向对应非零元素的相对位置，具体规则可参考稠密算法说明。稠密化过程生成的索引矩阵的数据类型为int2，索引矩阵在加载入Matmul前，需要拼成int8的数据类型。索引矩阵在一个int8的地址中的排布是逆序排布的，例如：索引矩阵1 2 0 1 0 21 0，在地址中的排布为1 0 2 1 0 1 2 0，其中1 0 2 1（对应索引矩阵前四位1 2 0 1）为一个int8，0 1 2 0（对应索引矩阵后四位0 2 1 0）为一个int8。

# 步骤2 使能Sparse Matmul场景

在Host侧，获取Tiling前需要通过SetSparse接口设置使能Sparse Matmul场景。

auto ascendcPlatform = platform_ascendc::PlatformAscendC(context->GetPlatformInfo()); 

matmul_tiling::MatmulApiTiling tiling(ascendcPlatform); 

tiling.SetAType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, 

```rust
matmul_tiling::DataType::DT_INT8);
tiling.SetBType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_INT8);
tiling.SetCType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_INT32);
tiling.SetBiasType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_INT32);
// 设置使能Sparse Matmul场景
tiling.SetSparse(true);
... // 其他实现内容
optiling::TCubeTiling tilingData;
int ret = tiling.GetTiling(tilingData);
```

# 步骤3 创建Matmul对象

在Kernel侧创建Matmul对象时，通过MatmulType定义A、C、Bias的参数类型信息，包括：内存逻辑位置、数据格式、数据类型。通过SparseMatmulType类型定义B矩阵的参数类型，包括：B矩阵的内存逻辑位置、索引矩阵的内存逻辑位置、数据格式、数据类型等。

```cpp
#include "lib/matmul_intf.h"

using A_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, ATYPE, false>; // 使用SparseMatmulType定义B矩阵的参数类型信息
using B_TYPE = AscendC::SparseMatmulType<AscendC::TPosition::GM, AscendC::TPosition::GM, CubeFormat::ND, BType, true>; using C_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, CType>; using BIAS_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BiasType>; AscendC::Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, CFG_MDL> mm;
```

# 步骤4 设置索引矩阵

通过SetSparseIndex接口传入稠密化过程中生成的索引矩阵。

```javascript
mm.SetTensorA(gm_a); // 设置左矩阵A
mm.SetTensorB(gm_b); // 设置右矩阵B
mm.SetSparseIndex(gm_index); // 传入稠密化过程中生成的索引矩阵
mm.SetBias(gm_bias); // 设置Bias
```

# 步骤5 完成矩阵乘操作

在Kernel侧，基于步骤4加载的索引矩阵，完成矩阵乘操作。Matmul API内部完成对A矩阵的稠密化，即根据索引矩阵从A矩阵的每4个元素中，选择2个对应位置元素参与计算。

```txt
// 调用Iterate和GetTensorC或IterateAll接口完成矩阵乘计算
while (mm.Iterate()) {
    mm.GetTensorC(gm_c);
}
// mm.IterateAll(gm_c);
mm.End();
```

----结束

# 参数说明


表 3-8 SparseMatmulType 类型参数说明


<table><tr><td>参数</td><td>说明</td></tr><tr><td>POSITION</td><td>内存逻辑位置。B矩阵仅支持设置为TPosition::GM。</td></tr><tr><td>INDEX_POSITION</td><td>索引矩阵内存逻辑位置。仅支持设置为TPosition::GM。</td></tr><tr><td>CubeFormat</td><td>数据的物理排布格式,详细介绍请参考数据格式。B矩阵支持设置为CubeFormat::ND,CubeFormat::NZ。</td></tr><tr><td>TYPE</td><td>B矩阵仅支持设置为int8_t数据类型。</td></tr><tr><td>ISTRANS</td><td>是否开启使能矩阵转置的功能。当前只支持取值为true,表示开启使能矩阵转置的功能。</td></tr><tr><td>LAYOUT</td><td>表征数据的排布。Sparse Matmul场景仅支持取值为LAYOUT::NONE。NONE:默认值,表示不使用BatchMatmul。</td></tr><tr><td>IBSHARE</td><td>是否使能IBShare(IntraBlock Share)。IBShare的功能是能够复用L1 Buffer上相同的A矩阵或B矩阵数据。当A矩阵和B矩阵同时使能IBShare时,表示L1 Buffer上的A矩阵和B矩阵同时复用。Sparse Matmul场景当前仅支持该参数取值为false,表示不使能IBShare。</td></tr></table>

# 使用场景

左矩阵A为稀疏矩阵、右矩阵B为4:2稠密化后的矩阵的Matmul计算场景。

# 约束说明

该场景仅支持MDL模板下的纯Cube模式（只有矩阵计算）。

通过SetSparseIndex接口传入的索引矩阵只支持int8数据类型和NZ数据排布格式。

原始稀疏矩阵B中每4个元素中应保证最多2个非零元素（即最少2个零元素），如果存在3个或更多非零元素，则仅使用前2个非零元素。

M、K、N中的任意一个值不能为0。

# 调用示例

Sparse Matmul场景的完整样例请参考Sparse Matmul场景的算子样例。

# 3.3.3.3.9 TSCM 输入的矩阵乘

# 功能介绍

TSCM表示L1 Buffer空间对应的逻辑内存，L1 Buffer相关内容见存储单元，开发者可以自行管理TSCM以高效利用硬件资源。比如，开发者可缓存一份TSCM数据，在不同使用场景中灵活配置为Matmul操作的A矩阵、B矩阵或Bias偏置矩阵，实现内存复用与计算效率优化。在TSCM输入场景，用户管理整块TSCM内存空间，Matmul直接使用传入的TSCM内存地址，不进行Global Memory到TSCM的数据搬入。

# 使用场景

用户需要自定义数据搬入到TSCM及自定义管理的场景，即需要自定义实现数据搬入功能，如非连续搬入或对搬入数据进行预处理等。用户通过自定义管理TSCM可灵活配置MTE2流水，实现跨Matmul对象的全局DoubleBuffer，MTE2相关内容见搬运单元。

# 约束说明

设置为TSCM输入的矩阵必须在TSCM中全载，全载即全部的矩阵数据同时搬入及保持在TSCM中。

# 调用示例

完整的算子样例请参考自定义数据来源为GM的TSCM输入的Matmul算子样例、BatchMatmul自定义TSCM输入的算子样例。

```cpp
TQue<TPosition::A1, 1> scm; // 队列逻辑位置A1，队列深度为1
pipe->InitBuffer(scm, 1, tiling.M * tiling.Ka * sizeof(A_T));
// A_TYPE的TPosition为TSCM， B_TYPE的TPosition为GM
Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE> mm1;
REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), mm1);
mm1.Init(&tiling);
// 自定义A矩阵GM到TSCM的搬运
auto scmTensor = scm.AllocTensor<A_T>();
DataCopy(scmTensor, gm_a, tiling.M * tiling.Ka);
scm.EnQue(scmTensor);
LocalTensor<A_T> scmLocal = scm.DeQue<A_T>();
// A矩阵设置为TSCM输入，B矩阵为GM输入
mm1.SetTensorA(scmLocal);
mm1.SetTensorB(gm_b);
mm1.SetBias(gm_bias);
mm1.IterateAll(gm_c);
scm.FreeTensor(scmLocal);
```

# 3.3.3.3.10 矩阵乘输出的 N 方向对齐

# 功能介绍

矩阵乘输出的N方向对齐，即矩阵乘结果C矩阵按ND_ALIGN格式输出。在Matmul矩阵乘法中，常用的矩阵数据格式有ND、NZ，相关介绍可参考数据格式章节。

ND_ALIGN是矩阵的另一种数据格式，该格式一般用于N方向非32字节对齐的矩阵乘计算中，配置结果C矩阵为ND_ALIGN格式后，将按照N方向32字节对齐的补齐规则输出C矩阵，详细内容请见ND_ALIGN。

以M=16，K=16，N=14，A、B矩阵数据类型为half的Matmul为具体示例，说明ND_ALIGN输出功能。当配置C矩阵为ND格式并输出到Global Memory时，按照原始N方向大小非32字节对齐输出如图3-38所示。当配置C矩阵为ND格式时，按照N方向32字节对齐输出如图3-39所示，C矩阵的N方向最后两列由下一行的实际数据进行填充补齐，以实现N方向对齐到32字节并输出。当配置C矩阵为ND_ALIGN格式时，Matmul API会在C矩阵的N方向上通过添加无效数据来填充最后两列，以确保N方向对齐至32字节并输出，如图3-40所示。


图 3-38 ND 格式 C 矩阵 N 方向非 32 字节对齐示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/e3ece175ce134a28ba25e2a49080f7a2d2487d23ab52f680ba275fa610f87acf.jpg)



矩阵A


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/81b02e519ac23c0da766ee079d9aea24095a30f8350a6b20994fa04bdc5ab86c.jpg)



矩阵b


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/bae963a36c2d6e88309bc5c5d5d221a878d52a12f0b03c6f518d1ecbac45bc3e.jpg)



矩阵c



图 3-39 ND格式C矩阵N方向32字节对齐示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/8078ee2e81bf94eb4e2a900ffcb2f4fe9843150f9e1a5c3284b14ff7f23cf294.jpg)



矩阵c



图 3-40 ND_ALIGN 格式 C 矩阵 N 方向 32 字节对齐示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/70b374bebcca783da40269d876286b58feb1b67c2b7852d65455063fe84c9096.jpg)



矩阵c


# 使用场景

Matmul计算中N方向非32字节对齐，输出C矩阵的N方向要求32字节对齐的场景。

# 约束说明

若配置C矩阵为ND_ALIGN格式输出，则为C矩阵申请的Buffer空间为N向上32字节对齐后的空间大小。

# 调用示例

完整的算子样例请参考matmul_nd_align算子样例。

Tiling实现

调用SetCType接口，设置C矩阵的数据格式为CubeFormat::ND_ALIGN，其它Tiling实现与基础场景相同。

```rust
auto ascendcPlatform = platform_ascendc::PlatformAscendC(context->GetPlatformInfo());
matmul_tiling::MatmulApiTiling tiling(ascendcPlatform);
tiling.SetAType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
tiling.SetBType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
// 设置C矩阵，buffer位置为GM，数据格式为ND_ALIGN
tiling.SetCType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND_ALIGN, matmul_tiling::DataType::DT_FLOAT);
tiling.SetBiasType(AscendC::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT);
... // 其他实现内容
optiling::TCubeTiling tilingData;
int ret = tiling.GetTiling(tilingData);
```

Kernel实现

相较于基础场景，ND_ALIGN输出功能要求在创建Matmul对象时，设置模板参数cType的数据格式为CubeFormat::ND_ALIGN。

```cpp
#include "lib/matmul_intf.h"

typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, half> aType;
typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, half> bType;
// 设置模板参数cType的数据格式为ND_ALIGN

typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND_ALIGN, float> cType;
typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, float> biasType;
AscendC::Matmul<aType, bType, cType, biasType> mm;
```

# 3.3.3.3.11 单次矩阵乘局部输出

# 功能介绍

单次矩阵乘局部输出，又称Partial Output。如基础知识中所述，一次Iterate计算过程中，会按K方向进行一次或多次基本块计算，其中的一次基本块计算为baseM*baseK和baseK*baseN大小的输入数据进行计算得到baseM*baseN大小的结果；每次基本块计算的结果进行累加后，便得到baseM*singleCoreK和singleCoreK*baseN大小的输入数据计算得到的结果baseM*baseN，并将其作为一次Iterate的最终结果输出。

开启Partial Output功能后，调用Iterate接口不会进行K轴累加，只进行单次基本块计算。用户可以通过GetTensorC接口获取对应的单片数据，最后自行进行K轴上的累加。


图 3-41 未开启 Partial Output 功能计算示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/7fad7ae14e0d748d59bc1fd7fe6a7e9ffb25ac98af8cb57bac8b2d0a895bf3f0.jpg)



图 3-42 开启 Partial Output 功能计算示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/8d7eb3d5b75cedcfaf2f7943c3d29f385f275df9d66efe256df18666a55ecaf3.jpg)


# 使用场景

矩阵乘计算结果不需要累加，只需要输出baseM*baseK和baseK*baseN的计算结果baseM*baseN。例如需要先获取单次基本块计算的数据进行反量化，再累加得到最终结果。

# 约束说明

该功能仅支持MDL模板。

获取矩阵乘计算结果时，仅支持调用Iterate和GetTensorC接口的连续写模式，不支持非连续写模式以及IterateAll接口获取计算结果，连续写模式的介绍请参考GetTensorC。

该功能不支持带有Bias矩阵的Matmul计算，即不支持输入Bias矩阵。

# 调用示例

完整的算子样例请参考开启Partial Output功能的算子样例。

```cpp
// 配置MDL模板，使能Partial Output
constexpr static MatmulConfigMode configMode = MatmulConfigMode::CONFIG_MDL;
constexpr static MatmulFuncParams funcParams = {
    false, false, false, false, 0, IterateOrder::UNDEF, ScheduleType::INNER_PRODUCT, true, true, true /* isPartialOutput */
};
constexpr static MatmulConfig CFG_PARTIAL = GetMMConfig<configMode>(funcParams);
Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, CFG_PARTIAL> mm;
REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), mm);
mm.Init(&tiling);
mm.SetTensorA(gmA, isTransposeA);
mm.SetTensorB(gmB, isTransposeB);
while (mm.Iterate()) {
    mm.GetTensorC(tmpGmC[dstOffset], false, true);
    dstOffset += baseM * baseN;
    // 其他操作
}
```

# 3.3.3.3.12 AIC 和 AIV 独立运行机制

# 功能介绍

AIC和AIV独立运行机制，又称双主模式。在分离模式下，区别于MIX模式（包含矩阵计算和矢量计算）通过消息机制驱动AIC运行，双主模式为AIC和AIV独立运行代码，不依赖消息驱动，使能双主模式能够提高Matmul计算性能。默认情况下，双主模式不使能，需要通过MatmulConfig中的enableMixDualMaster参数开启。

# 使用场景

算子中的矩阵计算和矢量计算相关代码独立运行，不依赖消息驱动时，可以开启双主模式，以提高Matmul计算性能。

# 约束说明

该功能仅支持Norm模板和MDL模板。

算子核函数的类型为MIX，同时AIC核数 : AIV核数为1:1。

算子核函数的类型为MIX，同时AIC核数 : AIV核数为1:2，且A矩阵和B矩阵同时使能IBSHARE参数。

同一算子中所有Matmul对象的该参数取值必须保持一致。

A、B、Bias矩阵只支持从Global Memory输入。

获取矩阵计算结果只支持调用IterateAll接口输出到GlobalTensor，即计算结果放置于Global Memory的地址，不能调用GetTensorC等接口获取结果。

# 调用示例

完整的算子样例请参考使能双主模式的算子样例。

// 修改模板参数enableMixDualMaster=true，Norm模板开启双主模式，MDL模板使用GetMDLConfig接口获取模板参数。

constexpr static MatmulConfig MM_CFG = GetNormalConfig(false, false, false, BatchMode::BATCH_LESS_THAN_L1, true, IterateOrder::ORDER_M, ScheduleType::OUTER_PRODUCT, false, true/*enableMixDualMaster*/); Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, MM_CFG> mm; 

// 常规Matmul计算REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), mm);mm.SetTensorA(gm_a);mm.SetTensorB(gm_b);

```javascript
mm.SetBias(gm_bias);
mm.IterateAll(gm_c); 
```

# 3.3.3.3.13 MxMatmul 场景

# 背景介绍

浮点数在科学计算、图像处理、神经网络等领域应用广泛。以AI训练为例，现有的浮点数格式或数值范围不足，或精度不高，这影响了模型的收敛速度和性能。如果要同时满足数值范围和精度的要求，将会导致内存占用过大，从而增加数据存储和传输的成本。基于此种情况，业内提出了一种新的浮点数格式——微缩放（Microscaling，MX）格式。MX格式的浮点数可以支持更低比特位宽的AI训练和推理，并且占用的内存更少。符合MX标准的数据格式在使用8位或更低比特位的情况下，能够实现稳健的AI训练和推理模型精度。

MX格式是一种块数据格式，若干个数据可以组成一个块（或者一个组），数据以块为单位。MX格式的数据由三部分构成：

共享缩放因子X，位宽为w bits；

私有元素Pi，位宽为d bits；

块大小k，表示多少个低比特数据形成一个块；

所有k个元素P有相同的位宽和数据类型，并且共享一个缩放因子X，每个包含k个元素的块可以使用（w+k*d）位进行编码。元素的数据类型和缩放因子可以独立选择。

下图为MX格式的浮点数的数据结构，S、E和M分别用于表示浮点数的符号、指数和尾数字段的值。其中，共享缩放因子X是一个用于整个数据块的缩放比例因子，它决定了数据块中所有元素的动态范围。通过引入共享缩放因子，MX格式的数据能够在保持低位宽的同时，灵活地表示不同范围的数据。块大小k指的是组成一个数据块（或组）的低比特数据的数量。私有元素P是指数据块中的每个低比特数据元素。这些元素经过缩放因子X的调整后，共同表示了一个高精度的浮点数或整数。


图 3-43 MX 格式组成示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/44f47bd895c5e839059b16cec10bcd75f289761b663edb7206e76a81a5ad7e04.jpg)


MX格式的数据类型包含多种，例如MXFP8、MXFP4、MXFP16、MXINT4等。下表列举了MxMatmul场景（全称Microscaling Matmul）支持的数据类型。


表 3-9 MxMatmul 支持 MX 格式的数据类型


<table><tr><td>数据类型</td><td>私有元素数据类型</td><td>私有元素位宽(d)</td><td>块大小(k)</td><td>共享缩放因子数据类型</td><td>共享缩放因子位宽(w)</td></tr><tr><td>MXFP8</td><td>fp8_e5m2_t</td><td>8</td><td>32</td><td>fp8_e8m0_t</td><td>8</td></tr><tr><td>MXFP8</td><td>fp8_e4m3fn_t</td><td>8</td><td>32</td><td>fp8_e8m0_t</td><td>8</td></tr><tr><td>MXFP4</td><td>fp4x2_e1m2_t</td><td>4</td><td>32</td><td>fp8_e8m0_t</td><td>8</td></tr><tr><td>MXFP4</td><td>fp4x2_e2m1_t</td><td>4</td><td>32</td><td>fp8_e8m0_t</td><td>8</td></tr></table>

# 功能介绍

MxMatmul（全称Microscaling Matmul）为带有量化系数的矩阵乘法，即左矩阵和右矩阵均有对应的量化系数矩阵，左量化系数矩阵scaleA和右量化系数矩阵scaleB。MxMatmul场景中，左量化系数矩阵与左矩阵乘积，右量化系数矩阵与右矩阵乘积，对两个乘积的结果做矩阵乘法。

MxMatmul的计算公式为： $\mathsf { C } = ( \mathsf { s c a l e A } \otimes \mathsf { A } ) \sp { \star } ( \mathsf { s c a l e B } \otimes \mathsf { B } ) + \mathsf { B i a s }$ ，“⊗”表示广播乘法，左/右矩阵与左/右量化系数矩阵做乘积时，K方向上每32个元素共享一个量化因子，如图3-44所示。

A、scaleA、B、scaleB为源操作数。A为左矩阵，形状为[M, K]；scaleA为左量化系数矩阵，形状为[M, K/32]；B为右矩阵，形状为[K, N]；scaleB为右量化系数矩阵，形状为[K/32, N]。

C为目的操作数，存放矩阵乘结果的矩阵，形状为[M, N]。

Bias为矩阵乘偏置，形状为[1, N]。对(scaleA ⊗ A) * (scaleB ⊗ B)结果矩阵的每一行都采用该Bias进行偏置。


图 3-44 MxMatmul 矩阵乘示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/4fb99269c5a912cdea8dd7e7250711045ce9f6563c1988a1a5f5a992fd472f03.jpg)


矩阵A、scaleA、B、scaleB在不同位置中的排布格式分别如下图所示。


图 3-45 A 矩阵在不同位置的排布格式


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/1b57b5e3dc76448086a586e511adc66e35c5a5557532c164f87bb7d87438c5f1.jpg)



图3-46 B矩阵在不同位置的排布格式


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/89d102884c7c6f0dbe8c50a4fc01199a63b82b8ce8b48502324f0c642d269403.jpg)



图 3-47 scaleA 矩阵在不同位置的排布格式


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/ea5b312722c1e5aae17db457c1581d13f9cf0af4a859b5ef45d3e7ded82334eb.jpg)



图3-48 scaleB矩阵在不同位置的排布格式


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/3589d458560cbc0fdcd6509b5b5796332be19161a8bca732169706edcc1c87b5.jpg)


# 使用场景

矩阵计算之前，需要对A、B矩阵进行量化操作的场景。当前该场景下，Matmul输入输出矩阵支持的数据类型如下表所示。


表 3-10 MxMatmul 支持的量化场景


<table><tr><td>A矩阵</td><td>B矩阵</td><td>ScaleA矩阵/ScaleB矩阵</td><td>Bias矩阵</td><td>C矩阵</td><td>支持平台</td></tr><tr><td>fp4x2_e1m2_t</td><td>fp4x2_e1m2_t/fp4x2_e2m1_t</td><td>fp8_e8m0_t</td><td>float/half/bfloat16_t</td><td>float/half/bfloat16_t</td><td>Atlas 350加速卡</td></tr><tr><td>fp4x2_e2m1_t</td><td>fp4x2_e2m1_t/fp4x2_e1m2_t</td><td>fp8_e8m0_t</td><td>float/half/bfloat16_t</td><td>float/half/bfloat16_t</td><td>Atlas 350加速卡</td></tr><tr><td>fp8_e4m3fn_t</td><td>fp8_e4m3fn_t/fp8_e5m2_t</td><td>fp8_e8m0_t</td><td>float/half/bfloat16_t</td><td>float/half/bfloat16_t</td><td>Atlas 350加速卡</td></tr><tr><td>fp8_e5m2_t</td><td>fp8_e4m3fn_t/fp8_e5m2_t</td><td>fp8_e8m0_t</td><td>float/half/bfloat16_t</td><td>float/half/bfloat16_t</td><td>Atlas 350加速卡</td></tr></table>

# 实现流程

Host侧自动获取Tiling参数的关键步骤介绍如下：

# 步骤1 创建Tiling对象。

```rust
auto ascendcPlatform = platform_ascendc::PlatformAscendC(context->GetPlatformInfo());
matmul_tiling::MatmulApiTiling cubeTiling(ascendcPlatform); 
```

传入硬件平台信息创建PlatformAscendC对象，然后创建Tiling对象，硬件平台信息可以通过GetPlatformInfo获取。

# 步骤2 设置A、B、C、Bias的内存逻辑位置、格式、数据类型以及是否转置的信息，设置scaleA、scaleB的内存逻辑位置、格式以及是否转置的信息。

调用SetScaleAType、SetScaleBType接口，设置scaleA、scaleB的内存逻辑位置、格式以及是否转置。

```rust
cubeTiling.SetAType(AscendC::TPosition::GM, CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT8_E5M2, false);
cubeTiling.SetBType(AscendC::TPosition::GM, CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT8_E5M2, true);
cubeTiling.SetScaleAType(AscendC::TPosition::GM, CubeFormat::ND, false);
cubeTiling.SetScaleBType(AscendC::TPosition::GM, CubeFormat::ND, true);
cubeTiling.SetCType(AscendC::TPosition::GM, CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT);
cubeTiling.SetBiasType(AscendC::TPosition::GM, CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT); 
```

# 步骤3 使能MxMatmul场景。

调用SetMadType接口，设置Tiling计算逻辑为MxMatmul场景。

```txt
cubetiling.SetMadType(MatrixMadType::MXMODE); 
```

# 步骤4 设置矩阵shape信息。

```javascript
cubeTiling.SetShape(M, N, K);
cubeTiling.SetOrgShape(M, N, K); // 设置原始完整的形状M、N、K
```

# 步骤5 设置可用空间大小信息。

设置Matmul计算时可用的L1 Buffer/L0C Buffer/Unified Buffer空间大小，-1表示AI处理器对应Buffer的大小。

```javascript
cubeTiling.SetBufferSpace(-1, -1, -1); 
```

# 步骤6 按需设置其他参数，比如设置bias参与计算。

```javascript
cubeTiling.EnableBias(true); 
```

# 步骤7 获取Tiling参数。

```txt
MatmulCustomTilingData tiling;
if (cubeTiling.GetTiling(tiling.cubeTilingData) == -1){
    return ge::GRAPH_FAILED;
} 
```

步骤8 Tiling参数的序列化保存等其他操作。

----结束

Kernel侧的关键步骤介绍如下：

# 步骤1 创建Matmul对象。

```cpp
// MxMatmul场景通过MatmulTypeWithScale定义A、scaleA、B、scaleB的参数类型信息
typedef AscendC::MatmulTypeWithScale<AscendC::TPosition::GM, AscendC::TPosition::GM, CubeFormat::ND, fp8_e5m2_t, isTransposeA> aType;
typedef AscendC::MatmulTypeWithScale<AscendC::TPosition::GM, AscendC::TPosition::GM, CubeFormat::ND, fp8_e5m2_t, isTransposeB> bType;
typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, float> cType;
typedef AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, float> biasType;
// 定义matmul对象时，传入MatmulWithScalePolicy表明使能MxMatmul模板策略
AscendC::Matmul<aType, bType, cType, biasType, CFG_MDL, MatmulCallBackFunc<nullptr, nullptr, nullptr>, AscendC::Impl::Detail::MatmulWithScalePolicy> mm;
```

创建对象时需要传入A、scaleA、B、scaleB、C、Bias的参数类型信息， A、scaleA、B、scaleB类型信息通过MatmulTypeWithScale来定义，C、Bias类型信息通过MatmulType来定义，包括：内存逻辑位置、数据格式、数据类型、转置信息。同时，通过模板参数MatmulPolicy传入MatmulWithScalePolicy表明使能MxMatmul场景。

```c
template <TPosition POSITION, TPosition SCALE_POSITION, CubeFormat FORMAT, typename TYPE, bool ISTRANS = false, TPosition SRCPOS = TPosition::GM, CubeFormat SCALE_FORMAT = FORMAT, bool SCALE_ISTRANS = ISTRANS, TPosition SCALE_SRCPOS = SRCPOS>
struct MatmulTypeWithScale: public MatmulType<POSITION, FORMAT, TYPE, ISTRANS> {
    constexpr static TPosition scalePosition = SCALE_POSITION;
    constexpr static CubeFormat scaleFormat = SCALE_FORMAT;
    constexpr static bool isScaleTrans = SCALE_ISTRANS;
    constexpr static TPosition srcScalePos = SCALE_SRCPOS;
}; 
```

# 步骤2 初始化操作。

```cpp
REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), mm, &tiling); // 初始化
```

# 步骤3 设置左矩阵A、右矩阵B、左量化系数矩阵scaleA、右量化系数矩阵scaleB、Bias。

通过SetTensorScaleA、SetTensorScaleB设置左量化系数矩阵scaleA、右量化系数矩阵scaleB。

```txt
mm.SetTensorA(gm_a, isTransposeA); // 设置左矩阵A
mm.SetTensorB(gm_b, isTransposeB); // 设置右矩阵B
mm.SetTensorScaleA(gm_scaleA, isTransposeScaleA); // 设置左量化系数矩阵scaleA
mm.SetTensorScaleB(gm_scaleB, isTransposeScaleB); // 设置右量化系数矩阵scaleB
mm.SetBias(gm_bias); // 设置Bias
```

# 步骤4 完成矩阵乘操作。

调用Iterate完成单次迭代计算，叠加while循环完成单核全量数据的计算。Iterate方式，可以自行控制迭代次数，完成所需数据量的计算，方式比较灵活。

```javascript
while (mm.Iterate()) {
    mm.GetTensorC(gm_c);
} 
```

调用IterateAll完成单核上所有数据的计算。IterateAll方式，无需循环迭代，使用比较简单。

mm.IterateAll(gm_c); 

# 步骤5 结束矩阵乘操作。

mm.End(); 

# ----结束

更多完整的算子样例请参考Scale的K方向为偶数的MxMatmul样例、Scale的K方向为奇数的MxMatmul样例、mx_ub_tscm_nz样例、matmul_mx_typepara样例。

# 参数说明


表 3-11 MatmulTypeWithScale 参数说明


<table><tr><td>参数</td><td>说明</td></tr><tr><td>POSITION</td><td>左右矩阵的内存逻辑位置。针对Atlas 350 加速卡:A矩阵可设置为TPosition::GM, TPosition::VECOUT, TPosition::TSCMB矩阵可设置为TPosition::GM, TPosition::VECOUT, TPosition::TSCM注意:A、B矩阵设置为TPosition::TSCM时,对应的Format仅支持CubeFormat::NZ。</td></tr><tr><td>SCALE_POSITION</td><td>量化系数矩阵的内存逻辑位置。针对Atlas 350 加速卡:scaleA矩阵可设置为TPosition::GM, TPosition::VECOUT, TPosition::TSCMscaleB矩阵可设置为TPosition::GM, TPosition::VECOUT, TPosition::TSCM注意:scaleA、scaleB矩阵设置为TPosition::TSCM时,对应的SCALE_FORMAT仅支持CubeFormat::NZ。</td></tr><tr><td>FORMAT</td><td>数据的物理排布格式,详细介绍请参考数据格式。针对Atlas 350 加速卡:A矩阵可设置为CubeFormat::ND, CubeFormat::NZ, CubeFormat::VECTORB矩阵可设置为CubeFormat::ND, CubeFormat::NZ注意:NZ排布格式,A/B的排布格式请参考数据格式。</td></tr><tr><td>TYPE</td><td>数据类型。针对Atlas 350加速卡:A矩阵可设置为fp4x2_e1m2_t、fp4x2_e2m1_t、fp8_e4m3fn_t、fp8_e5m2_tB矩阵可设置为fp4x2_e1m2_t、fp4x2_e2m1_t、fp8_e4m3fn_t、fp8_e5m2_t注意:具体数据类型组合关系请参考MxMatmul支持数据类型。</td></tr><tr><td>ISTRANS</td><td>是否开启使能A、B矩阵转置的功能。默认值为false。参数支持的取值如下:true:开启使能矩阵转置的功能,开启后,分别通过SetTensorA和SetTensorB中的isTransposeA、isTransposeB参数设置A、B矩阵是否转置。若设置A、B矩阵转置,Matmul会认为A矩阵形状为[K,M],B矩阵形状为[N,K]。false:不开启使能矩阵转置的功能,通过SetTensorA和SetTensorB不能设置A、B矩阵的转置情况。Matmul会认为A矩阵形状为[M,K],B矩阵形状为[K,N]。</td></tr><tr><td>SRCPOS</td><td>A/B矩阵的POSITION参数配置为TPosition::TSCM时,要设置TSCM中矩阵数据的来源的内存逻辑位置,默认为TPosition::GM。针对Atlas 350加速卡:A矩阵可设置为TPosition::GM,TPosition::VECOUTB矩阵可设置为TPosition::GM,TPosition::VECOUT</td></tr><tr><td>SCALE_FORMAT</td><td>量化系数矩阵的物理排布格式,详细介绍请参考数据格式。默认值为FORMAT。针对Atlas 350加速卡:scaleA矩阵可设置为CubeFormat::ND,CubeFormat::NZ,CubeFormat::VECTORscaleB矩阵可设置为CubeFormat::ND,CubeFormat::NZ注意:NZ排布格式请参考NZ。MxMatmul场景,scaleA、scaleB的数据类型为fp8_e8m0_t,分形大小H0=16,W0=2。在Scale矩阵为ND格式的场景中,当通过SetTensorScaleA接口设置scaleA矩阵转置时,scaleA内存排布格式必须按照(K/64,M,2)排布,通过SetTensorScaleB接口设置scaleB矩阵不转置时,scaleB内存排布格式必须按照(K/64,N,2)排布,详细介绍请参考数据格式。</td></tr><tr><td>SCALE_ISTRANS</td><td>是否开启使能scaleA、scaleB矩阵转置的功能。默认值为ISTRANS。参数支持的取值如下:true:开启使能矩阵转置的功能。开启后,分别通过SetTensorScaleA和SetTensorScaleB中的isTransposeScaleA、isTransposeScaleB参数设置scaleA、scaleB矩阵是否转置。在Scale矩阵为ND格式的场景中,若设置scaleA、scaleB矩阵转置,Matmul会认为scaleA矩阵形状为[Ceil(K/64), M, 2], scaleB矩阵形状为[N, Ceil(K/64), 2]。false:不开启使能矩阵转置的功能。通过SetTensorScaleA和SetTensorScaleB不能设置scaleA、scaleB矩阵的转置情况。Matmul会认为scaleA矩阵形状为[M, Ceil(K/64), 2], scaleB矩阵形状为[Ceil(K/64), N, 2]。使用该参数的完整样例请参考scaleA转置scaleB不转置的的MxMatmul样例、scaleA不转置scaleB转置的的MxMatmul样例。</td></tr><tr><td>SCALE_SRCPOS</td><td>scaleA、scaleB矩阵的SCALE_POSITION参数设置为TPosition::TSCM时,需要通过本参数设置TSCM中矩阵数据来源的内存逻辑位置,默认值为SRCPOS。针对Atlas 350 加速卡:scaleA矩阵可设置为TPosition::GM,TPosition::VECOUTscaleB矩阵可设置为TPosition::GM,TPosition::VECOUT</td></tr></table>

# 约束说明

MxMatmul场景仅支持Norm模板和MDL模板。

在MxMatmul场景中，如果A与B矩阵的位置同时为GM，对singleKIn没有特殊限制，在这种情况下，若scaleA和scaleB的K方向大小（即Ceil(singleKIn, 32)）为奇数，用户需自行在scaleA和scaleB的K方向补0至偶数。例如，当singleKIn为30时，Ceil(singleKIn, 32)为1，用户需要自行在scaleA和scaleB的K方向补0，使K方向为偶数。对于其它A、B矩阵逻辑位置的组合情况，即A与B矩阵的位置不同时为GM，singleKIn以32个元素向上对齐后的数值必须是32的偶数倍。

在MxMatmul场景中，当输入数据类型为fp4x2_e2m1_t/fp4x2_e1m2_t时，内轴必须为偶数。

在MxMatmul场景中，通过将A矩阵和scaleA矩阵的数据格式设置为VECTOR，来开启GEMV模式。在此模式下，A和scaleA矩阵仅支持内存逻辑位置为GM，并且均不支持转置。

A矩阵、B矩阵为UB输入时，矩阵的内轴需要向上32字节对齐，例如，A矩阵的形状为(M, K)时，将K对齐到32字节；A矩阵的形状为(K, M)时，将M对齐到32字节。

scaleA矩阵、scaleB矩阵为UB输入时，矩阵的内轴需要向上32字节对齐，例如，scaleA矩阵的形状为(M, K/32)时，将K/32对齐到32字节；scaleA矩阵的形状为(K/32, M)时，将M对齐到32字节。

当scaleA和scaleB矩阵以ND格式输入时，高阶API在内部实现格式转换时，需要占用UB临时空间。开发者需使用SetLocalWorkspace接口配置临时空间，临时空间大小（单位字节）的计算公式如下。

int32_t scaleATmpBuf = 0; 

int32_t scaleBTmpBuf = 0; 

```cpp
if constexpr (A_TYPE::scalePosition == TPosition::VECOUT) {
    if (A_TYPE::isScaleTrans) {
    scaleATmpBuf = CeilAlign(SingleCoreM, 32) * scaleK;
    } else {
    scaleATmpBuf = CeilAlign(scaleK, 32) * SingleCoreM;
    }
}
if constexpr (B_TYPE::scalePosition == TPosition::VECOUT) {
    if (B_TYPE::isScaleTrans) {
    scaleBTmpBuf = SingleCoreN * CeilAlign(scaleK, 32);
    } else {
    scaleBTmpBuf = scaleK * CeilAlign(SingleCoreN, 32);
    }
}
int32_t totalTmpBuf = scaleATmpBuf + scaleBTmpBuf; 
```

# 3.3.3.3.14 Batch Matmul 基础功能

# 功能介绍

Batch Matmul是指批量处理Matmul计算的场景。该场景对外提供了IterateBatch的调用接口，调用一次IterateBatch，可以计算出多个singleCoreM * singleCoreN大小的C矩阵。

Matmul单次计算的过程需要搬入和搬出数据，当进行多次Matmul计算且单次Matmul计算的输入shape较小时，搬运开销在整体耗时中占比较大。通过IterateBatch接口批量处理Matmul，可以有效提升带宽利用率。

Batch Matmul当前支持4种Layout类型：BSNGD、SBNGD、BNGS1S2、NORMAL（BMNK的数据排布格式），相关数据排布格式请参考IterateBatch。

下图为NORMAL数据排布格式的Batch Matmul计算。整个Matmul计算一共包含4个矩阵乘操作：mat_a1*mat_b1、mat_a2*mat_b2、mat_a3*mat_b3、mat_a4*mat_b4，需要单核上计算四个singleCoreM *singleCoreN。在该场景下，如果shape较小，可以将其视为Batch Matmul场景进行批量处理，以提升性能。一次IterateBatch可同时计算出mat_c1 = mat_a1 * mat_b1、mat_c2 = mat_a2 *mat_b2、mat_c3 = mat_a3 * mat_b3、mat_c4 = mat_a4 * mat_b4。


图 3-49 NORMAL 数据排布格式的 Batch Matmul 示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/e6eeae3702039bd8f696729160f18c11b9dacbea7d283eedc80a28e106a424f5.jpg)


# 使用场景

Matmul计算需要计算出多个singleCoreM * singleCoreN大小的C矩阵，且单次Matmul计算处理的shape较小。

# 约束说明

只支持Norm模板。

对于BSNGD、SBNGD、BNGS1S2 Layout格式，输入A、B矩阵按分形对齐后的多Batch数据总和应小于L1 Buffer的大小；对于NORMAL Layout格式没有这种限制，但需通过MatmulConfig配置batchMode参数，即输入A、B矩阵多Batch数据大小与L1 Buffer的大小关系；

对于BSNGD、SBNGD、BNGS1S2 Layout格式，称左矩阵、右矩阵的G轴分别为ALayoutInfoG、BLayoutInfoG，则ALayoutInfoG / batchA = BLayoutInfoG /batchB；对于NORMAL Layout格式，batchA 、batchB必须满足倍数关系。Bias的shape(batch, n)中的batch必须与C矩阵的batch相等。

如果接口输出到Unified Buffer上，输出C矩阵大小BaseM*BaseN应小于分配的Unified Buffer内存大小。

对于BSNGD、SBNGD Layout格式，输入输出只支持ND格式数据。对于BNGS1S2、NORMAL Layout格式， 输入支持ND/NZ格式数据。

Batch Matmul不支持量化/反量化模式，即不支持SetQuantScalar、SetQuantVector接口。

BSNGD场景，不支持一次计算多行SD，需要算子程序中循环计算。

异步模式不支持IterateBatch搬运到Unified Buffer上。

模板参数enableMixDualMaster（默认取值为false）设置为true，即使能MixDualMaster（双主模式）场景时，不支持Batch Matmul。

在batch场景，A矩阵、B矩阵支持half/float/bfloat16_t/int8_t数据类型，不支持int4b_t数据类型。

# 调用示例

以下是NORMAL数据排布格式的Batch Matmul调用示例。BSNDG数据排布格式的Batch Matmul完整示例请参考BatchMatmul样例。

Tiling实现

使用SetBatchInfoForNormal设置A/B/C的M/N/K轴信息和A/B矩阵的BatchNum。

```txt
auto ascendcPlatform = platform_ascendc::PlatformAscendC(context->GetPlatformInfo());
matmul_tiling::MultiCoreMatmulTiling tiling(ascendcPlatform);
int32_t M = 32;
int32_t N = 256;
int32_t K = 64;
tiling->SetDim(1);
tiling->SetAType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
tiling->SetBType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
tiling->SetCType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT);
tiling->SetBiasType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT);
tiling->SetShape(M, N, K);
tiling->SetOrgShape(M, N, K);
tiling->EnableBias(true);
tiling->SetBufferSpace(-1, -1, -1);

constexpr int32_t BATCH_NUM = 3;
tiling->SetBatchInfoForNormal(BATCH_NUM, BATCH_NUM, M, N, K); // 设置矩阵排布
tiling->SetBufferSpace(-1, -1, -1);

optiling::TCubeTiling tilingData;
int ret = tiling.GetTiling(tilingData);
```

Kernel实现

创建Matmul对象。

通过MatmulType设置输入输出的Layout格式为NORMAL。

```cpp
#include "lib/matmul_intf.h"

typedef AscendC::MatmulType <AscendC::TPosition::GM, CubeFormat::ND, half, false, LayoutMode::NORMAL> aType;
typedef AscendC::MatmulType <AscendC::TPosition::GM, CubeFormat::ND, half, true, LayoutMode::NORMAL> bType;
typedef AscendC::MatmulType <AscendC::TPosition::GM, CubeFormat::ND, float, false, LayoutMode::NORMAL> cType;
typedef AscendC::MatmulType <AscendC::TPosition::GM, CubeFormat::ND, float> biasType; constexpr MatmulConfig MM_CFG = GetNormalConfig(false, false, false, BatchMode::BATCH_LESS_THAN_L1);
AscendC::Matmul<aType, bType, cType, biasType, MM_CFG> mm; 
```

初始化操作。REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), mm, &tiling); // 初始化matmul对象

设置左矩阵A、右矩阵B、Bias。mm.SetTensorA(gm_a); // 设置左矩阵Amm.SetTensorB(gm_b); // 设置右矩阵Bmm.SetBias(gm_bias); // 设置Bias

完成矩阵乘操作。左矩阵每次计算batchA个MK数据，右矩阵每次计算batchB个KN数据。mm.IterateBatch(gm_c, batchA, batchB, false);

结束矩阵乘操作。mm.End();

# 3.3.3.3.15 Batch Matmul 复用 Bias 矩阵

# 功能介绍

在Batch Matmul场景中，Matmul API可以一次性计算出多个大小为singleCoreM *singleCoreN的C矩阵。当Batch Matmul场景有Bias输入时，默认的Bias输入矩阵包含Batch轴，即Bias的大小为Batch * N。通过开启Bias复用功能，当每个Batch计算使用的Bias数据相同时，只需输入一个不带Batch轴的Bias矩阵。Batch Matmul的Bias矩阵复用功能默认不启用，用户需要设置MatmulConfig中的isBiasBatch参数为false来开启此功能。


图 3-50 带有 Batch 轴的 Bias 计算示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/60b013a162637a945fc2d7c5ee4b9acb04859d0ec547031bd7abca0456b0817c.jpg)


如上图所示，Batch Matmul中未复用Bias矩阵的场景，每计算出一个singleCoreM *singleCoreN大小的C矩阵，都会与1 * singleCoreN大小的Bias矩阵相加。若不同Batch的计算使用的Bias数据相同，则多Batch计算可以复用同一个Bias矩阵，如下图所示，此场景中调用SetBias接口时，只需设置一个1 * singleCoreN大小的Bias矩阵。


图 3-51 复用 Bias 计算示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/c22ab2a14c7883f42aec77d71eeaae2746261cb6052411080dbca01abf35b9fd.jpg)


# 使用场景

Batch Matmul中每个Batch的Matmul计算可以使用相同的Bias矩阵。

# 约束说明

A、B、C矩阵的Layout类型都为NORMAL时，不支持batchMode参数设为SINGLE_LARGE_THAN_L1，即Bias复用场景下，单Batch的A、B矩阵数据总和不得超过L1 Buffer的大小。

# 调用示例

完整的算子样例请参考BatchMatmul复用Bias算子样例。

```cpp
// 自定义MatmulConfig参数，将其中的isBiasBatch参数设置为false，使能BatchMatmul的Bias复用功能。
constexpr MatmulConfigMode configMode = MatmulConfigMode::CONFIG_NORM;
constexpr MatmulBatchParams batchParams = {
    false, BatchMode::BATCH_LESS_THAN_L1, false /* isBiasBatch */
};
constexpr MatmulConfig CFG_MM = GetMMConfig<configMode>(batchParams);
AscendC::Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, CFG_MM> mm;
```

```txt
REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), mm, &tiling); // 初始化matmul对象
```

```javascript
mm.SetTensorA(gm_a); // 设置左矩阵A
mm.SetTensorB(gm_b); // 设置右矩阵B
mm.SetBias(gm_bias); // 设置Bias，矩阵大小为1 * singleCoreN
mm.IterateBatch(gm_c, batchA, batchB, false);
mm.End();
```

# 3.3.4 矩阵编程（基础 API）

# 3.3.4.1 耦合模式

# 说明

本节内容为针对耦合模式，使用基础API进行矩阵乘法的编程指导。

如下章节内容暂不支持Atlas 350 加速卡。

# 编程范式

Cube编程范式把算子的实现流程分为5个基本任务：CopyIn，Split，Compute，Aggregate，CopyOut。CopyIn负责搬入操作，Split负责数据切分操作，Compute负责矩阵指令计算操作，Aggregate负责数据汇聚操作，CopyOut负责搬出操作。


图3-52 矩阵编程基本任务设计


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/286cdd6973eb87aa461ef193c461353bba024f22cbc8dce2ac1a69b544a9efeb.jpg)


具体任务之间的交互流程和流程图如下。

步骤1 Stage1：CopyIn任务。

1. 使用6.2.3.1.1 DataCopy接口将GlobalTensor数据拷贝到LocalTensor。

2. 使用 EnQue将LocalTensor放入A1/B1的Queue中。

步骤2 Stage2：Split任务。

1. 使用 DeQue从A1/B1中取出LocalTensor。

2. 使用LoadData接口将LocalTensor从A1/B1中搬运到A2/B2。

3. 使用 EnQue将计算结果LocalTensor放入到A2/B2的Queue中。

步骤3 Stage3：Compute任务。

1. 使用 DeQue从A2/B2中取出LocalTensor。

2. 使用 Mmad接口完成矩阵计算。

3. 使用 EnQue将计算结果LocalTensor放入到CO1的Queue中。

步骤4 Stage4：Aggregate任务。

1. 使用 DeQue从CO1中取出LocalTensor。

2. 使用Ascend C接口拷贝结果矩阵到CO2。

3. 使用 EnQue将计算结果LocalTensor放入到CO2的Queue中。

步骤5 Stage5：CopyOut任务。

1. 使用 DeQue接口从CO2的Queue中取出LocalTensor。

2. 使用6.2.3.1.1 DataCopy接口将LocalTensor拷贝到GlobalTensor上。

----结束


图 3-53 矩阵编程 Queue 队列


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/5ccff4d2b06f8422690240ce8a51b61fd0f7320ca3c4539c7a8fed0514c89bc7.jpg)


# 开发流程

基于Ascend C方式实现矩阵算子的流程如下图所示。


图3-54 矩阵算子实现流程


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/ee89e70f6becb1270fd95a1005f7037c051086579f271b788ae63f1ff7d57b11.jpg)


算子分析：分析算子的数学表达式、输入、输出以及计算逻辑的实现，明确需要调用的Ascend C接口。

核函数定义：定义Ascend C算子入口函数。

根据矩阵编程范式实现算子类：完成核函数的内部实现，调用私有成员函数CopyIn、SplitA、SplitB、Compute、Aggregate、CopyOut完成矩阵算子的五级流水操作。

下文将以Matmul算子为例对上述步骤进行详细介绍，Matmul算子的代码框架如下，完整代码请参见Mmad样例。

```cpp
#include "kernel_operator.h"

// 根据编程范式实现算子类
class KernelMatmul {
public:
    __aicore__ inline void Init(GM_ADDR a, GM_ADDR b, GM_ADDR c)
    {
    // ...
    }
    __aicore__ inline void Process()
    {
    CopyIn();
    SplitA();
    AscendC::LocalTensor<half> b1Local = inQueueB1.DeQue<half>();
    AscendC::LocalTensor<half> a2Local = inQueueA2.DeQue<half>();
    AscendC::LocalTensor<float> c2Local = outQueueCO2.AllocTensor<float>();
    // split matrix b into 2 parts, [32, 16] and [32, 16]
    for (int i = 0; i < 2; ++i) {
    SplitB(b1Local, i);
    Compute(a2Local);
    Aggregate(c2Local, i);
    }
    inQueueB1.FreeTensor(b1Local);
    inQueueA2.FreeTensor(a2Local);
    outQueueCO2.EnQue<float>(c2Local);
    CopyOut();
}
private:
    __aicore__ inline void CopyIn()
    {
    // ...
```

```c
}
__aicore__ inline void SplitA()
{
    // ...
}
__aicore__ inline void SplitB(const LocalTensor<half>& b1Local, const int bSplitIdx)
{
    // ...
}
__aicore__ inline void Compute(const LocalTensor<half>& a2Local)
{
    // ...
}
__aicore__ inline void Aggregate(const LocalTensor<float>& c2Local, const int bSplitIdx)
{
    // ...
}
__aicore__ inline void CopyOut()
{
    // ...
}
private:
    // ...
};
//核函数定义
extern "C" __global__ __aicore__ void matmul_custom(GM_ADDR a, GM_ADDR b, GM_ADDR c)
{
    KernelMatmul op;
    op.Init(a, b, c);
    op.Process();
}
```

# 算子分析

在开发算子代码之前需要分析算子的数学表达式、输入、输出以及计算逻辑的实现，明确需要调用的Ascend C接口。

步骤1 明确算子的数学表达式及计算逻辑。

Matmul算子完成矩阵乘操作，其数学表达式如下，形状为[m, k]的矩阵a和形状为[k,n]的矩阵b相乘，得到形状为[m, n]的矩阵c。为了方便，令m=k=n=32。

```txt
c = a * b 
```

注意需要处理的数据过大时，需要对数据进行切分并分块搬运到A2、B2，分别计算后再进行汇聚。下文的计算逻辑为了展示Split和Aggregate阶段的样例，请您根据实际需要处理的数据大小决定是否需要切分和汇聚。

计算逻辑如下：

1. 分别搬运输入数据矩阵a、b至Local Memory A1、B1。

2. 将a矩阵从A1搬运至A2。为实现部分并行，将b矩阵切分为part1和part2，形状均为[k, n / 2]，切分后再分块搬运至B2。

3. a矩阵和b矩阵part1、part2分别做矩阵乘运算，获得矩阵c的part1和part2，形状均为[m, n / 2]。计算结果在CO1存储。

4. 将矩阵c的part1和part2分别拷贝到CO2进行合并。

5. 将合并后的输出数据从CO2搬出。

步骤2 明确输入和输出。

Matmul算子有两个输入：a与b，输出为c。

本样例中算子输入支持的数据类型为half（float16），算子输出的数据类型为float32。

矩阵a、b、c的形状均为[32, 32]。

算子输入输出支持的数据格式为：ND。

# 步骤3 确定核函数名称和参数。

您可以自定义核函数名称，本样例中核函数命名为matmul_custom。

根据对算子输入输出的分析，确定核函数有3个参数a，b，c；a，b为输入在Global Memory上的内存地址，c为输出在Global Memory上的内存地址。

# 步骤4 约束分析。

由于硬件架构对矩阵乘计算的输入输出有格式约束，需要在算子实现中增加格式转换的流程。

搬运矩阵a、b至A1、B1时，将ND格式的矩阵a、b转换为NZ格式。

从A1搬运矩阵a至A2时，将NZ格式的a矩阵转换为ZZ格式；从B1搬运矩阵b到B2时将NZ格式的b矩阵转换为ZN格式。

将计算结果从CO2搬出时，将NZ格式的c矩阵转换为ND格式。

数据排布格式的相关介绍详见数据排布格式。

# 步骤5 确定算子实现所需接口。

实现外部存储和内部存储间的数据搬运，查看Ascend C API参考中的数据搬移接口，具体参考6.2.3.1.1 DataCopy。

实现矩阵数据格式转换，查看Ascend C API参考中的数据转换接口，具体参考LoadData。

矩阵计算过程涉及矩阵乘法，查看Ascend C API参考中的矩阵计算接口，具体参考 Mmad。

计算中使用到的Tensor数据结构，使用Queue队列进行管理，会使用到 EnQue、DeQue等接口。

# ----结束

通过以上分析，得到Ascend C Matmul算子的计算流程图和设计规格如下：


图 3-55 Matmul 算子的计算流程图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/e31af027268724f94fd4868d6d8557468b73b86919ae6174873499d6fcece61d.jpg)



表 3-12 Ascend C Matmul 算子设计规格


<table><tr><td>算子类型(OpType)</td><td colspan="4">Matmul</td></tr><tr><td rowspan="3">算子输入</td><td>name</td><td>shape</td><td>data type</td><td>format</td></tr><tr><td>a</td><td>(m, k) = (32, 32)</td><td>half</td><td>ND</td></tr><tr><td>b</td><td>(k, n) = (32, 32)</td><td>half</td><td>ND</td></tr><tr><td>算子输出</td><td>c</td><td>(m, n) = (32, 32)</td><td>float32</td><td>ND</td></tr><tr><td>核函数名称</td><td colspan="4">matmul_custom</td></tr><tr><td rowspan="4">使用的主要接口</td><td colspan="4">DataCopy: 数据搬移接口</td></tr><tr><td colspan="4">LoadData: 矩阵数据格式转换接口</td></tr><tr><td colspan="4">Mmad: 矩阵乘计算接口</td></tr><tr><td colspan="4">EnQue、DeQue等接口: Queue队列管理接口</td></tr><tr><td>算子实现文件名称</td><td colspan="4">matmul_custom.cpp</td></tr></table>

# 核函数定义

根据2.2.3.2 核函数中介绍的规则进行核函数的定义。

步骤1 函数原型定义。

本样例中，函数名为matmul_custom（核函数名称可自定义）；根据算子分析中对算子输入输出的分析，确定有3个参数a，b，c，其中a，b都为输入内存，c为输出内存。根据2.2.3.2 核函数中核函数的规则介绍，函数原型定义如下所示：使用__global__函数类型限定符来标识它是一个核函数，可以被<<<>>>调用；使用__aicore__函数类型限定符来标识该核函数在设备端aicore上执行；为方便起见，统一使用GM_ADDR宏修饰入参，GM_ADDR宏定义请参考2.2.3.2 核函数。

```txt
extern "C" __global__ __aicore__ void matmul_custom(GM_ADDR a, GM_ADDR b, GM_ADDR c)
{
} 
```

步骤2 调用算子类的Init和Process函数。

算子类的Init函数，完成内存初始化相关工作，Process函数完成算子实现的核心逻辑，具体介绍参见算子类实现。

```txt
extern "C" __global__ __aicore__ void matmul_custom(GM_ADDR a, GM_ADDR b, GM_ADDR c)
{
    KernelMatmul op;
    op.Init(a, b, c);
    op.Process();
} 
```

步骤3 对核函数进行封装，得到matmul_custom_do函数，便于主程序调用。#ifndefASCENDC_CPU_DEBUG表示该封装函数仅在编译运行NPU侧的算子时会用到，编译运行CPU侧的算子时，可以直接调用matmul_custom函数。根据核函数定义和调用章节，调用核函数时，除了需要传入参数a，b，c，还需要传入numBlocks（核函数执行的核数），l2ctrl（保留参数，设置为nullptr），stream（应用程序中维护异步操作执行顺序的stream）来规定核函数的执行配置。

```c
#ifndef ASCENDC_CPU_DEBUG
// call of kernel function
void matmul_custom_do(uint32_t numBlocks, void* l2ctrl, void* stream, uint8_t* a, uint8_t* b, uint8_t* c)
{
    matmul_custom<<<numBlocks, l2ctrl, stream>>>(a, b, c);
}
#endif 
```

----结束

# 算子类实现

根据上一章节介绍，核函数中会调用算子类的Init和Process函数，本章具体讲解基于编程范式实现算子类。矩阵编程范式请参考编程范式。

算子类中主要包含对外开放的初始化Init函数和核心处理函数Process以及一些实现中会用到的私有成员。KernelMatmul算子类的定义如下：

```cpp
class KernelMatmul {
public:
    __aicore__ inline KernelMatmul() {}
    // 初始化函数，完成内存初始化相关操作
    __aicore__ inline void Init(GM_ADDR a, GM_ADDR b, GM_ADDR c) {}
    // 核心处理函数，实现算子逻辑
    // 调用私有成员函数CopyIn、SplitA、SplitB、Compute、Aggregate、CopyOut完成矩阵算子的五级流水操作
    __aicore__ inline void Process() {}

private:
    __aicore__ inline void CopyND2NZ(const LocalTensor<half>& dst, const GlobalTensor<half>& src, const uint16_t height, const uint16_t width) {}
    // 搬进函数，完成编程范式中的CopyIn阶段的处理，由Process函数调用
    __aicore__ inline void CopyIn() {}
    // 搬进函数，完成编程范式中的Split阶段的处理，由Process函数调用
    __aicore__ inline void SplitA() {}
```

```cpp
// 搬进函数，完成编程范式中的Split阶段的处理，由Process函数循环调用两次，分别搬运b矩阵的两个part
__aicore__ inline void SplitB(const LocalTensor<half>& b1Local, const int bSplitIdx) {}
// 计算函数，完成编程范式中的Compute阶段的处理，由Process函数循环调用两次，分别计算出矩阵c的两个part
__aicore__ inline void Compute(const LocalTensor<half>& a2Local) {}
// 搬出函数，完成编程范式中的Aggregate阶段的处理，由Process函数循环调用两次，分别搬出矩阵c的两个part
__aicore__ inline void Aggregate(const LocalTensor<float>& c2Local, const int bSplitIdx) {}
// 搬出函数，完成编程范式中的CopyOut阶段的处理，由Process函数调用
__aicore__ inline void CopyOut() {}

private:
AscendC::TPipe pipe; // Pipe内存管理对象，管理Queue队列的内存
AscendC::TQue<AscendC::TPosition::A1, 1> inQueueA1; // 输入数据的队列，TPosition为A1
AscendC::TQue<AscendC::TPosition::A2, 1> inQueueA2; // 输入数据的队列，TPosition为A2
AscendC::TQue<AscendC::TPosition::B1, 1> inQueueB1; // 输入数据的队列，TPosition为B1
AscendC::TQue<AscendC::TPosition::B2, 2> inQueueB2; // 输入数据的队列，TPosition为B2
AscendC::TQue<AscendC::TPosition::CO1, 2> outQueueCO1; // 输出数据的队列，TPosition为CO1
AscendC::TQue<AscendC::TPosition::CO2, 1> outQueueCO2; // 输出数据的队列，TPosition为CO2
// 管理输入输出Global Memory内存地址的对象，其中aGM，bGM为输入，cGM为输出
AscendC::GlobalTensor<half> aGM, bGM;
AscendC::GlobalTensor<float> cGM;

uint16_t m = 32;
uint16_t n = 32;
uint16_t k = 32;
uint16_t aSize, bSize, cSize, mBlocks, nBlocks, kBlocks;
};
```

# KernelMatmul构造函数实现

构造函数中对私有成员变量进行初始化，具体代码如下：

```lisp
__aicore__ inline KernelMatmul()
{
    aSize = m * k;
    bSize = k * n;
    cSize = m * n;
    mBlocks = m / 16;
    nBlocks = n / 16;
    kBlocks = k / 16;
} 
```

矩阵a的形状为[m, k]，矩阵b的形状为[k, n]，矩阵c的形状为[m,n]，此样例中m、n、k均设置为32。

aSize、bSize、cSize分别为矩阵a、b、c的数值个数。

mBlocks、 nBlocks、 kBlocks为m、n、k所占分形数量，half类型一个分形Shape为16 * 16，blocks计算公式为：

mBlocks = m / 16 

nBlocks = n / 16 

kBlocks = k / 16 

分形具体介绍可参考2.9.2.2 数据排布格式。

# Init函数实现

Init函数主要完成以下内容：

设置输入输出Global Tensor的Global Memory内存地址。

以设置输入a在Global Memory上的内存偏移地址为例：

aGM.SetGlobalBuffer((__gm__ half*)a); 

注意，因为本样例中Init函数的入参统一设置为uint8_t*，这里需要强转成具体的数据类型(__gm__ half*)，再进行偏移。

通过Pipe内存管理对象为输入输出Queue分配内存。

比如，为输入数据队列inQueueB2分配内存，可以通过如下代码段实现：

```javascript
pipe.InitBuffer(inQueueB2, 2, bSize * sizeof(half) / 2); 
```

此样例中将b矩阵切分为两个part，为inQueueB2分配内存时需要申请两块内存空间，每一块的大小为b矩阵大小的一半，outQueueCO1的内存初始化同理。


具体的初始化函数代码如下：


```txt
__aicore__ inline void Init(GM_ADDR a, GM_ADDR b, GM_ADDR c)
{
    aGM.SetGlobalBuffer((__gm__ half*)a);
    bGM.SetGlobalBuffer((__gm__ half*)b);
    cGM.SetGlobalBuffer((__gm__ float*)c);
    pipe.InitBuffer(inQueueA1, 1, aSize * sizeof(half));
    pipe.InitBuffer(inQueueA2, 1, aSize * sizeof(half));
    pipe.InitBuffer(inQueueB1, 1, bSize * sizeof(half));
    pipe.InitBuffer(inQueueB2, 2, bSize * sizeof(half) / 2);
    pipe.InitBuffer(outQueueCO1, 2, cSize * sizeof(float) / 2);
    pipe.InitBuffer(outQueueCO2, 1, cSize * sizeof(float));
} 
```

# Process函数实现

基于矩阵编程范式，将核函数的实现分为5个基本阶段：CopyIn，Split，Compute，Aggregate，CopyOut。Split，Compute，Aggregate阶段需要区分a、b矩阵。Process函数中通过如下方式调用这几个函数。

```cpp
__aicore__ inline void Process()
{
    CopyIn();
    SplitA();
    AscendC::LocalTensor<half> b1Local = inQueueB1.DeQue<half>();
    AscendC::LocalTensor<half> a2Local = inQueueA2.DeQue<half>();
    AscendC::LocalTensor<float> c2Local = outQueueCO2 AllocTensor<float>();
    // split matrix b into 2 parts, [32, 16] and [32, 16]
    for (int i = 0; i < 2; ++i) {
    SplitB(b1Local, i);
    Compute(a2Local);
    Aggregate(c2Local, i);
    }
    inQueueB1.FreeTensor(b1Local);
    inQueueA2.FreeTensor(a2Local);
    outQueueCO2.EnQue<float>(c2Local);
    CopyOut();
} 
```

两次循环内，SplitB需要从inQueueB1中分别搬运两个part的b矩阵，Compute需要分别计算a矩阵和两个part b矩阵的乘法，Aggregate要分别搬运两个part的c矩阵，具体五个阶段数据流通示意图如下：


图 3-56 数据流通示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/6dd27d43fd73783558d7b54fd3a54dac58094504b68906dde6fc43e365be7b7e.jpg)


切分b矩阵，可以实现一部分的并行，本样例的流水并行示意图如下：


图 3-57 并行示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/963def8f2c6fa5dded06a4dc1a9e6eba7a704cccbe2b5367259fc6a89daafee0.jpg)


步骤1 Stage1：CopyIn函数实现。

1. 使用 AllocTensor从A1，B1的Queue中申请a1Local和b1Local。

2. 使用6.2.3.1.1 DataCopy接口将矩阵a、b搬运到Local Memory，同时将其数据格式从ND转换为NZ。

一次DataCopy指令搬运height*16个数，循环执行width/16次。DataCopy的参数设置如下：

blockCount设置为height，共搬运height次。

blockLen设置为1，一次搬运16个类型为half的数。

srcStride设置为width/16 - 1，源矩阵每搬运一个block需要跳跃一行。

dstStride设置为0，目的矩阵每个block在内存中连续存储。

每次循环迭代，源矩阵首地址移动16个数，目的矩阵首地址移动16*height个数。

格式转换示意图如下，第一次循环搬运蓝色部分，第二次循环搬运绿色部分；图中width为32，占两个分形，height为32，占两个分形，一共搬运4个16*16分形。


图 3-58 ND to NZ 转换示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/b7ad1affed984c7cfd3fe86814800b5cab477cbd6df0921131e4a3605aa0fc43.jpg)


注意：上述ND到NZ的格式转换仅作为举例说明，开发者可根据实际情况选择合适的转换方式。

3. 使用 EnQue将a1Local、b1Local分别放入A1、B1的Queue中。


具体代码如下：


```cpp
__aicore__ inline void CopyND2NZ(const LocalTensor<half>& dst, const GlobalTensor<half>& src, const uint16_t height, const uint16_t width)
{
    for (int i = 0; i < width / 16; ++i) {
    int srcOffset = i * 16;
    int dstOffset = i * 16 * height;
    AscendC::DataCopy(dst[dstOffset], src[srcOffset], { height, 1, uint16_t(width / 16 - 1), 0 });
    }
}
__aicore__ inline void CopyIn()
{
    AscendC::LocalTensor<half> a1Local = inQueueA1.AllocTensor<half>();
    AscendC::LocalTensor<half> b1Local = inQueueB1.AllocTensor<half>();
    CopyND2NZ(a1Local, aGM, m, k);
    CopyND2NZ(b1Local, bGM, k, n);
    inQueueA1.EnQue(a1Local);
    inQueueB1.EnQue(b1Local);
} 
```

步骤2 Stage2：SplitA函数实现。

1. 使用 DeQue从A1的Queue中取出a1Local。

2. 使用 AllocTensor从A2的Queue中申请a2Local。

3. 使用 LoadData将矩阵a搬运到A2，同时将a矩阵从NZ格式转换为ZZ格式。

搬运及格式转换示意图如下：图中k为32，占kBlocks（k/16=2）个分形，m为32，占mBlocks（m/16=2）个分形，一共搬运4个16*16分形。本示例中，调用一次LoadData接口完成两个16*16分形的搬运，循环调用两次LoadData。第一次循环搬运蓝色部分两个分形，第二次循环搬运绿色部分两个分形。

单次循环中LoadData（本样例中要完成2个分形的搬运，蓝色部分或者绿色部分）的参数设置如下：

repeatTimes表示数据处理的迭代次数，因为LoadData每个迭代处理一个分形，所以也可以理解为待搬运分形的个数。本样例中即为k轴方向的分形个数，设置为kBlocks，表示搬运kBlocks个分形。

srcStride表示，相邻迭代间源操作数分形首地址之间的间隔，以搬运蓝色部分分形为例：下图中左侧源操作数矩阵，第一个蓝色分形和第二个蓝色分形起始地址之间的间隔为mBlocks个分形，此处设置为mBlocks。

dstGap使用默认值，目的矩阵两个分形连续存储。

ifTranspose设置为false，每块分形搬运前搬运后都为Z格式，不使能转置。

每次循环迭代源矩阵首地址偏移16*16，目的矩阵首地址偏移16*k。


图 3-59 NZ to ZZ 格式转换示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/f81e2389e68f12af570b52c569cf46b665d17a4f4519f83df8c5dd7c8d8fc449.jpg)


# 4. 使用 EnQue将计算结果a2Local放入到A2的Queue中。


具体代码如下：


```cpp
__aicore__ inline void SplitA()
{
    int srcOffset = 0;
    int dstOffset = 0;
    AscendC::LocalTensor<half> a1Local = inQueueA1.DeQue<half>();
    AscendC::LocalTensor<half> a2Local = inQueueA2.AllocTensor<half>();

    // transform nz to zz
    for (int i = 0; i < mBlocks; ++i) {
    AscendC::LoadData2DParams loadDataParams;
    loadDataParams.repeatTimes = kBlocks;
    loadDataParams.srcStride = mBlocks;
    loadDataParams.ifTranspose = false;

    AscendC::LoadData(a2Local[dstOffset], a1Local[srcOffset], loadDataParams);

    srcOffset += 16 * 16;
    dstOffset += k * 16;
    }

    inQueueA2.EnQue<half>(a2Local);
    inQueueA1.FreeTensor(a1Local);
} 
```

# 步骤3 Stage2：SplitB函数实现。

1. SplitB需要传入两个参数：使用 DeQue从B1的Queue中取出的b1Local和循环迭代变量index。

2. 使用 AllocTensor从B2的Queue中申请b2Local。

3. 使用 LoadData将b矩阵搬运到B2，同时从NZ格式转换为ZN格式。

搬运及格式转换示意图如下：图中k为32，占kBlocks（k/16=2）个分形，n为32，占nBlocks（n/16=2）个分形，一共搬运4个16*16分形。本示例中，调用一次LoadData接口完成两个16*16分形的搬运，循环调用两次LoadData。第一次循环搬运蓝色部分两个分形，第二次循环搬运绿色部分两个分形。

单次循环中LoadData（本样例中要完成2个分形的搬运，蓝色部分或者绿色部分）的参数设置如下：

repeatTimes表示数据处理的迭代次数，因为LoadData每个迭代处理一个分形，所以也可以理解为待搬运分形的个数。本样例中即为k轴方向的分形个数，设置为kBlocks，表示搬运kBlocks个分形。

srcStride相邻迭代间源操作数分形首地址之间的间隔，以搬运蓝色部分分形为例：下图中左侧源操作数矩阵，第一个蓝色分形和第二个蓝色分形起始地址之间的间隔为1个分形，此处设置为1，源矩阵两个分形连续存储。

dstGap使用默认值0，目的矩阵两个分形连续存储。

ifTranspose设置为true，每块分形搬运前为Z格式，搬运后需要为N格式，需要使能转置。

每次循环迭代，源矩阵首地址需要偏移k*n/2。


图 3-60 NZ to ZN 格式转换示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/dcae29319f508ef60257fea619c5ed019900b5bc153b21c1b3e85196b66fb5af.jpg)


# 4. 使用 EnQue将计算结果b2Local放入到B2的Queue中。


具体代码如下：


```cpp
__aicore__ inline void SplitB(const AscendC::LocalTensor<half>& b1Local, const int bSplitIdx)
{
    AscendC::LocalTensor<half> b2Local = inQueueB2.AllocTensor<half>();

    // transform nz to zn
    AscendC::LoadData2DParams loadDataParams;
    loadDataParams.repeatTimes = kBlocks;
    loadDataParams.srcStride = 1;
    loadDataParams.ifTranspose = true;

    AscendC::LoadData(b2Local, b1Local[bSplitIdx * bSize / 2], loadDataParams);

    inQueueB2.EnQue<half>(b2Local);
} 
```

步骤4 Stage3：Compute函数实现，完成核心的矩阵计算功能。

1. Compute函数需要传入参数a2Local，a2Local从A2的Queue中使用 DeQue取出。

2. 使用 AllocTensor从CO1的Queue中申请c1Local。

3. 使用 DeQue从B2中取出b2Local。

4. 使用 Mmad完成矩阵乘计算。

5. 使用 EnQue将计算结果c1Local放入到CO1的Queue中。


具体代码如下：


```cpp
__aicore__ inline void Compute(const AscendC::LocalTensor<half>& a2Local)
{
    AscendC::LocalTensor<half> b2Local = inQueueB2.DeQue<half>();
    AscendC::LocalTensor<float> c1Local = outQueueCO1.AllocTensor<float>();
    AscendC::MmadParams mmadParams;
    mmadParams.m = m;
    mmadParams.n = n / 2;
    mmadParams.k = k;
    AscendC::Mmad(c1Local, a2Local, b2Local, mmadParams);

    outQueueCO1.EnQue<float>(c1Local);
    inQueueB2.FreeTensor(b2Local);
} 
```

步骤5 Stage4：Aggregate函数实现，完成数据汇聚操作。

1. Aggregate需要传入两个参数：使用 AllocTensor从CO2的Queue中申请的c2Local和循环迭代变量index。

2. 使用 DeQue从CO1中取出c1Local。

3. 使用6.2.3.1.1 DataCopy将结果矩阵从CO1搬运到CO2。

DataCopy参数设置如下：

blockCount设置为1，blockLen设置为2，连续搬运两个分形，无需格式转换。

blockMode设置为BlockMode::BLOCK_MODE_MATRIX，表示需要按分形搬运。

c2Local首地址偏移量设置为index * cSize / 2。


具体代码如下：


```cpp
__aicore__ inline void Aggregate(const AscendC::LocalTensor<float>& c2Local, const int bSplitIdx)
{
    AscendC::LocalTensor<float> c1Local = outQueueCO1.DeQue<float>();
    AscendC::DataCopyParams dataCopyParams;
    dataCopyParams.blockCount = 1;
    dataCopyParams.blockLen = 2;
    AscendC::DataCopyEnhancedParams enhancedParams;
    enhancedParams.blockMode = AscendC::BlockMode::BLOCK_MODE_MATRIX;
    AscendC::DataCopy(c2Local[bSplitIdx * cSize / 2], c1Local, dataCopyParams, enhancedParams);
    outQueueCO1.FreeTensor(c1Local);
} 
```

步骤6 Stage5：CopyOut函数实现。

1. 使用 DeQue从CO2中取出c2Local。

2. 使用6.2.3.1.1 DataCopy将结果矩阵从CO2搬运到Global Memory，同时需要将格式从NZ转换为ND。

每次循环移动一个分形，搬运m*16个数。DataCopy参数说明如下：

blockCount设置为m，共搬运m次。

blockLen设置为2，DataCopy指令一次搬运2个block，每个block16个数。

srcStride设置为0，每两次搬运间没有间隙。

dstStride设置为(nBlocks - 1) * 2，每两次搬运间隔2个block。

每次循环迭代，目的矩阵偏移16，源矩阵偏移m*16。

格式转换示意图如下，第一次循环搬运蓝色部分数据，第二次循环搬运绿色部分数据。


图 3-61 NZ to ND 格式转换示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/d54f6fce39bc6993a9d43b68fc561fc61ff8fca8c9c5e52199dc25ff980035e4.jpg)



具体代码如下：


```cpp
__aicore__ inline void CopyOut()
{
    AscendC::LocalTensor<float> c2Local = outQueueCO2.DeQue<float>();
    // transform nz to nd
    for (int i = 0; i < nBlocks; ++i) {
    AscendC::DataCopy(cGM[i * 16], c2Local[i * m * 16], { m, 2, 0, uint16_t((nBlocks - 1) * 2) });
    }
    outQueueCO2.FreeTensor(c2Local);
} 
```

----结束

# 3.3.4.2 分离模式

# 说明

本节内容为针对分离模式，使用基础API进行矩阵乘法的编程指导。

如下章节内容暂不支持Atlas 350 加速卡。

针对分离模式，使用基础API进行矩阵乘法算子实现的编程范式和3.3.4.1 耦合模式致，由于硬件架构不同，具体实现有一些差异，本节仅提供差异点说明。完整代码请参见Mmad样例。

# CopyIn阶段差异

# 耦合模式

在CopyIn阶段，即从GM->A1/B1（L1 Buffer）的阶段，耦合模式下可以使用DataCopy接口直接将数据从GM搬入L1 Buffer，也可以将数据从GM搬入UB，再搬入L1 Buffer。若需要ND2NZ的格式转换，需要开发者自行完成；或使用DataCopy接口提供的随路格式转换功能，但该功能会使用UB临时空间。

如下示例，直接使用了GM->A1/B1的数据搬运指令，自行完成ND2NZ的格式转换。

```cpp
__aicore__ inline void CopyND2NZ(const AscendC::LocalTensor<half>& dst, const AscendC::GlobalTensor<half>& src,
    const uint16_t height, const uint16_t width)
{
    for (int i = 0; i < width / 16; ++i) {
    int srcOffset = i * 16;
    int dstOffset = i * 16 * height;
    AscendC::DataCopy(dst[dstOffset], src[srcOffset], { height, 1, uint16_t(width / 16 - 1), 0 });
    }
}
__aicore__ inline void CopyIn()
{
    AscendC::LocalTensor<half> a1Local = inQueueA1.AllocTensor<half>();
    AscendC::LocalTensor<half> b1Local = inQueueB1.AllocTensor<half>();

    CopyND2NZ(a1Local, aGM, m, k);
    CopyND2NZ(b1Local, bGM, k, n);

    inQueueA1.EnQue(a1Local);
    inQueueB1.EnQue(b1Local);
} 
```

# 分离模式

分离模式下数据无法经过VECIN/VECCALC/VECOUT (UB) 直接搬运到A1/B1(L1 Buffer) ，但是使用DataCopy接口提供的随路格式转换功能一条指令即可完成格式转换，无需UB作为临时空间。


示例如下：


```cpp
__aicore__ inline void CopyIn()
{
    AscendC::LocalTensor<half> a1Local = inQueueA1.AllocTensor<half>();
    AscendC::LocalTensor<half> b1Local = inQueueB1.AllocTensor<half>();

    AscendC::Nd2NzParams nd2nzA1Params;
    nd2nzA1Params.ndNum = 1;
    nd2nzA1Params.nValue = m;
    nd2nzA1Params.dValue = k;
    nd2nzA1Params.srcNdMatrixStride = 0;
    nd2nzA1Params.srcDValue = k;
    nd2nzA1Params.dstNzC0Stride = CeilCubeBlock(m) * CUBE_BLOCK;
    nd2nzA1Params.dstNzNStride = 1;
    nd2nzA1Params.dstNzMatrixStride = 0;
    AscendC::DataCopy(a1Local, aGM, nd2nzA1Params);

    AscendC::Nd2NzParams nd2nzB1Params;
    nd2nzB1Params.ndNum = 1;
    nd2nzB1Params.nValue = k;
    nd2nzB1Params.dValue = n;
    nd2nzB1Params.srcNdMatrixStride = 0;
    nd2nzB1Params.srcDValue = n;
    nd2nzB1Params.dstNzC0Stride = CeilCubeBlock(k) * CUBE_BLOCK;
    nd2nzB1Params.dstNzNStride = 1;
    nd2nzB1Params.dstNzMatrixStride = 0;
    AscendC::DataCopy(b1Local, bGM, nd2nzB1Params);

    inQueueA1.EnQue(a1Local);
    inQueueB1.EnQue(b1Local);
} 
```

# Aggregate及CopyOut阶段差异

# 耦合模式

耦合模式中，完成矩阵乘计算后数据存放于CO1（L0C Buffer），最终搬入GM需要通过CO2（UB），且NZ2ND的格式转换需要在CO1->CO2->GM阶段中手动完成。如下样例，在Aggregate阶段将NZ格式数据从CO1搬入CO2中，在CO2->GM的阶段使用for循环调用DataCopy完成了格式转换。

```cpp
__aicore__ inline void Aggregate(const AscendC::LocalTensor<float>& c2Local, const int bSplitIdx)
{
    AscendC::LocalTensor<float> c1Local = outQueueCO1.DeQue<float>();
    AscendC::DataCopyParams dataCopyParams;
    dataCopyParams.blockCount = 1;
    dataCopyParams.blockLen = 2;
    AscendC::DataCopyEnhancedParams enhancedParams;
    enhancedParams.blockMode = AscendC::BlockMode::BLOCK_MODE_MATRIX;
    AscendC::DataCopy(c2Local[bSplitIdx * cSize / 2], c1Local, dataCopyParams, enhancedParams);

    outQueueCO1.FreeTensor(c1Local);
}
__aicore__ inline void CopyOut()
{
    AscendC::LocalTensor<float> c2Local = outQueueCO2.DeQue<float>();
    // transform nz to nd
    for (int i = 0; i < nBlocks; ++i) {
    AscendC::DataCopy(cGM[i * 16], c2Local[i * m * 16], { m, 2, 0, uint16_t((nBlocks - 1) * 2) });
    }

    outQueueCO2.FreeTensor(c2Local);
} 
```

# 分离模式

分离模式下，矩阵乘的计算结果从CO1（L0C Buffer）可以通过Fixpipe通路直接写入GM，而且Fixpipe提供了随路NZ2ND的功能，方便用户做格式转换。样例如下，样例中省去了Aggregate阶段，直接CopyOut。

```cpp
__aicore__ inline void CopyOut()
{
    AscendC::LocalTensor<float> c1Local = outQueueCO1.DeQue<float>();
    AscendC::FixpipeParamsV220 fixpipeParams;
    fixpipeParams.nSize = n;
    fixpipeParams.mSize = m;
    fixpipeParams.srcStride = m;
    fixpipeParams.dstStride = n;

    fixpipeParams.ndNum = 1;
    fixpipeParams.srcNdStride = 0;
    fixpipeParams.dstNdStride = 0;
    AscendC::Fixpipe(cGM, c1Local, fixpipeParams);
    outQueueCO1.FreeTensor(c1Local);
} 
```

# 3.3.5 融合算子编程

# 3.3.5.1 CV 融合

# 3.3.5.1.1 基础知识

# 说明

学习融合算子编程之前，请确保已经掌握矩阵编程相关知识。

# CV融合算子

融合算子由多个独立的小算子融合而成，其功能与多个小算子的功能等价，性能方面通常优于独立的小算子。用户可以根据实际业务场景诉求，按照具体算法自由融合向量（Vector）、矩阵（Cube）算子以达到性能上的收益。融合了Cube计算、Vector计算的算子统称为CV融合算子。

比如对于LLM大模型中最核心的一个融合算子Flash Attention， 其核心实现如下图。图中的Matmul算子（Cube）、Scale算子（Vector）、Mask算子（Vector）、SoftMax算子（Vector）融合为一个大的算子Flash Attention。


图 3-62 Flash Attention 核心实现


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/30d102aa2d8bfb1f4cd7fa049da7d3db320f851194bb1155bbcc5f5ddc299ca4.jpg)


# 使用场景和优势

针对有数据依赖的矢量算子和矩阵算子，可以通过融合算子编程的方式，将矢量算子和矩阵算子进行融合，通过一个算子Kernel函数来承载，由此来获得性能上的收益。下图展示了独立矢量算子和矩阵算子、Mix融合算子的执行耗时对比，由此可以看出为什么开发Mix融合算子会带来性能上的收益。


图 3-63 独立矢量算子和矩阵算子、Mix 融合算子的执行耗时对比


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/79fb61a7a6184ff59c4d9d836df09a765bb57968f5dd6861f4bcef0971a7a42e.jpg)


独立的矢量算子和矩阵算子实现：矩阵计算后的结果需要搬运到Global Memory上，然后由Global Memory搬运到Local Memory，再进行矢量算子的计算，计算和搬运都是串行执行，另外多个算子的调度执行，会增加算子的调度耗时。

融合算子的实现方法：可以对数据进行切片，再通过流水的设计，使得矢量计算单元和矩阵计算单元实现并行计算；另外相比于不融合的单算子，减少了算子的调度耗时。

除了有效提升算子性能，充分发挥AI处理器的算力，融合算子还有如下优势：

减少计算量：融合算子可以将多个算子合并为一个，简化计算过程，减少计算量，提高计算效率。

减少内存占用：融合算子可以将多个算子的中间结果合并为一个，从而减少内存占用，提高内存利用率。

优化数据流：融合算子可以优化数据流，减少数据在不同算子之间的传输，从而提高数据处理效率。

简化代码实现：融合算子可以简化代码实现，减少代码量，提高代码可读性和可维护性。

总之，融合算子是一种优化计算的有效手段，可以提高计算效率和内存利用率，优化数据流，简化代码实现。

# 编程范式

Ascend C提供融合算子的编程范式，方便开发者基于该范式表达融合算子的数据流，快速实现自定义的融合算子。

融合算子数据流指融合算子的输入输出在各存储位置间的流向。以一个典型的Cube和Vector融合算子为例，逻辑位置间的数据流向如下图所示：

Cube的输出可以作为Vector的输入：CO2->VECIN

Vector的输出可以作为Cube的输入：VECOUT->A1->A2、VECOUT->B1->B2

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/ad48993c96b11253cc6ff02a01c6b44ed17c1b411493262f7724669117341b7d.jpg)


基于Matmul高阶API的融合算子编程范式，对上述数据流简化表达如下：


图3-64 融合算子编程范式


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/73e62146cc3ab4bb8855d50836e13f50a05d50b479d64a3c952bff2a5cba7c22.jpg)


1. 初始化一个Matmul对象。

2. 进行Matmul内部的计算。

3. 将Matmul的计算结果搬运到Vector核上。

4. 进行Vector矢量计算。

5. 将输出结果搬运到Global Memory上。

整个过程的示例代码如下（伪代码）。完整样例请参考MatmulLeakyRelu。

```cpp
template<typename aType, typename bType, typename cType, typename biasType>
__aicore__ inline void MatmulLeakyKernel<aType, bType, cType, biasType>::Process()
{
    uint32_t computeRound = 0;
    matmulObj.SetTensorA(aGlobal);
    matmulObj.SetTensorB(bGlobal);
    matmulObj.SetBias(biasGlobal);

    while (matmulObj.template Iterate<true>() { // 步骤2：进行Matmul内部的计算。
    // 步骤3：将Matmul的计算结果搬运到Vector核上。
    reluOutLocal = reluOutQueue_.AllocTensor<cType>();
    matmulObj.template GetTensorC<true>(reluOutLocal, false, true);
    // 步骤4：进行Vector矢量计算。
```

```rust
AscendC::LeakyRelu(reluOutLocal, reluOutLocal, (cType)alpha, tiling.baseM * tiling.baseN);
reluOutQueue_.EnQue(reluOutLocal);
// 步骤5：将输出结果搬运到Global Memory上
reluOutQueue_.DeQue<cType>();
...
AscendC::DataCopy(cGlobal[startOffset], reluOutLocal, copyParam);
reluOutQueue_.FreeTensor(reluOutLocal);
computeRound++;
}
matmulObj.End();
}

// kernel入口函数，__mix__(1, 2)表示：mix场景，AIC:AIV=1:2。
__global__ __mix__(1, 2) void matmul_leakyrelu_custom(GM_ADDR a, GM_ADDR b, GM_ADDR bias, GM_ADDR c,
    __kfc_workspace__ GM_ADDR workspace, AscendC::tiling::TCubeTiling
tiling)
{
    AscendC::TPipe pipe;
    MatmulLeakyKernel<half, half, float, float> matmulLeakyKernel;
    matmulLeakyKernel.Init(a, b, bias, c, workspace, tiling, &pipe);
    // 步骤1：初始化Matmul对象。
    REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), matmulLeakyKernel.matmulObj, &matmulLeakyKernel.tiling);
    matmulLeakyKernel.Process(&pipe);
}
```

# 3.3.5.1.2 算子实现

下文将以Matmul+LeakyRelu融合算子的实现为例，介绍Mix融合算子的设计和实现流程。该样例仅支持在Atlas A2 训练系列产品/Atlas A2 推理系列产品上运行。

算子的设计过程分为算子分析、数据流分析、Tiling策略设计三部分。

# 算子分析

算子分析是指明确算子的数学表达式、输入、输出，核函数的名称等信息。

步骤1 明确算子的数学表达式及计算逻辑。该算子的计算逻辑为，先进行一个矩阵乘操作，然后将矩阵乘的结果与一个alpha参数进行LeakyRelu操作。数学表达式如下：

```javascript
c = LeakyRelu(a * b + bias, alpha); 
```

步骤2 明确输入和输出。

Matmul+LeakyRelu算子输入为a、b、bias，输出为c。alpha作为激活函数LeakyRelu的系数，为固定值，可以在算子实现中直接使用常数值参与计算。

本样例中算子输入a、b支持的数据类型为half（float16），算子输入bias支持的数据类型为float32，算子输出c的数据类型为float32。

输入矩阵a的形状为[M，K]，输入矩阵b的形状为[K, N]，输出矩阵c的形状为[M，N]，输入bias的形状为[1, N]。

算子输入输出支持的数据格式为：ND。

步骤3 确定核函数名称和参数。

● 您可以自定义核函数名称，本样例中核函数命名为matmul_leakyrelu_custom。

根据对算子输入输出的分析，确定核函数的参数a，b，bias，c；a，b, bias为输入在Global Memory上的内存地址，c为输出在Global Memory上的内存地址。

----结束

通过以上分析，得到Ascend C Matmul+LeakyRelu算子的设计规格如下：

算子类型（OpType）：MATMUL_LEAKYRELU

算子输入输出：


表 3-13 MATMUL_LEAKYRELU 算子输入输出规格


<table><tr><td>name</td><td>shape</td><td>data type</td><td>format</td></tr><tr><td>a(输入)</td><td>[M, K]</td><td>half</td><td>ND</td></tr><tr><td>b(输入)</td><td>[K, N]</td><td>half</td><td>ND</td></tr><tr><td>bias(输入)</td><td>[1, N]</td><td>float32</td><td>ND</td></tr><tr><td>z(输出)</td><td>[M, N]</td><td>float32</td><td>ND</td></tr></table>

核函数名称：matmul_leakyrelu_custom

# 数据流分析

进行算子的数据流分析：数据流向为在Cube核上完成Matmul计算后将数据搬运至Vector核进行LeakyRelu计算。根据上述数据流并结合融合算子的编程范式，规划并行的流水任务。如下图所示：

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/b62d25120965e9808edee6932fa847cee0d2ced6663992e590cd8fd269e73fef.jpg)


步骤1 将输入数据从Global Memory搬运到Cube核。

步骤2 进行Matmul内部的计算，计算公式和计算示意图如下：

注：bias的shape为[1, N]，对A*B结果矩阵的每一行都采用该bias进行偏置。


图 3-65 Matmul 矩阵乘示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/0ab54b217db60bbb802526b38eaa38af088e6a16e803623576d708e1cb9345fd.jpg)


步骤3 将Matmul的计算结果搬运到Vector核。

步骤4 进行Vector矢量计算，该样例中进行LeakyReLU计算。

Leaky ReLU（带泄露线性整流函数）激活函数，是人工神经网络中一种常用的激活函数，其数学表达式和函数图像如下所示：

$$
f (x _ {i}) = \left\{ \begin{array}{l l} x _ {i} & \text {if} x _ {i} \geq 0 \\ a x _ {i} & \text {if} x _ {i} <   0 \end{array} \right.
$$

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/60e54daf10c39a101136b36525c65de9537d9dda682221b95fc846fc7bde1670.jpg)


步骤5 将输出结果搬运到Global Memory。

----结束

前三步的内容都封装在Matmul高阶API内，本样例中可以简化为3个stage。如下图所示：

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/9998025f08f3235ab5b582f3baf5b005a6ca18ee0df89df240e5a02bcff38b15.jpg)


根据上述分析，明确实现过程中会使用到Matmul高阶API接口，LeakyRelu Vector计算接口、6.2.3.1.1 DataCopy、 EnQue、 DeQue接口。

# Tiling 策略设计

Tiling策略的设计主要包括多核切分和核内切分策略。

多核切分: 根据当前核数，对输入shape的M, K, N进行多核切分，得到单核内shape大小singleCoreM, singleCoreK, singleCoreN。

核内切分: 根据Local Memory的大小约束，对单核内的shape大小进一步切分，得到A、B、C矩阵参与一次矩阵乘指令的shape大小baseM, baseN, baseK。切分时需要注意：GetTensorC的结果如果放在LocalMemory（UB）上，需要注意，baseM * baseN的大小不能超出UB的限制。

切分策略示意图如下，更多切分策略相关原理请参考数据分块（Tiling）


iM


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/a26613dde9c7d71a59a2b49018d0341a89b8b8e6553ecb03de6e1b9fcceb66d4.jpg)



K


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/50343cf87601fbb6326d14af9b2bac2a0e870d278757dc1510883e26892c70a6.jpg)


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/09e8d8edb80fa487822994f0973ce37463ec3965bb2d44303e01ac59c92f9785.jpg)


# 算子实现

在矩阵编程章节，我们得知Ascend C提供一组Matmul高阶API，封装了常用的切分和数据搬运、计算的算法逻辑，方便用户快速实现Matmul矩阵乘法的运算操作。融合算子中矩阵编程部分的实现与之类似，开发者在host侧通过调用API自动获取Tiling参数，该参数传递到kernel侧后，在初始化操作时传入，通过几个简单的API即可完成矩阵乘操作。再结合上文的融合算子的编程范式，融合算子实现的步骤如下。完整样例请参考MatmulLeakyRelu。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/2080738787344b07120507a1b5599d8dacfac114d85d12f4bab2d2d7cb0ee69b.jpg)


kernel侧实现的代码框架如下，在完成Matmul对象的初始化、左矩阵A、右矩阵B、Bias的设置后，通过单次Iterate叠加while循环的方式完成后续的Matmul计算、LeakyRelu计算、CopyOut流程。

```cpp
template<typename aType, typename bType, typename cType, typename biasType>
__aicore__ inline void MatmulLeakyKernel<aType, bType, cType, biasType>::Process(){
    uint32_t computeRound = 0;

    // 设置Matmul的输入（包括左矩阵、右矩阵、bias）
    matmulObj.SetTensorA(aGlobal);
    matmulObj.SetTensorB(bGlobal);
    matmulObj.SetBias(biasGlobal);

    // 调用matmul iterate获取一块[baseM, baseN]的计算结果
    while (matmulObj.template Iterate<true>() )
    {
    MatmulCompute();
    LeakyReluCompute();
    CopyOut(computeRound);
    computeRound++;
    }
    matmulObj.End();
}

// kernel入口函数，mix场景，AIC:AIV=1:2
__global__ __mix__(1, 2) void matmul_leakyrelu_custom(GM_ADDR a, GM_ADDR b, GM_ADDR bias, GM_ADDR c,
    __kfc_workspace__ GM_ADDR workspace, AscendC::tiling::TCubeTiling
tiling)
{
    AscendC::TPipe pipe;
    MatmulLeakyKernel<half, half, float, float> matmulLeakyKernel;
    matmulLeakyKernel.Init(a, b, bias, c, workspace, tiling, &pipe);
    // Matmul对象初始化
    REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), matmulLeakyKernel.matmulObj, &matmulLeakyKernel.tiling);
    matmulLeakyKernel.Process(&pipe);
}
```

Matmul计算、LeakyRelu计算、CopyOut的具体实现代码如下：

# 步骤1 Matmul计算：

1. 在Cube核上，进行Matmul内部的计算。

2. 将Matmul的计算结果搬运到Vector核。

```cpp
template<typename aType, typename bType, typename cType, typename biasType>
__aicore__ inline void MatmulLeakyKernel<aType, bType, cType, biasType>::Process(){
    uint32_t computeRound = 0;
    // ...
    // 调用matmul iterate获取一块[baseM, baseN]的计算结果
    while (matmulObj.template Iterate<true>());
    {
    MatmulCompute();
    // ...
    computeRound++;
    }
    matmulObj.End();
}

template<typename aType, typename bType, typename cType, typename biasType>
__aicore__ inline void MatmulLeakyKernel<aType, bType, cType, biasType>::MatmulCompute(){
    reluOutLocal = reluOutQueue_.AllocTensor<cType>();
    // 调用GetTensorC将Matmul的计算结果搬运到Vector核。
    matmulObj.template GetTensorC<true>(reluOutLocal, false, true);
}
```

步骤2 LeakyRelu计算。

```txt
// 调用LeakyRule接口进行计算
template<typename aType, typename bType, typename cType, typename biasType>
```

```cpp
__aicore__ inline void MatmulLeakyKernel<aType, bType, cType, biasType>::LeakyReluCompute(){ AscendC::LeakyRelu(reluOutLocal, reluOutLocal, (cType)alpha, tiling.baseM * tiling.baseN); reluOutQueue_.EnQue(reluOutLocal); } 
```


步骤3 CopyOut，将输出结果搬运到Global Memory。


```cpp
// 将结果搬出到GM
template<typename aType, typename bType, typename cType, typename biasType>
__aicore__ inline void MatmulLeakyKernel<aType, bType, cType, biasType>::CopyOut(uint32_t count){
reluOutQueue_.DeQue<cType>();
const uint32_t roundM = tiling.singleCoreM / tiling.baseM;
const uint32_t roundN = tiling.singleCoreN / tiling.baseN;
uint32_t startOffset = (count % roundM * tiling.baseM * tiling.N + count / roundM * tiling.baseN);
AscendC::DataCopyParams copyParam = {(uint16_t)tiling.baseM,
(uint16_t)(tiling.baseN * sizeof(cType) / DEFAULT_C0_SIZE), 0,
(uint16_t)((tiling.N - tiling.baseN) * sizeof(cType) / DEFAULT_C0_SIZE)};
AscendC::DataCopy(cGlobal[startOffset], reluOutLocal, copyParam);
reluOutQueue_.FreeTensor(reluOutLocal);
}
```

# ----结束

host侧实现GenerateTiling函数，在该函数中自动获取Tiling参数，关键步骤介绍如下：


步骤1 创建Tiling对象。


```rust
auto ascendcPlatform = platform_ascendc::PlatformAscendC(context->GetPlatformInfo());
matmul_tiling::MultiCoreMatmulTiling cubeTiling(ascendcPlatform); 
```

创建对象时需要传入硬件平台信息，硬件平台信息可以通过GetPlatformInfo获取。


步骤2 设置A、B、Bias的数据类型和格式。


```txt
设置示例如下，其中TPosition::LCM是Unified Buffer上的逻辑位置，等同于TPosition::VECCALC，关于TPosition的详细内容请参考TPosition。
```

```rust
cubeTiling.SetAType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
cubeTiling.SetBType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
cubeTiling.SetCType(matmul_tiling::TPosition::LCM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT);
cubeTiling.SetBiasType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT); 
```


步骤3 设置矩阵shape信息。


```javascript
cubeTiling.SetShape(M, N, K);
cubeTiling.SetOrgShape(M, N, K); 
```


步骤4 设置可用空间大小信息。


```txt
设置Matmul计算时可用的L1 Buffer/LOC Buffer/Unified Buffer空间大小，-1表示AI处理器对应Buffer的大小。
```

```javascript
cubeTiling.SetBufferSpace(-1, -1, -1); 
```


步骤5 按需设置其他参数，比如设置bias参与计算。


```javascript
cubeTiling.SetBias(true); 
```


步骤6 获取Tiling参数。


```txt
MatmulLeakyreluCustomTilingData tiling;
if (cubeTiling.GetTiling(tiling.cubeTilingData) == -1){
    return ge::GRAPH_FAILED;
} 
```


步骤7 Tiling参数的序列化保存等其他操作。


# ----结束

# 说明

● 特别的对于多核场景，需要通过SetDim接口设置Matmul计算所用的核数，MIX模式（包含矩阵计算和矢量计算）的设置规则如下：

分离模式：Matmul API都是从AIV侧发起的，调用Iterate计算时在AIV侧只会起到通知的作用，通知AIC去做矩阵计算，计算完成后AIC告知AIV计算完成。这个架构下，SetBlockDim设置为实际计算会用到的AI Core（AIC、AIV组合）的数量，SetDim设置为实际计算会用到的AIV的数量。例如，SetBlockDim时可以设置为20，启动20个AICore（AIC AIV的组合），SetDim设置为40，表示按照40个AIV进行切分。

耦合模式：SetBlockDim加载的核数就是Matmul API实际计算会用到的核数，SetDim和SetBlockDim设置的值是一样的。

● Matmul高阶API内部实现时需要使用系统workspace，开发者需要：

在host侧Tiling实现时，设置总的workspace的数值大小（包含用户workspace和系统workspace），workspace空间由框架来申请并管理。系统workspace的空间大小通过6.4.2.1.13 GetLibApiWorkSpaceSize获取。size_t userWorkspaceSize = 0;size_t systemWorkspaceSize = ascendcPlatform.GetLibApiWorkSpaceSize();size_t *currentWorkspace = context->GetWorkspaceSizes(1);currentWorkspace[0] = userWorkspaceSize + systemWorkspaceSize;

若算子工程不是自定义算子工程，也不是带有HAVE_WORKSPACE编译宏的Kernel直调算子工程，kernel侧需要在Matmul初始化前，通过 SetSysWorkSpace设置系统workspace。// 使用Matmul时必须设置workspace空间SetSysWorkspace(workspace);if (GetSysWorkSpacePtr() == nullptr) {return;}

● 上文介绍的实现方法，AIC侧和AIV侧的代码隔离和核间同步由框架来完成，开发者无需关心。除该方法外，开发者也可以选择底层编码的方式在分离模式下实现融合算子，这种方式将更加灵活。采用底层编码方式时，需要注意以下几点：

通过ASCEND_IS_AIV和ASCEND_IS_AIC实现AIV和AIC代码之间的隔离。

自行实现AIC和AIV核之间的同步：比如Matmul + LeakyRelu算子样例中，需要确保在AIC完成矩阵计算后，AIV再进行LeakyRelu的计算。

● 使用高阶API Matmul时需要设置ASCENDC_CUBE_ONLY，表示仅在AIC侧调用MatmulAPI。

● 使用设置Kernel类型接口设置Kernel类型为KERNEL_TYPE_MIX_xxx，同时启用AIV核和AIC核。

```cpp
#define ASCENDC_CUBE_ONLY // 指定Matmul运行在AIC上
KERNEL_TASK_TYPE_DEFAULT(KERN_TYPE_MIX_AIC_1_2); // 设置Kernel类型为
KERNEL_TYPE_MIX_xxx
if ASCEND_IS_AIC {
    ...
    // AIC核进行Matmul计算
    // AIC核完成计算后，通过AscendC::CrossCoreSetFlag<modeId, pipe>(flagId)发送同步flag
}
if ASCEND_IS_AIV {
    ...
    // AIV核通过AscendC::CrossCoreWaitFlag(flagId)接收同步flag
    // AIV核进行LeakyRelu计算
}
```

完整样例请参考BareMix样例。

# 3.3.5.2 通算融合

# 3.3.5.2.1 基础知识

# 说明

本节内容为通算融合算子的理论背景和开发指导，学习本节内容之前，请确保已经掌握矩阵编程和《HCCL集合通信库》中的相关知识。

通算融合算子一般支持如下产品型号：

Atlas 350 加速卡

Atlas A3 训练系列产品/Atlas A3 推理系列产品

Atlas A2 训练系列产品/Atlas A2 推理系列产品

# 通算融合算子

相比于一般的计算或搬运类算子，通算融合算子将原本串行的通信和计算操作融合在一起，通过在算子内部进行数据切分，实现了计算和通信任务在算子内的并行执行，从而提升算子性能。通算融合算子统称为MC²算子，即Matrix Computation &Communication。

如下图所示，串行的通信算子和计算算子的理想执行耗时为两个算子执行时间的加和，而在融合通信和计算任务得到的通算融合算子内，将需要通信和计算的数据进行切分，一次通信和计算的数据量减少，整个通信和计算任务分多次进行，使得计算与通信流水并行，理论执行耗时大大缩短，从而带来性能收益。


图3-66 通信计算融合前后的理论执行耗时对比示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/308a3e1867f961506c936b57b49136d6875c79c5dbfc20ebadc2b80591e8d581.jpg)


# 使用场景和优势

随着模型规模的增长，单设备上的训练和推理在计算能力、内存容量和能效等方面面临瓶颈，因此分布式并行计算成为必选技术路径。对于大模型分布式训练和推理过程中的通信和计算任务，可根据通信和计算的依赖关系分为两类：

# 弱依赖计算通信任务

通信或计算的结果不会立即被对方使用，两者虽有依赖，但在两者中间可以调度执行其他无依赖的计算或通信任务。如图2所示，通信1与计算1-2、计算4有依赖关系，通信1与计算1-1、计算2-1、计算2-2、计算3无依赖关系；通信2与计算2-2、计算4有依赖关系，通信2与计算2-1、计算3无依赖关系。因此，通信1和通信2都有较大的流水空间，可以被与它们无依赖的计算任务所掩盖。如图3所示，通信1和通信2均可以被无依赖的计算任务掩盖。在模型中，此类无依赖的通信和计算可以实现任务级并行，无需做算子融合。因此，弱依赖计算通信任务不适用通算融合场景。


图 3-67 弱依赖计算通信任务示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/bc0e7f98fbefe337d3cbffdc1c4f1326f0901947b6bf4e5c74274460d8591db1.jpg)



图3-68 弱依赖计算通信任务的调度模拟示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/49e57eff741f63ec5afa4f4589563d6704cd5ac5d6299ea84a3835ad711e2c3d.jpg)


# 强依赖计算通信任务

通信或计算的结果立即被对方使用，两者间存在紧密依赖关系。如下图所示，计算通信任务必须串行执行，在通信1和通信2执行过程中，硬件计算资源闲置，此类通信计算模式若在模型中大量出现，将导致算力利用率低，通信成为主要性能瓶颈。强依赖计算通信任务适合融合为通算融合算子，利用通算融合技术提升性能。


图3-69 强依赖计算通信任务示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/a33a3da4a34010dedf182c4a0911ec4127324fbb2b379bc4c9018bf2ce4fd3a8.jpg)



图3-70 强依赖计算通信任务的调度模拟示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/8ae9431adc8827f1761e73853708b539e77daa5f5e0ee53974151b650661ac01.jpg)


通算融合技术与网络模型结构密切相关，一般而言，符合上述强依赖计算通信任务都有可能通过通算融合算子实现性能提升。

# 3.3.5.2.2 算子实现

在通算融合类算子的实现中，通信操作使用Hccl高阶API，矩阵乘计算操作使用Matmul高阶API。关于更多集合通信的内容和相关概念请参考《HCCL集合通信库》。通算融合算子的开发过程与一般算子相同，但请注意，当前通算融合算子暂不支持Kernel直调和入图（GE图）开发，仅支持单算子API调用。

下文将以AllGatherMatmulCustom算子（简称AllGatherMatmul）的实现为例，从算子分析、数据流分析、创建算子工程、原型定义、Tiling实现、Kernel实现、编译与运行等方面介绍通算融合算子的设计和实现流程。本样例中算子的完整代码请参见AllGatherMatmul样例。该样例仅支持在Atlas A2 训练系列产品/Atlas A2 推理系列产品上运行。

# 算子分析

算子分析是指明确算子的数学表达式、输入、输出，核函数的名称等信息。

步骤1 明确算子的数学表达式及通信计算逻辑。

AllGatherMatmul算子实现了AllGather通信和Matmul矩阵乘法的融合。算子逻辑为：对输入的通信矩阵a做AllGather通信得到Matmul计算的左矩阵，即通信结果gather_out，将gather_out和右矩阵b做Matmul运算得到输出c。对应的数学表达式为：

```python
gather_out = AllGather(a)
c = gather_out * b 
```

步骤2 明确输入、输出和属性。

a、b为源操作数，a为通信的输入矩阵，形状为[M, K]；b为Matmul的右矩阵，形状为[K, N]。在样例中，M、K、N分别固定为512、5120、640。

gather_out为目的操作数，存放AllGather通信结果，形状为[M * rankDim, K]，其中，rankDim为通信域内的卡数，在样例中固定为8。

● c为目的操作数，存放Matmul运算结果，形状为[M * rankDim, N]。

算子输入输出的数据类型为float16，format为：ND。

group为算子属性，表示通信域名称，明确算子运行时所在的通信域。

步骤3 确定核函数名称和参数。

本样例中核函数命名为all_gather_matmul_custom。

根据对算子输入输出的分析，确定核函数的参数aGM，bGM，cGM，gatherOutGM；aGM，bGM为输入在Global Memory上的内存地址，cGM，gatherOutGM为输出在Global Memory上的内存地址。注意，核函数的参数和单算子API调用的输入输出在命名上存在区别，原因是核函数的参数是输入输出在Global Memory上的内存地址，而单算子API调用时输入输出的类型是aclTensor，两者并不完全一致。

步骤4 确定算子实现所需接口。

算子涉及AllGather通信，查看Ascend C API参考中的通信相关接口，需要使用Hccl高阶API来实现AllGather通信。

算子涉及Matmul左右矩阵在外部存储和内部存储间的数据搬运，查看Ascend CAPI参考中的数据搬运接口，需要使用DataCopy来实现数据搬运。

计算过程涉及矩阵计算操作，查看Ascend C API参考中的矩阵计算相关接口，需要使用Matmul高阶API实现矩阵乘计算。

----结束


表 3-14 AllGatherMatmulCustom 算子规格


<table><tr><td>算子类型(OpType)</td><td colspan="4">AllGatherMatmulCustom</td></tr><tr><td rowspan="5">算子输入输出</td><td>name</td><td>shape</td><td>data type</td><td>format</td></tr><tr><td>a</td><td>[512, 5120]</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>[5120, 640]</td><td>float16</td><td>ND</td></tr><tr><td>c</td><td>[4096, 640]</td><td>float16</td><td>ND</td></tr><tr><td>gather_out</td><td>[4096, 5120]</td><td>float16</td><td>ND</td></tr><tr><td>算子属性</td><td colspan="4">group (char*), Host侧标识通信域的字符串,表示通信域名称。</td></tr><tr><td>核函数名称</td><td colspan="4">all_gather_matmul_custom</td></tr></table>

# 数据流分析

AllGatherMatmul算子的数据在卡间进行AllGather通信，在卡内进行Matmul计算，通信和计算按照数据切分后的主块、尾块分多次进行，流水互相掩盖。分析过程中，假定通信矩阵的切分策略为按M轴进行切分，切分后主块数（tileCnt）为2，尾块数（tailCnt）为1，则可得到通信计算掩盖示意图如下。


图 3-71 AllGatherMatmul 通信计算掩盖示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/ac467289f7f91747ecf1e8fb45334505cc4768b3a6cd30a717ce0ab043315aa1.jpg)


AllGather的功能为将通信域内所有卡的输入按照卡id重新排序，然后拼接起来，再将结果发送到所有卡。因此，AllGather的结果中包含本卡数据，即本卡输入的通信矩阵a，算子无需等待这部分数据的通信完成，也无需对数据进行切分，可直接基于完整的通信矩阵a进行Matmul计算。AllGatherMatmul算子首先做本卡数据的Matmul计算，这样做的好处在于主块1的通信能与Matmul计算互相掩盖，同时，主块1、主块2、尾块1的计算无需再包括对本卡数据的Matmul计算，可以减少后续主尾块的计算量，增加通信计算的掩盖率，从而提高性能。注意，不是所有的通算融合算子都适合首先进行本卡数据的Matmul计算。因为AllGatherMatmul算子的通信在计算之前，所以先进行本卡数据的Matmul计算，可以实现本卡数据计算和第一次通信之间的互相掩盖。如果是计算在通信前的算子，如MatmulAllReduce，建议将本卡数据的计算放在最后，与最后一次通信互相掩盖，如下图所示。


图 3-72 MatmulAllReduce 通信计算掩盖示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/452ceb89e2f756ded1e631fcbe01fef6ed0b011d90544f11621394cab2d11e8a.jpg)


AllGatherMatmul算子逻辑分析：

步骤1 AI Core将要执行的通信信息写入Global Memory中的消息区，实现任务下发。消息区是特定地址的Global Memory，AI Core和AI CPU通过向其写入和轮询读取来实现消息在两者间的传递，这些操作统一封装于Hccl高阶API中。


图 3-73 通算融合算子通信流程示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/a7130788e023fc67d2b06d3202bfcb843b5ecea1ceb6514541eb7c18ca64ec13.jpg)


步骤2 AI CPU从消息区读取到所有通信任务信息，开始基于HCCS（华为缓存一致性系统，用于CPU/NPU之间的高速互联）或RoCE（承载在融合以太网上的RDMA技术，即跨越以太网的RDMA通信方式）等链路执行第一轮AllGather集合通信任务。与此同时，AICore开始对本卡数据进行Matmul计算。

下图是通信卡数为4时，第一轮通信与本卡计算的示意图。tile 1表示图示为第一轮通信和与其相互掩盖的矩阵乘计算的处理流程。图中切分后的小矩阵中形如X-Y的数字表示它所在的数据块对应于第X张卡第Y块数据。


图 3-74 AllGatherMatmul 第一轮通信与 rank0 上的本卡数据矩阵乘示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/6188ff16ed5d6bfec835e489431735efdbb19760f236aab71a2753257329dd5b.jpg)


步骤3 AI CPU完成第一轮通信任务后，向消息区写入第一轮通信任务已完成的消息，并开始执行第二轮通信任务。同时，AI Core在完成本卡数据的Matmul计算后，通过轮询消息区等到第一轮通信任务已完成的消息，开始进行第一轮通信结果即主块1的Matmul计算。

下图是通信卡数为4时，第二轮通信与rank0计算的示意图。tile 2表示图示为第二轮通信和与其相互掩盖的矩阵乘计算的处理流程。


图 3-75 AllGatherMatmul 第二轮通信与 rank0 上主块 1 的矩阵乘示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/b9fc519a618c3424cb3889298a9221265c991e313249f28cb647bdc45944de17.jpg)


步骤4 类似步骤3，逐步完成剩余所有数据块的通信和计算。

----结束

# 创建算子工程

创建通算融合算子的算子工程与一般算子相同，具体请参考2.10.2.2 创建算子工程章节。本样例基于如下原型定义json文件，使用自定义算子工程生成工具msOpGen，为AllGatherMatmul算子创建算子工程。

```json
[
    {
    "op": "AllGatherMatmulCustom",
    "input_desc": [
    {
    "name": "a",
    "param_type": "required",
    "format": [
    "ND"
    ],
    "type": [
    "float16"
    ]
    },
    {
    "name": "b",
    "param_type": "required",
    "format": [
    "ND"
    ],
    "type": [
    "float16"
    ]
    }
    ],
    "output_desc": [
    {
    "name": "c",
    "param_type": "required",
    "format": [
    "ND"
    ],
    "type": [
    "float16"
    ]
    ]
] 
```

```txt
},
{
    "name": "gather_out",
    "param_type": "required",
    "format": [
    "ND"
    ],
    "type": [
    "float16"
    ]
},
"attr": [
    {
    "name": "group",
    "type": "string",
    "default_value": "",
    "param_type": "required"
    }
]
} 
```

# 算子原型定义

相比于一般算子，通算融合算子在实现算子原型定义时，有如下约束：

必须定义一个表示算子通信域名称的属性。通信域是集合通信执行的上下文，管理对应的通信实体（例如一个NPU就是一个通信实体）和通信所需的资源。

必须通过原型注册中的MC2接口注册该算子为通算融合算子，并通过HcclGroup接口配置该算子的通信域名称。

AllGatherMatmul算子使用"group"属性表示该算子的通信域名称，其在算子原型中定义如下：

this->Attr("group").AttrType(REQUIRED).String(); // "group"为通算融合算子的属性，表示通信域名称，原型定义中的String类型对应单算子API中的char*类型

```txt
... this->MC2().HcclGroup("group"); // 将"group"属性配置为该算子的通信域名称
```

AllGatherMatmul算子的完整原型定义如下：

```cpp
namespace ops {
    class AllGatherMatmulCustom : public OpDef {
    public:
    explicit AllGatherMatmulCustom(const char *name) : OpDef(name) {
    this->Input("a")
    .ParamType(REQUIRED)
    .DataType({ge::DT_FLOAT16})
    .Format({ge::FORMAT_ND})
    .UnknownShapeFormat({ge::FORMAT_ND});
    this->Input("b")
    .ParamType(REQUIRED)
    .DataType({ge::DT_FLOAT16})
    .Format({ge::FORMAT_ND})
    .UnknownShapeFormat({ge::FORMAT_ND})
    .IgnoreContiguous();
    this->Output("c")
    .ParamType(REQUIRED)
    .DataType({ge::DT_FLOAT16})
    .Format({ge::FORMAT_ND})
    .UnknownShapeFormat({ge::FORMAT_ND});
    this->Output("gather_out")
    .ParamType(REQUIRED)
    .DataType({ge::DT_FLOAT16})
    .Format({ge::FORMAT_ND}) 
```

```cpp
.UnknownShapeFormat({ge::FORMAT_ND});
this->Attr("group").AttrType(REQUIRED).String();
this->AlCore().SetTiling(AllGatherMatmulCustomTilingFunc); // 注册
AllGatherMatmulCustomTilingFunc为Tiling入口函数
this->AlCore().AddConfig("ascendxxx"); // ascendxxx请修改为对应的AI处理器型号。
this->MC2().HcclGroup("group");
}
};OP_ADD(AllGatherMatmulCustom);
}
```

# Tiling 实现

通算融合算子Tiling策略的设计主要包括通信切分策略、Matmul多核切分和核内切分策略。

通信切分策略：每轮通信数据块的大小，对通算融合算子的性能有较大影响。样例中按照主块M轴长度448对通信矩阵A的M轴进行切分。具体场景中如何确定切分策略请参考MC²算子性能调优案例。

Matmul多核切分和核内切分:

多核切分: 根据当前核数，对输入shape的M、K、N进行多核切分，得到单核内shape大小singleCoreM、singleCoreK、singleCoreN。

核内切分: 根据Local Memory的大小约束，对单核内的shape大小进一步切分，得到A、B、C矩阵参与一次矩阵乘指令的shape大小baseM、baseN、baseK。

如上所述，通信矩阵被切分为主块、尾块，主块、尾块的通信结果以及本卡数据需要分别进行Matmul计算。如下图，主块、尾块和本卡数据在M轴的长度分别为tileM、tailM和rankM，即Matmul计算时的左矩阵存在三种不同的形状，因此需要分别以通信矩阵主块、尾块和本卡数据块的大小为矩阵乘原始的输入形状，调用Matmul高阶API提供的Tiling接口，得到对应这三种形状的多核切分和核内切分策略。这里，singleCoreM、baseM等概念和相关原理的介绍请参考3.3.3.1 基础知识。


图 3-76 AllGatherMatmul 算子在 rank0 的矩阵乘示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/9eb0fd1f3b832a3b78da8721039ee58573be063209cc02b257973632b4678694.jpg)


下面给出Tiling实现的关键步骤：

步骤1 定义AllGatherMatmul算子的Tiling结构体。

通信和Matmul融合得到的通算融合算子的Tiling结构体一般包括如下三个部分：

Hccl高阶API的Tiling结构体。定义Mc2InitTiling和Mc2CcTiling参数。

Mc2InitTiling参数用于初始化通信任务配置，必须定义为算子Tiling结构体的第一个参数。Mc2CcTiling为具体每个通信任务的参数配置，由于AllGatherMatmul算子中只有AllGather一个通信任务，因此仅需定义一个Mc2CcTiling参数。

Matmul高阶API的Tiling结构体TCubeTiling。一般而言，主块、尾块和本卡数据的shape是不同的，由于TCubeTiling只能存储对一个输入形状进行Tiling计算得到的结果，因此需要分别定义主块、尾块和本卡数据块的Tiling结构体，来存放它们的多核切分和核内切分策略。

● AllGatherMatmul算子额外需要的自定义结构体AllGatherMatmulTiling。


AllGatherMatmul算子的完整Tiling结构体定义如下：


```cpp
struct AllGatherMatmulTiling {
    uint32_t rankM; // A矩阵M轴的长度
    uint32_t rankN; // B矩阵N轴的长度
    uint32_t rankK; // A、B矩阵K轴的长度
    uint32_t tileNum; // 主块个数
    uint32_t tailM; // 尾块的M轴长度
    uint32_t tailNum; // 尾块个数（0或1）
};

class AllGatherMatmulCustomTilingData {
public:
    Mc2InitTiling mc2InitTiling;
    Mc2CcTiling mc2CcTiling;
    TCubeTiling localTiling;
    TCubeTiling tileTiling;
    TCubeTiling tailTiling;
    AllGatherMatmulTiling cfg;
};
```

步骤2 获取AllGatherMatmul算子的Tiling结构体对象指针。

```txt
AllGatherMatmulCustomTilingData *tiling = context->GetTilingData<AllGatherMatmulCustomTilingData>();
context为TilingContext的对象指针，该指针由框架自动从注册的Tiling入口函数
AllGatherMatmulCustomTilingFunc传入，用于保存算子Tiling计算的上下文。在
AllGatherMatmul算子的Tiling实现中，通过该上下文context获取计算Tiling所需要的输入输出shape、输入属性等参数，然后将Tiling结果（例如TilingKey、TilingData）保存至上下文中，供后续算子执行时使用。
```

步骤3 设置算子自定义的Tiling结构体参数。

```c
tiling->cfg.tileNum = rankM / TILE_M; // TILE_M在样例中为常量448，表示通信数据块切分后的主块在M轴的长度
tiling->cfg.tailM = rankM % TILE_M;
tiling->cfg.tailNum = (rankM % TILE_M == 0) ? 0 : 1;
tiling->cfg.rankM = rankM;
tiling->cfg.rankN = rankN;
tiling->cfg.rankK = rankK;
```

步骤4 设置Matmul高阶API Tiling结构体。

通过matmul_tiling::MultiCoreMatmulTiling获取TCubeTiling结构体，首先创建多核Tiling对象mmTiling，然后设置A、B、C的参数类型信息，M、N、K形状信息等，最后调用GetTiling接口，获取Tiling信息，具体方法可详见Matmul Tiling类。

AllGatherMatmul算子中将上述逻辑封装为matmulTilingFunc函数，再分别根据主块、尾块和本卡数据的形状大小，调用matmulTilingFunc函数，得到对应的TCubeTiling参数。

```cpp
// 封装设置TCubeTiling结构体的函数为matmulTilingFunc
auto matmulTilingFunc = [&](int64_t m, int64_t n, int64_t k, TCubeTiling &cubeTiling) -> bool {
    matmul_tiling::MultiCoreMatmulTiling mmTiling;
    mmTiling.SetAType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
    mmTiling.SetBType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
    mmTiling.SetCType(matmul_tiling::TPosition::GM, matmul_tiling::CubeFormat::ND, matmul_tiling::DataType::DT_FLOAT16);
    mmTiling.SetBias(false);
    mmTiling.SetDim(aicCoreNum);
    mmTiling.SetShape(m, n, k);
    mmTiling.SetOrgShape(m, n, k);
    mmTiling.SetBufferSpace(L1_BUFFER_SIZE);
    if (mmTiling.GetTiling(cubeTiling) != 0) {
    return false;
    }
    return true;
};

// 设置本卡数据的Matmul TCubeTiling结构体
if (!matmulTilingFunc(rankM, rankN, rankK, tiling->localTiling)) {
    ERROR_LOG("Get local matmul tiling failed");
    return ge::GRAPH_FAILED;
}

// 设置主块的Matmul TCubeTiling结构体
if (!matmulTilingFunc(TILE_M, rankN, rankK, tiling->tileTiling)) {
    ERROR_LOG("Get tile matmul tiling failed");
    return ge::GRAPH_FAILED;
}

// 设置尾块的Matmul TCubeTiling结构体
if (!matmulTilingFunc(rankM % TILE_M, rankN, rankK, tiling->tailTiling)) {
    ERROR_LOG("Get tail matmul tiling failed");
    return ge::GRAPH_FAILED;
}
```

步骤5 设置Hccl高阶API Tiling结构体。

根据通信任务类型、算法配置等，创建一个Mc2CcTilingConfig类对象，通过向GetTiling方法传入算子Tiling结构体中mc2InitTiling和mc2CcTiling成员的引用，完成需要传递给Kernel侧的Mc2InitTiling参数和Mc2CcTiling参数的获取。Hccl高阶APITiling结构体的具体使用方法详见Hccl Tiling使用说明。

```cpp
uint32_t opType = HCCL_CMD_ALLGATHER; // 设置通信任务类型
std::string algConfig = "AllGather=level0:doublering"; // 设置通信算法，该参数为预留字段，配置后不生效
uint32_t reduceType = HCCL_REDUCE_SUM; // 设置Reduce操作类型，仅对有归约操作的通信任务有效，作为
AllGather通信，可以直接配置默认值HCCL_REDUCE_SUM
AscendC::Mc2CcTilingConfig mc2CcTilingConfig(group, opType, algConfig, reduceType);
mc2CcTilingConfig.GetTiling(tiling->mc2InitTiling);
mc2CcTilingConfig.SetSkipLocalRankCopy(0); // 输出gatherOut需带有本卡的A矩阵，因此设置为0
mc2CcTilingConfig.GetTiling(tiling->mc2CcTiling);
```

# ----结束

# Kernel 实现

在AllGatherMatmul算子的Kernel实现中，需要对本卡数据、通信主块、通信尾块共三种形状的左矩阵进行Matmul运算，为避免重复代码，有必要抽象出一个通用的适用于不同输入形状的Matmul计算函数。设计该Matmul计算函数前，需要考虑Matmul计算需要的基本信息，罗列如下：

输入A、B矩阵和输出C矩阵的地址。

TCubeTiling结构体：包含矩阵A、B、C的形状、数据类型等信息，以及A、B矩阵进行Matmul运算时在核间和核内的切分策略。

除了上述Matmul运算所需的信息外，为了快速实现Matmul矩阵乘法，可以使用Matmul高阶API中的Matmul对象来执行计算。如果Matmul对象在Matmul计算函数中定义，每次调用该函数时都会实例化Matmul对象并释放资源，这将导致较大的运行时开销。因此，将该对象也作为Matmul计算函数的参数，以实现对象的复用。

综上所述，在Kernel实现中定义的适用于不同输入形状的Matmul计算函数如下。其中Matmul计算函数函数名定义为MatmulKernel，入参aGM、bGM、cGM表示需要运算的原始输入输出矩阵的地址，入参tiling表示TCubeTiling结构体，入参mm对应Matmul高阶API的实现类。MATMUL_TYPE是特化了MatmulType模板的类型别名。

```txt
using MATMUL_TYPE = MatmulType<AscendC::TPosition::GM, CubeFormat::ND, half>;
__aicore__ inline void MatmulKernel(GM_ADDR aGM, GM_ADDR bGM, GM_ADDR cGM, TCubeTiling &tiling, Matmul<MATMUL_TYPE, MATMUL_TYPE, MATMUL_TYPE> &mm) 
```

MatmulKernel函数的实现步骤如下。

步骤1 TCubeTiling结构体存储了Matmul计算所需的核数，在无需计算的核上直接返回，结束计算。

```javascript
if (GetBlockIdx() >= tiling.usedCoreNum) {
    return;
} 
```

步骤2 Matmul高阶API要求使用GlobalTensor作为输入输出矩阵，因此，根据函数输入的A、B、C矩阵在Global Memory的地址，分别定义aGlobal、bGlobal、cGlobal三个GlobalTensor。

```c
GlobalTensor<half> aGlobal, bGlobal, cGlobal;
aGlobal.SetGlobalBuffer(reinterpret_cast<__gm__ half *>(aGM), tiling.M * tiling.Ka);
bGlobal.SetGlobalBuffer(reinterpret_cast<__gm__ half *>(bGM), tiling.Ka * tiling.N);
cGlobal.SetGlobalBuffer(reinterpret_cast<__gm__ half *>(cGM), tiling.M * tiling.N); 
```

步骤3 为了实现多核并行，提升计算效率，将矩阵数据进行切分，切分后的数据分配到不同的核上进行处理。这里采用了不切分K轴、仅切分M、N轴的切分策略，示意图如下。在这种场景下，每个核需要计算待处理的矩阵数据相对于原始矩阵的偏移量，并将偏移后的矩阵作为传入A、B、C矩阵时的入参。同时，为支持分核后的尾块数据的处理，每个核需要计算实际处理的singleCoreM、singleCoreN大小，并在下一步中通过调用Matmul高阶API进行设置。


图 3-77 Matmul 计算分核示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/7afe93bdc3bb981f8b709f9f890eb4ccd874a53b2e4ab87fd16cf2a662fb826a.jpg)


```rust
int mSingleBlocks = (tiling.M + tiling.singleCoreM - 1) / tiling.singleCoreM;
int mCoreIndex = GetBlockIdx() % mSingleBlocks;
int nCoreIndex = GetBlockIdx() / mSingleBlocks;
// 计算当前核需要处理的矩阵数据相对于原始矩阵的偏移
int offsetA = mCoreIndex * tiling.Ka * tiling.singleCoreM;
int offsetB = nCoreIndex * tiling.singleCoreN;
int offsetC = mCoreIndex * tiling.N * tiling.singleCoreM + nCoreIndex * tiling.singleCoreN;
// 计算当前核的singleCoreM/singleCoreN，作为后续SetTail接口的入参
int tailM = Std::min(tiling.M - mCoreIndex * tiling.singleCoreM, tiling.singleCoreM);
int tailN = Std::min(tiling.N - nCoreIndex * tiling.singleCoreN, tiling.singleCoreN);
```

步骤4 调用Matmul高阶API设置Matmul计算的原始完整的形状、当前核处理的输入输出矩阵的地址和计算的实际singleCoreM、singleCoreN的大小，并完成矩阵乘运算。

```javascript
mm.SetOrgShape(tiling.M, tiling.N, tiling.Ka, tiling.Kb);
mm.SetTensorA(aGlobal[offsetA]);
mm.SetTensorB(bGlobal[offsetB]);
mm.SetTail(tailM, tailN);
mm.IterateAll(cGlobal[offsetC]); 
```

----结束

AllGatherMatmul算子的核函数定义如下，aGM、bGM、cGM、gatherOutGM参数含义如算子分析中所述，workspaceGM和tilingGM分别表示wrokspace空间和tiling数据在Global Memory的地址。

```c
extern "C" __global__ __aicore__ void all_gather_matmul_custom(GM_ADDR aGM, GM_ADDR bGM, GM_ADDR cGM, GM_ADDR gatherOutGM, GM_ADDR workspaceGM, GM_ADDR tilingGM) 
```

下面介绍AllGatherMatmul算子主流程实现的具体步骤。

步骤1 Matmul计算依赖AIC核，因此控制算子逻辑仅运行于AIC中。通过ASCEND_IS_AIV宏，判断如果当前核为AIV核，直接返回，结束当前核的运行。

```txt
if ASCEND_IS_AIV {
    return;
} 
```

步骤2 注册算子Tiling结构体、获取Tiling，并初始化TPipe。

```txt
REGISTER_TILING_DEFAULT(AllGatherMatmulCustomTilingData);
GET_TILING_DATA(tilingData, tilingGM);
TPipe pipe; 
```

步骤3 定义并赋值后续计算所需变量。

```javascript
auto &&localTiling = tilingData.localTiling;
auto &&tileTiling = tilingData.tileTiling;
auto &&tailTiling = tilingData.tailTiling;
const auto tileNum = tilingData.cfg.tileNum;    // 主块数量
const auto tailNum = tilingData.cfg.tailNum;    // 尾块数量
const auto aTileEleCnt = tileTiling.M * tileTiling.Ka;    // 通信矩阵主块元素数
const auto aTileSize = tileTiling.M * tileTiling.Ka * sizeof(half);    // 通信矩阵主块字节数
const auto cTileSize = tileTiling.M * tileTiling.N * sizeof(half);    // 通信矩阵主块对应在输出矩阵的字节数
const auto aTailEleCnt = tailTiling.M * tailTiling.Ka;    // 通信矩阵尾块元素数
const auto aRankEleCnt = localTiling.M * localTiling.Ka;    // 通信矩阵元素数
const auto aRankSize = localTiling.M * localTiling.Ka * sizeof(half); // 通信矩阵字节数
const auto cRankSize = localTiling.M * localTiling.N * sizeof(half); // 通信矩阵对应在输出矩阵的字节数
```

步骤4 初始化hccl对象并下发AllGather通信任务。

```txt
Hccl hccl;
GM_ADDR contextGM = GetHcclContext<HCCL_GROUP_ID_0>();
hccl.InitV2(contextGM, &tilingData);
hccl.SetCcTilingV2(offsetof(AllGatherMatmulCustomTilingData, mc2CcTiling));
auto handleId = hccl.AllGather<true>(aGM, gatherOutGM, aTileEleCnt,
HcclDataType::HCCL_DATA_TYPE_FP16, aRankEleCnt, tileNum);
auto tailHandleId = hccl.AllGather<true>(aGM + tileNum * aTileSize, gatherOutGM + tileNum * aTileSize, aTailEleCnt,
HcclDataType::HCCL_DATA_TYPE_FP16, aRankEleCnt, tailNum); 
```

步骤5 初始化Matmul对象，对本卡数据进行Matmul计算。

```txt
Matmul<MATMUL_TYPE, MATMUL_TYPE, MATMUL_TYPE> mm;
REGIST_MATMUL_OBJ(GetTPipePtr(), GetSysWorkSpacePtr(), mm);
mm.Init(&localTiling);
MatmulKernel(aGM, bGM, cGM + hccl.GetRankId() * cRankSize, localTiling, mm); 
```

步骤6 逐轮等待主块的通信完成，并对其进行Matmul计算。

```javascript
auto aAddr = gatherOutGM;
auto cAddr = cGM;
mm.Init(&tileTiling);
for (uint32_t i = 0; i < tileNum; i++) {
    hccl.Wait(handleId);
    for (uint32_t rankId = 0; rankId < hccl.GetRankDim(); rankId++) {
    if (rankId == hccl.GetRankId())
    continue;
    MatmulKernel(aAddr + rankId * aRankSize, bGM, cAddr + rankId * cRankSize, tileTiling, mm);
    }
    aAddr += aTileSize;
    cAddr += cTileSize;
} 
```

步骤7 等待尾块的通信完成，并对其进行Matmul计算。

```txt
aAddr = gatherOutGM + tileNum * aTileSize;
cAddr = cGM + tileNum * cTileSize;
if (tailNum > 0) {
    mm.Init(&tailTiling);
    hccl.Wait(tailHandleId);
    for (uint32_t rankId = 0; rankId < hccl.GetRankDim(); rankId++) {
    if (rankId == hccl.GetRankId())
    continue;
    MatmulKernel(aAddr + rankId * aRankSize, bGM, cAddr + rankId * cRankSize, tailTiling, mm);
    }
} 
```

步骤8 释放资源。

```javascript
mm.End();
hccl.Finalize(); 
```

# ----结束


整合前述代码 ，完整Kernel代码如下。


```cpp
c
#define ASCENDC_CUBE_ONLY
#include "kernel_operator.h"
#include "lib/matmul_intf.h"
#include "all_gather_matmul_custom_tiling.h"
using namespace AscendC;
using MATMUL_TYPE = MatmulType<AscendC::TPosition::GM, CubeFormat::ND, half>;
__aicore__ inline void MatmulKernel(GM_ADDR aGM, GM_ADDR bGM, GM_ADDR cGM, TCubeTiling &tiling, Matmul<MATMUL_TYPE, MATMUL_TYPE, MATMUL_TYPE> &mm)
{
    if (GetBlockIdx() >= tiling.usedCoreNum) {
    return;
    }

    GlobalTensor<half> aGlobal, bGlobal, cGlobal;
    aGlobal.SetGlobalBuffer(reinterpret_cast<_gm_ half *>(aGM), tiling.M * tiling.Ka);
    bGlobal.SetGlobalBuffer(reinterpret_cast<_gm_ half *>(bGM), tiling.Ka * tiling.N);
    cGlobal.SetGlobalBuffer(reinterpret_cast<_gm_ half *>(cGM), tiling.M * tiling.N);

    int mSingleBlocks = (tiling.M + tiling.singleCoreM - 1) / tiling.singleCoreM;
    int mCoreIndx = GetBlockIdx() % mSingleBlocks;
    int nCoreIndx = GetBlockIdx() / mSingleBlocks;
    int offsetA = mCoreIndx * tiling.Ka * tiling.singleCoreM;
    int offsetB = nCoreIndx * tiling.singleCoreN;
    int offsetC = mCoreIndx * tiling.N * tiling.singleCoreM + nCoreIndx * tiling.singleCoreN;
    int tailM = Std::min(tiling.M - mCoreIndx * tiling.singleCoreM, tiling.singleCoreM);
    int tailN = Std::min(tiling.N - nCoreIndx * tiling.singleCoreN, tiling.singleCoreN);

    mm.SetOrgShape(tiling.M, tiling.N, tiling.Ka, tiling.Kb);
    mm.SetTensorA(aGlobal[offsetA]);
    mm.SetTensorB(bGlobal[offsetB]);
    mm.SetTail(tailM, tailN);
    mm.IterateAll(cGlobal[offsetC]);

    extern "C" __global__ __aicore__ void all_gather_matmul_custom(GM_ADDR aGM, GM_ADDR bGM, GM_ADDR cGM,
    GM_ADDR gatherOutGM, GM_ADDR workspaceGM, GM_ADDR tilingGM)
    {
    if ASCEND_IS_AIV {
    return;
    }
    REGISTER_TILING_DEFAULT(AllGatherMatmulCustomTilingData);
    GET_TILING_DATA(tilingData, tilingGM);
    TPipe pipe;

    auto &&localTiling = tilingData.localTiling;
    auto &&tileTiling = tilingData.tileTiling;
    auto &&tailTiling = tilingData.tailTiling;
    const auto tileNum = tilingData.cfg.tileNum; // 主块数量
    const auto tailNum = tilingData.cfg.tailNum; // 尾块数量
    const auto aTileEleCnt = tileTiling.M * tileTiling.Ka; // 通信矩阵主块元素数
    const auto aTileSize = tileTiling.M * tileTiling.Ka * sizeof(half); // 通信矩阵主块字节数
    const auto cTileSize = tileTiling.M * tileTiling.N * sizeof(half); // 输出矩阵主块字节数
    const auto aTailEleCnt = tailTiling.M * tailTiling.Ka; // 通信矩阵尾块元素数
    const auto aRankEleCnt = localTiling.M * localTiling.Ka; // 单卡通信矩阵元素数
    const auto aRankSize = localTiling.M * localTiling.Ka * sizeof(half); // 单卡通信矩阵字节数
    const auto cRankSize = localTiling.M * localTiling.N * sizeof(half); // 单卡输出矩阵字节数

Hccl hccl;
GM_ADDR contextGM = GetHcclContext<HCCL_GROUP_ID_0>();
hccl.InitV2(contextGM, &tilingData);
hccl.SetCcTilingV2(offsetof(AllGatherMatmulCustomTilingData, mc2CcTiling));
```

```cpp
auto handleId = hccl.AllGather<true>(aGM, gatherOutGM, aTileEleCnt,
HcclDataType::HCCL_DATA_TYPE_FP16, aRankEleCnt, tileNum);
auto tailHandleId = hccl.AllGather<true>(aGM + tileNum * aTileSize, gatherOutGM + tileNum * aTileSize,
aTailEleCnt,
HcclDataType::HCCL_DATA_TYPE_FP16, aRankEleCnt, tailNum);

Matmul<MATMUL_TYPE, MATMUL_TYPE, MATMUL_TYPE> mm;
REGIST_MATMUL_OBJ(GetTPipePtr(), GetSysWorkSpacePtr(), mm);
mm.Init(&localTiling);
MatmulKernel(aGM, bGM, cGM + hccl.GetRankId() * cRankSize, localTiling, mm);

auto aAddr = gatherOutGM;
auto cAddr = cGM;
mm.Init(&tileTiling);
for (uint32_t i = 0; i < tileNum; i++) {
    hccl.Wait(handleId);
    for (uint32_t rankId = 0; rankId < hccl.GetRankDim(); rankId++) {
    if (rankId == hccl.GetRankId())
    continue;
    MatmulKernel(aAddr + rankId * aRankSize, bGM, cAddr + rankId * cRankSize, tileTiling, mm);
    }
    aAddr += aTileSize;
    cAddr += cTileSize;
}

aAddr = gatherOutGM + tileNum * aTileSize;
cAddr = cGM + tileNum * cTileSize;
if (tailNum > 0) {
    mm.Init(&tailTiling);
    hccl.Wait(tailHandleId);
    for (uint32_t rankId = 0; rankId < hccl.GetRankDim(); rankId++) {
    if (rankId == hccl.GetRankId())
    continue;
    MatmulKernel(aAddr + rankId * aRankSize, bGM, cAddr + rankId * cRankSize, tailTiling, mm);
    }
}

mm.End();
hccl.Finalize();
} 
```

# 编译和运行

下面从编译、安装、运行三个步骤对AllGatherMatmul样例作简要介绍。

# 步骤1 编译

参考AllGatherMatmul样例中生成自定义算子工程、编译算子的命令，运行install.sh脚本完成编译。

样例目录结构如下，AllGatherMatmulCustom目录为必要的算子实现，install.sh脚本使用msOpGen在21_all_gather_matmul_custom目录下创建一个CustomOp目录，并将算子实现文件复制到对应目录下，再调用msOpGen生成的编译入口脚本build.sh编译算子。

```javascript
21_all_gather_matmul_custom
    —AclNNInvocation    // 通过aclnn调用的方式调用AllGatherMatmulCustom算子
    —AllGatherMatmulCustom    // AllGatherMatmulCustom算子工程
    —all_gather_matmul_custom.json    // AllGatherMatmulCustom算子的原型定义json文件
    —all_gather_matmul_demo_def.h    // AllGatherMatmulCustom算子参数配置
    —install.sh    // 脚本，调用msOpGen生成自定义算子工程，并编译
```

msOpGen生成的CustomOp目录结构如下。

```txt
CustomOp // msOpGen生成的AllGatherMatmul自定义算子工程
cmake
op_host // host侧实现文件
```

```txt
op_kernel // kernel侧实现文件  
scripts // 自定义算子工程打包相关脚本所在目录  
build.sh // 编译入口脚本  
CMakeLists.txt // 算子工程的CMakeLists.txt  
CMakePresets.json // 编译配置项
```

# 步骤2 安装

部署自定义算子包前，请确保环境中存在自定义算子包默认部署路径的环境变量ASCEND_OPP_PATH。

```shell
# 查看环境变量输出
echo $ASCEND_OPP_PATH
```

```shell
# 若无输出，则需设置环境变量，ASCEND_INSTALL_PATH为CANN软件包安装路径
source [ASCEND_INSTALL_PATH]/set_env.bash
# 例如 source /usr/local/Ascend/cann/set_env.sh
```

然后执行如下命令，切换目录为编译出的自定义算子安装包所在目录，并安装自定义算子包。

```txt
cd CustomOp/build_out
./custom_opp_<target os>_<target architecture>.run 
```

命令执行成功后，自定义算子包中的相关文件将部署至环境变量ASCEND_OPP_PATH指向的的vendors/customize目录中。

# 步骤3 运行

切换目录为AclNNInvocation目录，执行run.sh脚本运行单算子样例。

```shell
cd .././AclNNInvocation
bash run.sh 
```

样例中的AclNNInvocation目录提供了完整的单算子API调用的示例代码。完成前两个步骤自定义算子的编译部署后，会自动生成单算子API，该API可以直接在应用程序中调用。算子API的形式一般为“两段式接口”，形如：

```txt
// 获取算子使用的workspace空间大小
aclnnStatus aclnnAllGatherMatmulCustomGetWorkspaceSize(
    const aclTensor *a,
    const aclTensor *b,
    char *group,
    const aclTensor *cOut,
    const aclTensor *gatherOutOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);
// 执行算子
aclnnStatus aclnnAllGatherMatmulCustom(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    const aclrtStream stream);
```

其中aclnnAllGatherMatmulCustomGetWorkspaceSize为第一段接口，主要用于计算本次API调用计算过程中需要的workspace内存大小。按照该workspaceSize大小申请Device侧内存，然后调用第二段接口aclnnAllGatherMatmulCustom执行计算。详细内容请参考单算子API调用章节。

在通算融合场景，单算子API调用的程序中需要调用《HCCL集合通信库》>接口参考中的接口创建通信域，并在多线程上执行AllGatherMatmul算子。以下给出main函数和线程调用函数中关键步骤的代码示例，仅供参考。

```txt
int main(int argc, char **argv)
{
    // 1.AscendCL初始化
    if (aclInit(NULL) != ACL_SUCCESS) {
    ERROR_LOG("aclInit failed");
    }
}
```

```c
return FAILED;
}
// 2.通信域创建
HcclComm comms[RANK_DIM]; // RANK_DIM为卡数，示例中为8
int32_t devices[RANK_DIM];
for (int32_t i = 0; i < RANK_DIM; i++) {
    devices[i] = i;
}
if (HcclCommInitAll(RANK_DIM, devices, comms) != HCCL_SUCCESS) {
    ERROR_LOG("Hccl comm init failed.");
    (void)aclFinalize();
    return FAILED;
}
// 3.创建多线程以在通信域的所有卡上都调用AllGatherMatmul算子
std::vector<std::unique_ptr<std::thread>> threads(RANK_DIM);
for (uint32_t rankId = 0; rankId < RANK_DIM; rankId++) {
    threads[rankId].reset(new(std::nothrow) std::thread(&RunOp, rankId, std::ref(comms[rankId]));
}
for (uint32_t rankId = 0; rankId < RANK_DIM; rankId++) {
    threads[rankId]->join();
}
// 4.AscendCL去初始化
(void)aclFinalize();
return SUCCESS;
}
```

在main函数中，通过HcclCommInitAll接口在当前进程统一创建了RANK_DIM张卡的通信域，一张卡对应后续创建的一个线程。每个线程都调用RunOp函数，该函数负责卡上运行时资源申请和单算子API的二阶段接口调用。RunOp函数的代码示例如下。

```txt
bool RunOp(uint32_t rankId, HcclComm &comm)
{
    // 1.申请当前线程的context、stream等资源
    aclrtContext context;
    aclrtCreateContext(&context, rankId);
    aclrtStream stream;
    aclrtCreateStream(&stream);
    aclrtSetCurrentContext(context);

    // 2.获取当前线程对应卡的通信域名称
    char group[128] = {0};
    HcclGetCommName(comm, group);

    // 3.申请device侧内存存放算子的输入输出
    // ....

    // 4.计算workspace大小并申请内存
    size_t workspaceSize = 0;
    aclOpExecutor *handle = nullptr;
    auto ret = aclnnAllGatherMatmulCustomGetWorkspaceSize(a, b, group, c, gatherOut, &workspaceSize, &handle);
    void *workspace = nullptr;
    if (workspaceSize != 0) {
    aclrtMalloc(&workspace, workspaceSize);
    }

    // 5.执行算子
    ret = aclnnAllGatherMatmulCustom(workspace, workspaceSize, handle, stream);

    // 6.同步等待
    ret = aclrtSynchronizeStreamWithTimeout(stream, 10000); // 10000ms 流同步超时

    // 7.释放算子输入、输出和workspace等device侧内存
    // ....

    // 8.释放通信域、context、stream等资源
    (void)HcclCommDestroy(comm);
    (void)aclrtDestroyStream(stream);
    (void)aclrtDestroyContext(context);
    (void)aclrtResetDevice(rankId);
```

```lua
return true;
} 
```

----结束

# 3.3.5.2.3 特性场景

# 重执行

为避免执行通信任务的环境中硬件闪断导致发生通信中断，通算融合算子可通过配置编译宏与环境变量，开启重执行能力。通算融合算子开启重执行后，AI CPU在检测到环境异常时，通过下图示意的机制，通知AI Core重新下发通信任务，避免由于硬件闪断造成的通信中断，提升通信稳定性。

当前，该能力的支持情况如下：

Atlas 350 加速卡不支持通算融合算子的重执行。

Atlas A2 训练系列产品/Atlas A2 推理系列产品不支持通算融合算子的重执行。

Atlas A3 训练系列产品/Atlas A3 推理系列产品支持通算融合算子的重执行。


图3-78 通信任务重执行机制


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/ab1b115ae95874a178730eb4dafd8984a3c769a6e1f32640e02d3967fc7c6b27.jpg)


# 开启重执行的条件如下：

通算融合算子的输出内存地址和输入内存地址不相同。

通算融合算子仅存在Server间通信（Server为计算节点，通常是8卡或16卡的昇腾NPU设备组成的服务器形态的统称）。

算子编译时，配置编译宏AICORE_EXCEPTION_RESTART，如下所示。具体的编译宏配置阶段和方式请参考支持自定义编译选项。

```cmake
add_ops_compile_options(ALL OPTIONS -DAICORE_EXCEPTION_RESTART) 
```

配置HCCL重执行环境变量HCCL_OP_RETRY_ENABLE，开启重执行的检测和上报能力，该环境变量的说明请参考《环境变量参考》“集合通信 > 可靠性相关 >HCCL_OP_RETRY_ENABLE”。请在算子执行前设置该环境变量，具体配置如下：# server间L1需配置为1, 不支持跨超节点，L2配置为0。export HCCL_OP_RETRY_ENABLE="L1:1, L2:0"

注意，开启重执行后，若AI Core第一次下发通信任务后通信中断，默认只重执行一次。若需修改重执行次数或重传间隔时间，请参考《环境变量参考》“集合通信 > 可靠性相关 > HCCL_OP_RETRY_PARAMS” 。

# 3.4 SIMT 算子实现

# 3.4.1 基础知识

# 说明

纯SIMT编程场景当前不支持使用SIMT API，敬请期待后续版本的正式发布。

本节内容为使用SIMT API进行SIMT编程的指导。

与SIMD编程不同，在SIMT编程中Global Memory上的数据可以被直接读取和使用。SIMT编程常通过组织线程的层次结构来实现数据切分，使用threadIdx等SIMT BuiltIn关键字计算线程应处理的数据索引，完成对应数据的计算，从而将函数的实现简化为标量计算。

SIMT是Ascend C单指令多线程的编程抽象，允许一条指令对数据进行独立寻址，更加灵活， 如下图所示，对于离散访问的算子，适合使用SIMT编程实现，例如Scatter类与Gather类算子。此外，线程可以独立执行，每个线程具有较高的灵活性，能够执行不同的逻辑分支以完成复杂的逻辑实现。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/c3b6e5aeef5ecfe0cb511a204a88937652d110965678db6121e2898eeadb1728.jpg)



连续寻址操作


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/a7f82f3c9f9fe1cf47eaea85f0937a6a30f9f341042feff30bb9cef27616cdf4.jpg)



离散寻址操作


# 3.4.2 算子实现

本章节将以Gather类算子为例，介绍SIMT算子实现的基本流程，如下图所示：

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/d59f359c2ee4b779a3edb005056b946f607628a04cb480ec22b358a780b37b47.jpg)


算子分析与核函数定义：明确算子的输入和输出，分析最大线程数的设置方案。明确算子核函数名、输入输出参数，确定动态参数空间大小，配置最大线程数。

Host侧线程切分计算：根据输入数据的shape信息，计算和设置gridDim、blockDim等参数。

Kernel侧算子实现：实现单线程内的计算逻辑。

下文将对上述步骤进行详细介绍。完整的算子实现请参考pure_simt_gather算子实现样例。

# 算子分析与核函数定义

算子分析具体步骤如下：

步骤1 明确算子的功能和计算逻辑。

gather算子的功能为从输入张量中获取指定索引行的数据，即从形状为M * N的二维向量input中获取指定索引的m行数据，这m行的行索引由输入index指定。算子输出output第i行数据的计算公式为：

```txt
output[i] = input[index[i]] 
```

步骤2 明确算子的输入和输出。

gather算子有两个输入：input与index；输出为output。

本样例中算子输入input的数据类型支持float、half、int32_t，index的数据类型为uint32_t，算子输出的数据类型与输入的数据类型相同。

每个线程处理一行数据，需要传入每行数据的长度in_width以及需要处理的总行数index_total_length，以确保尾部线程不会进行无效操作。

在算子实现中，无需使用大量临时变量，为了提高性能，可以在默认最大线程数（1024）的基础上适当增大核函数的最大线程数。

步骤3 明确函数名称和参数。

自定义核函数名称，本样例中核函数命名为gather_custom。

通过分析算子的输入和输出，使用模板参数来支持不同的输入输出数据类型。

<table><tr><td>模板参数名</td><td>模板参数类型</td><td>参数定义</td></tr><tr><td>type_data</td><td>typename</td><td>输入输出的数据类型</td></tr><tr><td>type_idx</td><td>typename</td><td>index的数据类型</td></tr></table>


函数入参定义如下：


<table><tr><td>参数名</td><td>参数类型</td><td>参数定义</td></tr><tr><td>input</td><td>type_data*</td><td>输入数据在Global Memory上的内存地址</td></tr><tr><td>index</td><td>type_idx*</td><td>索引数据在Global Memory上的内存地址</td></tr><tr><td>gather_output</td><td>type_data*</td><td>输出数据在Global Memory上的内存地址</td></tr><tr><td>in_width</td><td>uint32_t</td><td>输入数据第二维的长度(列宽)</td></tr><tr><td>index_total_length</td><td>uint32_t</td><td>index数据的总长度</td></tr></table>

步骤4 明确SIMT核函数gridDim、blockDim等动态参数设置方案。

本样例采用均匀切分方案，根据可用核数和最大线程数的限制，计算和调整gridDim（启用的线程块的个数）、blockDim（一个线程块启用的的线程个数），同时保证gridDim不超过65535、blockDim不超过最大线程数2048。

本算子实现逻辑中无需使用动态UB空间。

# ----结束

通过以上分析，得到SIMT Gather算子的设计规格如下：

算子类型（OpType）：Gather

算子输入输出：


表 3-15 Gather 算子输入输出规格


<table><tr><td>name</td><td>shape</td><td>data type</td><td>format</td></tr><tr><td>input(输入)</td><td>(M, N)</td><td>float/half/int32_t</td><td>ND</td></tr><tr><td>index(输入)</td><td>(m), m &lt; M</td><td>uint32_t</td><td>ND</td></tr><tr><td>output(输出)</td><td>(m, N)</td><td>float/half/int32_t</td><td>ND</td></tr></table>

核函数名称：gather_custom

核函数定义如下：

```c
constexpr uint32_t MAX_THREAD_COUNT = 2048;

template <typename type_data, typename type_idx>
__global__ launch_bounds__(MAX_THREAD_COUNT) void gather_custom(
    type_data* input,
    type_idx* index,
    type_data* gather_output,
    uint32_t in_width,
    uint32_t index_total_length) 
```

# 说明

在定义核函数时，使用__launch_bounds__(MAX_THREAD_COUNT)来指定最大线程数。最大线程数的设置范围为1到2048。设置的最大线程数越大，支持启用的线程越多，性能越好，但每个线程可使用的内部寄存器数量会减少。若未设置，最大线程数默认值为1024。在上述分析中已明确计算不需要过多寄存器，因此设置最大线程数为2048。在实际的算子开发过程中，应根据具体的算子实现来调整该值。

# Host侧线程切分计算

本样例以简单的均匀切分方案介绍如何实现动态切分参数的计算。

步骤1 设置初始gridDim。

考虑到如果gridDim设置得比实际AIV核数少，会导致空闲核浪费，因此将初始gridDim设置为当前芯片的实际AIV核数量。AIV数量的获取方法如下所示：

```cpp
uint32_t real_core_num = 0;
const auto& platformInfoMgr = platform_ascendc::PlatformAscendCManager::GetInstance();
real_core_num = platformInfoMgr->GetCoreNumAiv();
block_num = real_core_num; // block_num为初始gridDim
```

步骤2 计算blockDim。

根据输入index的长度index_total_length、初始gridDim计算一个线程块启用的的线程个数blockDim。

```c
// thread_num_per_block为blockDim值
thread_num_per_block = (index_total_length + block_num - 1) / block_num;
```

步骤3 调整blockDim。

若blockDim超出最大线程数限制，调整blockDim值为最大线程数值。

```c
if (thread_num_per_block > MAX_THREAD_COUNT) {
    thread_num_per_block = MAX_THREAD_COUNT;
} 
```

步骤4 调整gridDim。

重新计算gridDim，确保gridDim * blockDim > index_total_length，即确保所有启用的线程能够处理完指定行数的数据。

```txt
block_num = (index_total_length + thread_num_per_block - 1) / thread_num_per_block; 
```

# ----结束

完整的切分计算代码如下：

```cpp
constexpr uint32_t MAX_THREAD_COUNT = 2048;
constexpr uint32_t MAX_BLOCK_COUNT = 65535;

bool block_split(uint32_t index_total_length, uint32_t &block_num, uint32_t &thread_num_per_block) {
    uint32_t real_core_num = 0;
    const auto& platformInfoMgr = platform_ascendc::PlatformAscendCManager::GetInstance();
    if (platformInfoMgr == nullptr) {
    std::cout << "[ERROR] Get platform info failed, please check device status."<< std::endl;
    return false;
    }
    real_core_num = platformInfoMgr->GetCoreNumAiv();
    block_num = real_core_num;
    thread_num_per_block = (index_total_length + block_num -1) / block_num;
    if (thread_num_per_block > MAX_THREAD_COUNT) {
    thread_num_per_block = MAX_THREAD_COUNT;
    block_num = (index_total_length + thread_num_per_block - 1) / thread_num_per_block;
    if (block_num > MAX_BLOCK_COUNT) {
    std::cout << "[ERROR] index_total_length: "<< index_total_length << " can not be bigger than "
    << MAX_THREAD_COUNT * MAX_BLOCK_COUNT << "."<< std::endl;
    return false;
    }
    }
    return true;
} 
```

# Kernel 侧算子实现

步骤1 根据均匀切分算法，获取当前线程的位置偏移量。

在本算子中，仅使用gridDim、blockDim等线程维度的第一维，因此计算偏移量时只需考虑x维信息。如下代码所示，threadIdx表示线程在其所在线程块内的索引，blockDim表示一个线程块中设置的线程数，而blockIdx表示线程块的索引。

```txt
// 计算线程索引
int32_t out_row = blockIdx.x * blockDim.x + threadIdx.x;
```

步骤2 根据线程索引，获取当前线程需要处理数据的行索引，计算对应的输入、输出位置偏移量，实现整行数据的获取采集。

```txt
uint32_t in_row = index[out_row];
int input_idx = in_row * in_width;
int output_idx = out_row * in_width; 
```

```txt
for (int32_t col = 0; col < in_width; col++) {
    gather_output[output_idx] = input[input_idx];
    input_idx += 1;
    output_idx += 1;
} 
```

# ----结束


完整的核函数功能代码如下：


```lisp
constexpr uint32_t MAX_THREAD_COUNT = 2048;
constexpr uint32_t MAX_BLOCK_COUNT = 65535;

template <typename type_data, typename type_idx>
__global__ launch_bounds_(MAX_THREAD_COUNT) void gather_custom(
    type_data* input,
    type_idx* index,
    type_data* gather_output,
    uint32_t in_width,
    uint32_t index_total_length)
{
    // Calculate global thread ID
    int32_t out_row = blockIdx.x * blockDim.x + threadIdx.x;
    // Maps to the row index of output tensor
    if (out_row >= index_total_length) {
    return;
    }
    // Single thread processes entire row (all columns) - enables coalesced memory access
    uint32_t in_row = index[out_row];
    int input_idx = in_row * in_width;
    int output_idx = out_row * in_width;
    for (int32_t col = 0; col < in_width; col++) {
    gather_output[output_idx] = input[input_idx];
    input_idx += 1;
    output_idx += 1;
    }
} 
```

# 运行验证

核函数即算子Kernel程序开发完成后，即可编写Host侧的核函数调用程序，实现从Host侧的APP程序调用算子，进行运行验证。


Host侧的关键代码如下：


```cpp
std::vector<float> gather(std::vector<float>& input, const uint32_t* in_shape, std::vector<uint32_t>& index)
{
    ...
    // 计算切分参数，设置动态UB内存
    uint32_t block_num = 0;
    uint32_t thread_num_per_block = 0;
    block_split(index_total_length, block_num, thread_num_per_block))
    ...
    // 计算切分参数，设置动态UB内存
    uint32_t dyn_ubuf_size = 0; // No need to alloc dynamic memory.
    // 用内存调用符<<...>>>调用核函数完成指定的运算
    gather_custom<<block_num, thread_num_per_block, dyn_ubuf_size, stream>>(input_device, index_device, output_device, in_shape[1], index_total_length);
}
```

# 3.5 SIMD 与 SIMT 混合算子实现

# 3.5.1 基础知识

本节内容为使用Reg矢量计算API和SIMT API进行SIMD与SIMT混合编程的指导。

在Vector Core中，SIMT单元和SIMD单元共享片上存储，因此可以利用片上存储Unified Buffer完成SIMD与SIMT混合编程，具体硬件架构的介绍请参考2.6.2.4 NPU架构版本351x。在进行后续内容的学习前，请先了解SIMD与SIMT混合编程的编程模型：2.2.3.4 SIMD与SIMT混合编程。

SIMD编程提供了基于寄存器（Regbase）开发的Reg矢量计算API，Reg矢量计算API可以直接操作Vector Core中的SIMD寄存器，API单次处理的数据量上限等于寄存器的大小，通过GetVecLen接口获取该值。在算子实现中，需要多次调用Reg矢量计算API完成对单核数据的处理。

与SIMD编程不同的是，在SIMT编程中Global Memory上的数据可以被直接读取和使用。SIMT编程常通过组织线程的层次结构来实现数据的切分，使用threadIdx等SIMTBuiltIn关键字计算线程应处理的数据索引，完成索引对应数据的计算，从而将函数实现简化为标量计算。

# 3.5.2 算子实现

# 说明

本样例的目的是通过一个简单的算子实现展示SIMD与SIMT混合编程方式，不是该算子功能的最佳实践。

基于SIMD与SIMT混合编程方式实现矢量算子核函数的流程如下图所示。


图 3-79 SIMD 与 SIMT 混合核函数实现流程


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/cd550af53832b6eea3a687d6162e036e5331d5c4a4c749a9ff6f5585bf3a375a.jpg)


算子分析：分析算子的输入、输出、数学表达式和计算逻辑。

核函数开发：定义并实现Ascend C算子入口函数。

SIMD VF函数开发：定义并实现SIMD VF入口函数。

SIMT VF函数开发：定义并实现SIMT VF入口函数。

以下内容以从长度为10万的一维向量中提取指定索引的8192个数据，并对提取的数据分别执行加1运算的gather & adds算子为例，对上述步骤进行详细说明。本样例中介绍的算子完整代码请参见SIMD与SIMT混合编程实现gather&adds算子样例。

# 算子分析

算子分析具体步骤如下：

步骤1 明确算子的输入和输出。

gather & adds算子有两个输入input与index，input是原始数据，index是要获取的数据在input中的索引；输出为output。

本样例中算子的输入input支持的数据类型为float，输入index支持的数据类型为uint32_t，输出output的数据类型与输入input的数据类型相同。

算子输入input支持的shape为[100000]；输入index支持的shape为[8192]，且index数据取值在[0, 100000)范围内；输出output的shape与输入index的shape相同。

算子输入支持的format为：ND。

步骤2 明确算子的数学表达式及计算逻辑。

gather & adds算子输出output中第i个数据为：

```javascript
output[i] = input[index[i]] + 1 
```

计算逻辑如下：

使用SIMT编程方式从输入input（Global Memory）中获取指定索引的数据，存储到Unified Buffer上。

使用SIMD编程方式在片上存储（Unified Buffer）做数据加1运算。

● 将Unified Buffer上的计算结果搬出到外部存储（Global Memory）上。


图 3-80 算子计算逻辑


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/19bb653d39515dc439473a6865e9070bae96fd068936e6b4decbccf3393377b7.jpg)


# 说明

simd_adds中加1运算实际可以在simt_gather函数中快速实现，本样例的目的是通过一个简单的算子实现展示SIMD与SIMT混合编程方式，不是该算子功能的最佳实践。

# 步骤3 确定核函数名称和参数。

本样例中核函数命名为gather_and_adds_kernel。

根据对算子输入输出的分析，确定核函数有5个参数input，index，output，input_total_length，index_total_length；input，index为输入在Global Memory上的内存地址，output为输出在Global Memory上的内存地址，input_total_length是input的数据长度，index_total_length是index的数据长度，也是output的数据长度。

# 步骤4 明确分核策略、SIMT线程配置和SIMD Reg矢量计算API循环调用次数。

本例中算子输入index的形状为8192，可设置核数为8，每个核处理数据量为1024。

对于SIMT实现，可设置线程数为1024，每个线程处理1个数据，单个核只需调用1次simt_gather函数即可完成gather运算。

对于SIMD Reg矢量计算实现，单核处理数据量为1024，Reg矢量计算API单次处理的数据长度one_repeat_size为GetVecLen/sizeof(float)，API的循环调用次数repeat_times为1024/one_repeat_size。

# 步骤5 确定SIMT VF函数名称和参数。

本样例中SIMT VF函数命名为simt_gather。

根据SIMT线程配置策略，确定SIMT VF函数有6个参数input，index，gather_output，input_total_length，index_total_length，output_total_length；input，index为输入在Global Memory上的内存地址，gather_output为输出在Unified Buffer上的内存地址，input_total_length是input的数据长度，index_total_length是index的数据长度，output_total_length是单核上gather_output的数据长度。

# 步骤6 确定SIMD VF函数名称和参数

本样例中SIMD VF函数命名为simd_adds。

根据上述SIMD策略，确定SIMD VF函数有5个参数output，input，count，one_repeat_size，repeat_times；output为输出在Unified Buffer上的内存地址，input为输入在Unified Buffer上的内存地址，count是单核处理的数据总量，one_repeat_size是单次循环处理的数据量，repeat_times是Reg矢量计算API循环调用次数。

# ----结束

通过以上分析，得到Ascend C gather & adds算子的设计规格如下：

算子类型（OpType）：Gather_Adds

算子输入输出：


表 3-16 gather & adds 算子输入输出规格


<table><tr><td>name</td><td>shape</td><td>data type</td><td>format</td></tr><tr><td>input(输入)</td><td>100000</td><td>float</td><td>ND</td></tr><tr><td>index(输入)</td><td>8192</td><td>uint32_t</td><td>ND</td></tr><tr><td>output(输出)</td><td>8192</td><td>float</td><td>ND</td></tr></table>

核数：8

SIMT线程数：1024

核函数名称：gather_and_adds_kernel

SIMT VF函数名称：simt_gather

SIMD VF函数名称：simd_adds

算子实现文件名称：gather_and_adds.asc

# 核函数定义与实现

根据核函数中介绍的规则进行核函数的定义。

# 步骤1 函数原型定义

本样例中，函数名为gather_and_adds_kernel（核函数名称可自定义），根据上述分析，函数原型定义如下：

```c
__global__ __aicore__ void gather_and_adds_kernel(__gm__ float* input, __gm__ uint32_t* index, __gm__ float* output, uint32_t input_total_length, uint32_t index_total_length)
{
} 
```

步骤2 启动SIMT VF函数simt_gather，从input中获取指定索引的数据。

1. 计算单核应处理的数据量。数据总量为index_total_length，除以核数即可得到单核应处理的数据量。

```txt
uint32_t index_total_length_per_block = index_total_length / AscendC::GetBlockNum(); 
```

2. 使用Alloc接口申请Unified Buffer内存空间，并将该Tensor作为simt_gather函数的输出。

3. 使用asc_vf_call接口启动SIMT_VF函数simt_gather。第一个参数为dim3结构，代表线程的三维层次结构，本例中初始化为dim3(1024)，使用一维定义方式，线程总数为1024。

```cpp
constexpr uint32_t THREAD_COUNT = 1024;

__global__ __aicore__ void gather_and_adds_kernel(__gm__ float* input, __gm__ uint32_t* index, __gm__ float* output, uint32_t input_total_length, uint32_t index_total_length)
{
    // 设置kernel type为AIV_ONLY
    KERNEL_TASK_TYPE_DEFAULT(KERN_TYPE_AIV_ONLY);

    // 定义UB内存分配对象
    AscendC::LocalMemAllocator<AscendC::Hardware::UB> ub_allocator;

    // 计算单核应处理的数据量
    uint32_t index_total_length_per_block = index_total_length / AscendC::GetBlockNum();
    // 申请UB内存作为simt_gather的输出
    AscendC::LocalTensor<float> gather_output = ub_allocator Alloc<float>(index_total_length_per_block);
    // 1. 调用simt函数获取指定索引的1024个数据
    asc_vf_call<simt_gather>(dim3(THREAD_COUNT), input, index, (_ubuf__ float *)gather_output.GetPhyAddr(),
    input_total_length,
    index_total_length,
    index_total_length_per_block);

    // 2. 调用SIMD函数完成加1操作
    ...

    // 3. 将数据搬运到GM
}
```

步骤3 启动SIMD VF函数simd_adds，对Unified Buffer上的数据做加1计算。

1. 使用Alloc接口申请Unified Buffer内存空间，并将该Tensor作为simt_adds函数的输出。

2. 使用GetVecLen接口除以单个数据长度，计算Reg矢量计算API单次处理的数据量one_repeat_size。使用单核应处理的数据量index_total_length_per_block除以单次处理数据量one_repeat_size，计算Reg矢量计算API循环调用次数。

3. 使用asc_vf_call接口启动SIMD VF函数simd_adds。

```cpp
__global__ __aicore__ void gather_and_adds_kernel(__gm__ float *input, __gm__ uint32_t *index, __gm__ float *output, uint32_t input_total_length, uint32_t index_total_length)
{
    // 1. 调用simt函数获取指定索引的1024个数据
    ...
    // 申请UB作为simd_adds的输出
    AscendC::LocalTensor<float> adds_output = ub_allocator Alloc<float>(index_total_length_per_block);
    // 计算Reg矢量计算API单次调用处理的数据量
    constexpr uint32_t one_repeat_size = AscendC::GetVecLen() / sizeof(float);
    // 计算Reg矢量计算API循环调用次数
    uint16_t repeat_times = (index_total_length_per_block + one_repeat_size - 1) / one_repeat_size;
    // 2. 调用SIMD函数完成加1操作
    asc_vf_call<simd_adds>((__ubuf__ float *)adds_output.GetPhyAddr(),
    (__ubuf__ float *)gather_output.GetPhyAddr(), index_total_length_per_block, one_repeat_size, repeat_times);

    // 依赖PIPE_V和PIPE_MTE3流水同步
    AscendC::SetFlag<AscendC::HardEvent::V_MTE3>(0);
    AscendC::WaitFlag<AscendC::HardEvent::V_MTE3>(0);

    // 3. 将数据搬运到GM
    ...
}
```

步骤4 使用DataCopy接口将结果数据搬运到Global Memory。

```cpp
__global__ __aicore__ void gather_and_adds_kernel(__gm__ float* input, __gm__ uint32_t* index, __gm__ float* output, uint32_t input_total_length, uint32_t index_total_length)
{
    // 1. 调用simt函数获取指定索引的1024个数据
    ...
    // 2. 调用SIMD函数完成加1操作
    ...
    // 3. 将数据搬运到GM
    // 定义GlobalTensor对象，用于数据搬运
    AscendC::GlobalTensor<float> output_global_tensor;
    // 根据核偏移初始化GlobalTensor地址
    output_global_tensor.SetGlobalBuffer(output + index_total_length_per_block * AscendC::GetBlockIdx());
    // 调用数据搬运接口将数据搬运到GM
    AscendC::DataCopy(output_global_tensor, adds_output, index_total_length_per_block);
}
```

----结束

# SIMT VF 函数定义与实现

步骤1 定义函数原型。

根据上述对SIMT VF函数的参数分析，定义SIMT VF函数原型。使用__simt_vf__函数类型限定符标识SIMT VF核函数入口，使其可以被asc_vf_call调用。

# 说明

在SIMT 编程中，__launch_bounds__(thread_num)是可选配置，用于在编译期指定核函数启动的最大线程数（如果不配置，thread_num默认为1024），使用时请注意：thread_num >= x * y* z （即：asc_vf_call的第一个参数：dim3{x, y, z}）, 线程数thread_num的取值范围为1到2048。最大线程数决定了每个线程可分配的寄存器数量，具体对应关系请见表2-23，寄存器用于存储线程中的局部变量，若局部变量的个数超出寄存器个数，容易出现栈溢出等问题。

```c
constexpr uint32_t THREAD_COUNT = 1024;

__simt_vf__ __launch_bounds__(THREAD_COUNT) inline void simt_gather(
    __gm__ float* input,
    __gm__ uint32_t* index,
    __ubuf__ float* gather_output,
    uint32_t input_total_length,
    uint32_t index_total_length,
    uint32_t output_total_length)
{
} 
```

# 步骤2 实现函数。

simt_gather函数实现从输入input（Global Memory）中获取指定索引的数据。基于上述数据切分策略，首先计算线程应处理数据的索引，然后通过赋值操作将数据存储到Unified Buffer上。

本例中核数设置为8，线程的层次结构为{1024, 1, 1}，数据总量为8192（8 *1024）。每个线程只需处理一个数据，应处理数据在index中的索引计算逻辑为：当前核id*每核线程数+当前线程的id，代码如下：

```txt
int idx = blockIdx.x * blockDim.x + threadIdx.x; 
```

blockIdx用于获取当前核id。blockDim用于获取线程三维层次结构{x, y, z}，本例中为{1024, 1, 1}，其中第2，3维度均为1，使用一维层次结构，因此线程数可写作blockDim.x。threadIdx用于获取三维线程索引{x, y, z}，本例中仅使用第1维x，可通过threadIdx.x获取当前线程的id。

```c
__simt_vf__ launch_bounds__(THREAD_COUNT) inline void simt_gather(
    __gm__ float* input,
    __gm__ uint32_t* index,
    __ubuf__ float* gather_output,
    uint32_t input_total_length,
    uint32_t index_total_length,
    uint32_t output_total_length)
{
    // 异常判断，防止越界
    if (threadIdx.x >= output_total_length) {
    return;
    }

    // 计算线程应处理的数据在输入index的索引
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    // 异常判断，防止越界
    if (idx >= index_total_length) {
    return;
    }

    // 读取线程应获取的数据在输入input中的索引
    uint32_t gather_idx = index[idx];
    // 异常判断，防止越界
    if (gather_idx >= input_total_length) {
    return;
    }

    // 将input中索引为gather_idx的数据存到UB上
```

```txt
gather_output[threadIdx.x] = input[gather_idx];
} 
```

----结束

# SIMD VF 函数定义与实现

步骤1 定义函数原型。

根据上述对SIMD VF函数的参数分析，定义SIMD VF函数原型。使用__simd_vf__函数类型限定符标识SIMD VF入口函数，使其可以被asc_vf_call关键字调用。

```c
__simd_vf__ inline void simd_adds(__ubuf__ float *output, __ubuf__ float *input, uint32_t count, uint32_t one_repeat_size, uint16_t repeat_times)
{ 
```

步骤2 循环调用repeat_times次Reg矢量计算API完成加1运算。

1. 使用连续对齐搬入接口将数据从Unified Buffer搬运到Reg矢量计算基本单元RegTensor。

2. 使用Adds接口完成将数据加1运算。

3. 使用连续对齐搬出接口将数据从RegTensor搬运到Unified Buffer。

```cpp
constexpr float ADDS_ADDEND = 1.0f;

__simd_vf__ inline void simd_adds(__ubuf__ float *output, __ubuf__ float *input, uint32_t count, uint32_t one_repeat_size, uint16_t repeat_times)
{
    // 初始化Reg矢量计算单元，源操作数
    AscendC::Reg::RegTensor<float> src_reg0;
    // 初始化Reg矢量计算单元，目的操作数
    AscendC::Reg::RegTensor<float> dst_reg0;
    // 初始化Reg矢量计算标记寄存器
    AscendC::Reg::MaskReg mask;

    for (uint16_t i = 0; i < repeat_times; i++) {
    // 从UB搬运数据到Reg矢量计算基本单元
    mask = AscendC::Reg::UpdateMask<float>(count);
    AscendC::Reg::LoadAlign(src_reg0, input + i * one_repeat_size);
    // 调用Adds接口完成加1运算
    AscendC::Reg::Adds(dst_reg0, src_reg0, ADDS_ADDEND, mask_reg);
    // 从Reg矢量计算基本单元搬运数据到UB
    AscendC::Reg::StoreAlign(output + i * one_repeat_size, dst_reg0, mask_reg);
    }
}
```

----结束

# 3.6 功能调试

# 3.6.1 运行正常

运行正常是算子开发后进行调测的前提，是后续进行性能优化的基础。Ascend C提供了多种调测方法和调测工具，供开发者使用，具体内容请参考2.7.2 功能调试。

# 3.6.2 精度正常

性能优化的基础是算子运行得到正确的计算结果。评判计算结果正确性需要有一定的评判标准，即使用已知的正确的输出和实际结果进行比较。优化过程中，每次迭代修改后，都需要验证性能优化的新结果是否满足精度评判标准。

下文先介绍几个会影响精度正常的因素：

正确插入同步，同步机制是并行计算架构的核心特点之一，对于有数据依赖的场景需要正确插入同步。

正确计算偏移地址，多核并行计算时，计算数据在内存上的正确偏移对保证计算结果的正确性至关重要。

浮点数计算，在涉及浮点计算的情况下，精度正常不能期望数值逐bit一致。因为浮点数的计算本身不满足交换律、结合律，另外不同硬件对浮点数的支持不同，都可能导致精度结果存在差异。

然后介绍编码过程中需要严格遵守的规则（禁止修改kernel函数参数），防止出现不必要的精度问题。

# 正确插入同步

核内同步

AI Core内部包括MTE1、MTE2、MTE3、Cube、Vector、Scalar等多条流水线。Ascend C框架默认使能auto sync（自动插入同步）编译选项，编译器可以正常插入同步；Ascend C编程模型也会帮助开发者完成部分流水的同步控制。流水类型的详细介绍、同步类型的分类、编译器自动同步的约束限制、何时需要开发者手动插入同步可参考同步控制。

核间同步

上文描述的都是核内同步的情况。特别的，当算子使用多核同步时（多核同步概念可参考多核同步），逻辑核数NumBlocks必须保证不大于实际运行该算子的AI处理器核数，否则框架插入同步会出现异常，导致Kernel“卡死”现象。

【反例】

```javascript
// 存在多核同步逻辑的代码中NumBlocks大于CoreNum
// 比如在Tiling计算中没有做该校验
FlashAttentionScoreApiTiling(tilingData);
FlashAttentionScoreGetTensorSize(tilingData);
CoreNum = ascendcPlatform.GetCoreNum();
context->SetBlockDim(CoreNum + 1);
```

【正例】

```txt
FlashAttentionScoreApiTiling(tilingData);
FlashAttentionScoreGetTensorSize(tilingData);
// 在Kernel中有使用多核同步指令时，Host设置NumBlocks需要保证不大于CoreNum
CoreNum = ascendcPlatform.GetCoreNum();
context->SetBlockDim(CoreNum);
```

# 正确计算偏移地址

算子使能多核计算时，需要在Tiling的时候确定单核的计算量，Kernel侧根据单核计算量进行地址的偏移。

比如如下样例的分配方案：数据整体长度TOTAL_LENGTH为8 * 2048个元素，平均分配到8个核上运行，每个核上处理的数据大小BLOCK_LENGTH为2048。x +BLOCK_LENGTH * GetBlockIdx()即为单核处理程序中输入x在Global Memory上的内存偏移地址，获取偏移地址后，使用GlobalTensor类的SetGlobalBuffer接口设定该核上Global Memory的起始地址以及长度。具体示意图请参考图3-81。

```txt
xGm.SetGlobalBuffer((__gm__ half*)x + BLOCK_LENGTH * GetBlockIdx(), BLOCK_LENGTH); 
```


图 3-81 多核并行处理示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/e2790d42354194ebbf7550b578c47701ac496eb8e1b5b8c6795f64737a255c14.jpg)


# 浮点数计算

每个浮点算术运算都涉及一定量的舍入。因此，算术运算的执行顺序很重要。如果A、B和C是浮点值，则(A+B)+C不能保证像数学计算中那样等于A+(B+C)。当并行计算时，可能会更改操作的顺序，因此并行结果可能与顺序结果不匹配。这种情况带来的精度差异是浮点值计算固有存在的。

某些型号的AI处理器指令支持的数据类型有限，当发现某个API支持的数据类型不能满足需求时，应更倾向于先将数据转换到更高的精度进行计算后，再将计算结果转换到目标精度，来防止精度损失。例如，某款AI处理器Vector计算类API不支持bfloat16的计算，需要先使用Cast接口转换成float数据类型，进行计算后再使用Cast接口转换回bfloat16数据类型。

# 【正确示例】

```c
// dst = src0 + src1, src0、src1、dst均为bfloat16类型，tmp0、tmp1、tmp2均为float类型
...
Cast(tmp0Tensor, src0Tensor, RoundMode::CAST_NONE, computeSize);
Cast(tmp1Tensor, src1Tensor, RoundMode::CAST_NONE, computeSize);
Add(tmp2Tensor, tmp0Tensor, tmp1Tensor, computeSize);
Cast(dstTensor, tmp2Tensor, RoundMode::CAST_FLOOR, computeSize);
...
```

昇腾AI处理器都遵循IEEE 754标准进行二进制浮点表示，除了一些小的例外。这些例外可能会导致与在主机系统上计算的IEEE 754值不同的结果。例如，使用了复合指令的API Axpy，源操作数中每个元素与标量求积后和目的操作数中的对应元素相加，该复合指令将乘加操作组合到单个指令执行，计算结果可能与分别执行这两个操作的单指令得到的结果略有不同。开发者在使用此类API时，需要考虑这种精度差异。

# 禁止修改Kernel函数参数

禁止修改Kernel函数参数，不能对函数参数重新进行赋值和修改。例如：

FlashAttentionKernel函数定义如下，其参数query、key、tilingData等为指针类型，该指针本身禁止修改。对于算子输入参数，指针指向的内容不可以修改；作为一个例外，算子输出参数，指针指向的内容可以进行修改。特别要强调一下，为了实现静态编译，无论是对tilingData指针本身，还是对tilingData指针指向的内容均禁止修改。

```c
__aicore__ __global__ void FlashAttentionKernel(__gm__ uint8_t* query, __gm__ uint8_t* key, ..., __gm__ uint8_t* attention,..., __gm__ uint8_t* tilingData) {
    ......
} 
```

# 【反例】

```javascript
// 对Kernel函数参数重新赋值、对TilingData内容进行修改是不允许的，以下是错误示例
query = tmpQueryPtr;
key = tmpKeyPtr;
tilingData = tmpTilingDataPtr;
tilingData[0] = 2;
```

# 【正例】

```txt
// 输入参数仅进行读操作
inputQueryGMTensor.SetGlobalBuffer(query);

// 输出参数attention指针本身是只读，但其指向的内存可以读写
outputAttentionGMTensor.SetGlobalBuffer(attention);
...
DataCopy(outputAttentionGMTensor, outputAttentionLocalTensor, count);
```

# 3.6.3 算子调试

从异构计算章节可以了解到Ascend C算子主要包括Tiling和Kernel实现两部分组成。

Tiling实现运行在Host侧CPU上，一般使用传统的调测手段（比如gdb工具）即可完成调试。

Kernel实现运行在Device侧NPU上，Ascend C提供了多种调试方式，包括孪生调试、上板调试等，具体的调试方法请参考算子调试。

# 3.7 性能分析

# 3.7.1 获取性能数据

在进行性能优化之前，需要拿到准确的性能数据，了解性能现状，并根据性能现状分析下一步的优化方向。Ascend C提供了多种性能测试方法，包括上板Profiling、单算子性能仿真流水图等手段。

# 上板 Profiling

如下命令行是一个算子上板性能数据采集的示例，可以根据自身的需要灵活组合配置参数。示例中--output为可选参数，用于指定收集到的性能数据的存放路径；$HOME/projects/MyApp/out/main为算子可执行文件。

```txt
msprof op --output=$HOME/projects/output $HOME/projects/MyApp/out/main 
```

如下示例则展示了部分性能数据文件的样例：


图 3-82 PipeUtilization.csv（计算单元和搬运单元耗时占比）文件示例


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/7b338e49fbbc69162856ab66c0e33d4ebad6493a5dab742b5190681bef52c455.jpg)


详细的字段说明和性能分析工具的具体使用方法请参考《算子开发工具》。

# 算子仿真流水图

算子调优工具msProf支持仿真环境下的性能数据采集和自动解析。使用msProf工具获取仿真流水图的具体方式请参考《算子开发工具》。

支持以下两种可视化呈现方式：

Chrome浏览器

在Chrome浏览器中输入“chrome://tracing”地址，并将通过msprof opsimulator生成指令流水图文件（trace.json）拖到空白处打开，键盘上输入快捷键（W：放大，S：缩小，A：左移，D：右移）可进行查看。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/7ca729c3ab9df9b115b6db2fcbde2b965e8ebbac85126035cf8eed05269bfca5.jpg)


指令流水图支持MindStudio Insight可视化呈现，MindStudio Insight工具以时序图方式为用户提供指令在昇腾AI处理器上的运行情况，用户可通过分析时序图中的指令详情、指令执行时间、指令关联代码的调用栈及指令/流水间同步连线等信息，识别微观指令的时序优化点。


图3-83 时间线界面


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/a9b73e3af47473d7cdadac179b49691da8d8ac3d9bbe342e9dcde4375fe511a4.jpg)


# 说明

本文部分样例中展示的算子仿真流水图和上述两种可视化呈现方式不一致，但是其中的关键字段含义是对应的，开发者可以参考《算子开发工具》查看具体字段的含义。

# 3.7.2 分析性能数据

# 理论参数

理论性能为算子实际性能的理想目标。不同的硬件平台的硬件规格各异，理论性能可以帮助我们了解硬件的潜能，从而设定性能优化的目标。

搬运相关流水（MTE1/MTE2/MTE3等）的理论耗时 = 搬运数据量（单位：Byte） / 理论带宽。例如：某款AI处理器的GM峰值带宽约为1.8TB/s，想要进行一次float数据类型、4096 * 4096大小的矩阵搬运，搬运的理论耗时是sizeof(float) * 4096 * 4096 / 1.8TB/s = 37.28us（按照1TB =1012Byte来计算）。

# 说明

● 搬运指令同时存在时，会存在共享带宽的情况，并不能每条都以接近理论带宽的速率搬运数据。比如，当MTE2/MTE3同时进行GM读写时，搬运流水线的耗时应该是（MTE2搬运量 + MTE3搬运量）/ GM带宽。

● 搬运不同大小的数据块时，对带宽的利用率（有效带宽/理论带宽）不一样。针对每次搬运数据量较小的情况，实测性能达不到理论带宽。

计算相关流水（Cube/Vector/Scalar等）的理论耗时 = 计算数据量（单位：Element） / 理论算力。例如：某款AI处理器对float数据类型的Vector理论峰值算力为11.06TOPS，想要进行一次32K float类型的Element单指令计算，计算的理论耗时是32K / 11.06TOPS = 0.003us （按照1K =1000来计算）。

# 查找瓶颈

获取性能数据后，和理论数值差异较大的地方、耗时较长的流程被认为是“瓶颈点”。下文将介绍如何通过性能数据找到瓶颈点和对应的优化方向。

# 方法一：通过上板Profiling分析流水情况

查看上板Profiling解析后的op_summary_*.csv文件分析流水情况。注：“*”表示时间戳。

# 说明

在SIMD与SIMT混合编程场景中，由于硬件架构的固有特性，所有计算任务均以VF（Vector Function）为基本调度执行单元。因此，在Profiling数据中，SIMT和SIMD的VF整体执行时间均被统计为aiv_vec_time。特别地，SIMT VF执行过程中对Global Memory的读写耗时，也会被统计至aic_vec_time指标内。


图 3-84 op_summary_*.csv 示例一


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/c0d929aa5c152511885e9e00b2738331c728c530388c19399603d8eebd858dc9.jpg)


每条流水线的利用率理想情况下应为100%，没有达到100%的流水就可能有提升空间。上图示例中为某款AI处理器上获取的数据，可以看到Cube算子

MatMulV2，Cube流水的利用率aic_mac_ratio在80%左右，初步判断没有充分发挥算力；MTE2流水的利用率aic_mte2_ratio已经在95%左右，判断MTE2是最长的流水。

然后比较最长的流水和理论的差距：输入左右矩阵的shape分别为（2048，12288）、（12288，6144），数据类型为bfloat16；Bias输入的shape为（6144），数据类型为float。由此可以计算出总共需要搬运的数据量，继而通过理论参数中介绍的搬运流水理论耗时计算方法计算出理论值为(sizeof(bfloat16) *$( 2 0 4 8 ^ { \star } 1 2 2 8 8 + 1 2 2 8 8 ^ { \star } 6 1 4 4 ) + \mathsf { s i z e o f } ( \mathsf { f l o a t } ) ^ { \star } 6 1 4 4 ) / 1 . 8 \mathsf { T B } / \mathsf { s } \approx 1 1 1 . 8 \mathsf { u s }$ （按照1TB =1012Byte来计算），与实际性能数据aic_mte2_time存在比较大的差距。经分析输入数据的总大小已经超过L1的空间（512KB），做MatMul计算会存在输入矩阵数据重复搬运的情况，重复搬运的次数是否合理，需要结合流水优化和Tiling优化手段进行优化，可参考方法三、查看仿真流水图分析各条流水的情况进一步分析。


图 3-85 op_summary_*.csv 示例二


<table><tr><td></td><td>G</td><td>H</td><td>J</td><td>M</td><td>N</td><td>O</td><td>P</td><td>Q</td><td>R</td><td>AG</td><td>AH</td><td>AI</td><td>AJ</td><td>AK</td><td>AL</td><td>AM</td><td>AN</td><td>AO</td><td>AP</td></tr><tr><td>t</td><td>Start Time(us)</td><td>Duration( us)</td><td>Block Dim</td><td>Input Shapes</td><td>Input Data Types</td><td>Input Formats</td><td>Output Shapes</td><td>Output Data Types</td><td>Output Formats</td><td>aiv_time( us)</td><td>aiv_total_cycles</td><td>aiv_vec_time(us)</td><td>aiv_vec_ratio</td><td>aiv_scalar_time(us)</td><td>aiv_scalar_ratio</td><td>aiv_mte2_time(us)</td><td>aiv_mte2_ratio</td><td>aiv_mte3_time(us)</td><td>aiv_mte3_ratio</td></tr><tr><td>of</td><td>1.7E+15</td><td>350.3</td><td>40</td><td>*8192,1,8192*</td><td>FLOAT</td><td>FORMAT_1*8192,1,8192*</td><td>DT_BF16</td><td>FORMAT_1</td><td>346.83</td><td>24971839</td><td>19.7136</td><td>0.0568</td><td>1.8592</td><td>0.0054</td><td>343.7998</td><td>0.9913</td><td>137.559</td><td>0.3966</td><td></td></tr><tr><td>of</td><td>1.7E+15</td><td>349.40</td><td>92</td><td>*8192,1,8192*</td><td>FLOAT</td><td>FORMAT_1*8192,1,8192*</td><td>DT_BF16</td><td>FORMAT_1</td><td>346.2</td><td>24926318</td><td>19.7137</td><td>0.0569</td><td>1.8618</td><td>0.0054</td><td>342.772</td><td>0.9901</td><td>137.6721</td><td>0.3977</td><td></td></tr></table>

上图示例中算子输入的shape为（8192，8192），数据类型为float。由此可以计算出总共需要搬运的数据量，继而通过理论参数中介绍的搬运流水理论耗时计算方法计算出理论值为sizeof(float) * (8192 * 8192) / 0.8TB/s ≈ 335.5us （按照1TB =1012Byte来计算，不同的AI处理器其理论带宽有差异），与实际性能数据aiv_mte2_time相符，可以判断该算子基本是一个搬运MTE2 bound（达到上限）的算子。本示例中总体执行时间Duration为350us，和MTE2的实际耗时持平，说明该算子已经调优完成。如果MTE2耗时和总体执行时间有较大差距，那么下一步优化方向主要是流水优化结合Tiling优化，使得其他的流水尽量隐藏在MTE2的流水中，可参考方法三、查看仿真流水图分析各条流水的情况进行进一步分析。

# 方法二：通过上板Profiling分析Tiling情况

查看上板Profiling解析后的op_summary_*.csv文件分析Tiling情况。


图 3-86 op_summary_*.csv 示例


<table><tr><td>OP Type</td><td>Task Type</td><td>Task Start Time(us)</td><td>Task Duration( us)</td><td>Block Dim</td><td>Input Shapes</td><td>Input Data Types</td><td>Input Form</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252899848.070</td><td>1122.96</td><td>48</td><td>&quot;14592,12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252902039.430</td><td>3.24</td><td>12</td><td>&quot;12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252902057.450</td><td>3.42</td><td>12</td><td>&quot;12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252902076.310</td><td>3.06</td><td>12</td><td>&quot;12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252902095.030</td><td>3</td><td>12</td><td>&quot;12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252902113.930</td><td>2.98</td><td>12</td><td>&quot;12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252902132.930</td><td>2.96</td><td>12</td><td>&quot;12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252902286.730</td><td>31.72</td><td>48</td><td>&quot;1536,12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252902361.790</td><td>3.3</td><td>12</td><td>&quot;12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252902876.890</td><td>392.28</td><td>48</td><td>&quot;4608,12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252903611.830</td><td>2.18</td><td>5</td><td>&quot;4608;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252904186.570</td><td>492.82</td><td>48</td><td>&quot;12288,6144;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252905129.550</td><td>1.92</td><td>6</td><td>&quot;6144;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252905703.770</td><td>491.86</td><td>48</td><td>&quot;6144,12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252906650.210</td><td>2.76</td><td>12</td><td>&quot;12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr><tr><td>Mul</td><td>AI_VECTOR_COR</td><td>1706851252906667.790</td><td>2.96</td><td>12</td><td>&quot;12288;&quot;</td><td>FLOAT;FLOAT</td><td>FOR</td></tr></table>

上图示例中为某款AI处理器上获取的数据，通过硬件平台可以查看该AI处理器有48个Vector核，Mul算子是一个纯Vector算子，但是有些场景没有用满所有Vector核（Block Dim < 48），造成算力浪费。那么下一步的主要优化方向为Tiling优化。

# 方法三：通过仿真流水图分析流水情况


图3-87 仿真流水图示例


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/7f75f3dc2ee96e36ca0c509b13e9fe89aabdb2a8047b173182eba585a91fed26.jpg)


上图示例中为某款AI处理器上获取的数据，可以看到Vector核的相关流水（vec0的MTE2、MTE3，vec1的MTE2、MTE3等）有规律性的断流现象。可以结合算子逻辑分析，是否存在数据依赖等因素导致断流。那么下一步的主要优化方向为流水优化，其次结合Tiling优化和内存优化等手段进一步提升Vector流水利用率。

# 方法四：通过上板Profiling查看头开销

头开销是算子执行计算前产生的时延，包含核启动、核取址TLB MISS、同地址访问（由于硬件限制，多核同时访问相同内存地址冲突带来额外的时延）以及变量资源初始化带来的时延。以Atlas A2 训练系列产品/Atlas A2 推理系列产品为例，满核头开销约为20~21微秒。对于推理领域等本身延迟为微秒级别的算子，头开销是一个值得优化的对象。

通过上板Profiling数据（空Kernel时的TaskDuration数据）可以看到每个核的启动开销耗时，继而通过使用恰当的核数和算子Kernel Type等方法来不断的实践，尝试找到最优的配置，具体优化方向可以参考3.8.3 头尾开销优化。

# 3.8 SIMD 算子性能优化

# 3.8.1 优化建议总览表


表3-17 性能优化建议总览表


<table><tr><td>分类</td><td>分类描述</td><td>优化建议</td></tr><tr><td>Tiling策略</td><td>提供Tiling相关的优化建议,便于开发者选择合适的Tiling切分策略。</td><td>核间负载均衡</td></tr><tr><td rowspan="5">头尾开销优化</td><td rowspan="5">提供降低算子头尾开销(算子执行计算前后产生的时延)的优化建议。</td><td>设置合适的核数和算子Kernel类型</td></tr><tr><td>限制TilingData结构大小</td></tr><tr><td>避免TPipe在对象内创建和初始化</td></tr><tr><td>核函数内删除Workspace相关冗余操作</td></tr><tr><td>设置DCI编译选项来减少算子尾开销</td></tr><tr><td rowspan="2">流水编排</td><td rowspan="2">通过任务并行化、异步调度等方法,提升硬件资源利用率,实现更高的吞吐率。</td><td>使能DoubleBuffer</td></tr><tr><td>使能Iterate或IterateAll异步接口避免AIC/AIV同步依赖</td></tr><tr><td rowspan="10">内存访问</td><td rowspan="10">通过控制搬运的数据块大小和GM地址等来实现搬运效率的最大化;通过Buffer的共享与复用、数据压缩精简、使用专用存储空间、访存调度优化等方法来减少内存占用,提升计算效率。</td><td>尽量一次搬运较大的数据块</td></tr><tr><td>GM地址尽量512B对齐</td></tr><tr><td>高效的使用搬运API</td></tr><tr><td>避免同地址访问</td></tr><tr><td>设置合理的L2 CacheMode</td></tr><tr><td>算子与高阶API共享临时Buffer</td></tr><tr><td>纯搬运类算子VECIN和VECOUT建议复用</td></tr><tr><td>通过缩减Tensor ShapeInfo维度,优化栈空间</td></tr><tr><td>避免Unified Buffer的bank冲突</td></tr><tr><td>L2 Cache切分</td></tr><tr><td rowspan="3">矢量计算</td><td rowspan="3">矢量计算相关优化建议。</td><td>通过Unified Buffer融合实现连续vector计算</td></tr><tr><td>Vector算子灵活运用Counter模式</td></tr><tr><td>选择低延迟指令,优化归约操作性能</td></tr><tr><td rowspan="5">矩阵计算</td><td rowspan="5">矩阵计算相关优化建议。</td><td>通过BT Buffer实现高效的bias计算</td></tr><tr><td>通过FP Buffer存放量化参数实现高效随路量化</td></tr><tr><td>通过LOC Buffer数据暂存实现高效的矩阵乘结果累加</td></tr><tr><td>较小矩阵长驻L1 Buffer,仅分次搬运较大矩阵</td></tr><tr><td>Matmul使能AtomicAdd选项</td></tr></table>

# 3.8.2 Tiling 策略

# 3.8.2.1 核间负载均衡

【优先级】：中

【描述】AI处理器的物理核数是固定的，当L2 Cache切分之后，可能发生部分核有计算拖尾的情况，即每次所有核计算量除以每个核处理的数据量不能被核数整除，导致最后需要部分尾核来计算尾块数据。而在尾核计算时，部分核始终处于空闲状态，从而使得算子的整体性能变差。如图1，假设总的数据量为TotalSize，L2 Cache切分之后分为两份TotalSize / 2，每个核每次的计算量为TotalSize / 2 / 25，即需要25个核进行处理，由于AI处理器的核数为20，因此每次计算时，1到5核的每个核需要多算一份数据，导致发生拖尾的情况。

【反例】


图3-88 计算拖尾示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/be9c2ea3c4b6fd06ad7cadc3b4ec99b5c6409100d52a2189508ce969877da088.jpg)


【正例】

针对上述切分策略，调整拖尾核的位置后可以达到全局负载最优，如图2所示。完成所有计算时，1到10核多一次数据块的计算，可以实现全局负载最优。


图3-89 核间负载均衡示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/1706efc79da2e92a3e133b9a04cf70c6dbc15c65a4283856f303bcfad9e71971.jpg)


# 3.8.3 头尾开销优化

# 3.8.3.1 设置合适的核数和算子 Kernel 类型

在算子执行过程中，可能会因为以下几个原因产生额外的启动开销或者头开销：

1. 核启动：每个核在启动时需要进行初始化操作，加载必要的配置和资源。

2. 核取址TLB MISS：当核在访问内存时，如果Translation Lookaside Buffer（TLB）中没有对应的页表项，就需要从内存中加载页表项，这会导致额外的延迟。

3. 同地址访问冲突：由于硬件限制，多个核同时访问相同的内存地址时可能会发生冲突，导致额外的时延。

4. 变量资源初始化：在算子执行前，需要初始化一些变量和资源，这也可能带来额外的性能开销。

头开销会随着使用的核数增加而增加。下图展示了这部分头开销随启动核数的变化情况。


图 3-90 头开销随启动核数的变化


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/b2fed9de2feb162f60dfbdc6b390fb34e6529f51d22b9de3be3d4ddbf3fae777.jpg)


对于整体耗时在微秒级别且单核计算量耗时较少的算子，可以通过减少启动核数并增加单核计算量的方式来获得性能提升。这种优化方式的本质是在头开销耗时和单核计算量耗时之间进行权衡。为了达到最佳性能，开发者需要通过实践尝试，找到最合适的核数设置。

对于自定义算子工程，可以在TilingFunc（算子工程提供的在Host侧计算Tiling的默认函数）中通过SetBlockDim接口来设置算子使用的核数，具体设置方法请参考SetBlockDim；对于Kernel直调工程，可以在<<<>>>调用时指定算子使用的核数。

此外，算子的Kernel类型也会影响算子启动的核数。以纯Vector算子为例，如果以混合启动的方式执行该算子，调度器会同时启动Vector核和Cube核。然而，此时Cube核并没有实际的计算指令，但仍会产生核启动和核初始化的头开销。因此，建议设置合适的Kernel类型以最小化头开销。

通常，算子工程会通过算子使用的指令自动识别算子类型，但该功能无法区分AIC和AIV的配比，默认按照AIV:AIC为1:2的配比下发任务。此外，自动识别功能可能失效，因为其依赖于编译优化的结果。所以推荐用户手动设置算子的Kernel类型。具体设置方法请参考设置Kernel类型。

# 3.8.3.2 限制 TilingData 结构大小

【优先级】中

【描述】TilingData结构是Tiling切分信息的载体，当Host侧按照Tiling切分策略计算完Tiling后，算子会以入参的方式将Tiling切分信息从Host侧传递到Device侧，此时Tiling信息存放在GM上。调用GET_TILING_DATA宏后，会将Tiling信息从GM拷贝到AI处理器的栈空间上，期间会有拷贝开销，由于GM访问效率较低，同时考虑到栈空间限制，需要限制TilingData结构大小。拷贝耗时为us级别，在小shape的场景下，进行此类优化收益会更加明显。

限制TilingData结构大小，可以从以下方面考虑：

减少不必要的TilingData结构变量；

根据Tiling的数据范围选择合适的变量类型；

合理排布TilingData结构；

TilingData整体结构要求8字节补齐。

# 【反例】

如下的示例中存在TilingData结构变量冗余的情况：NumBlocks信息已经通过SetBlockDim接口进行设置，可以在Kernel侧调用GetBlockNum接口获取，无需通过TilingData结构传递。

此外，变量的数据类型也不合理：formerNum和tailNum分别为计算整块数据的核数和计算尾块数据的核数，不会超过NUM_BLOCKS的值，使用uint8_t类型即可；formerLength等变量根据其计算逻辑，不会超出uint32_t的范围，使用uint32_t类型即可。

```cpp
// Tiling结构体定义
BEGIN_TILING_DATA_DEF(TilingDataUnalign)
    TILING_DATA_FIELD_DEF(uint64_t, numBlocks);
    TILING_DATA_FIELD_DEF(uint64_t, formerNum);
    TILING_DATA_FIELD_DEF(uint64_t, tailNum);
    TILING_DATA_FIELD_DEF(uint64_t, formerLength);
    TILING_DATA_FIELD_DEF(uint64_t, tailLength);
    TILING_DATA_FIELD_DEF(uint64_t, alignNum);
END_TILING_DATA_DEF;
// Host侧Tiling函数计算Tiling结构信息
constexpr uint32_t NUM_BLOCKS = 8;
constexpr uint32_t SIZE_OF_HALF = 2;
constexpr uint32_t BLOCK_SIZE = 32;
constexpr uint32_t ALIGN_NUM = BLOCK_SIZE / SIZE_OF_HALF;
static ge::graphStatus TilingFunc(gert::TilingContext *context)
{
    TilingDataUnalign tiling;
    uint32_t totalLength = context->GetInputTensor(0)->GetShapeSize();
    // NumBlocks信息已经通过SetBlockDim接口进行设置
    context->SetBlockDim(NUM_BLOCKS);
    uint32_t totalLengthAligned = ((totalLength + ALIGN_NUM - 1) / ALIGN_NUM) * ALIGN_NUM;
    // formerNum、tailNum保证不超过0-NUM_BLOCKS数据范围
    uint32_t formerNum = (totalLengthAligned / ALIGN_NUM) % NUM_BLOCKS;
    uint32_t tailNum = NUM_BLOCKS - formerNum;
    // formerLength等变量根据其计算逻辑，不会超出uint32_t的范围
    uint32_t formerLength = ((totalLengthAligned / NUM_BLOCKS + ALIGN_NUM - 1) / ALIGN_NUM) * ALIGN_NUM;
    uint32_t tailLength = (totalLengthAligned / NUM_BLOCKS / ALIGN_NUM) * ALIGN_NUM;
}
```

# 【正例】

Tiling变量无冗余，变量数据类型最小化。

```c
BEGIN_TILING_DATA_DEF(TilingDataUnalign)
TILING_DATA_FIELD_DEF(uint8_t, formerNum);
TILING_DATA_FIELD_DEF(uint8_t, tailNum);
TILING_DATA_FIELD_DEF(uint32_t, formerLength); 
```

```c
TILING_DATA_FIELD_DEF(uint32_t, tailLength);
TILING_DATA_FIELD_DEF(uint32_t, alignNum);
END_TILING_DATA_DEF; 
```

# 【反例】

如下的示例中TilingData结构不合理：由于AI处理器访存需要8字节对齐，在用户定义TilingData结构后，Ascend C工程框架会按照8字节对齐的方式对字节进行补齐，并保证整体TilingData结构满足8字节对齐要求。如下TilingData结构formerNum和tailNum变量都会补充3个字节，整体TilingData结构会因为8字节对齐再补充4个字节，该TilingData结构共计补充10个字节。

```c
BEGIN_TILING_DATA_DEF(TilingDataUnalign)
TILING_DATA_FIELD_DEF(uint8_t, formerNum); // 需补充3个字节，使得formerLength变量访问无误
TILING_DATA_FIELD_DEF(uint32_t, formerLength);
TILING_DATA_FIELD_DEF(uint8_t, tailNum); // 需补充3个字节，使得tailLength变量访问无误
TILING_DATA_FIELD_DEF(uint32_t, tailLength);
TILING_DATA_FIELD_DEF(uint32_t, alignNum); // 需补充4个字节，使得下个TilingData结构访问无误
END_TILING_DATA_DEF;
```

# 【正例】

如下的示例中，对Tiling参数的排布进行了调整，字节排布合理，只需要补充2个字节，达到了减小TilingData结构的目的。

```c
BEGIN_TILING_DATA_DEF(TilingDataUnalign)
TILING_DATA_FIELD_DEF(uint8_t, formerNum);
TILING_DATA_FIELD_DEF(uint8_t, tailNum); // 需补充2个字节，使得formerLength变量访问无误
TILING_DATA_FIELD_DEF(uint32_t, formerLength);
TILING_DATA_FIELD_DEF(uint32_t, tailLength);
TILING_DATA_FIELD_DEF(uint32_t, alignNum);
END_TILING_DATA_DEF;
```

# 3.8.3.3 避免 TPipe 在对象内创建和初始化

# 【优先级】中

【编译器背景知识】创建类对象时，会分配内存空间，用于存储类中的相关成员变量或函数。当类中变量需要参与计算时，变量值从内存被加载到寄存器，计算完成后，变量从寄存器存储回内存。Scalar常量折叠和常量传播是编译器编译时的优化方式，优化前编译器会判断变量是否只初始化过一次或只赋值过一次，若满足此编译优化的前提条件，变量值将会尽量驻留在寄存器中，从而在后续使用变量时，将减少读取内存的操作，提升运行性能。

【描述】TPipe是用来管理全局内存和同步的框架，用户可以调用TPipe的接口，为TQue/TBuf进行内存分配。在编写Ascend C算子过程中，经常用一个类存放计算所需的相关变量，这里称类名为KernelExample。当TPipe对象在KernelExample类的实现中定义并初始化时，TPipe对象的内存空间在整个KernelExample对象的内存空间之中；需要注意的是，创建TPipe对象时，对象初始化会设置全局变量的TPipe指针，这导致KernelExample对象的内存有被外部污染的风险，此时编译器的编译优化将采取保守策略，不会对KernelExample对象中的Scalar变量进行常量折叠和常量传播。因此，在任何场景下，我们都建议将TPipe对象创建于KernelExample类外部，使得TPipe对象的内存空间独立于KernelExample类对象的内存空间，触发编译器对KernelExample类内Scalar的编译优化，减少算子Scalar指令耗时。

# 【反例】

代码中TPipe对象由KernelExample类内部创建并初始化，影响编译器Scalar折叠优化，在NPU侧导致Scalar不必要的增加。

```txt
template <typename ComputeT> class KernelExample {
public: 
```

```txt
__aicore__ inline KernelExample() {}
__aicore__ inline void Init(...)
{
    ...
    pipe.InitBuffer(xxxBuf, BUFFER_NUM, xxxSize);
    ...
}
private:
    ...
    TPipe pipe;
    ...
};
extern "C" __global__ __aicore__ void example_kernel(...) {
    ...
    KernelExample<float> op;
    op.Init(...);
    ...
} 
```

# 【正例】

改为由Kernel入口函数创建TPipe对象，在KernelExample类中保存TPipe指针使用。

```txt
template <typename ComputeT> class KernelExample {
public:
    __aicore__ inline KernelExample() {}

    __aicore__ inline void Init(..., TPipe* pipeIn)
    {
    ...
    pipe = pipeIn;
    pipe->InitBuffer(xxxBuf, BUFFER_NUM, xxxSize);
    ...
    }

private:
    ...
    TPipe* pipe;
    ...
};
extern "C" __global__ __aicore__ void example_kernel(...) {
    ...
    TPipe pipe;
    KernelExample<float> op;
    op.Init(..., &pipe);
    ...
} 
```

# 【性能对比】


图 3-91 aiv_scalar_time 优化前后对比


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/5c287e72a7e59d6ab1b34fdcce2e423c8ae41d3a7460b72c858a0b3a679686f2.jpg)



图 3-92 aiv_scalar_ratio 优化前后对比


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/22709a2a4cfbd1c817dfd18aefab97ef0ce14abe1329eda01e82e01a4aaaa44f.jpg)


通过性能数据对比可以看出，Scalar优化效果显著，平均时间从281us减少到236us，下降17%；平均scalar_time时延占比从21%下降到17%。因此在Scalar bound（达到上限）的场景下可以使用此优化措施。

# 3.8.3.4 核函数内删除 Workspace 相关冗余操作

【优先级】中

【描述】在Ascend C算子工程中，编写核函数时传入的参数workspace已经直接赋值为用户Workspace，因此无需再通过SetSysWorkspace和GetUserWorkspace来设置和获取Workspace。减少这些冗余判断后，编译器可以在不使用该参数的情况下进一步优化未用到的workspace变量。

【反例】

fast_gelu函数的参数workspace等价于用户workspace，且不为空，仍然对workspace进行判空，并且设置SetSysWorkspace和GetUserWorkspace来获取用户Workspace。

```cpp
template <uint64_t schMode, uint64_t dType>
__global__ __aicore__ void fast_gelu(GM_ADDR x, GM_ADDR y, GM_ADDR workspace, GM_ADDR tiling)
{
    // 反例，冗余判断
    if (workspace == nullptr) {
    return;
    }
    SetSysWorkspace(workspace);
    GM_ADDR userWS = GetUserWorkspace(workspace);
    if (userWS == nullptr) {
    return;
    }
    REGISTER_TILING_DEFAULT(EleBaseTilingDataV2);
    GET_TILING_DATA_WITH_STRUCT(EleBaseTilingDataV2, tilingData, tiling);
    KERNEL_TASK_TYPE_DEFAULT(KERN_TYPE_AIV_ONLY);
    TPipe pipe;
    if constexpr (dType == static_cast<uint64_t>(TPL_FP16)) {
    ElementwiseSch<schMode, FastGeluDag::FastGeluNeedCast<half>::OpDag> sch(&tilingData, &pipe);
    sch.Init(x, y);
    sch.Process();
    } else if constexpr (dType == static_cast<uint64_t>(TPL_BF16)) {
    ElementwiseSch<schMode, FastGeluDag::FastGeluNeedCast<bfloat16_t>::OpDag> sch(&tilingData, &pipe);
    sch.Init(x, y);
    sch.Process();
    } else if constexpr (dType == static_cast<uint64_t>(TPL_FP32)) {
    ElementwiseSch<schMode, FastGeluDag::FastGeluNoCast<float>::OpDag> sch(&tilingData, &pipe);
    sch.Init(x, y);
    sch.Process();
    }
}
```

# 【正例】

fast_gelu函数中删除对workspace参数进行空指针判断，也无需设置SetSysWorkspace和通过GetUserWorkspace来获取Workspace。

```cpp
template <uint64_t schMode, uint64_t dType>
__global__ __aicore__ void fast_gelu(GM_ADDR x, GM_ADDR y, GM_ADDR workspace, GM_ADDR tiling)
{
    REGISTER_TILING_DEFAULT(EleBaseTilingDataV2);
    GET_TILING_DATA_WITH_STRUCT(EleBaseTilingDataV2, tilingData, tiling);
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    TPipe pipe;
    if constexpr (dType == static_cast<uint64_t>(TPL_FP16)) {
    ElementwiseSch<schMode, FastGeluDag::FastGeluNeedCast<half>::OpDag> sch(&tilingData, &pipe);
    sch.Init(x, y);
    sch.Process();
    } else if constexpr (dType == static_cast<uint64_t>(TPL_BF16)) {
    ElementwiseSch<schMode, FastGeluDag::FastGeluNeedCast<bfloat16_t>::OpDag> sch(&tilingData, &pipe);
    sch.Init(x, y);
    sch.Process();
    } else if constexpr (dType == static_cast<uint64_t>(TPL_FP32)) {
    ElementwiseSch<schMode, FastGeluDag::FastGeluNoCast<float>::OpDag> sch(&tilingData, &pipe);
    sch.Init(x, y);
    sch.Process();
    }
} 
```

# 3.8.3.5 设置DCI编译选项来减少算子尾开销

# 说明

该性能优化建议适用于如下型号：

● Atlas 350 加速卡

【优先级】高

【描述】算子执行结束时，需要将DCache置为无效，防止后续算子继续使用DCache中的数据而受到影响。可以通过在编译选项中添加--cce-no-dcache-flush=true，用于在算子尾部增加DCI（DataCacheInvalid）指令来使DCache失效。如果不开启该选项，则会默认增加DCCI（DataCacheCleanAndInvalid）指令来使DCache失效。

插入DCI指令相比于插入DCCI指令，其减少了数据从DCache同步到GM（Clean）的过程，性能上会有一定优势。插入DCCI是一种额外的容错保证，如果开发者使用了*_gm__的方式改写GM内存，或者调用GlobalTensor.SetValue函数时，没有正确的调用DataCacheCleanAndInvalid接口来保证Cache一致性，编译框架自动插入DCCI恰好可以保证算子精度正常。

所以在如下场景，可以通过开启该编译选项来降低算子尾部开销：

算子使用* __gm__的方式改写GM内存，或者调用GlobalTensor.SetValue函数时，正确的使用DataCacheCleanAndInvalid接口，手动将数据从DCache中回刷到GM上，保证Cache的一致性。不依赖编译框架自动插入DCCI指令来保证一致性。

算子不包含使用* __gm__的方式改写GM内存，或者调用GlobalTensor.SetValue函数的代码。

# 3.8.4 流水编排

# 3.8.4.1 使能 DoubleBuffer

【优先级】中

【描述】执行于AI Core上的指令队列主要包括如下几类，Vector指令队列（V）、Cube指令队列（M）、Scalar指令队列（S）和搬运指令队列（MTE1/MTE2/MTE3）。不同指令队列间的相互独立性和可并行执行特性，是DoubleBuffer优化机制的基石。

以纯Vector计算为例，矢量计算前后的CopyIn、CopyOut过程使用搬运指令队列（MTE2/MTE3），Compute过程使用Vector指令队列（V），不同指令队列可并行执行，意味着CopyIn、CopyOut过程和Compute过程是可以并行的。如图3-93所示，考虑一个完整的数据搬运和计算过程，CopyIn过程将数据从Global Memory搬运到LocalMemory，Vector计算单元完成Compute计算后，经过CopyOut过程将计算结果搬回Global Memory。


图 3-93 数据搬运与 Vector 计算过程


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/be388e3d1c056c20c1a0557e603a378f372e6e90cdc6d24cb2185ac6a281dc79.jpg)



图 3-94 未使能 DoubleBuffer 的流水图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/b4ff6d460cc477a0598be64b3cf5d77f0ef3544a6043f13fcd46583371687173.jpg)


在此过程中，数据搬运与Vector计算串行执行，Vector计算单元不可避免存在资源闲置问题，假设CopyIn、Compute、CopyOut三阶段分别耗时相同均为t，则Vector的利用率仅为1/3，等待时间过长，Vector利用率严重不足。

为减少Vector等待时间，使能DoubleBuffer机制将待处理的数据一分为二，例如Tensor1、Tensor2。如图3-95所示，当Vector单元对Tensor1中数据进行Compute计算时，Tensor2数据流可以执行CopyIn的过程；而当Vector切换到计算Tensor2时，Tensor1数据流可以执行CopyOut的过程。由此，数据的进出搬运和Vector计算实现并行执行，Vector闲置问题得以有效缓解。

总体来说，DoubleBuffer是基于MTE指令队列与Vector指令队列的独立性和可并行性，通过将数据搬运与Vector计算并行执行以隐藏大部分的数据搬运时间，并降低Vector指令的等待时间，最终提高Vector单元的利用效率。通过为队列申请内存时设置内存块的个数为2，使能DoubleBuffer，实现数据并行，简单代码示例如下：

pipe.InitBuffer(inQueueX, 2, 256); 


图 3-95 DoubleBuffer 机制


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/a9492af334a166466ae71250feca02209728038de44d6a7004ea43ab7a3dab1e.jpg)



图 3-96 使能 DoubleBuffer 的流水图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/683651642c67d4dd1376d5c412b704146550df7b6d111c406535ba48e6cffcba.jpg)


# 需要注意：

多数情况下，采用DoubleBuffer能有效提升Vector的利用率，缩减算子执行时间。然而，DoubleBuffer机制缓解Vector闲置问题，并不代表它总能带来明显的整体性能提升。例如：

当数据搬运时间较短，而Vector计算时间较长时，由于数据搬运在整个计算过程中的时间占比较低，DoubleBuffer机制带来的性能收益会偏小。

当原始数据较小且Vector可一次性完成所有数据量的计算时，强行使用DoubleBuffer会降低Vector计算资源的利用率，最终效果可能适得其反。

因此，DoubleBuffer的使用需综合考虑Vector算力、数据量大小、搬运与计算时间占比等多种因素。

# 【反例】

```txt
__aicore__ inline void Init(__gm__ uint8_t* src0Gm, __gm__ uint8_t* src1Gm, __gm__ uint8_t* dstGm)
{
    src0Global.SetGlobalBuffer((__gm__ half*)src0Gm);
    src1Global.SetGlobalBuffer((__gm__ half*)src1Gm);
    dstGlobal.SetGlobalBuffer((__gm__ half*)dstGm);
    // 不使能DoubleBuffer,占用的物理空间是1 * sizeSrc0 * sizeof(half)
    // 3个InitBuffer执行后总空间为1 * (sizeSrc0 * sizeof(half) + sizeSrc1 * sizeof(half) + sizeDst0 * sizeof(half))
    pipe.InitBuffer(inQueueSrc0, 1, sizeSrc0 * sizeof(half));
    pipe.InitBuffer(inQueueSrc1, 1, sizeSrc1 * sizeof(half));
    pipe.InitBuffer(outQueueDst, 1, sizeDst0 * sizeof(half));
    }
__aicore__ inline void Process()
{
    // 需要round*2次循环才能处理完数据
    for (uint32_t index = 0; index < round * 2; ++index) {
    CopyIn(index);
    Compute();
    CopyOut(index);
    }
}
```

# 【正例】

```txt
__aicore__ inline void Init(__gm__ uint8_t* src0Gm, __gm__ uint8_t* src1Gm, __gm__ uint8_t* dstGm)
{
    src0Global.SetGlobalBuffer((__gm__ half*)src0Gm);
    src1Global.SetGlobalBuffer((__gm__ half*)src1Gm);
    dstGlobal.SetGlobalBuffer((__gm__ half*)dstGm);
    // InitBuffer中使用2表示使能DoubleBuffer,占用的物理空间是2 * sizeSrc0 * sizeof(half)
    // 3个InitBuffer执行后总空间为2 * (sizeSrc0 * sizeof(half) + sizeSrc1 * sizeof(half) + sizeDst0 * sizeof(half))
    pipe.InitBuffer(inQueueSrc0, 2, sizeSrc0 * sizeof(half));
    pipe.InitBuffer(inQueueSrc1, 2, sizeSrc1 * sizeof(half));
    pipe.InitBuffer(outQueueDst, 2, sizeDst0 * sizeof(half));
    }
__aicore__ inline void Process()
{
    // 开启DoubleBuffer的前提是循环次数 >= 2
    for (uint32_t index = 0; index < round; ++index) {
    CopyIn(index);
    Compute();
    CopyOut(index);
    }
}
```

# 3.8.4.2 使能 Iterate 或 IterateAll 异步接口避免 AIC/AIV 同步依赖

【优先级】高

【描述】在MIX场景，即AIC（AI Cube核）和AIV（AI Vector核）混合编程中，调用Matmul Iterate或者IterateAll时，AIV发送消息到AIC启动Matmul计算。若通过Iterate<true>同步方式，如图1 同步方式消息发送示意图，每次调用都会触发一次消息发送，而通过Iterate<false>异步方式，如图2 异步方式消息发送示意图，仅第一次需要发送消息，后续无需发送消息，从而减少Cube与Vector核间交互，减少核间通信开销。因此，MIX场景推荐使用Iterate<false>或者IterateAll<false>异步接口（注意：使用异步接口时需要设置Workspace）。


图3-97 同步方式消息发送示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/3dd7eddea7efc0235c4fe27b00c726de85b081e4b74a8945165035dfec983d6e.jpg)



图3-98 异步方式消息发送示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/9e8e0d702406561b2d8ed0fd063252ea14c461846a896a2474133b141a6e6e0b.jpg)


【反例】

MIX场景使用Iterate接口的同步方式。

```cpp
TQueBind<TPosition::CO2, TPosition::VECIN> qVecIn; TQueBind<TPosition::VECIN, TPosition::VECOUT> qVecOut; mm.SetTensorA(gmA); 
```

```txt
mm.SetTensorB(gmB);
int16_t scalar = 2;

while(mm.template Iterate()){
    auto cInUB = qVecIn.AllocTensor<float>();
    mm.GetTensorC(cInUB);
    qVecIn.EnQue(cInUB);
    cInUB = qVecIn.DeQue<float>();
    auto cOutUB = qVecOut.AllocTensor<float>();
    Muls(cOutUB, cInUB, scalar, baseM*baseN);
    qVecIn.FreeTensor(cInUB);
    ...
} 
```

# 【正例】


MIX场景使用Iterate接口的异步方式。


```cpp
TQueBind<TPosition::CO2, TPosition::VECIN> qVecIn;
TQueBind<TPosition::VECIN, TPosition::VECOUT> qVecOut;
mm.SetTensorA(gmA);
mm.SetTensorB(gmB);
mm.SetWorkspace(workspace, size); // 其中，workspace为临时空间的物理地址，size为
singleCoreM*singleCoreN大小的矩阵C占用的内存大小：singleCoreM*singleCoreN*sizeof(float)
int16_t scalar = 2;

while(mm.template Iterate<false>()){
    auto cInUB = qVecIn.AllocTensor<float>();
    mm.GetTensorC(cInUB);
    qVecIn.EnQue(cInUB);
    cInUB = qVecIn.DeQue<float>();
    auto cOutUB = qVecOut.AllocTensor<float>();
    Muls(cOutUB, cInUB, scalar, baseM*baseN);
    qVecIn.FreeTensor(cInUB);
    ...
}
```

# 3.8.5 内存访问

# 3.8.5.1 尽量一次搬运较大的数据块

# 【优先级】高

【描述】搬运不同大小的数据块时，对带宽的利用率（有效带宽/理论带宽）不一样。根据实测经验，单次搬运数据长度16KB以上时，通常能较好地发挥出带宽的最佳性能。因此对于单次搬运，应考虑尽可能的搬运较大的数据块。下图展示了某款AI处理器上实测的不同搬运数据量下带宽的变化图。

# 说明

测试数据与处理器型号相关，且实际测试时可能会存在略微抖动，具体带宽数值并不一定和下文的测试数据严格一致。


图 3-99 UB->GM 方向不同单次搬运数据量下实际占用带宽的变化


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/78a4e8b3aa16ee1914bf67509ea5e2d4fc37e65cd55829524f19cc157e106791.jpg)



图3-100 GM->UB 方向不同单次搬运数据量下实际占用带宽的变化


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/01d3429cbc1a5567d72ccecd7dee90aaa5d76b4d8e4c817baea6120a70c2df5d.jpg)


# 3.8.5.2 GM 地址尽量 512B 对齐

# 【优先级】高

【描述】由于AI处理器内部设计约束，从GM向Local Memory搬运数据时，保证GM地址512B对齐可以最有效的发挥出带宽的效率。如下图示例，展示了在512B对齐以及32B对齐情况下单核的带宽效率：搬运同等数据量，带宽差距最大的情况，32B对齐场景只能达到512B对齐场景的70%。

# 说明

● 本性能优化手段仅针对Atlas A2 训练系列产品/Atlas A2 推理系列产品生效。

● 测试数据与处理器型号相关，且实际测试时可能会存在略微抖动，具体带宽数值并不一定和下文的测试数据严格一致。


图 3-101 GM->UB 方向 512B 对齐和 32B 对齐实测带宽的差异对比


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/5b8cfcbfb995aa7d05599f8756b8a39a047b128dfae7982b32d10f3458fe6cf3.jpg)



图 3-102 UB->GM 方向 512B 对齐和 32B 对齐实测带宽的差异对比


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/007affac172c3e82b184eea0fa3bd8569dce94c0f72609af6ad07bd671f28368.jpg)


# 3.8.5.3 高效的使用搬运 API

# 【优先级】高

【描述】在使用搬运API时，应该尽可能地通过配置搬运控制参数实现连续搬运或者固定间隔搬运，避免使用for循环，二者效率差距极大。如下图示例，图片的每一行为16KB，需要从每一行中搬运前2KB，针对这种场景，使用for循环遍历每行，每次仅能搬运2KB。若直接配置DataCopyParams参数（包含srcStride/dstStride/blockLen/blockCount），则可以达到一次搬完的效果，每次搬运32KB；参考3.8.5.1 尽量一次搬运较大的数据块章节介绍的搬运数据量和实际带宽的关系，建议一次搬完。


图 3-103 待搬运数据排布


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/b7dc00ee3e85a92c191302e443d03c3e4a2fdaf53b86fda1db3d5a8fcc0664f7.jpg)


# 【反例】

```txt
// 搬运数据存在间隔，从GM上每行16KB中搬运2KB数据, 共16行
LocalTensor<float> tensorIn;
GlobalTensor<float> tensorGM;
...
constexpr int32_t copyWidth = 2 * 1024 / sizeof(float);
constexpr int32_t imgWidth = 16 * 1024 / sizeof(float);
constexpr int32_t imgHeight = 16;
// 使用for循环，每次只能搬运2K，重复16次
for (int i = 0; i < imgHeight; i++) {
    DataCopy(tensorIn[i * copyWidth], tensorGM[i * imgWidth], copyWidth);
}
```

# 【正例】

```txt
LocalTensor<float> tensorIn;
GlobalTensor<float> tensorGM;
...
constexpr int32_t copyWidth = 2 * 1024 / sizeof(float);
constexpr int32_t imgWidth = 16 * 1024 / sizeof(float);
constexpr int32_t imgHeight = 16;
// 通过DataCopy包含DataCopyParams的接口一次搬完
DataCopyParams copyParams;
copyParams.blockCount = imgHeight;
copyParams.blockLen = copyWidth / 8; // 搬运的单位为DataBlock(32Byte)，每个DataBlock内有8个float
copyParams.srcStride = (imgWidth - copyWidth) / 8; // 表示两次搬运src之间的间隔，单位为DataBlock
copyParams.dstStride = 0; // 连续写，两次搬运之间dst的间隔为0，单位为DataBlock
DataCopy(tensorGM, tensorIn, copyParams);
```

# 3.8.5.4 非对齐场景减少无效数据的搬运

【优先级】中

# 说明

该性能优化建议适用于如下型号：

● Atlas 350 加速卡

【描述】在非对齐数据搬运场景中，Atlas 350 加速卡在基础API层面提供了

DataCopyPad接口，该接口支持Normal、Compact（紧凑）两种搬运模式。搬运多块非32B对齐数据块的场景下，使用Compact模式在可以减少搬运的无效数据量，节省带宽。

假设需要搬运三个数据块，每块数据块大小为48B，数据类型为float类型。除了这三个48字节的数据块之外，其他所有数据均为无效数据。


【反例】使用DataCopyPad接口进行Normal模式搬运数据


```cpp
__aicore__ inline void CopyIn(){
    AscendC::LocalTensor<T> xLocal = inQueueX.AllocTensor<T>();
    AscendC::Duplicate<T>(xLocal, 0, count);
    AscendC::DataCopyParams dataCopyParams;
    dataCopyParams.blockCount = 3;
    dataCopyParams.blockLen = 48;
    dataCopyParams.srcStride = 0;
    dataCopyParams.dstStride = 0;
    AscendC::DataCopyPadParams dataCopyPadParams;
    dataCopyPadParams.isPad = 1;
    dataCopyPadParams.leftPadding = 0;
    dataCopyPadParams.rightPadding = 4;
    dataCopyPadParams.paddingValue = 0;
    AscendC::DataCopyPad<T, AscendC::PaddingMode::Normal>(xLocal, xGm, dataCopyParams, dataCopyPadParams);
    inQueueX.EnQue<T>(xLocal);
} 
```

# 搬运后UB内数据如下：

$[1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 0., 0., 0., 0., 1., 1., 1., 1., 1., 1., 1., 1., 0., 0., 0., 0., 0., 1., 1., 1., 1., 1., 1., 1., 1., 1., 0., 0., 0., 0.....]$ 


图 3-104 Normal 模式搬运


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/9d6bc1b10fe1f0730abe447850cbb8225277d1ad88d67e66ab702bc7e65d0e5d.jpg)


如图所示，由于每块数据块为48B，非32B对齐，因此搬运每块数据块时需要插入16B大小的padding数据使得数据32B对齐，最终搬运192B大小的数据到UB，其中包含48B的无效数据。


【正例】改用Compact模式搬运进行优化


```cpp
__aicore__ inline void CopyIn() {
    AscendC::LocalTensor<T> xLocal = inQueueX.AllocTensor<T>();
    AscendC::Duplicate<T>(xLocal, 0, count);
    AscendC::DataCopyParams dataCopyParams;
    dataCopyParams.blockCount = 3;
    dataCopyParams.blockLen = 48;
    dataCopyParams.srcStride = 0;
    dataCopyParams.dstStride = 0;
    AscendC::DataCopyPadParams dataCopyPadParams;
    dataCopyPadParams.isPad = 1;
    dataCopyPadParams.leftPadding = 0;
    dataCopyPadParams.rightPadding = 4;
    dataCopyPadParams.paddingValue = 0; 
```

```rust
AscendC::DataCopyPad<T, AscendC::PaddingMode::Compact>(xLocal, xGm, dataCopyParams, dataCopyPadParams);
inQueueX.EnQue<T>(xLocal);
} 
```

搬运后UB内数据如下：

```txt
[1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 0. 
```


图 3-105 Compact 模式搬运


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/127fc023499c724f340e4ca62ccab1f45bab613db2f1fbab01fb2e349910eeca.jpg)


根据Compact模式搬运的示意图，最终搬运了160B大小的数据，其中包含16B的无效数据。

【总结】通过比较可以发现，搬运多块非32B对齐数据块的场景下，使用Compact模式在可以减少搬运的无效数据量，节省带宽。

# 3.8.5.5 非连续搬运场景减少搬运次数

【优先级】中

# 说明

该性能优化建议适用于如下产品型号：

● Atlas 350 加速卡

在非连续搬运场景可以使用DataCopyPad接口的Loop模式和DataCopy的多维数据搬运接口来减少搬运次数，优化搬运性能。

# 使用 Loop 模式减少非连续搬运的次数

【描述】DataCopyPad接口在Normal/Compact模式基础上，可以使用Loop模式搬运二维数据，假设我们希望以下图的方式搬运8个48B大小的数据块：

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/3a88b42b62caa8b6548babd242a94a2a19cb4d54cbc2a6cd3f515951e591f826.jpg)


【反例】调用多次搬运接口进行搬运（以DataCopyPad为例）

```cpp
__aicore__ inline void CopyIn3(){
    AscendC::LocalTensor<T> xLocal = inQueueX.AllocTensor<T>();
    AscendC::Duplicate<T>(xLocal, 0, count);
    AscendC::DataCopyParams dataCopyParams;
    dataCopyParams.blockCount = 2; 
```

```cpp
dataCopyParams.blockLen = 48;
dataCopyParams.srcStride = 0;
dataCopyParams.dstStride = 0;
AscendC::DataCopyPadParams dataCopyPadParams;
dataCopyPadParams.isPad = 0;
dataCopyPadParams.leftPadding = 0;
dataCopyPadParams.rightPadding = 0;
dataCopyPadParams.paddingValue = 0;
AscendC::DataCopyPad<T, AscendC::PaddingMode::Compact>(xLocal, xGm, dataCopyParams, dataCopyPadParams);
AscendC::DataCopyPad<T, AscendC::PaddingMode::Compact>(xLocal[32], xGm[24], dataCopyParams, dataCopyPadParams);
AscendC::DataCopyPad<T, AscendC::PaddingMode::Compact>(xLocal[72], xGm[48], dataCopyParams, dataCopyPadParams);
AscendC::DataCopyPad<T, AscendC::PaddingMode::Compact>(xLocal[104], xGm[72], dataCopyParams, dataCopyPadParams);
inQueueX.EnQue<T>(xLocal);
} 
```


图 3-106 使用多次 DataCopyPad 接口进行搬运


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/b85c8c91555b3201f11856649c741469af0dc9f8ba1aa597b95d7c066109652f.jpg)



【正例】使用Loop模式进行搬运


```cpp
__aicore__ inline void CopyIn3(){
    AscendC::LoopModeParams loopModeParams;
    loopModeParams.loop1Size = 2;
    loopModeParams.loop2Size = 2;
    loopModeParams.loop1SrcStride = 96;
    loopModeParams.loop1DstStride = 128;
    loopModeParams.loop2SrcStride = 192;
    loopModeParams.loop2DstStride = 288;
    AscendC::LocalTensor<T> xLocal = inQueueX.AllocTensor<T>();
    AscendC::Duplicate<T>(xLocal, 0, count);
    AscendC::DataCopyParams dataCopyParams;
    dataCopyParams.blockCount = 2;
    dataCopyParams.blockLen = 48;
    dataCopyParams.srcStride = 0;
    dataCopyParams.dstStride = 0;
    AscendC::DataCopyPadParams dataCopyPadParams;
    dataCopyPadParams.isPad = 0;
    dataCopyPadParams.leftPadding = 0;
    dataCopyPadParams.rightPadding = 0;
    dataCopyPadParams.paddingValue = 0;
    AscendC::SetLoopModePara(loopModeParams, AscendC::DataCopyMVType::OUT_TO_UB);
    AscendC::DataCopyPad<T, AscendC::PaddingMode::Compact>(xLocal, xGm, dataCopyParams, dataCopyPadParams);
    AscendC::ResetLoopModePara(AscendC::DataCopyMVType::OUT_TO_UB);
    inQueueX.EnQue<T>(xLocal);
} 
```


图 3-107 使用 Loop 模式进行搬运


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/b57d866f84f2d8f3c4de42f853d265748b8d2a2d690fb2b361a79bfb99dd15ce.jpg)


【总结】当数据块之间需要插入不同大小Padding时，使用Loop模式搬运代替多次的DataCopyPad能够减少搬运指令的使用，提升性能。

# 使用多维数据搬运减少非连续搬运次数

【描述】假设我们希望以下图的方式搬运2个8B大小的数据块：


图3-108 搬运前后数据


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/cb7fdf417254aa1c42a4a96dffb7b3d363a86eac114aab3de98b080a191fbf3b.jpg)


【反例】使用多次DataCopyPad进行搬运


图 3-109 使用多次 DataCopyPad 进行搬运


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/7d66f7ba6180b2e4cf405071745c9ae4f3dd9bfbeddfd405089b6aaef0af365b.jpg)


```cpp
__aicore__ inline void CopyIn5(){
    AscendC::LocalTensor<T> xLocal = inQueueX.AllocTensor<T>();
    AscendC::Duplicate<T>(xLocal, 0, count);
    AscendC::DataCopyParams dataCopyParams;
    dataCopyParams.blockCount = 1;
    dataCopyParams.blockLen = 8;
    dataCopyParams.srcStride = 0;
    dataCopyParams.dstStride = 0;
    AscendC::DataCopyPadParams dataCopyPadParams;
    dataCopyPadParams.isPad = 1;
    dataCopyPadParams.leftPadding = 5;
    dataCopyPadParams.rightPadding = 1;
    dataCopyPadParams.paddingValue = 0; 
```

```cpp
// 第一次搬运
AscendC::DataCopyPad<T, AscendC::PaddingMode::Normal>(xLocal, xGm, dataCopyParams, dataCopyPadParams);
dataCopyPadParams.isPad = 1;
dataCopyPadParams.leftPadding = 1;
dataCopyPadParams.rightPadding = 5;
dataCopyPadParams.paddingValue = 0;
// 第二次搬运
AscendC::DataCopyPad<T, AscendC::PaddingMode::Normal>(xLocal[8], xGm[2], dataCopyParams, dataCopyPadParams);
inQueueX.EnQue<T>(xLocal);
}
```

# 【正例】使用多维数据搬运

DataCopy接口在Atlas 350 加速卡上支持多维数据的搬运，具体可参考 多维数据搬运（ISASI）。以2D场景的搬运为例，代码如下：

```cpp
__aicore__ inline void CopyIn6() {
    AscendC::LocalTensor<T> xLocal = inQueueX.AllocTensor<T>(); 
    AscendC::Duplicate<T>(xLocal, 0, count);
    AscendC::NdDmaLoopInfo<2> loopInfo{{1, 2}, {1, 4}, {2, 2}, {1, 1}, {1, 1}};
    AscendC::NdDmaParams<T, 2> params = {loopInfo, 0};
    AscendC::NdDmaDci();
    static constexpr AscendC::NdDmaConfig config = {false};
    AscendC::DataCopy<T, 2, config>(xLocal, xGm, params);
    inQueueX.EnQue<T>(xLocal);
} 
```


图3-110 搬运前后数据


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/4f9fb63cf2cb74a50a5816dfaecb046a790542be9d981c73f759eb497670ba63.jpg)


【总结】使用多维数据搬运在部分场景下能够减少搬运指令的条数，从而提升性能。

# 3.8.5.6 避免同地址访问

【优先级】高

# 说明

该性能优化指导适用于如下产品型号：

● Atlas A3 训练系列产品/Atlas A3 推理系列产品

● Atlas A2 训练系列产品/Atlas A2 推理系列产品

【描述】MTE2、MTE3、Scalar等单元访问Global Memory数据时，其地址请求会按照512字节粒度对齐后进行处理。当同时访问Global Memory的数据，且地址处于连续的512字节范围内时，由于数据一致性的原因，多个请求会被串行处理，进而影响数据搬运效率。

当前算子执行机制保证用户Kernel入参（包括Workspace/Tiling）的地址512字节对齐，因此开发者只需要根据地址的偏移量即可判断两个地址是否会落入连续的512字节范围内。

如下图所示，AI Core内的各个核对Global Memory的数据同时发出读写请求，尽管addr0~addr5是多个不同的地址，但因为落在连续的512字节范围内，被视为同一个地址请求，此时这几个数据请求会被串行处理，数据访问效率会降低。同地址访问的影响受同时访问的核数影响，同地址访问的核数越多时，串行导致的性能劣化越严重。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/8b817a94e9c08d48819ee80c06f669ff208e1e901b1157cab726749ae40a7550.jpg)


避免同地址访问的方法主要有以下两种：调整数据访问顺序和修改切分策略。下文介绍配套的样例请参考避免同地址访问样例。

# 调整数据访问顺序

以一个形状为 (8192, 128) 的float类型输入进行Adds计算为例。

为了体现同地址冲突的影响，上述场景设计中每一行的数据大小为512字节（128个float），每个核每一轮计算处理512 * 8字节的数据，并进行全核同步（实际场景中并不需要），每一轮计算都需要等待所有核完成当前数据块的计算后，再进行下一轮。

<table><tr><td>实现方案</td><td>原始实现</td><td>优化实现</td></tr><tr><td>实现方法</td><td>使用16个核参与计算,按列方向进行切分,每个核总计算数据量为8192*8;单核执行循环16次,每次计算的数据量为512*8;每个核的循环顺序如下图所示,列方向0~15表示每个核的数据块执行顺序。由于多个核同时访问同一行数据(512字节),导致同地址冲突的发生。</td><td>使用16个核参与计算,按列方向进行切分,每个核总计算数据量为8192*8;单核执行循环16次,每次计算的数据量为512*8;每个核的循环顺序如下图所示,列方向0~15表示每个核的数据块执行顺序。由于每个核每一轮处理的地址在不同行,不会同时访问连续的512字节,所以不会导致同地址访问冲突。</td></tr></table>

<table><tr><td>实现方案</td><td colspan="9">原始实现</td><td colspan="13">优化实现</td><td colspan="4"></td><td></td><td></td></tr><tr><td rowspan="18">示意图</td><td></td><td>0核</td><td>1核</td><td>2核</td><td>3核</td><td>4核</td><td>5核</td><td>6核</td><td>7核</td><td>8核</td><td>9核</td><td>10核</td><td>11核</td><td>0核</td><td>1核</td><td>2核</td><td>3核</td><td>4核</td><td>5核</td><td>6核</td><td>7核</td><td>8核</td><td>9核</td><td>10核</td><td>11核</td><td></td><td></td><td></td></tr><tr><td>0x00000-0x03FFF</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td><td>8</td><td>9</td><td>10</td><td>11</td><td></td><td></td><td></td></tr><tr><td>0x04000-0x07FFF</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td><td>8</td><td>9</td><td>10</td><td>11</td><td>12</td><td></td><td></td><td></td></tr><tr><td>0x08000-0x0BFFF</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td><td>8</td><td>9</td><td>10</td><td>11</td><td>12</td><td>13</td><td></td><td></td><td></td></tr><tr><td>0x0C000-0x0FFFF</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td><td>8</td><td>9</td><td>10</td><td>11</td><td>12</td><td>13</td><td>14</td><td></td><td></td><td></td></tr><tr><td>0x10000-0x13FFF</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>5</td><td>6</td><td>7</td><td>8</td><td>9</td><td>10</td><td>11</td><td>12</td><td>13</td><td>14</td><td>15</td><td></td><td></td><td></td></tr><tr><td>0x14000-0x17FFF</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>6</td><td>7</td><td>8</td><td>9</td><td>10</td><td>11</td><td>12</td><td>13</td><td>14</td><td>15</td><td>0</td><td></td><td></td><td></td></tr><tr><td>0x18000-0x1BFFF</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>7</td><td>8</td><td>9</td><td>10</td><td>11</td><td>12</td><td>13</td><td>14</td><td>15</td><td>0</td><td>1</td><td></td><td></td><td></td></tr><tr><td>0x1C000-0x1FFFF</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>8</td><td>9</td><td>10</td><td>11</td><td>12</td><td>13</td><td>14</td><td>15</td><td>0</td><td>1</td><td>2</td><td></td><td></td><td></td></tr><tr><td>0x20000-0x23FFF</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>9</td><td>10</td><td>11</td><td>12</td><td>13</td><td>14</td><td>15</td><td>0</td><td>1</td><td>2</td><td>3</td><td></td><td></td><td></td></tr><tr><td>0x24000-0x27FFF</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>10</td><td>11</td><td>12</td><td>13</td><td>14</td><td>15</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td></td><td></td><td></td></tr><tr><td>0x28000-0x2BFFF</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>11</td><td>12</td><td>13</td><td>14</td><td>15</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td></td><td></td><td></td></tr><tr><td>0x2C000-0x2FFFF</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>12</td><td>13</td><td>14</td><td>15</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td></td><td></td><td></td></tr><tr><td>0x30000-0x33FFF</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>13</td><td>14</td><td>15</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td><td></td><td></td><td></td></tr><tr><td>0x34000-0x37FFF</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>14</td><td>15</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td><td>8</td><td></td><td></td><td></td></tr><tr><td>0x38000-0x3BFFF</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>15</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td><td>8</td><td>9</td><td></td><td></td><td></td></tr><tr><td>0x3C000-0x3FFFF</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td><td>8</td><td>9</td><td>10</td><td></td><td></td><td></td></tr><tr><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td></tr><tr><td>示例代码</td><td colspan="9">for (int32_t i = 0; i &lt; tiling-&gt;loopOneCore; i+ ) {AscendC::SyncAll();CopyIn(i);Compute();AscendC::SyncAll();CopyOut(i);}</td><td colspan="19">for (int32_t i = 0; i &lt; tiling-&gt;loopOneCore; i++) { int32_t newProgress = (i + AscendC::GetBlockIdx()) % tiling-&gt;loopOneCore;AscendC::SyncAll();CopyIn(newProgress);Compute();AscendC::SyncAll();CopyOut(newProgress);}</td></tr></table>

# 修改切分策略

仍以一个形状为 (8192, 128) 的float类型输入进行Adds计算为例。

为了体现同地址冲突的影响，上述场景设计中每一行的数据大小为512字节（128个float），每个核每一轮计算处理512 * 8字节的数据，并进行全核同步（实际场景中并不需要），每一轮计算都需要等待所有核完成当前数据块的计算后，再进行下一轮。

<table><tr><td>实现方案</td><td colspan="9">原始实现</td><td colspan="11">优化实现</td><td></td><td></td></tr><tr><td>实现方法</td><td colspan="9">使用16个核参与计算,按列方向进行切分,每个核总计算数据量为8192*8;单核执行循环16次,每次计算的数据量为512*8;每个核的循环顺序如下图所示,列方向0~15表示每个核的数据块执行顺序。由于多个核同时访问同一行数据(512字节),导致同地址冲突的发生。</td><td colspan="13">使用16个核参与计算,按行方向进行切分,每个核总计算数据量为512*128;单核执行循环16次,每次计算的数据量为512*8;每个核的循环顺序如下图所示(行方向),均为从块0~块15。由于每个核每一轮处理的地址在不同行,不会同时访问连续的512字节,所以不会导致同地址访问冲突。</td></tr><tr><td rowspan="18">示意图</td><td></td><td>0核</td><td>1核</td><td>2核</td><td>3核</td><td>4核</td><td>5核</td><td>6核</td><td>7核</td><td>8核</td><td>9核</td><td>10核</td><td>11核</td><td>12核</td><td>13核</td><td>14核</td><td>15核</td><td>16核</td><td>17核</td><td>18核</td><td></td><td></td></tr><tr><td>0x00000-0x03FFF</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x04000-0x07FFF</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x08000-0x0BFFF</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>2</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x0C000-0x0FFFF</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>3</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x10000-0x13FFF</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>4</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x14000-0x17FFF</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>5</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x18000-0x1BFFF</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>6</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x1C000-0x1FFFF</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>7</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x20000-0x23FFF</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>8</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x24000-0x27FFF</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>9</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x28000-0x2BFFF</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>10</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x2C000-0x2FFFF</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>11</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x30000-0x33FFF</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>12</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x34000-0x37FFF</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>13</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x38000-0x3BFFF</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>14</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td>0x3C000-0x3FFFF</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>15</td><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td></tr><tr><td></td><td colspan="8"></td><td colspan="12"></td><td></td></tr></table>

<table><tr><td>实现方案</td><td>原始实现</td><td>优化实现</td></tr><tr><td>示例代码</td><td>__aicore__ inline void Init(GM_ADDR x, GM_ADDR z, AddsCustomTilingData* tilingPtr) {tiling = tilingPtr;xGm.SetGlobalBuffer(__gm__ float *)x + AscendC::GetBlockIdx() * tiling-&gt;tileN);zGm.SetGlobalBuffer(__gm__ float *)z + AscendC::GetBlockIdx() * tiling-&gt;tileN);// we disable the L2 cache mode to highlight the influence of the gm address conflictxGm.SetL2CacheHint(AscendC::CacheMode::CACHE_MODE_DISABLE);zGm.SetL2CacheHint(AscendC::CacheMode::CACHE_MODE_DISABLE);pipe.InitBuffer(inQueueX, BUFFER_NUM, tiling-&gt;tileM * tiling-&gt;tileN * sizeof(float));pipe.InitBuffer(outQueueZ, BUFFER_NUM, tiling-&gt;tileM * tiling-&gt;tileN * sizeof(float));}</td><td>__aicore__ inline void Init(GM_ADDR x, GM_ADDR z, AddsCustomTilingData* tilingPtr) {tiling = tilingPtr;// change the tile method from column split to row splitxGm.SetGlobalBuffer(__gm__ float *)x + AscendC::GetBlockIdx() * tiling-&gt;tileM * tiling-&gt;n);zGm.SetGlobalBuffer(__gm__ float *)z + AscendC::GetBlockIdx() * tiling-&gt;tileM * tiling-&gt;n);// we disable the L2 cache mode to highlight the influence of the gm address conflictxGm.SetL2CacheHint(AscendC::CacheMode::CACHE_MODE_DISABLE);zGm.SetL2CacheHint(AscendC::CacheMode::CACHE_MODE_DISABLE);pipe.InitBuffer(inQueueX, BUFFER_NUM, tiling-&gt;tileM * tiling-&gt;tileN * sizeof(float));pipe.InitBuffer(outQueueZ, BUFFER_NUM, tiling-&gt;tileM * tilling-&gt;tileN * sizeof(float));}</td></tr></table>

# 说明

你可以通过执行如下命令行，通过算子调优（msProf）工具获取上述示例的性能数据并进行对比。

```batch
msprof op --launch-count=3 --output=./prof ./execute_adds_op 
```

重点关注PipeUtilization.csv中的aiv_mte2_time(us)和aiv_mte3_time(us)搬运指令耗时。

# 3.8.5.7 设置合理的 L2 CacheMode

【优先级】高

# 说明

该性能优化指导适用于如下产品型号：

● Atlas A3 训练系列产品/Atlas A3 推理系列产品

● Atlas A2 训练系列产品/Atlas A2 推理系列产品

【描述】L2 Cache常用于缓存频繁访问的数据，其物理位置如下图所示：

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/624e6951d932c6a6b4d391cf35d574771373de7e5c271a3325b9fea7ae6afc8c.jpg)



L2 Cache的带宽相比GM的带宽有数倍的提升，因此当数据命中L2 Cache时，数据的搬运耗时会优化数倍。通常情况下，L2 Cache命中率越高，算子的性能越好，在实际访问中需要通过设置合理的L2 CacheMode来保证重复读取的数据尽量缓存在L2Cache上。


# L2 Cache 访问的原理及 CacheMode 介绍

数据通过MTE2搬运单元搬入时，L2 Cache访问的典型流程如下：

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/fe31750c91908bf062c7e1ac97b804f93b486ac4ae426b5efa6448d1e4fa1b07.jpg)



数据通过MTE3或者Fixpipe搬运单元搬出时，L2 Cache访问的典型流程如下：


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/cc22d3046127d7f773238dbe8e5d13a1a2751d42ab1c9e2b0a3089165e620e1a.jpg)


从上面的流程可以看出，当数据访问总量超出L2 Cache容量时，AI Core会对L2 Cache进行数据替换。由于Cache一致性的要求，替换过程中旧数据需要先写回GM（此过程中会占用GM带宽），旧数据写回后，新的数据才能进入L2 Cache。

开发者可以针对访问的数据设置其CacheMode，对于只访问一次的Global Memory数据设置其访问状态为不进入L2 Cache，这样可以更加高效的利用L2 Cache缓存需要重复读取的数据，避免一次性访问的数据替换有效数据。

# 设置 L2 CacheMode 的方法

Ascend C基于GlobalTensor提供了SetL2CacheHint接口，用户可以根据需要指定CacheMode。

考虑如下场景，构造两个Tensor的计算，x的输入Shape为(5120, 5120)，y的输入Shape为(5120, 15360)，z的输出Shape为(5120, 15360)，由于两个Tensor的Shape不相等，x分别与y的3个数据块依次相加。该方案主要为了演示CacheMode的功能，示例代码中故意使用重复搬运x的实现方式，真实设计中并不需要采用这个方案。下文完整样例请参考设置合理L2 CacheMode样例。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/5b2fa07f1b5e93d119db7857980af32bcbd7324930ddde4b47d9a80acfb93bed.jpg)


<table><tr><td>实现方案</td><td>原始实现</td><td>优化实现</td></tr><tr><td>实现方法</td><td>总数据量700MB,其中:x:100MB;y:300MB;z:300MB。使用40个核参与计算,按列方向切分。x、y、z对应GlobalTensor的CacheMode均设置为CACHE_MODE_NORMAL,需要经过L2 Cache,需要进入L2 Cache的总数据量为700MB。</td><td>总数据量700MB,其中:x:100MB;y:300MB;z:300MB。使用40个核参与计算,按列方向切分。x对应的GlobalTensor的CacheMode设置为CACHE_MODE_NORMAL;y和z对应的GlobalTensor的CacheMode设置为CACHE_MODE_DISABLE。只有需要频繁访问的x,设置为需要经过L2 Cache。需要进入L2 Cache的总数据量为100MB。</td></tr><tr><td>示例代码</td><td>xGm.SetGlobalBuffer(__gm__ float *)x +AscendC::GetBlockIdx() * TILE_N);yGm.SetGlobalBuffer(__gm__ float *)y +AscendC::GetBlockIdx() * TILE_N);zGm.SetGlobalBuffer(__gm__ float *)z +AscendC::GetBlockIdx() * TILE_N);</td><td>xGm.SetGlobalBuffer(__gm__ float *)x +AscendC::GetBlockIdx() * TILE_N);yGm.SetGlobalBuffer(__gm__ float *)y +AscendC::GetBlockIdx() * TILE_N);zGm.SetGlobalBuffer(__gm__ float *)z +AscendC::GetBlockIdx() * TILE_N);// disable the L2 cache mode of y and zyGm.SetL2CacheHint(AscendC::CacheMode::CACHE_MODE_DISABLE);zGm.SetL2CacheHint(AscendC::CacheMode::CACHE_MODE_DISABLE);</td></tr></table>

# 说明

你可以通过执行如下命令行，通过算子调优（msProf）工具获取上述示例的性能数据并进行对比。

msprof op --launch-count=2 --output=./prof ./execute_add_op 

重点关注Memory.csv中的aiv_gm_to_ub_bw(GB/s)和aiv_main_mem_write_bw(GB/s)写带宽的速率。

# 3.8.5.8 算子与高阶 API 共享临时 Buffer

# 【优先级】高

【描述】如果算子使用的高阶API需要传入临时Buffer，如SoftMax，该临时空间会挤占算子其他计算的空间，从而导致单次计算搬运的数据量变少，搬运的次数变多。此场景可通过共享临时Buffer空间，提升单次搬运的数据量，减少搬运的次数，提升内存使用效率。

# 【反例】

SoftMax高阶API计算需要临时Buffer空间，算子在进行其他计算时拥有独立临时Buffer。UB空间是固定的，假设可以给SoftMax和Add能分配临时空间为64KB，SoftMax计算需要的临时Buffer空间tmpSoftmaxBuf占用32KB，则存储Add计算结果的LocalTensor tmpSumBuf最多只能分配32KB。如果src0Tensor计算的数据量是512KB，则需要搬运512 / 32 = 16次。

```txt
...
constexpr int32_t blockLen = 32 * 1024;
TBuf<TPosition::VECCALC> tmpSoftmaxBuf; 
```

```cpp
pipe.InitBuffer(tmpSoftmaxBuf, softmaxBufSize * sizeof(uint8_t)); // 单独分配Softmax的临时Buf 32KB TBuf<TPosition::VECCALC> tmpSumBuf;
pipe.InitBuffer(tmpSumBuf, sumBufSize * sizeof(T)); // 单独分配Add的临时Buf，且softmaxBufSize * sizeof(uint8_t) + sumBufSize * sizeof(T) <= 64KB
...
for (int i = 0; i < 16; i++) {
    ...
    LocalTensor<uint8_t> tmpSoftmaxTensor = tmpSoftmaxBuf.Get<uint8_t>(softmaxBufSize);
    SoftMax<T, true, true>(dstTensor, expSumTensor, dstMaxTensor, srcTensor, tmpSoftmaxTensor, tiling);
    ...
    DataCopy(src0Tensor, src0Gm[i * blockLen / sizeof(T)], Params);
    ...
    LocalTensor<T> tmpSumTensor = tmpSumBuf.Get<T>(sumBufSize);
    Add<T>(tmpSumTensor, src0Tensor, src1Tensor, count);
    ...
}
```

# 【正例】

SoftMax高阶API计算需要临时Buffer空间，算子在进行其他计算时可以共享此临时Buffer，按照上述假设只需要搬运512 / 64 = 8次。

```txt
...
constexpr int32_t blockLen = 64 * 1024;
TBuf<TPosition::VECCALC> tmpSharedBuf;
pipe.InitBuffer(tmpSharedBuf, bufferSize); // 共享分配bufferSize = MAX(softmaxBufSize * sizeof(uint8_t), sumBufSize * sizeof(T)) <= 64KB
...
for (int i = 0; i < 8; i++) {
    ...
    LocalTensor<uint8_t> tmpSharedTensor = tmpSharedBuf.Get<uint8_t>(softmaxBufSize);
    SoftMax<T, true, true>(dstTensor, expSumTensor, dstMaxTensor, srcTensor, tmpSharedTensor, tiling);
    ...
    DataCopy(src0Tensor, src0Gm[i * blockLen / sizeof(T)], Params);
    ...
    LocalTensor<T> tmpSumTensor = tmpSharedBuf.Get<T>(sumBufSize);
    Add<T>(tmpSumTensor, src0Tensor, src1Tensor, count);
    ...
}
```

# 3.8.5.9 纯搬运类算子 VECIN 和 VECOUT 建议复用

# 【优先级】高

【描述】纯搬运类算子在执行时并不涉及实际vector计算，若存在冗余的vector指令，会导致算子整体执行时间变长。这种场景可以使用Ascend C针对纯搬运类算子提供的TQueBind接口，该接口可以将VECIN与VECOUT绑定，省略将数据从VECIN拷贝到VECOUT的步骤，从而避免vector的无谓消耗。

# 【反例】

此段代码为了保证数据搬入和数据搬出之间的流水同步，存在LocalTensor ->LocalTensor的DataCopy指令。

```cpp
template <typename ComputeT> class KernelExample {
public:
    ...
    __aicore__ inline void Process(...)
    {
    for (int i = 0; i < iLen; ++i) {
    ...
    auto iLocal = Quel.AllocTensor<ComputeT>();
    DataCopy(iLocal, inGm[i * 32], size);
    Quel.EnQue(iLocal);
    iLocal = Quel.DeQue<ComputeT>();
    for (int j = 0; j < jLen; ++j) { 
```

```cpp
...
auto oLocal = QueO.AllocTensor<ComputeT>();
DataCopy(oLocal, iLocal, size); // LocalTensor -> LocalTensor的DataCopy指令,以实现数据从VECIN到VECOUT的搬运
QueO.EnQue(oLocal);

auto oLocal = QueO.DeQue<ComputeT>();
DataCopyPad(outGm[j], oLocal, ...);
QueO.FreeTensor(oLocal);
}
Quel.FreeTensor(iLocal);
}
private:
...
TQue<TPosition::VECIN, BUFFER_NUM> Quel;
TQue<TPosition::VECOUT, BUFFER_NUM> QueO;
...
};
extern "C" __global__ __aicore__ void example_kernel(...)
{
...
op.Process(...);
}
```

# 【正例】

将LocalTensor -> LocalTensor的DataCopy指令替换为TQueBind接口，减少将VECIN拷贝到VECOUT的步骤，从而避免了冗余拷贝。

```cpp
template <typename ComputeT> class KernelExample {
public:
    ...
    __aicore__ inline void Process(...)
    {
    for (int i = 0; i < iLen; ++i) {
    ...
    auto bindLocal = queBind.AllocTensor<ComputeT>();
    DataCopy(bindLocal, inGm[i * 32], size);
    queBind.EnQue(bindLocal);
    bindLocal = queBind.DeQue<ComputeT>();
    for (int j = 0; j < jlen; ++j) {
    ...
    DataCopyPad(outGm[j], bindLocal, ...);
    }
    queBind.FreeTensor(bindLocal);
    }
    }

private:
    ...
    TQueBind<TPosition::VECIN, TPosition::VECOUT, BUFFER_NUM> queBind; // 使用TQueBind替换原来的
    QueI, QueO
    ...
};
extern "C" __global__ __aicore__ void example_kernel(...)
{
    ...
    op.Process(...);
}
```

# 【性能对比】


图 3-111 aiv_vec_time 优化前后对比


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/7fb5b1404d5f659372cac01a42536cfb15e38d94e9e39b8e85539894f43971fd.jpg)


如上图所示，将反例中DataCopy指令替换为TQueBind之后有明显优化。由于省略了数据从VECIN拷贝到VECOUT的步骤，aiv_vec_time几乎缩减为0。

# 3.8.5.10 通过缩减 Tensor ShapeInfo 维度，优化栈空间

【优先级】中

【描述】GlobalTensor和LocalTensor中通过ShapeInfo类型的成员变量来保存shape信息，SetShapeInfo/GetShapeInfo可以设置或者获取ShapeInfo，在算子实现内部用于shape信息保存和传递。默认情况下支持的最大维度为8。在不使用上述ShapeInfo功能的情况下，不需要这些信息，可以通过K_MAX_SHAPE_DIM宏将其设置为0。经实测减小K_MAX_SHAPE_DIM值，可缩减栈空间，减少scalar指令和cache miss几率，提升算子性能。

```txt
...
#ifndef K_MAX_SHAPE_DIM
#define K_MAX_SHAPE_DIM 8
#endif

...
struct ShapeInfo {
public:
    ...
    uint32_t shape[K_MAX_SHAPE_DIM];
    uint32_t originalShape[K_MAX_SHAPE_DIM];
};

template <typename T> class GlobalTensor {
...
private:
    ShapeInfo shapeInfo_;
}
template <typename T> class LocalTensor {
...
private:
    ShapeInfo shapeInfo_;
}
... 
```

# 【反例】

算子无需使用ShapeInfo，但未对ShapeInfo大小进行限制（使用默认值8），导致浪费K_MAX_SHAPE_DIM * sizeof(uint32_t) * 2 * 4字节的栈空间。2表示有shape和originalShape两个数组，4表示该样例中使用了GlobalTensor和LocalTensor共4个Tensor。

```c
#include "kernel_operator.h" ...
extern "C" __global__ __aicore__ void add_custom(GM_ADDR x, GM_ADDR y, GM_ADDR z, GM_ADDR workspace, GM_ADDR tiling)
{
    ...
    GlobalTensor<T> dataIn;
    GlobalTensor<T> dataOut;
    LocalTensor<T> vecIn;
    LocalTensor<T> vecOut;
    ...
} 
```

# 【正例】

算子无需使用ShapeInfo，设置#define K_MAX_SHAPE_DIM 0，有效缩减了K_MAX_SHAPE_DIM * sizeof(uint32_t) * 2 * 4大小的栈空间。

```c
#define K_MAX_SHAPE_DIM 0
...
#include "kernel_operator.h" //需注意定义K_MAX_SHAPE_DIM宏的位置须在包含Ascend C相关头文件之前
...
extern "C" __global__ __aicore__ void add_custom(GM_ADDR x, GM_ADDR x, GM_ADDR z, GM_ADDR workspace, GM_ADDR tiling)
{
    ...
    GlobalTensor<T> dataIn;
    GlobalTensor<T> dataOut;
    LocalTensor<T> vecIn;
    LocalTensor<T> vecOut;
    ...
}
```

# 3.8.5.11 避免 Unified Buffer 的 bank 冲突

# 3.8.5.11.1 概述

【优先级】高

【概述】

为了提高数据访问的效率和吞吐量，Unified Buffer采用了大小相等的内存模块（bank）结构设计。当多条读写指令同时访问Unified Buffer时，由于硬件资源的限制，这些指令不能同时执行，从而引发bank冲突。在这种情况下，指令需要排队等待资源，无法在一个指令周期内完成。

针对NPU架构版本220x

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/f7d80ff858b1f6da6c1f525902cb2562fad9da1f0cee9d8a05870c85f5afd533.jpg)


UB总大小为192KB，包含16个bank group，每个bank group包含3个bank。每个bank大小为4KB，由128行组成，每行长度为32B。

读写冲突：读操作和写操作同时尝试访问同一个bank。

写写冲突：多个写操作同时尝试访问同一个bank group。

读读冲突：多个读操作同时尝试访问同一个bank group。


针对Atlas 350 加速卡


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/2b1b7f29be0a15205b205b47d57135e0506595f7c2980f907b2fbaefc391005b.jpg)


UB总大小为256KB，包含8个bank group，每个bank group包含2个bank。每个bank大小为16KB，由512行组成，每行长度为32B。

读写冲突：读操作和写操作同时尝试访问同一个bank。

写写冲突：多个写操作同时尝试访问同一个bank group。

读读冲突：两个读操作同时尝试访问同一个bank，或者两个以上读操作同时尝试访问同一个bank group。

可以看出bank冲突的场景与Unified Buffer的规格密切相关，规格的变化通常会导致bank冲突场景的变化。

由于Atlas 350 加速卡的bank group上有两组读口和写口，因此两次读操作访问同一个bank group的不同bank时，不会引起冲突。

假设读指令操作的地址为0x0000（bank0），写指令操作的地址为0x10000 ，在NPU架构版本220x中，地址0x10000（bank16）不会发生读写冲突，而在Atlas350 加速卡中，这个地址0x10000（bank0）会引发读写冲突。

下文介绍不同硬件架构下如何避免bank冲突。

# 3.8.5.11.2 避免 bank 冲突（NPU 架构版本 220x）

【优先级】高

# 说明

该性能优化建议适用于如下产品型号：

● Atlas A3 训练系列产品/Atlas A3 推理系列产品

● Atlas A2 训练系列产品/Atlas A2 推理系列产品

【描述】为了提高数据访问的效率和吞吐量，Unified Buffer采用了bank（大小相等的内存模块）结构设计。Unified Buffer总大小为192K，划分为48个bank。每个bank由128行组成，每行长度为32B。这48个bank进一步组织为16个bank group，每个bankgroup包含3个bank，例如bank15、bank31和bank47组成一个bank group。


图 3-112 bank 结构示意图（图中箭头方向表示内存排布的顺序）


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/6cffab2e1e2ccebad6441e7a61f2f4088efcbb46a5e5d4b3f88d62f05036fdc6.jpg)


每个bank可以独立地进行数据的读写操作，允许多个数据请求同时进行。然而，当多个读写操作试图同时访问同一个bank或bank group时，由于硬件资源的限制，这些操作必须排队等待，会导致bank冲突，引起性能下降。

具体来说，Vector计算单元每拍（一个指令周期）能够从每个bank group中读取或写入一行数据。如果同一个API中的多个操作试图同时访问同一个bank或bank group，Vector计算单元无法在同一个周期内处理所有请求，导致这些请求排队等待。这种排队增加了数据访问的延迟，降低了系统的整体性能。

# bank冲突的典型场景

bank冲突主要可以分为以下三种场景：

读写冲突：读操作和写操作同时尝试访问同一个bank。

写写冲突：多个写操作同时尝试访问同一个bank group。

读读冲突：多个读操作同时尝试访问同一个bank group。

下文给出了一些具体的示例，假设，0x10000地址在bank16上，0x10020在bank17上，0x20020在bank33上，如下图所示：


图3-113 地址分配示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/6db8f9e729a26681327ac11da829c6eaef0a507f048ee4f39c471277ac7e93e0.jpg)


读写冲突示例

Vector指令的源操作数src和目的操作数dst同时读写到同一个bank时造成读写冲突，具体分析如下：


表3-18 读写冲突示例


<table><tr><td>序号</td><td>src地址</td><td>dst地址</td><td>bank</td><td>bank group</td><td>结论</td></tr><tr><td>示例1</td><td>0x10020</td><td>0x10000</td><td>bank_id0 != bank_id1</td><td>bank_group_id0 != bank_group_id1</td><td>src地址和dst地址分别属于bank16和bank17,故无冲突。</td></tr><tr><td>示例2</td><td>0x10020</td><td>0x10E20</td><td>bank_id0 == bank_id1</td><td>bank_group_id0 == bank_group_id1</td><td>src地址和dst地址的地址都在bank17,故存在冲突。</td></tr></table>

# 写写冲突示例

Vector指令目的操作数dst对应的8个DataBlock（block0-block7）同时写到一个bank group时造成写写冲突，具体分析如下：


表3-19 写写冲突示例


<table><tr><td>序号</td><td>dst地址</td><td>blk_stride</td><td>block0_addr</td><td>block1_addr</td><td>block2_addr</td><td>...</td><td>结论</td></tr><tr><td>示例1</td><td>0x1FE00</td><td>16</td><td>0x1FE00</td><td>0x20000</td><td>0x20200</td><td>...</td><td>8个DataBlock均在一个bank group下,故全部冲突,8拍完成一个Repeat的写入。</td></tr><tr><td>示例2</td><td>0x1FE00</td><td>8</td><td>0x1FE00</td><td>0x1FF00</td><td>0x20000</td><td>...</td><td>block0和block2在一个bank group,存在冲突,4拍完成一个Repeat的写入。</td></tr></table>

# 读读冲突

Vector指令多个源操作数同时读到同一个bank group时造成读读冲突，具体分析如下：


表3-20 双src场景读读冲突示例


<table><tr><td>序号</td><td>src0地址</td><td>src1地址</td><td>bank</td><td>bank group</td><td>结论</td></tr><tr><td>示例1</td><td>0x10020</td><td>0x20020</td><td>bank_id0 != bank_id1</td><td>bank_group_id0 == bank_group_id1</td><td>存在冲突。</td></tr><tr><td>示例2</td><td>0x10020</td><td>0x10000</td><td>bank_id0 != bank_id1</td><td>bank_group_id0 != bank_group_id1</td><td>无冲突。</td></tr></table>

Vector指令某一个源操作数对应的8个DataBlock（block0-block7）读到同一个bank group时造成读读冲突，具体分析如下：


表 3-21 单 src 场景读读冲突示例


<table><tr><td>序号</td><td>src地址</td><td>blk_stride</td><td>block0_addr</td><td>block1_addr</td><td>block2_addr</td><td>...</td><td>结论</td></tr><tr><td>示例1</td><td>0x1FE00</td><td>16</td><td>0x1FE00</td><td>0x20000</td><td>0x20200</td><td>...</td><td>8个DataBlock均在一个bank group下,故全部冲突,8拍完成一个Repeat的读操作。</td></tr><tr><td>示例2</td><td>0x1FE00</td><td>8</td><td>0x1FE00</td><td>0x1FF00</td><td>0x20000</td><td>...</td><td>block0和block2在同一个bank group下,存在冲突,4拍完成一个Repeat。</td></tr></table>

# 说明

通过msProf工具可以进行资源冲突占比的相关性能数据采集。

工具的具体使用方法和资源冲突占比文件性能数据文件说明请参考《算子开发工具》。

# 如何避免bank冲突

避免bank冲突的方法有两种：优化计算逻辑和优化地址分配。

# 优化计算逻辑

对一个shape为(8, 16, 16)的输入做(1, 0, 2)的transpose操作，输出shape为(16,8, 16)。通过将计算逻辑由“跳读，连续写”修改为“连续读，跳写”可避免冲突问题。实现方案对比如下：

<table><tr><td>实现方案</td><td>原始实现</td><td>优化实现</td></tr><tr><td>实现方法</td><td>跳读,连续写同一Repeat内输入的8个DataBlock都在同一个bank group而发生读读冲突。</td><td>连续读,跳写同一个Repeat内输入的8个DataBlock不在同一个bank group内,避免了读读冲突。</td></tr></table>

```lisp
{
    uint32_t index = threadIdx.x;
    auto rem = self[index] % other[index];
    bool signs_differ = ((rem < 0) != (other[index] < 0));
    if (signs_differ && (rem != 0)) {
    out[index] = rem + other[index];
    } else {
    out[index] = rem;
    }
} 
```

【性能对比】

如下图所示，基于SIMD实现的floor_mod算子的Kernel执行耗时为4.03us。


图 3-142 SIMD 实现 floor_mod 的耗时


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/3a18b92f7d3742c6065f4d3b13cc0acf746c4d663e5211de11083f94aed65e9c.jpg)


如下图所示，基于SIMT实现的floor_mod算子的Kernel执行耗时为3.444us。


图 3-143 SIMT 实现 floor_mod 的耗时


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/6d42867272a20232f4c9b6edc5df49efb63f426b71739226cdc08536cf2c699b.jpg)


在核数不变、每个核处理的数据量相同且数据统一搬运到Unified Buffer上进行计算的情况下，使用SIMT实现分支判断的性能比使用SIMD实现的性能提升了14.6%。

# 3.10 优秀实践

# 3.10.1 FlashAttention 算子性能调优案例

# 案例介绍

本案例中的算子FlashAttentionScoreGrad，用于训练场景下计算注意力的反向输出，即FlashAttentionScore算子的反向计算。

已知注意力的正向计算公式为：

$$
Y = \text { Dropout } (\text { Softmax } (M a s k (\frac {Q K ^ {T} + p s e}{\sqrt {d}}), \text { atten\_mask }), \text { keep\_prob }) V
$$

为方便表达，以变量S和P表示计算公式：

$$
S = M a s k (\frac {Q K ^ {T} + p s e}{\sqrt {d}}, a t t e n \_ m a s k)
$$

$$
P = \text { Dropout } (\text { Softmax } (S), \text { keep\_prob })
$$

$$
Y = P V
$$

则注意力的反向计算公式为：

$$
d V = P ^ {T} d Y
$$

$$
d Q = \frac {((d S) ^ {*} K)}{\sqrt {d}}
$$

$$
d K = \frac {((d S) ^ {T *} Q)}{\sqrt {d}}
$$

$$
d (p s e) = d S ^ {*} \sqrt {d}
$$

计算流程图如下：


图 3-144 算子计算流程


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/a1c3e138c67947859ec9b8efa15a72393a99e5c2adbd601bebba368e7c7209fa.jpg)


按照FlashAttention反向计算流程的实现，简介整体计算流程如下。对本算子的算法感兴趣的用户可简单了解，无需重点关注。

1. 重计算p，本步骤重计算了FlashAttention流程中的softmax结果p，计算结果保存在ub中。

$$
p = \text { SimpledSoftmax } (M a s k (M a t m u l (q u e r y, k e y ^ {T}) + p s e) * s c a l e)
$$

2. 计算dp，该计算包含matmul计算和dropout计算，matmul计算中，左矩阵为dy，右矩阵为转置后的value。

$$
d p = \text { Dropout } (\text { Matmul } (d y, v a l u e ^ {T}))
$$

3. 计算ds，本计算中，FlashSoftmaxGrad计算的入参为dy、正向输出attention_in，该结果与dp做减操作，最终的结果与p相乘得到结果ds。

$$
d s = p ^ {*} \text { Sub } (d p, \text { FlashSoftmaxGrad } (d y, \text { attentio\_in }))
$$

4. 计算dq，本计算将ds结果与key做matmul计算，并将结果与scale相乘得到结果dq。

$$
d q = \text { Matmul } (d s, k e y) ^ {*} \text { scale }
$$

5. 计算dk，本计算将转置后的ds结果与query做matmul计算，并将结果与scale相乘得到结果dk。

$$
d k = M a t m u l (d s ^ {T}, q u e r y) ^ {*} s c a l e
$$

6. 计算dv，本计算将p的结果做drop计算，转置后与dy做matmul计算。

$$
d v = \text { Matmul } (D r o p O u t (p) ^ {T}, d y)
$$

本案例的验证平台为Atlas A2 训练系列产品/Atlas A2 推理系列产品，以两个场景为例，第一个场景的输入维度信息为：B=1，N1=12，N2=12，S1=6144，S2=6144，D=128，causal场景，即atten_mask的形状为下三角，如图3-145。第二个场景的输入维度信息为：B=24，N1=5，N2=5，S1=9216，S2=9216，D=64，不带atten_mask和drop_mask输入。主要涉及的优化手段包括tiling基本块大小调整，核间负载均衡，CV流水并行，MTE2流水优化以及FixPipe流水优化等优化手段。


图 3-145 causal 场景 atten_mask 形状


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/cbcdce1477b718afea929c59e57ab900a08b78978f4469641991ca38363da470.jpg)


# 获取性能数据

流水优化分析工具包括CAModel和Profiling工具，分别从两个方面分析：第一个是从Profiling工具生成的Profiling数据中分析各项流水的占比，第二个是从CAModel工具生成的打点图分析各流水并行情况。

# 分析主要瓶颈点

通过观察分析流水图和Profiling数据，结合优化经验来判断性能瓶颈点。在优化过程中不同阶段可能会出现不同的瓶颈点，需要不断优化以达到最佳性能。

根据优化经验，循环间会存在一些不必要的性能开销，循环越多性能可能越差；满足UB最大空间限制的情况下，UB切分的基本块越大，循环越少。算子中通过InitBuffer接口分配UB buffer大小。

pipe->InitBuffer(ubBuffer, 120 * 1024); 

pipe->InitBuffer(tmpBuffer, 30 * 1024); 

pipe->InitBuffer(vecClc3, 8 * 1024); 

如上代码所示，InitBuffer接口的第二个参数表示buffer占用的大小，所有buffer大小的和即为占用的总空间。这里1 $2 0 \times 1 0 2 4 + 3 0 \times 1 0 2 4 + 8 \times 1 0 2 4 = 1 5 8 \mathsf { K B } <$ UB Size，没有充分利用UB空间。

观察如下流水图，绿色为Vector侧的流水，橙色为Cube侧的流水，可以看出两侧的流水都存在大段的空隙，CV之间流水很大程度上未并行，需要考虑CV流水优化。


图3-146 优化前算子流水


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/3dc65411a37b89af7bd392c19ebaf61ab61c62545ee49b8d40b3ccf9493a403e.jpg)


对于上述场景一，causal场景下可能存在核间分布不均匀的情况，如下图经过atten_mask掩码后，红色部分是算子需要计算的部分，绿色无需计算；如果不按照基本块的个数来分核，按照第一根轴的大小8（行）来分核，假设平均分到9个核上，每个核做ceil(8 / 9) = 1行，则第1个核只需做1个基本块，但是第8个核需要做8个基本块的计算，出现明显的负载不均衡。因此需要考虑将红色块均匀分到多个核上计算。


图 3-147 causal 场景 atten_mask 形状


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/a96818a3b84d453e0e10e49fa8017f7dbf57f193237b063b8da1a8dd035dde10.jpg)


场景一的Profiling数据如下，aic_fixpipe_ratio占比极高，可能存在FixPipebound。

图 3-148 场景一 Profiling 数据

0206737.67.44E+0926507.620.12822049101 0.99122384710.11535707396 e2 rati061aic_fxpipe 0.8151 0.0025206734.51.49E+1055978.650.270824983120.120838686.490.18712699382 a0.13060.001 

● 场景二的Profiling数据如下，mte2_ratio占比高，可能存在MTE2 bound。

图 3-149 场景二 Profiling 数据

N/A N/A 028782.411.04E+093326.0890.115628226.01 0.98073064.068 0.10615044.92 0.5227 2242.57 e.78670.001228772.292.07E096043.186 ve0.213085.8920.10735507.290.191 

# 设计优化方案

# 优化点一：调整tiling基本块

在满足UB空间大小限制的前提下，tiling基本块的切分应尽可能大。如下图为优化前按照(64, 128)切分计算，总共需要循环计算32次。


图3-150 优化前计算基本块及次数


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/41fc8d77e5b8e6200a3a9eb7610960ef7357630f2258374f752f080bc16fca18.jpg)


考虑到UB空间没有用满，基本块调整到(128, 128)，如下图优化后只需循环计算16次，切分后算子性能提升一倍。


图3-151 优化后计算基本块及次数


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/e48ddb9ed1ca2c625e372642239e9c27a3866bd23d232b123a36a81c00a773d5.jpg)


# 优化点二：CV流水并行

由于FAG算子中Cube计算比Vector计算快且存在依赖性，同时为了减少CV之间的通信次数，通过缓存机制使matmul提前计算多块，这里的缓存机制指的是将mm一次性计算多个基本块缓存到GM上。如下代码中，SetTail设置的singleCoreM和singleCoreN大小分别为BaseM，BaseN的倍数，即matmul一次发起多个基本块的计算，实现matmul结果的缓存，Vector侧分多次取matmul的结果。

```txt
mm3.SetTail(s2CvExtend, -1, preS1Extend);
mm3.SetTensorA(mulWorkSpaceGm[pingpongIdx * coreNum * cubeBaseMN + cBlockIdx * cubeBaseMN], true);
mm3.SetTensorB(queryGm[mm2aTensorOffsetCv]);
mm3.template IterateAll<false>(dkWorkSpaceGm[bTensorOffsetCv], true); 
```


图 3-152 完成 mm1/mm2/mm3 缓存的流水


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/27d163617857c1c50e280861db1d7762bb9a100bb7914059e778e6b016fe8a33.jpg)


如上图是实现mm1、mm2和mm3缓存的流水图，并行度提高，CV的间隔减小，提升了算子性能。


图 3-153 Vector 等 Cube 流水的间隔插入 Vector 计算


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/82f5658ecf38cb02ec5cc7a7c29569454a5503a9e74c940ae7c3f2fda2578700.jpg)


基于缓存mm1/mm2/mm3的优化后，在本轮Vector计算等Cube流水的间隔，插入下一轮循环的Vector计算，如上图所示，这样使Vector流水与Cube流水之间的并行度更高，反映到流水图中为Vector计算更密集。原计算过程伪代码与在CV间隔中插入下一轮Vector计算的伪代码，分别如以下两段所示。

```javascript
// 原计算过程伪代码
// mm1计算;
dropout();
Sub();
// mm2计算;
Softmax();
AttenMask();
...
// 在Vector等Cube流水的间隔中，插入下一轮循环的Vector计算伪代码
// mm1计算;
dropout();
Sub();
dropout(); // 下一轮循环的Vector计算
Sub(); // 下一轮循环的Vector计算
// mm2计算;
Softmax();
AttenMask();
...
```

# 优化点三：每个核负载均衡


图 3-154 causal 场景优化前每个核计算量


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/d16c65838bc121490fe8367abefdae625893cac8c734739ce818abdbfd59d237.jpg)



图 3-155 causal 场景优化后每个核计算量


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/8dbbdb2c-2ebf-4561-afa3-031935b6c088/f5c66f763d645f34b1147aaf05471135145fe1ad9aa281c48afb74f7f055a098.jpg)


尽量实现每个核的计算量均匀，负载均衡。优化前的分核及每个核的计算量如图11 causal场景优化前每个核计算量所示，按照第一根轴的大小8（行）来分核，平均分到9个核上，每个核计算ceil(8/9)=1行，第1个核只计算1个基本块，但是第8个核计算8个基本块。优化后如图12 causal场景优化后每个核计算量所示，红色块总共36个基本块，均分到每个核上，每个核的计算量为4个基本块，性能提升一倍。

优化点四： FIXPIPE优化

从采集的Profiling数据来看，Cube FixPipe占比高达81%，出现了很严重的bound（达到上限）。CAModel工具打印发现存在很多异常的128B搬运，排查代码，发现workspace地址未512B对齐。

图 3-156 场景一优化前 Profiling 数据

0@07.7687.4ta0ga15.507620282@209101 .922341ac .1e53507366 e2 (02761 1a6sppe pe_r8151 Cac252067345149E105978.65027082498312012083886490187269982aV0136.001 

代码实现中使用SetGlobalBuffer接口设置workspace的起始地址，如果起始地址不是按照512B对齐，搬运效率会很低，可以强制GM地址512Byte对齐来避免这个情况。下面代码中ADDR_ALIGN_SIZE即为512。

```javascript
// init workspace address
syncGlobal.SetGlobalBuffer((__gm__ int32_t*)workspace);
uint64_t workspaceOffsets = SYNC_GLOBAL_WORKSPACE_SIZE;
dqWorkSpaceGm.SetGlobalBuffer((__gm__ float*)workspace + workspaceOffsets / sizeof(T2));
workspaceOffsets = (workspaceOffsets + qPostBlockTotal * sizeof(float) + ADDR_ALIGN_SIZE) / ADDR_ALIGN_SIZE * ADDR_ALIGN_SIZE; dkWorkSpaceGm.SetGlobalBuffer((__gm__ float*)workspace + workspaceOffsets / sizeof(T2));
workspaceOffsets = (workspaceOffsets + kvPostBlockTotal * sizeof(float) + ADDR_ALIGN_SIZE) / ADDR_ALIGN_SIZE * ADDR_ALIGN_SIZE; dvWorkSpaceGm.SetGlobalBuffer((__gm__ float*)workspace + workspaceOffsets / sizeof(T2));
workspaceOffsets = (workspaceOffsets + kvPostBlockTotal * sizeof(float) + ADDR_ALIGN_SIZE) / ADDR_ALIGN_SIZE * ADDR_ALIGN_SIZE;
// matmul1 and matmul2 workspace size
matmulWorkspaceSize = cubeBaseMN * sizeof(float);
mm1WorkspaceGm.SetGlobalBuffer((__gm__ T2*)(workspace + workspaceOffsets + cBlockIdx * matmulWorkspaceSize)); mm2WorkspaceGm.SetGlobalBuffer((__gm__ T2*)(workspace + workspaceOffsets + coreNum * matmulWorkspaceSize + cBlockIdx * matmulWorkspaceSize); // drop 
```

```txt
workspace offset
workspaceOffsets = (workspaceOffsets + coreNum * cubeBaseMN * sizeof(float) * INPUT_NUMS + ADDR_ALIGN_SIZE) / ADDR_ALIGN_SIZE * ADDR_ALIGN_SIZE;
dropWorkSpaceGm.SetGlobalBuffer((__gm__ T1*)workspace + workspaceOffsets / sizeof(T1));
// mul workspace offset
workspaceOffsets = (workspaceOffsets + coreNum * cubeBaseMN * sizeof(half) * 2 + ADDR_ALIGN_SIZE) / ADDR_ALIGN_SIZE * ADDR_ALIGN_SIZE;
mulWorkSpaceGm.SetGlobalBuffer((__gm__ T1*)workspace + workspaceOffsets / sizeof(T1)); 
```

修改代码，workspace地址经过512B对齐后，FixPipe时间减半。


图 3-157 场景一优化后 Profiling 数据


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/a3fa2b98cb6f5f2d51b183a9094a64b54378973cc1541155a3dbc5fbbc00dd49.jpg)


优化点五：MTE2优化

结合如下的Profiling数据和流水图，可以看出MTE2 bound，且部分MTE2搬运时间异常。


图 3-158 场景二 Profiling 数据


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/edc16f5af6c53dd531f23a2711bf3e50dd218a3e00773c1c856d39c3dad29906.jpg)



图3-159 场景二流水图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/31164dc0cfa49cd8d10d82828eff6a11645f0139aebae0cb8124036ae92deb6c.jpg)


将输入数据排布格式从BSH更改为BNSD后，数据搬运连续，不需要跳地址读取数据，搬运效率提升一倍，部分异常搬运时长降低了一半。

# 验证优化方案性能收益

调整tiling基本块：理论评估Vector切块越大，计算和搬运循环次数越少，同时能够充分利用搬运带宽和Vector算力。基本块大小从(64, 128)增大到(128, 128)后，性能提升一倍，实测与理论分析一致。

CV流水并行：CV流水掩盖的时间即为提升的性能，符合预期的收益。

核间负载均衡：优化前负载最多的核计算量减少的倍数，即为预期提升的性能；案例中优化前负载最多的核的计算量大小为8块，优化后为4块，实际性能提升一倍，符合预期的收益。

FixPipe优化：从Profiling数据看出FixPipe占比0.8，优化后占比0.55，实测算子性能提升45%，与理论分析一致。

MTE2优化：从Profiling数据看出MTE2占比0.52，优化后占比减少一半，实测算子性能提升30%，与理论分析一致。

# 总结

融合算子场景，可参考此优化。

# 3.10.2 GroupedMatmul 算子性能调优案例

# 案例介绍

本案例对分组Matmul即GroupedMatmul算子的per-token量化场景进行性能分析和优化，GroupedMatmul算子计算过程（通过python代码表达）为：

```python
offset = 0
for i in range(g):
    mmOut = x[offset:offset + groupList[i]] * weight[i] + bias[i]
    y[offset:offset + groupList[i]] = Gelu(mmOut * scale[i] * pertokenScale[offset:offset + groupList[i]])
    offset += groupList[i] 
```

验证平台为Atlas A2 训练系列产品/Atlas A2 推理系列产品。

优化分析以如下算子规格为例：


表3-28 算子规格


<table><tr><td>input</td><td>shape</td><td>data type</td><td>format</td></tr><tr><td>x</td><td>(1024,1024)</td><td>int8</td><td>ND</td></tr><tr><td>weight</td><td>(8,1024,8192)</td><td>int8</td><td>NZ</td></tr><tr><td>bias</td><td>(8,8192)</td><td>int32</td><td>ND</td></tr><tr><td>groupList</td><td>8</td><td>int64</td><td>ND</td></tr><tr><td>scale</td><td>(8,8192)</td><td>float</td><td>ND</td></tr><tr><td>pertokenScale</td><td>1024</td><td>float</td><td>ND</td></tr><tr><td>y</td><td>(1024,8192)</td><td>float16</td><td>ND</td></tr></table>

# 主要介绍以下优化方法：

对于Vector计算占比较高（Vector Bound）的场景，将AI Core中的AIC核和AIV核启动比例设置为1:2；

优化CV并行流水，减少Cube和Vector计算间的空闲等待时间；

优化Vector计算流水，提高Vector并行计算速度。

# 获取性能数据

固定8核测试，即当前性能和后续优化tiling中numBlocks固定设置为8。

通过msProf算子调优工具获取算子性能数据：

获取真实环境执行的性能数据（指令的cycle占比数据

ArithmeticUtilization.csv），包含各个流水的占比情况；

获取仿真性能数据（指令流水图），包含各个流水的占用区间，可观察流水间依赖情况，从而优化并行效率。

# 分析主要瓶颈点

固定8核进行测试的情况下，通过msprof op命令获取指令的cycle占比数据如下：


图 3-160 指令的 cycle 占比数据 ArithmeticUtilization.csv（性能总耗时为 218.1us）


<table><tr><td>Op Name</td><td>OP Type</td><td>Task Dura</td><td>aicore_tim</td><td>aic_mte2</td><td>aic_mte2</td><td>aiv_time(u</td><td>aiv_vec_tir</td><td>aiv_vec_ra</td><td>aiv_mte2</td><td>aiv_mte2_ratio</td></tr><tr><td>quant_grc</td><td>quant_grc</td><td>218.064</td><td>215.38</td><td>95.262</td><td>0.442</td><td>212.04</td><td>91.333</td><td>0.431</td><td>27.016</td><td>0.127</td></tr></table>

通过msprof op simulator获取到的指令流水图如下图所示：


图 3-161 指令流水图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/54a8503fdeb8927db46cdd0596f2903c145a1ee2b7bae081dc33ef5b219abc8c.jpg)


结合上述两种数据（真实数据和仿真数据）进行性能分析：

Vector计算bound，当前为减少核启动开销设置为1:1；

实际优化过程中，对上述问题进行优化、Vector计算占比下降后，Cube和Vector计算各自都有间隙，相互之间都有等待耗时；

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/3f54adc5a0c6264612ef0ae50e485c7f96cb4a4dea0a79bfcd9a1231653c8ccb.jpg)


· Vector计算没有开启double buffer，计算和数据搬运部分没有并行。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/45945459de0eeceb3b14c644ba93b60dcd4d032c5c3061a9949c7ca54cfb2b6d.jpg)


# 设计优化方案

将AI Core中的AIC核和AIV核启动比例设置为1:2。每次AIC输出的数据，由两个AIV并行计算对应的反量化和激活函数；在Vector侧代码的循环里，AIV0和AIV1交替进行计算（前提条件，循环次数不为1）。代码示例如下：

```cpp
uint32_t vecCount = 0;
uint32_t taskRation = GetTaskRatio();
for (uint32_t offsetN = 0; offsetN < curCubeSingleN; offsetN += mnConfig.baseN) {
    if (unlikely(offsetN + mnConfig.baseN >= curCubeSingleN)) {
    curVecBaseN = curCubeSingleN - offsetN;
    }
    uint32_t alignBaseN = Ceil(curVecBaseN, uint32_t(8)) * 8;
    DataCopyScale(curVecBaseN, alignBaseN, scaleOffset + offsetN);
    uint32_t curVecBaseM = vecBaseM;
    uint64_t mmOutOffset = mnConfig.workSpaceOffset + offsetN * mnConfig.baseM;
    CrossCoreWaitFlag(SYNC_AIC_TO_AIV);
    for (uint32_t offsetM = 0; offsetM < curCubeSingleM; offsetM += vecBaseM) {
    vecCount++;
    if (vecCount % taskRation != subBlockIdx) {
    continue; // AIV0和AIV1交替进行计算
    }
    if (unlikely(offsetM + vecBaseM >= curCubeSingleM)) {
    curVecBaseM = curCubeSingleM - offsetM;
    }
    // 使用AscendDequant接口做perchannel反量化
    LocalTensor<cT::T> mmOutLocal = vecInQueue.AllocTensor<cT::T>();
    DataCopyPad2D(mmOutLocal, mmOutGm[mmOutOffset + offsetM * curVecBaseN], curVecBaseM, curVecBaseN, curVecBaseN);
    vecInQueue.EnQue(mmOutLocal);
    ComputeDequantAndActivate(mnConfig, curVecBaseM, alignBaseN, curVecBaseN, offsetM);
    LocalTensor<DTYPE_Y> yLocal = vecOutQueue.DeQue<DTYPE_Y>();
    DataCopyPad2D(yGm[outOffset + offsetM * tiling->n + offsetN], yLocal, curVecBaseM, curVecBaseN, alignBaseN, tiling->n);
    vecOutQueue.FreeTensor(yLocal);
    }
    ...
}
```

AIC和AIV启动比例设置为1:2后，出现Cube和Vector计算各自都有间隙、相互之间都有等待耗时的情况。分析原因是因为Vector和Cube计算存在使用一份workspace进行数据传递的场景，通过4份workspace的方案进行优化：host按4倍baseM * baseN申请workspace，Cube侧代码在计算前可以跳过前4轮的等待。

```cpp
if ASCEND_IS_AIC {
    if (cubeCount >= tiling->parallNum) { // tiling->parallNum设置为4
    CrossCoreWaitFlag(SYNC_AIV_TO_AIC);
    }
    mm.SetOrgShape(mnConfig.m, tiling->n, tiling->k);
    mm.SetSingleShape(curSingleM, curSingleN, tiling->k);
    mm.SetTensorA(xGm[xOffset]);
    auto weightSlice = weightGm[weightOffset];
    if (mnConfig.numBlocksM == 1) {
    weightSlice.SetL2CacheHint(CacheMode::CACHE_MODE_DISABLE);
    }
    mm.SetTensorB(weightSlice);
    uint64_t workspaceOffset = mnConfig.workSpaceOffset;
```

```java
while (mm.Iterate()) {
    mm.GetTensorC(mmOutGm[workspaceOffset], 0, true);
    CrossCoreSetFlag<2, PIPE_FIX>(SYNC_AIC_TO_AIV);
    workspaceOffset += (mnConfig.baseM * mnConfig.baseN);
}
cubeCount++; 
```

Vector计算开启double buffer，InitBuffer指定分配内存块个数为2。

```c
pipe->InitBuffer(scaleInQueue, 2, tiling->mmTilingData.baseN * sizeof(DTYPE_SCALE));
pipe->InitBuffer(perTokenScaleInQueue, 2, tiling->mmTilingData.baseM * sizeof(float));
pipe->InitBuffer(vecInQueue, 2, tiling->ubCalSize * sizeof(cT::T));
pipe->InitBuffer(vecOutQueue, 2, tiling->ubCalSize * sizeof(DTYPE_Y)); 
```

# 验证优化方案性能收益

将AI Core中的AIC和AIV启动比例设置为1:2后，执行总耗时从218.1us下降为154.2us。指令流水图显示Cube计算间等待变小。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/476f6e53bc0a4b6603d6ad3be7c2715531a491bacb09095e1e0c08dc0d2cc525.jpg)


如上图所示，Vector计算已经不处于bound状态，但Cube和Vector计算都有间隙，未被充分利用（上述两个箭头的位置）。分析原因如下：

Vector计算在等Cube计算输出的数据，Cube侧需要等Vector计算完释放workspace以存放下一轮的计算结果，当前为了让Cube、Vector计算流水并行，workspace使用了两份空间：

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/298e18c7a2d4c118460d9ef8491505c72c31831775fc55a1c6d045c90bc8d777.jpg)


因为Vector和Cube计算存在使用一份workspace进行数据传递的场景，存在数据依赖，所以会有等待的间隔。

可以采用4份workspace进行优化：

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/bda29297cab0db18856d52fb529645f153b06c3f44391d753332ec31f415b78d.jpg)


优化后，总耗时由154.2us下降为131.8us。指令流水图显示Vector、Cube计算各自间隙明显减小。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/86d1da15b4bc0266cc373a44ced2f99106d57c2df55d5607f1939066e4b7cb02.jpg)


. Vector计算开启double buffer，优化后执行总耗时从131.8us下降为128.1us。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/43b05c393bf0d8f65309e19b9185bc21f151cac54b74283743f96f9609835c1f.jpg)


# 总结

● 在Vector计算为主要瓶颈点时，将AI Core中的AIC核和AIV核启动比例设置为1:2；

Cube、Vector计算时间接近，且两者都有因相互等待导致的间隙时，采用4份workspace优化；

观察数据搬运是否与计算相互掩盖，多轮计算没有数据依赖，且buffer够大时，开启double buffer，增加并行效率。

# 3.10.3 MC²算子性能调优案例

# 案例介绍

MC2通算融合算子的性能收益主要来自于通信、计算的并行执行，即将输入数据切分为多个子块，子块的计算和通信任务形成两条流水线，通过两条流水线上任务的并行执行，实现流水掩盖，从而提升算子性能。如下图所示，MC2算子先做Matmul计算、后通信的场景，输入矩阵沿M轴被切分为两块，第二块数据的Matmul计算和第一块数据的通信可以并行执行，从而达到计算和通信时间相互掩盖的目的。本节的所有图示中MM代表Matmul计算，hcom代表通信任务。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/c21f46aa6e3d7116823a663328333e28359861698b0490590dc686c4e5c6a0f5.jpg)


本案例将介绍如何分析通算融合算子的性能收益、如何制定较好的数据切分策略。更多MC2算子的完整样例请参考MatmulAllReduce样例、MatmulReduceScatter样例、AllGatherMatmul样例。

# 获取性能数据

通过msProf算子调优工具获取算子性能数据：

获取真实环境执行的性能数据，包含各个流水的占比情况；

获取仿真性能数据（指令流水图），包含各个流水的占用区间，可观察流水间依赖情况，从而优化并行效率。

# 分析主要瓶颈点

${ \mathsf { M } } { \mathsf { C } } ^ { 2 }$ 算子性能收益公式为：

融合前算子串行耗时 = 融合前计算算子耗时 + 融合前通信算子耗时

${ \mathsf { M } } { \mathsf { C } } ^ { 2 }$ 算子收益 = (融合前算子串行耗时 - 融合后 ${ \mathsf { M } } { \mathsf { C } } ^ { 2 } .$ 算子耗时) / 融合前算子串行耗时融合后 ${ \mathsf { M } } { \mathsf { C } } ^ { 2 } .$ 算子的执行耗时，受以下因素制约，从而影响算子性能收益。

因素一：计算和通信的执行时间差异

若计算和通信任务的执行时间相差不大，则融合后， ${ \mathsf { M } } { \mathsf { C } } ^ { 2 }$ 算子的计算和通信并行执行，实现流水掩盖效果，性能收益较大。

若计算和通信任务的执行时间差异较大，则融合后，MC2算子内计算和通信并行执行，能够掩盖的时间较少，算子整体执行耗时与未切分串行时的算子执行耗时接近，此时无法获得较大性能收益。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/e37b5f1cd7549d6f5ab1bbb30a7af8e367d71a8d4a93f9752d87293cd6f42efa.jpg)


# 因素二：数据切分导致的计算或通信的执行时间膨胀

当对输入数据进行切分后，原本的整块数据被切分成若干小数据块，对若干小数据块分别做Matmul计算或者执行通信任务，此时相比切分前，计算或者通信任务的执行时间可能发生膨胀（即执行时间变长）。该膨胀产生的原因包括：切分后的数据块过小导致计算或通信的效率降低、切分的数据块过多导致增加额外的调度开销、并行执行后计算和通信对L2 Cache或device存储内存的访问冲突等。以Matmul计算为例，简单说明数据切分后执行时间可能发生的膨胀情况。

# 未发生膨胀：

数据切分前，Matmul执行时间为200us，将Matmul的输入均匀切分为两块，假设切分后，每块数据的Matmul执行时间都是100us，通过计算的并行执行，下图实际性能收益为100us。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/b7c4d534892aaba067b479b300b4a1fb50fea30d570f3aeedf093502cf63cafa.jpg)


# 发生一般程度的膨胀：

数据切分前，Matmul执行时间为200us，将Matmul的输入均匀切分为两块，假设切分后，每块数据的Matmul执行时间都是150us，通过计算的并行执行，下图实际性能收益为50us。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/0067a6eff3769bd12cb18f6ecb8b0f8901da99613fab3da6cc2ffe58326cffd1.jpg)


# 发生严重程度的膨胀：

数据切分前，Matmul执行时间为200us，将Matmul的输入均匀切分为两块，假设切分后，每块数据的Matmul执行时间都是200us，通过计算的并行执行，下图实际性能收益为劣化50us。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/478a18ada8afefdc7bf4abddb6dc9467fc4a72859630507f43dc26b3cca6cba7.jpg)


综合上述分析，计算和通信执行时间较均衡的场景，有更好的流水掩盖和性能收益；同时，性能收益也受到数据切分导致的执行时间膨胀的影响。下文将介绍如何制定数据切分策略，以达到最佳流水掩盖效果。

# 设计优化方案

以Atlas A2 训练系列产品/Atlas A2 推理系列产品，输入数据格式为ND、数据类型为half的MatmulAllReduce算子为例，该算子中计算执行在前，通信任务执行在后。假定Matmul计算中左矩阵的形状为[M, K]，右矩阵的形状为[K, N]，算子中的通信对象为Matmul的输出矩阵，则通信任务的输入shape为[M, N]。因为K轴只在Matmul计算中存在，当K轴较大时，计算量大，计算执行时间大于通信执行时间，此时计算为算子的瓶颈（bound）；反之，当K轴较小时，计算量小，计算执行时间小于通信执行时间，此时通信为算子的瓶颈。在制定数据切分策略前，对原始矩阵分别执行计算和通信任务，根据两个任务的执行时间判定bound场景。

该算子的数据切分策略应满足如下要求：

只切分M轴。因为通信任务调用的Hccl API要求分块数据内存连续，若按N轴切分，则每行数据都被切断，导致通信数据的内存不连续，不满足通信要求；若按M轴切分，则每行数据都是内存连续的，满足通信要求。

若A表示长块、B表示短块，只能切出A或B连续排布的形式，例如AAAB、BAAA等情况。

如上文所述，数据切分的目标是达成尽可能多的流水掩盖。根据计算与通信任务的执行时间差异，实际场景可以分解为如下两个具体场景，两个场景有各自细分的切分目标。

计算bound：

对于同一个切分后的数据块，计算执行耗时大于通信执行耗时，此时，计算连续，且通信的尾块要短，如下图所示。


图 3-162 计算 bound 示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/d01e3b93b62138322eda9cfb737db71b9d2d2f5e69733b7ce545c1cef4ac3b85.jpg)


通信bound：

对于同一个切分后的数据块，通信执行耗时大于计算执行耗时，此时，通信连续，且计算的头块要短，如下图所示。


图 3-163 通信 bound 示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/50c78185347a29d576983a4c7115b0441ce4de25d00faf5c3ca4ca253d2ea540.jpg)


# 前置工作：

在进行最终的数据切分前，需要做的前置工作有：判定bound场景、分别对Matmul计算和AllReduce通信的数据量与执行时间关系做公式拟合。具体步骤如下。

1. 将输入数据分别单独执行Matmul计算和AllReduce通信任务，利用msProf工具分别采集执行时间，判定时间较长的任务为对应的bound场景。例如通信执行时间大于计算执行时间，则为通信bound场景。

2. 将输入数据按M轴切分，分别切成M轴为256、512、768、1024、2048的若干数据块，这里可以根据实际情况调整切块大小。

3. 将步骤2得到的数据块分别做AllReduce通信，利用msProf工具采集执行时间，得到每个数据块的执行时间 $\operatorname { l t } _ { 1 } , \operatorname { t } _ { 2 } , . . . , \operatorname { t } _ { \ n \circ }$ 。然后，作图分析数据量与对应的执行时间的关系，拟合得到公式t $\mathbf { \xi } : = \mathsf { C o s t C o m m } ( \mathsf { m } )$ ，其中m表示数据块的M轴长度，t表示数据块的通信执行时间，CostComm表示拟合得到的m和t的映射关系。该映射关系一般为线性，若不满足线性关系，可以采用分段拟合的方式。示例如下：

<table><tr><td>m值</td><td>数据量</td><td>执行时间us</td></tr><tr><td>1</td><td>1M</td><td>42</td></tr><tr><td>2</td><td>2M</td><td>62.1</td></tr><tr><td>4</td><td>4M</td><td>109.8</td></tr><tr><td>8</td><td>8M</td><td>168.8</td></tr><tr><td>16</td><td>16M</td><td>284.8</td></tr></table>

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/e7993a7125344a54710e97366d96ea7bc824b3fbe7f9ad4f8c4ee71c9030e4db.jpg)


数据量 $\mathsf { x } = \mathsf { m } \cdot \mathsf { N } \cdot \mathsf { s i z e o f ( d a t a T y p e ) }$ ，单位是Bytes。该拟合公式表示为：

– x小于8MB： $\mathrm { t } = - 0 . 9 6 9 8 2 0 2 ^ { \star } \times ^ { \star } \times + 2 7 . 0 6 2 2 5 7 3 ^ { \star } \times + 1 4 . 7 6 9$ ，单位是us。

x大于等于8MB： $\mathrm { t } = 1 3 . 5 8 4 9 1 2 6 3 \mathrm { \Omega } ^ { \star } \times { } + 6 1 . 5 0 8 3 3 3$ ，单位是us。

4. 将步骤2切分得到的各数据块做Matmul计算，按与步骤3相同的方式采集各数据块的计算执行时间t，并拟合得到M轴长度和计算执行时间关系的公式t =CostMM(m)，CostMM是拟合得到的m和t的映射关系。

# 切分算法步骤：

1. 根据输入矩阵的shape：M、K、N，按照经验值设置合适的M轴方向切分的短块长度。如下表达式中，a、b、c表示根据经验值给出的短块长度的备选，将K、N带入如下三个表达式后，取a、b、c的最小值m0为选定的短块长度。

$- \quad \mathtt { \Large { a } } ^ { \star } \mathsf { K } ^ { \star } \mathsf { N } > = 4 ^ { \star } 1 0 2 4 ^ { \star } 1 0 2 4 ^ { \star } 1 0 2 4 , \mathtt { a } \mathtt { H } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt \mathtt { K } \mathtt { K } \mathtt { K } \mathtt { K } \mathtt \mathtt { K } \mathtt { K } \mathtt \mathtt { K } \mathtt { K } \mathtt \mathtt { K } \mathtt { K } \mathtt \mathtt { K } \mathtt { K } \mathtt \mathtt { K } \mathtt \mathtt { K } \mathtt \mathtt { K } \mathtt \mathtt { K } \mathtt \mathtt { K } \mathtt \mathtt { K } \mathtt \mathtt { K } \mathtt \mathtt { K } \mathtt \mathtt { K } \mathtt \mathtt { K } \mathtt \mathtt \mathtt { K } \mathtt \mathtt { K } \mathtt \mathtt \mathtt { K } \mathtt \mathtt \mathtt { K } \mathtt \mathtt \mathtt { K } \mathtt \mathtt \mathtt { K } \mathtt \mathtt \mathtt { K } \mathtt \mathtt \mathtt { K } \mathtt \mathtt \mathtt  K $ 

– $b ^ { \star } \mathsf { K } ^ { \star } \mathsf { N } / \uparrow 0 2 4 + \mathsf { b } ^ { \star } \mathsf { N } > = 6 ^ { \star } 1 0 2 4 ^ { \star } 1 0 2 4 , \mathsf { b } \sharp \nabla \mathcal { I } _ { \star } \frac { \llangle \phi } { \pmb { \Sigma } } \underline { { \hat { \mathbf { z } } } } \{ \hat { \mathsf { H } } \} _ { \boxed { \hat { \mathbf { z } } } } ^ { \boxplus } / \mathsf { N } \big / \boxed { \hat { \mathbf { z } } }$ 

c >= 3 * 128，c取不等式的最小值

$- \quad \mathsf { m o } = \mathsf { m i n } ( \mathsf { a } , \mathsf { b } , \mathsf { c } )$ 

2. 根据短块长度m0和拟合公式，分别得到计算执行时间 $\mathsf { t } 0 = \mathsf { C o s t M M } ( \mathsf { m } 0 ) \mathrel { \star } 1 . 1 5$ ，通信执行时间 $\mathsf { \Omega } _ { \mathsf { C } 1 } = \mathsf { C o s t C o m m } ( \mathsf { m } 0 ) \star \mathsf { 1 } . 1 5$ 。

注意：通信和计算并行执行时，可能出现抢占内存带宽的情况，导致执行时间增加，一般按经验在拟合公式中乘以1.15的系数，用户可以根据实测情况调整该系数。

3. 根据短块的长度，按图1或图2配平通算，得到长块的长度，长块长度尽量对齐128个元素，以保证计算亲和。本案例为通信bound场景，这里的配平，即将短块的通信时间和长块的计算时间匹配相等：将t1作为长块的计算执行时间，带入t1 =CostMM(m1)公式，计算得到m1，即为长块的长度。

4. 根据短块长度m0、长块长度m1、原始M轴长度M，得出长块的切块个数count =(M - m0) / m1。该式一般不能整除，此时需要做如下处理：

将结果的小数部分舍弃，保留整数部分作为切块个数。

由于舍弃了小数部分，M轴长度有剩余，因此需要调整长块长度m1 = (M -m0) / count。

为保持计算亲和，将长块长度m1调整至128对齐，即向下取128倍数的整数，更新长块长度m1。

由于调整m1后，M轴长度有剩余，因此调整短块长度m0 = M - (m1 *count)。

最终得到短块长度m0，长块长度m1，长块个数count。

# 验证优化方案性能收益

制定切分策略并验证性能收益

本MatmulAllReduce案例中，给定的输入矩阵Shape为M=4096，K=3072，N=8192，数据类型为half，分核数为8，通过融合前msProf工具采集，得到该输入的Matmul计算执行时间为803us，AllReduce通信的执行时间为1071us，总耗时1874us，属于通信bound场景。按照上述的切分算法，本案例的具体切分情况如下：

a. 根据经验值选定短块（bound场景中短块为头块，即切分后的第一个数据块）M方向长度m0为384，则通信数据量x为：384 * 8192 * 2 / 1024 / 1024= 6MB。按通信拟合公式估算通信的执行时间为143us。考虑可能会发生内存带宽冲突，因此再乘以1.15的系数，得出通信执行时间164us。

b. 计算长块M方向长度。根据短块通信时间，配平长块的计算执行时间同样为164us，按计算拟合公式估算出该长度m1为768。

c. 根据M=4096、m0=384、m1=768，计算长块个数：(4096 - 384) / 768 =4.83，向下取整为4。

d. 根据短块长度m0=384，长块个数4，调整长块m1长度：(4096 - 384) / 4 =928，向下按128对齐，调整m1为896。

e. 根据长块长度m1=896，长块个数4，调整短块m0长度：4096 - 896 * 4 =512。

f. 最终得到将原始输入矩阵切分为5个数据块，长度分别为：{512，896，896，896，896}。

如下代码所示，在算子的Tiling代码中设置制定好的切分策略。按该切分策略测试，融合后该算子的执行时间为1262us，则融合算子的性能收益为(1874 -1262) / 1874 = 32.7%。

```txt
MatmulAllReduceCustomTilingData *tiling = context->GetTilingData < MatmulAllReduceCustomTilingData>();
tiling->param.rankDim = 8;
tiling->param.tileM = 512; // 短块大小
tiling->param.tileNum = 1; // 短块个数
tiling->param.tailM = 896; // 长块大小
tiling->param.tailNum = 4; // 长块个数
```

```txt
tiling->param.rankM = 4096;
tiling->param.rankN = 8192;
tiling->param.rankK = 4096;
tiling->param.isTransposeA = 0;
tiling->param.isTransposeB = 0;
tiling->param.cToFloatLen = 0;
tiling->param.nd2NzWorkLen = true;
tiling->param.dataType = static_cast<uint8_t>(HCCL_DATA_TYPE_MAP.at(aType)); 
```

# 针对切分膨胀做调整

上文提到，切分数据会引起计算或通信执行时间的膨胀，使实测结果与理论值有偏差。比如切块数量较多时，执行时间的膨胀对性能影响较大，可能导致性能收益变小或者出现性能劣化，因此最终需要根据上述理论切分策略，结合实测，对切分策略做调整。

# 总结

MC2算子通过数据切分后计算和通信的并行执行，获得性能收益，但受数据切分后执行时间膨胀的影响。对MC2算子进行性能调优的主要方式是制定数据切分策略，开发人员需要根据理论推导找到理想切分策略，然后根据实测结果调整，最终找到最优切分策略。

# 3.10.4 Matmul 性能调优案例

# 3.10.4.1 Matmul 性能优化策略总览

本节提供了一系列包含Matmul计算的算子性能调优案例，开发者可根据实际应用场景，参考相关案例中的优化方法和思路，应用于具体实践中。案例分为如下五类，各分类的简介请参见如下表格，详细内容请阅读后续章节。

# Tiling优化


表 3-29 Tiling 优化策略总览


<table><tr><td>分类</td><td>适用场景</td><td>相关案例</td></tr><tr><td>Tiling优化:优化 Tiling分核及基本块切分策略。</td><td>数据量足够多的大Shape场景。</td><td>Matmul算子优化 Tiling策略</td></tr></table>

# 并行度优化


表3-30 并行度优化策略总览


<table><tr><td>分类</td><td>适用场景</td><td>相关案例</td></tr><tr><td>多核间任务并行:合理地将数据分配给不同的核来执行任务。</td><td>矩阵的K轴较大、M轴和N轴相比K轴较小的场景。</td><td>Matmul高阶API使能多核切K</td></tr><tr><td>多核间数据访问并行:优化多核数据并行访问机制,如多核场景同一内存数据的地址访问冲突优化,实现多核数据访问效率提升。</td><td>多核执行Matmul,输入矩阵的K轴较大且K轴非全载的场景。</td><td>Matmul高阶API使能多核K轴错峰访问内存</td></tr><tr><td>单核内流水并行:利用不同指令队列间的相互独立性和可并行执行特性,优化核内流水并行度。</td><td>1. 算子的MMAD流水和FIXPIPE流水之间串行执行,同步等待的时间在算子整体执行耗时中占比较高。2. MTE2 Bound且MTE2流水和其他流水串行执行。</td><td>1. Matmul高阶API使能UnitFlag2. Matmul高阶API使能NBuffer33模板</td></tr></table>

# 内存优化


表3-31 内存优化策略总览


<table><tr><td>分类</td><td>适用场景</td><td>相关案例</td></tr><tr><td>内存共享与复用:通过Buffer的共享与缓存复用,减少重复的数据搬运带来的开销。</td><td>MIX场景下,多个AIV的A矩阵或B矩阵GM地址相同,且多个AIV复用的A矩阵或B矩阵在L1 Buffer上全载。</td><td>Matmul高阶API使能IBShare模板共享A和B矩阵数据Matmul高阶API使能IBShare模板共享B矩阵数据</td></tr><tr><td>内存对齐:确保处理的数据满足特定的对齐要求,针对非对齐数据使用不同的搬运策略,以提升数据搬运的效率。</td><td>输入矩阵内轴非256字节对齐,且数据量较大的场景。</td><td>AIV核上的ND2NZ格式转换</td></tr></table>

# Scalar优化


表 3-32 Scalar 优化策略总览


<table><tr><td>分类</td><td>适用场景</td><td>相关案例</td></tr><tr><td>Tiling常量化:在Kernel编译期间完成Matmul Tiling的计算,由变量转化为常量扩散到系统中,减少Scalar提升性能。</td><td>Matmul初始化时的Scalar计算较多,影响指令头开销。Matmul迭代之间的Scalar计算较多,阻塞MTE2流水。</td><td>Matmul高阶API使能Tiling全量常量化</td></tr><tr><td>纯Cube模式:减少消息处理机制带来额外的Scalar开销。</td><td>相较于MIX模式,没有矢量计算,只有矩阵计算的场景。</td><td>Matmul高阶API使能纯Cube模式</td></tr></table>

# 搬运优化


表3-33 搬运优化策略总览


<table><tr><td>分类</td><td>适用场景</td><td>相关案例</td></tr><tr><td>搬运吞吐量优化:通过合理控制搬运数据块的大小,提升带宽利用效率,实现搬运效率的提升。</td><td>1. MTE2循环搬运次数多的大shape场景。2. 输入和输出的数据量超过L2 Cache大小的场景。</td><td>1. Matmul 高阶API使能MDL模板2. Matmul高阶API使能L2 Cache切 分</td></tr><tr><td>预加载搬运:预加载需要搬运的数据块,减少流水之间的间隙。</td><td>MTE2流水间隙较大,且M或N数值较大的场景。</td><td>Matmul高阶API使能MTE2 Preload</td></tr></table>

# 3.10.4.2 Matmul 算子优化 Tiling 策略

# 案例介绍

本案例对Matmul算子进行性能分析和优化。Matmul算子实现的功能是矩阵乘法，其中主要包含数据搬入和搬出流水，Cube计算流水。

以矩阵维度M = 4096，N = 5120，K = 4096，输入数据类型half，输出数据类型float，输出格式是ND为例，性能验证平台为Atlas A2 训练系列产品/Atlas A2 推理系列产品，介绍针对Matmul算子的优化手段，包括优化分核逻辑、优化基本块、使能大包搬运。

分核逻辑：开启尽量多的Cube核使能并行计算。

优化基本块，选择最优的baseM、baseN、baseK参数，其中baseM、baseN、baseK为Matmul Tiling中的参数。

使能大包搬运：从GM搬运数据到L1时，对于A矩阵，一次搬入depthA1个基本块，基本块大小为baseM * baseK，对于B矩阵，一次搬入depthB1个基本块，基本块大小为baseN * baseK。使能大包搬运后，一次搬入的数据量变大，从而提升MTE2搬运效率。

# 获取性能数据

使用msProf工具获取算子的Profiling数据，重点分析MTE2，Cube，Scalar pipeline的流水情况。

# 分析主要瓶颈点


图 3-164 优化前 Profiling 数据


```csv
Block Dim Mix Block [aicore_tim] aic_total_c aic_mac_tir aic_mac_ra aic_scalar laic_scalar laic_mte1_t aic_mte1_ra aic_mte2_t aic_mte2_ra aic_fixpipe aic_fixpipe
4 8 12045.68 89138067 3531.415 0.293 6057.364 0.503 6066.411 0.504 11723.63 0.973 3461.819 0.287 
```

由以上Profiling数据，可以看出MTE2耗时占比较大，当前性能瓶颈点在于MTE2流水。

Profiling数据的Block Dim可见分核未分满，考虑分核逻辑的优化。设CurrentCore是未优化前分核的Cube核数，MaxCore为最大Cube核数，当开启全部核并行做当前shape数据量的计算时，预估性能收益为MaxCore / CurrentCore的倍数。

优化基本块切分，将影响搬运数据的效率，算子搬运的总数据量为搬运的左矩阵和右矩阵数据量之和。在Matmul计算K方向不能全载的场景下，根据矩阵乘法的算法，搬运左矩阵的次数为N / baseN，搬运右矩阵的次数为M / baseM，即搬运总数据量totalCnt = (N / baseN) * M * K + (M / baseM) * K * N。预估性能收益为搬运数据量的比值，优化前搬运数据量totalCnt0/优化后搬运数据量totalCnt1，化简后结果为(1 / baseM0 + 1 / baseN0) / (1 / baseM1 + 1 /baseN1)，其中，baseM0, baseN0为优化前基本块参数，baseM1, baseN1为优化后基本块参数。

使能大包搬运后，指令条数变化、地址对齐等因素会影响性能，按照经验预估，对于MTE2为性能瓶颈的场景，会有20%以上的MTE2性能收益。

# 设计优化方案

优化点一：优化分核逻辑

由Profiling数据看出分核数为4，启动更多的核同时计算，可以提高计算并行度。当前案例使用的AI处理器共20个核，每个核中包含1个Cube Core和2个VectorCore。NPU调用程序中设置numBlocks为实际使用的核数20。

```cpp
// 代码片段
uint32_t numBlocks = 20; // 优化前numBlocks为4
CHECK_ACL(aclInit(nullptr));
int32_t deviceId = 0;
CHECK_ACL(aclrtSetDevice(deviceId));
aclrtStream stream = nullptr;
CHECK_ACL(aclrtCreateStream(&stream));

uint8_t *aHost;
uint8_t *aDevice;
CHECK_ACL(aclrtMallocHost((void **)(&aHost), aFileSize));
CHECK_ACL(
    aclrtMalloc((void **)&aDevice, aFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
ReadFile("./input/x1_gm.bin", aFileSize, aHost, aFileSize);
// PrintData(aHost, 16, printDataType::HALF);
CHECK_ACL(aclrtMemcpy(aDevice, aFileSize, aHost, aFileSize,
    ACL_MEMCPY_HOST_TO_DEVICE));

uint8_t *bHost;
uint8_t *bDevice;
CHECK_ACL(aclrtMallocHost((void **)(&bHost), bFileSize));
CHECK_ACL(
    aclrtMalloc((void **)&bDevice, bFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
ReadFile("./input/x2_gm.bin", bFileSize, bHost, bFileSize);
// PrintData(bHost, 16, printDataType::HALF);
CHECK_ACL(aclrtMemcpy(bDevice, bFileSize, bHost, bFileSize,
    ACL_MEMCPY_HOST_TO_DEVICE));

uint8_t *workspaceHost;
```

```c
uint8_t *workspaceDevice;
CHECK_ACL(aclrtMallocHost((void **)(&workspaceHost), workspaceSize));
CHECK_ACL(aclrtMalloc((void **)&workspaceDevice, workspaceSize, ACL_MEM_MALLOC_HUGE_FIRST));

uint8_t *tilingHost;
uint8_t *tilingDevice;
CHECK_ACL(aclrtMallocHost((void **)(&tilingHost), tilingFileSize));
CHECK_ACL(aclrtMalloc((void **)&tilingDevice, tilingFileSize, ACL_MEM_MALLOC_HUGE_FIRST));
CHECK_ACL(aclrtMemcpy(tilingHost, tilingFileSize, GenerateTiling(), tilingFileSize, ACL_MEMCPY_HOST_TO_HOST));
// PrintData(tilingHost, 16, printDataType::UINT32_T);
CHECK_ACL(aclrtMemcpy(tilingDevice, tilingFileSize, tilingHost, tilingFileSize, ACL_MEMCPY_HOST_TO_DEVICE));

uint8_t *cHost;
uint8_t *cDevice;
CHECK_ACL(aclrtMallocHost((void **)(&cHost), cFileSize));
CHECK_ACL( aclrtMalloc((void **)&cDevice, cFileSize, ACL_MEM_MALLOC_HUGE_FIRST));

// ACLRT_LAUNCH_KERNEL(matmul_custom)
// (numBlocks, stream, aDevice, bDevice, cDevice, workspaceDevice, tilingDevice);
matmul_custom_do(numBlocks, stream, aDevice, bDevice, cDevice, workspaceDevice, tilingDevice); 
```

由于Matmul API都是从Vector侧发起的，当前案例使用的AI处理器中Cube Core和Vector Core的配比为1 : 2，所以在Matmul tiling计算中需要按照2倍的numBlocks数切分，即Vector Core数。NPU调用程序中设置的实际运行核数是20核，所以Tiling代码中设置Tiling API按照40个核进行数据切分，如下代码所示。

intusedCoreNum $= 40$ ;//优化前usedCoreNum是8  
int runMode $= 1$ int32_t baseM $= 64$ ; //64  
int32_t baseN $= 64$ ; //64  
optiling::TCubeTiling tilingData;  
MultiCoreMatmulTiling tilingApi;  
tilingApi.SetDim(usedCoreNum);


图 3-165 优化分核逻辑后 Profiling 数据


<table><tr><td>aicore_tim</td><td>aic_total_c</td><td>aic_mac_tir</td><td>aic_mac_ra</td><td>aic_scalar</td><td>aic_scalar</td><td>aic_mte1</td><td>taic_mte1_r</td><td>aic_mte2</td><td>taic_mte2_r</td><td>aic_fixpipe</td><td>aic_fixpipe</td></tr><tr><td>2532.53</td><td>93703665</td><td>706.283</td><td>0.279</td><td>1225.87</td><td>0.484</td><td>1201.55</td><td>0.474</td><td>2452.38</td><td>0.968</td><td>724.456</td><td>0.286</td></tr></table>

修改代码后，算子执行时间从12045us下降到2532us，约等于(20核 / 4核) = 5倍的性能提升。

# 优化点二：优化基本块

当前Tiling中设置的base块为 [baseM, baseN, baseK] = [64, 64, 256]，这种基本块Cube计算cycle少，计算访存比（即计算量与需要数据量的比值）低；搬出一次Matmul结果到GM的base块是64 * 64，由于输出格式是ND，数据类型是float，搬出下一次Matmul结果的起始地址需要偏移一个baseN的大小，即64 * 4 = 256字节，导致fixpipe搬出时GM地址非512byte对齐，那么需要设置更优的基本块。

针对当前shape较大的场景，基本块的选择原则为计算访存比最大，即在Cube计算量最大的情况下，访存的数据量最小。在输入为fp16类型的情况下，Cube执行单元1 cycle能算16 * 16 * 16个数。根据经验，[baseM, baseN, baseK] = [128,256, 64]和[128, 128, 128]两种切分方案均满足搬出时GM地址512Byte对齐（每搬出一次Matmul结果时，地址分别偏移256 * 4byte和128 * 4byte），Cube计算cycle数一致，为(128 * 64 * 256) / (16 * 16 * 16) = (128 * 128 * 128) / (16 * 16* 16) = 512cycle。针对[baseM, baseN, baseK] = [128, 256, 64]，计算访存比为512cycle / (128 * 64 * 2 + 256 * 64 * 2) = 512cycle / 48KB；针对[baseM,baseN, baseK] = [128, 128, 128]，计算访存比为512cycle / (128 * 128 * 2 + 128* 128 * 2) = 512cycle / 64KB；可见[128, 256, 64]基本块方案的计算访存比更

高，计算密度更大，同样的计算量，需要的数据量最小，最大限度提高Cube单元的计算量。

修改Tiling代码，通过SetFixSplit()接口设置baseM和baseN，tiling函数会自动计算出最优baseK，这里得到64。

```c
int32_t baseM = 128; // 优化前baseM是64
int32_t baseN = 256; // 优化前baseN是64
```

```cpp
optiling::TCubeTiling tilingData;
MultiCoreMatmulTiling tilingApi;
tilingApi.SetDim(usedCoreNum);
tilingApi.SetAType(leftPos, leftFormat, leftDtype, bool(transposeA));
tilingApi.SetBType(rightPos, rightFormat, rightDtype, bool(transposeB));
tilingApi.SetCType(resPos, resFormat, resDtype);
tilingApi.SetBiasType(biasPos, biasFormat, biasDtype);

tilingApi.SetOrgShape(M, N, K);
tilingApi.SetShape(M, N, K);
tilingApi.SetFixSplit(baseM, baseN, -1); 
```

使能这组基本块后，MTE2耗时（对应aic_mte2_time）从2452us降低到808us，MTE2性能提升3倍。


图 3-166 优化基本块后 Profiling 数据


```txt
aicore_tim(aic_total_c) aic_mac_tiaic_mac_ra aic_scalar_taic_scalar_1aic_mte1_taic_mte1_raic_mte2_taic_mte2_raic_fixpipe_aic_fixpipe_835.63 30918284 615.797 0.737 618.704 0.74 415.068 0.497 808.522 0.968 87.006 0.104 
```

# 优化点三：使能大包搬运

当前带宽利用率为：totalSize / mte2Time = totalCnt * dtype / mte2Time，代入数据计算为2491GB/s。未使能大包搬运的情况下，矩阵从GM搬运到L1一次只搬运1个基本块。通过模板参数使能大包搬运，一次搬运多个基本块，提高MTE2带宽利用率。

```cpp
// 原始matmul对象定义:
Matmul<AscendC::MatmulType<TPosition::GM, CubeFormat::ND, A_T>,
AscendC::MatmulType<TPosition::GM, CubeFormat::ND, B_T>,
AscendC::MatmulType<TPosition::GM, CubeFormat::ND, C_T>,
AscendC::MatmulType<TPosition::GM, CubeFormat::ND, BiasT>>>
mm;
// 通过在定义matmul对象的模板参数里加上CFG_MDL参数使能大包搬运功能:
Matmul<AscendC::MatmulType<TPosition::GM, CubeFormat::ND, A_T>,
AscendC::MatmulType<TPosition::GM, CubeFormat::ND, B_T>,
AscendC::MatmulType<TPosition::GM, CubeFormat::ND, C_T>,
AscendC::MatmulType<TPosition::GM, CubeFormat::ND, BiasT>, CFG_MDL>> mm;
```

从下图可以看到，使能大包搬运后，MTE2耗时从808us下降到591us，带宽利用率代入数据计算为3406GB/s，利用率提升36%+，Cube利用率达到80%+。


图 3-167 使能大包搬运后 Profiling 数据


```txt
aicore_tim_ aic_total_c_ aic_mac_tir_ aic_mac_ra_ aic_scalar_ aic_scalar_ aic_mte1_ taic_mte1_ raic_mte2_ taic_mte2_ raic_fixpipe_ aic_fixpipe_ 710.46 26286857 581.815 0.819 472.152 0.665 481.476 0.678 591.088 0.832 26.882 0.038 
```

# 验证优化方案性能收益

优化分核逻辑，实际收益4.75倍，约等于(20核 / 4核) = 5倍收益，并且考虑到核的启动开销，可以认为收益一致。

优化基本块，实际收益约3倍，理论评估代入上述分析公式，收益为(1 / 64 + 1 /64) / (1 / 128 + 1 / 256)，约等于2.7倍，考虑到cache缓存的影响，认为收益一致。

大包搬运，实际收益25%+，与经验值一致。

# 总结

优化点一和优化点二的适用场景，需要shape足够大，数据量足够多，才能分满核和使能最优的基本块。大shape场景下，MTE2 Bound算子可参考此案例的优化手段。

# 3.10.4.3 Matmul 高阶 API 使能纯 Cube 模式

# 案例介绍

本案例呈现了在矩阵乘算子场景中，使能Matmul高阶API的纯Cube模式对算子性能的提升效果。如下图所示，Matmul API默认使用MIX模式，即用户从AIV侧发起消息，通过消息通信框架中转消息后，在AIC侧执行Matmul计算。这套消息处理机制会带来额外的Scalar性能开销。相较于MIX模式，纯Cube模式可以直接跳过消息通信框架，完成Matmul计算，提升算子性能。


图 3-168 默认 MIX 模式的 Matmul 流程示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/71e9d5a747c3f1c470a38a721f4a91179d4e81b484480a3cc74f2774a5b3937b.jpg)


使能纯Cube模式的适用场景

非融合算子，只有矩阵计算的场景。即相较于MIX模式（包含矩阵计算和矢量计算），没有矢量计算的场景。本案例的算子规格如下：


表3-34 算子用例规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>a</td><td>128, 64</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>64, 30720</td><td>float16</td><td>ND</td></tr></table>

当前案例使用的AI处理器共24个核，每个核中包含1个AIC核和2个AIV核。

Tiling参数如下：

原始shape：M=128, N=30720, K=64。

单核shape：

MIX场景：按48个AIV核进行切分，singleCoreM=128，singleCoreN=640，singleCoreK=64。

纯Cube场景：按24个AIC核进行切分，singleCoreM=128，singleCoreN=1280，singleCoreK=64。

基本块shape：baseM=128，baseN=256，baseK=64。

L1相关Tiling参数：stepM=1，stepN=1，stepKa=4，stepKb=4，depthA1=8，depthB1=8。

# 获取性能数据

使用msProf工具获取算子仿真流水图和上板Profiling数据。因为纯Cube模式主要优化Scalar流水性能，可以重点分析Scalar的流水情况。

# 分析主要瓶颈点

优化前的Profiling数据如下，从C列的aic_time数据可以看出，多个核中最大算子执行耗时为17.85us。从G列的aic_scalar_time数据可以看出，Scalar平均耗时为15.02us，性能瓶颈在Scalar流水。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/aa0c53c7366ac259de14211aade04ed3061b083720532d93a6e16a4619271bca.jpg)


优化前的流水图如下，由于默认为MIX模式，每次Matmul计算均涉及消息通信框架对消息进行处理，Scalar流水重，性能开销较大，如下图红框所示。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/b4a72dc83d5303f651bd04c97cb52aa74d7949585b0fea071941f0071c304524.jpg)


# 设计优化方案

默认MIX模式下，用户在AIV侧发起消息，通过消息通信框架中转消息后，在AIC侧执行Matmul计算。基于这样的流程，用户使用Matmul高阶API编写算子代码时，可以使用REGIST_MATMUL_OBJ宏，无需区分AIV和AIC，但也因这套消息处理机制导致产生了额外的性能开销，如图1 默认MIX模式的Matmul流程示意图所示。

实现默认MIX模式的具体步骤如下：

步骤1 Kernel侧，定义Matmul对象。

#include "lib/matmul_intf.h" 

```julia
using A_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, AType>;
using B_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BType>;
using C_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, CType>;
using BIAS_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BiasType>;
AscendC::Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, CFG_NORM> matmulObj; 
```

步骤2 Host侧，Matmul多核Tiling对象调用SetDim接口设置参与运算的核数。

```cpp
auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance();
matmul_tiling::MultiCoreMatmulTiling cubeTiling(*ascendcPlatform);
int32_t numBlocks = ascendcPlatform->GetCoreNumAiv(); // MIX模式使用GetCoreNumAiv获取AI处理器可用的核数。
cubeTiling.SetDim(numBlocks);
```

步骤3 调用核函数，参考核函数定义和调用，设置核函数的numBlocks参数配置。

```txt
matmul_custom_do(ascendcPlatform->GetCoreNumAic(), stream, x1, x2, bias, y, workspaceDevice, tilingDevice); // MIX模式下，启动时，按照AIV和AIC组合启动，numBlocks用于设置启动多少个AI Core。
```

----结束

在没有矢量计算的算子场景下，可以跳过消息通信框架的机制，使能纯Cube模式完成Matmul计算，减少消息通信的性能开销，提升算子性能。


图 3-169 纯 Cube 模式的 Matmul 流程示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/9d44c402b62e26f9095f839170095fcc59ca2a4c43039162b6c7ca77a0d95188.jpg)


Matmul API使能纯Cube模式的完整样例请参考Matmul API性能优化样例。使能纯Cube模式的主要步骤如下：

步骤1 Kernel侧，在定义Matmul对象的代码中，包含matmul_intf.h头文件前设置ASCENDC_CUBE_ONLY宏。

```c
#define ASCENDC_CUBE_ONLY // 在#include "lib/matmul_intf.h"前，设置ASCENDC_CUBE_ONLY宏
#include "lib/matmul_intf.h"
```

```julia
using A_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, AType>;
using B_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BType>;
using C_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, CType>;
using BIAS_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BiasType>;
AscendC::Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, CFG_NORM> matmulObj; 
```

步骤2 Host侧，Matmul多核Tiling对象调用SetDim接口设置参与运算的核数。

```cpp
auto ascendcPlatform = platform_ascendc::PlatformAscendCManager::GetInstance();
matmul_tiling::MultiCoreMatmulTiling cubeTiling(*ascendcPlatform);
int32_t numBlocks = ascendcPlatform->GetCoreNumAic(); // 纯Cube模式使用GetCoreNumAic接口获取AI处理器可用的核数。
cubeTiling.SetDim(numBlocks);
```

步骤3 调用核函数，参考核函数定义和调用，设置核函数的numBlocks参数配置。

```txt
matmul_custom_do(ascendcPlatform->GetCoreNumAic(), stream, x1, x2, bias, y, workspaceDevice, tilingDevice); // 仅包含Cube计算的算子，numBlocks用于设置启动多少个AIC。
```

步骤4 Kernel侧，核函数实现中增加AIV侧返回分支。

```txt
extern "C" __global__ __aicore__ void matmul_custom(GM_ADDR a, GM_ADDR b, GM_ADDR bias, GM_ADDR c, GM_ADDR workspace, GM_ADDR tilingGm)
{
    if (g_coreType == AscendC::AIV) { // 纯Cube模式，AIV侧直接return return;
    }
    ...
    // 其他代码
}
```

----结束

# 验证优化方案性能收益

优化后的Profiling数据如下，从C列的aic_time数据来看，多个核中最大算子执行耗时为11.21us，较优化前的17.85us有较大提升。从G列的aic_scalar_time数据来看，Scalar平均耗时从优化前的15.02us降低至5.17us。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/e30bef7aee4423ccafbf02be9391478d370be7757810ada5d7101398ab3e972c.jpg)


优化后的流水图如下。对比优化前的流水图，红框所示位置的Scalar流水明显变稀疏。纯Cube模式相较于MIX模式，减少了对消息通信的处理，优化了整体Scalar性能开销。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/9188e935c0e3d8169ed34f5b2ff601912de3f4fe78dffb5b0bf49ffed4a9021d.jpg)


# 总结

在只有矩阵计算，没有矢量计算的场景下，可以考虑使能纯Cube模式，优化Matmul计算中的消息通信性能开销，提升算子性能。

# 3.10.4.4 Matmul 高阶 API 使能 MDL 模板

# 案例介绍

本案例呈现了在矩阵乘算子场景中，使用Matmul高阶API进行矩阵乘法计算，使能MDL模板对算子性能的提升效果。在MDL模板中，MTE2流水从Global Memory到A1/B1的数据搬运为一次性大包搬运，即一次MTE2能搬入多个Matmul计算的基本块，提升带宽利用率，使后续的MTE1流水尽可能复用A1/B1内基本块的缓存数据，减少MTE2的搬运次数。MDL模板的详细介绍请参考MatmulConfig。

MDL模板的适用场景

一般适用于MTE2循环搬运次数多的大shape场景，MDL模板在A1/B1中缓存多次计算需要的数据，避免MTE2频繁搬运。

MDL模板的约束条件

MDL模板的TCubeTiling结构体需要满足TCubeTiling约束条件和MDL模板补充约束条件，具体请参考TCubeTiling结构体。

本案例的算子规格如下：


表 3-35 算子规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>a</td><td>128, 1024</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>1024, 30720</td><td>float16</td><td>ND</td></tr></table>

当前案例使用的AI处理器共24个核，每个核中包含1个AIC核和2个AIV核。

Tiling参数如下：

原始shape：M=128, N=30720, K=1024。

单核shape：按24个AIC核进行切分，singleCoreM=128，singleCoreN=1280，singleCoreK=1024。

对于B矩阵，沿着N轴进行切分，切分成24份的singleCoreN，单核上处理K *SingleCoreN大小的数据。对于A矩阵，M轴不进行切分即singleCoreM=M，单核上处理singleCoreM * K大小的数据。总共24个核参与计算。

基本块shape：baseM=128，baseN=256，baseK=64。

L1相关Tiling参数：stepM=1，stepN=1，stepKa=4，stepKb=4，depthA1=8，depthB1=8。

# 获取性能数据

使用msProf工具获取算子仿真流水图和上板Profiling数据，因为MDL模板主要优化MTE2搬运效率，重点分析MTE2的流水情况。

# 分析主要瓶颈点

优化前的Profiling数据如下，Matmul默认为Norm模板。从C列的aic_time数据可以看出，多个核中最大算子执行耗时为83.68us。从C列的aic_time、L列的aic_mte2_time和M列的aic_mte2_ratio几组数据来看，MTE2平均耗时75.64us，耗时占比达到92%以上，因此需要优化MTE2流水的耗时。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/38f9638803202e6c84e4207a33f6080c81f5905d4593ca80238de19b8d6867f1.jpg)


优化前的流水图如下，MTE2分多次从Global Memory搬运基本块到A1/B1。由于输入的矩阵Shape较大，MTE2循环搬运的次数多，但每次只搬运1个基本块，导致带宽利用率低，整体的MTE2搬运耗时长。进而影响后续的MTE1和MMAD流水，导致流水之间同步等待时间偏长。如红框所示，第一个基本块（baseM*baseN）的计算需要调用16次MMAD指令（singleCoreK/baseK=16），从左侧的第1个MMAD指令调用开始，到右侧的第16个MMAD指令调用结束，期间耗时10.899us，其中大部分是流水同步等待耗时。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/36c9f8075ec162c2cfa42fd36d166eaf38c492593751de3a81b665f70ddcee13.jpg)


# 设计优化方案

下图是默认的Norm模板的Matmul计算流水示意图，MTE2分多次从Global Memory搬运基本块到A1或B1，每次只搬运一个基本块。Norm模板的优势为启动开销小，可以提前启动MTE1流水；Norm模板的劣势为在大Shape场景，MTE2搬运次数多，搬运带宽利用率低，整体性能开销大。


图3-170 默认Norm模板流水示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/996229c9001ce6a4324a9665bad57e6b6b5cadea95c4e142a9c252031fe334c8.jpg)


实现Norm模板的具体步骤如下：

步骤1 创建Matmul对象，使用默认的Norm模板参数CFG_NORM。

```cpp
#define ASCENDC_CUBE_ONLY
#include "lib/matmul_intf.h"

using A_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, AType>;
using B_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BType>;
using C_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, CType>;
using BIAS_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BiasType>;
AscendC::Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, CFG_NORM> matmulObj; // 使用CFG_NORM定义 Matmul对象
```

----结束

下图是MDL模板的Matmul计算流水示意图，MTE2一次性从Global Memory搬运多个基本块到A1或B1，每次搬运stepM * stepKa个基本块到A1或搬运stepN * stepKb个基本块到B1。MDL模板的优势为MTE2一次性搬运多个基本块，带宽利用率高，后续的MTE1流水能尽可能复用A1或B1的缓存数据，MTE2重复搬运次数少。MDL模板的劣势为MTE2头开销时间较长，MTE1流水需要等待MTE2流水完成后才启动，MTE1启动时间晚。


图 3-171 MDL 模板流水示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/98038ab805f9943be3e2b39dc6ecdf4db0233b67ce078e950e737be7fdc6d76b.jpg)


Matmul API使能MDL模板的完整样例请参考Matmul API性能优化样例。使能MDL模板的主要步骤如下：

步骤1 创建Matmul对象，使用默认的MDL模板参数CFG_MDL。

```cpp
#define ASCENDC_CUBE_ONLY
#include "lib/matmul_intf.h"

using A_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, AType>;
using B_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BType>;
using C_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, CType>;
using BIAS_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BiasType>;
AscendC::Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, CFG_MDL> matmulObj; // 使用CFG_MDL定义 Matmul对象
```

# ----结束

# 验证优化方案性能收益

优化后的Profiling数据如下，从C列的aic_time数据可以看出，多个核中最大算子执行耗时为53.4us，相较于优化前的83.68us有较大提升。从L列的aic_mte2_time数据可以看出，MTE2平均耗时下降较多，从优化前的75.64us降低至46.24us。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/19a1a9b68ac0ab5cf4a3538a6fa3153a90d871f9a8ff575012a919cdadb9d8ef.jpg)


优化后的流水图如下，MDL模板相较于默认的Norm模板，MTE2可以一次性搬运多个基本块，整体的MTE2搬运次数减少了。同时因为MTE2一次搬运多个基本块到A1/B1，后续的MTE1流水能尽量复用A1/B1的缓存数据，减少了流水同步等待，提升了算子整体性能。如红框所示，第一个基本块（baseM*baseN）的计算需要调用16次MMAD指令（singleCoreK/baseK=16），从左侧的第1个MMAD指令调用开始，到右侧的第16个MMAD指令调用结束耗时约5.198us，较优化前的10.899us提升较大，其中流水同步等待时间大幅减少。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/ee6afbc3c18d71238c2458f4f04e7c9519ca762769930ab6a51ca25845261269.jpg)


# 总结

大Shape输入、MTE2搬运次数多，且MTE1流水等MTE2流水的同步等待耗时较长的场景下，可以使能MDL模板。通过实现MTE2从Global Memory一次性搬入多个基本块到A1或B1，使后续的MTE1流水能尽量复用A1/B1的缓存数据，减少MTE2的搬运次数，从而提升算子性能。

# 3.10.4.5 Matmul 高阶 API 使能 UnitFlag

# 案例介绍

本案例呈现了在矩阵乘算子场景中，使用Matmul高阶API进行矩阵乘法计算，使能UnitFlag功能对算子性能的提升效果。UnitFlag功能为AIC核中MMAD计算指令和FIXPIPE数据搬运指令提供了基于内存访问的细粒度同步，使计算与搬运流水并行。使能UnitFlag功能的方式为将MatmulConfig中的enUnitFlag参数设置为true。enUnitFlag参数的详细介绍请参考MatmulConfig。

# 使能UnitFlag的适用场景

算子的MMAD流水和FIXPIPE流水之间串行执行，FIXPIPE等待MMAD计算完成才搬出结果，这个指令同步等待的时间在算子整体执行耗时中占比较高。这种场景可以使能UnitFlag功能，以获得MMAD和FIXPIPE流水并行的性能收益。如果算子原本的MMAD、FIXPIPE流水可以被其他流水掩盖（比如MTE2 Bound），这时使能UnitFlag功能总体收益很小。

# 使能UnitFlag的约束条件

UnitFlag功能仅支持Norm、IBShare、MDL三个模板。

使能UnitFlag功能时，不支持算子内同时存在CO1(L0C)搬出到GlobalMemory和A1(L1)搬出到Global Memory的两种流水。

使能UnitFlag功能时，若同时使能L0C累加功能，不支持多次Iterate计算、一次GetTensorC输出。

本案例的算子规格如下：


表3-36 算子规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>a</td><td>128, 64</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>64, 30720</td><td>float16</td><td>ND</td></tr></table>

当前案例使用的AI处理器共20个核，每个核包含1个AIC核和2个AIV核。

算子的Tiling参数如下：

原始shape：M=128, N=30720, K=64。

单核shape：按20个AIC核进行切分，singleCoreM=128，singleCoreN=1536，singleCoreK=64。

对于B矩阵，沿着N轴进行切分，切分成20份singleCoreN，单核上处理K *SingleCoreN大小的数据。对于A矩阵，M轴不进行切分即singleCoreM=M，单核上处理singleCoreM * K大小的数据。总共20个核参与计算。

基本块shape：baseM=128，baseN=256，baseK=64。

L1相关Tiling参数：stepM=1，stepN=1，stepKa=4，stepKb=4，depthA1=8，depthB1=8。

# 获取性能数据

使用msProf工具获取算子仿真流水图和上板Profiling数据。因为UnitFlag功能主要优化MMAD和FIXPIPE流水串行问题，所以获取性能数据后重点分析Cube、FIXPIPE的流水情况。

# 分析主要瓶颈点

优化前的流水图如下。如下图中红框所示，每一轮MMAD计算流水和FIXPIPE数据搬出流水之间都是串行执行的，完成MMAD计算后才开始FIXPIPE数据搬出，考虑实现MMAD与FIXPIPE之间流水并行来优化算子性能。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/aa961bbc45435da31d1395233d99170e339b74daf0e510be8389702e711fbe1e.jpg)


优化前的Profiling数据如下，从C列的aic_time数据可以看出，多个核中最大算子执行耗时为37.39us。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/ec09354ea3abe6b3f2feae5d3f6be0a0fd476c32efbfe82775ab3e9f460b2f0a.jpg)


# 设计优化方案

如下图所示，未开启UnitFlag功能时，MMAD和FIXPIPE是指令级别的同步，FIXPIPE指令需要等MMAD指令执行完成才进行结果搬出，MMAD和FIXPIPE之间流水串行。


图 3-172 未开启 UnitFlag 功能


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/c8bba23c81d43c636e301bd7f7090b2886c5f4403da1cc2199811f2e975ebe0d.jpg)


如下图所示，开启UnitFlag功能时，MMAD和FIXPIPE指令是512B大小的细粒度同步。在一条MMAD指令执行过程中，每当完成一个512B数据结果的计算，FIXPIPE立即开始搬出该512B的数据，从而实现MMAD和FIXPIPE之间的流水并行，提升算子性能。


图 3-173 开启 UnitFlag 功能


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/cb21d0d5ae614c82228ea88fcdd2232920086881dedd05d191f2d545b1b55bd0.jpg)


Matmul API使能UnitFlag功能的完整样例请参考Matmul API性能优化样例。使能UnitFlag功能的主要步骤如下：

步骤1 自定义MatmulConfig模板参数，将其中的enUnitFlag参数设置为true，使能UnitFlag功能。

```txt
__aicore__ inline constexpr MatmulConfig GetCustomMDLCFG()
{
    auto mmCfg = CFG_MDL;
    mmCfg.enUnitFlag = true;
    return mmCfg;
}
constexpr static MatmulConfig CUSTOM_CFG_MDL = GetCustomMDLCFG(); 
```

步骤2 基于自定义的MatmulConfig模板参数，创建Matmul对象。

```julia
using A_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, AType>;
using B_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BType>;
using C_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, CType>;
using BIAS_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BiasType>;
AscendC::Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, CUSTOM_CFG_MDL > matmulObj; 
```

----结束

# 验证优化方案性能收益

优化后的流水图如下，MMAD计算流水和FIXPIPE数据搬出流水之间实现了流水并行。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/dccbabfd569aa73b702c1c4de11567552afa0aa9a36e6c55773fe69b87ef1db7.jpg)


优化后的Profiling数据如下，从C列的aic_time数据可以看出，多个核中最大算子执行耗时为34.66us，较优化前的37.39us有约7.3%的性能提升。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/643659b283b113ac1adabf4aa3a0a76bc5c7718596797ddf4c82dffb99368a70.jpg)


# 总结

在算子的MMAD计算流水和FIXPIPE数据搬出流水串行且未被其他流水掩盖（比如MTE2 Bound）时，考虑使能UnitFlag功能，实现MMAD计算流水和FIXPIPE数据搬出流水的流水并行，提升算子性能。

# 3.10.4.6 Matmul 高阶 API 使能 Tiling 全量常量化

# 案例介绍

本案例呈现了在使用Matmul高阶API进行矩阵乘法计算时，使能Matmul Tiling全量常量化对算子性能的提升效果。Matmul API在初始化和迭代过程中有大量Scalar计算，Matmul初始化时的Scalar计算影响指令头开销，Matmul迭代间的Scalar计算可能阻塞MTE2流水。在调用Matmul API实现矩阵乘法时，使用MatmulApiStaticTiling参数替代TCubeTiling变量参数，将Scalar计算提前到编译期进行，以减少运行时的Scalar计算开销，实现算子性能的提升。

Matmul Tiling常量化的适用场景：

Matmul初始化时的Scalar计算较多，影响指令头开销。

Matmul迭代之间的Scalar计算较多，阻塞MTE2流水。

Matmul Tiling常量化需要在编译期确定部分Tiling参数，根据确定参数的不同，分为全量常量化和部分常量化两种场景，使用Matmul Tiling常量化需要满足两种场景中任一场景的条件：

全量常量化：能够确定常量singleCore Shape（singleCoreM/singleCoreN/singleCoreK）和常量base Shape（basicM/basicN/basicK，也称baseM/baseN/baseK）。

部分常量化：能够确定常量base Shape（basicM/basicN/basicK，也称baseM/baseN/baseK）。

其中，全量常量化场景比部分常量化场景可以减少更多的Scalar计算开销。

本案例的算子规格如下：


表 3-37 算子规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>a</td><td>128, 64</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>64, 30720</td><td>float16</td><td>ND</td></tr></table>

当前案例使用的AI处理器共24个核，每个核中包含1个AIC核和2个AIV核。

Tiling参数如下：

原始shape：M=128, N=30720, K=64。

单核shape：按24个AIC核进行切分，singleCoreM=128，singleCoreN=1280，singleCoreK=64。

对于B矩阵，沿着N轴进行切分，切分成24份的singleCoreN，单核上处理K *singleCoreN大小的数据。对于A矩阵，M轴不进行切分即singleCoreM=M，单核上处理singleCoreM * K大小的数据。总共24个核参与计算。

基本块shape：baseM=128，baseN=256，baseK=64。

L1相关Tiling参数：stepM=1，stepN=1，stepKa=4，stepKb=4，depthA1=8，depthB1=8。

# 获取性能数据

使用msProf工具获取算子仿真流水图和上板Profiling数据。相较于基础场景，Tiling常量化在编译期期间将部分或全部Tiling参数由变量转化为常数值，在算子执行时直接使用常量化的Tiling参数，可以减少Scalar性能开销，所以重点分析Scalar流水。

# 分析主要瓶颈点

优化前的流水图如下，默认不使能Tiling常量化，Tiling参数需要从Host侧拷贝到Kernel侧，导致Matmul初始化时的Scalar计算较多，第一个MTE2指令开始于3.536us左右，MTE2前的指令头开销在算子整个流水中占比较大，因此需要优化Scalar计算。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/dd5b8e76609607127c652b62ca986d363648903b6f0a522745be0cc3af8858a9.jpg)


优化前的Profiling数据如下，从C列的aic_time数据来看，多个核中最大算子执行耗时为10.62us，从G列的aic_scalar_time数据来看，Scalar平均耗时6.32us。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/ce80303257e488bca3c4e9a6c79e5cb048dce980815138bb0081116dea0b602b.jpg)


# 设计优化方案

如下图所示，默认不使能Tiling常量化功能时，开发者在host侧创建Tiling对象，通过调用API自动获取Tiling参数。然后将Tiling参数从Host侧传递到Kernel侧，在Kernel侧初始化操作时传入。在算子执行时，使用Tiling变量参数完成矩阵乘操作。


图 3-174 默认不使能 Tiling 常量化的 Matmul 计算流程示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/45a479729a601eec42bb7703b5a5efa14e0eb85035a203f5665fab1773da622d.jpg)


如下图所示，使能Tiling常量化功能时，开发者只需要在Kernel侧创建Matmul对象时，调用GetMatmulApiTiling接口在编译期获取常量化Tiling信息，即可完成Tiling常量化。在算子执行时，使用常量化的Tiling参数完成矩阵乘操作，减少Scalar计算开销。


图 3-175 使能 Tiling 常量化的 Matmul 计算流程示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/63003bc2775e263b08e8fdddfb962f14fcddda3af2d49391e18c8501f4450bff.jpg)


Matmul API使能Tiling全量常量化的完整样例请参考Matmul Tiling常量化的算子样例。使能Tiling全量常量化功能的步骤如下：

步骤1 调用获取MatmulConfig模板的接口GetMMConfig时，使用常数值设置MatmulShapeParams，得到带有常量化参数的自定义MatmulConfig模板CUSTOM_CFG。

```cpp
constexpr int32_t MAX_M = 10000; // custom matmul kernel support max value of M Dim shape
constexpr int32_t MAX_N = 10000; // custom matmul kernel support max value of N Dim shape
constexpr int32_t MAX_K = 10000; // custom matmul kernel support max value of K Dim shape
constexpr int32_t BASE_M = 128; // BASE_M * BASE_K * sizeof(typeA) <=LOA size
constexpr int32_t BASE_N = 256; // BASE_N * BASE_K * sizeof(typeB) <=LOB size
constexpr int32_t BASE_K = 64; // BASE_M * BASE_N * sizeof(typeC) <=LOC size
constexpr MatmulShapeParams shapeParams = { MAX_M,
MAX_N,
MAX_K,
BASE_M,
BASE_N,
BASE_K };
constexpr MatmulConfig CUSTOM_CFG =
GetMMConfig<MatmulConfigMode::CONFIG_MDL>(shapeParams); 
```

步骤2 创建Matmul对象。首先调用GetMatmulApiTiling接口，将Tiling信息常量化，得到常量化模板参数CONSTANT_CFG，包括常量化的Matmul Tiling信息和MatmulConfig模板。创建Matmul对象时，使用常量化模板参数CONSTANT_CFG。

```txt
using A_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, aType>;
using B_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, bType>;
using C_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, cType>;
using BIAS_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, biasType>;
constexpr static auto CONSTANT_CFG = AscendC::GetMatmulApiTiling<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE>(CUSTOM_CFG);
AscendC::Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, CONSTANT_CFG> matmulObj; 
```

步骤3 初始化操作。全量常量化时，可以在REGIST_MATMUL_OBJ接口的入参传递Tiling参数的位置，使用空指针替代。部分常量化时，在Kernel侧使用REGIST_MATMUL_OBJ接口初始化Matmul对象时，仍需要使用Tiling。

```txt
// 全量常量化场景，初始化操作示例
REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), matmulObj, (TCubeTiling*)nullptr);
// 部分常量化场景，初始化操作示例
REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), matmulObj, &tiling);
```

----结束

# 验证优化方案性能收益

优化后的流水图如下，通过使能Tiling全量常量化，无需将Tiling参数从Host侧拷贝到Kernel侧，在编译期完成Tiling常量化，减少了Matmul初始化时的Scalar计算。从0us起到第一个MTE2指令发起，这之间的时间为Matmul初始化时间，Matmul初始化时间从优化前的3.536us减少到2.185us，性能有所提升。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/53d4250e43b81db2d13ea8b2fbd52db80a07e23e38e27588c0d33e7bc00d47a4.jpg)


优化后的Profiling数据如下，从C列的aic_time数据来看，多个核中最大算子执行耗时为7.87us，相较于优化前的10.62us提升了25.9%。从G列的aic_scalar_time数据来看，Scalar平均耗时3.38us，相较于优化前的6.32us提升了46.5%。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/2aecd77e2120ee524171172c7069681d9e7b6aa95fe63b56f8253e87b4c83476.jpg)


# 总结

算子在调用Matmul API完成矩阵乘计算时，若Matmul初始化时的Scalar计算较多，影响了指令头开销，或Matmul迭代间的Scalar计算较多，阻塞了MTE2流水。在这两类场景下，满足上文提及的Tiling常量化使能条件（全量常量化或部分常量化），可以考虑使能Tiling常量化，减少Scalar计算开销，提升算子性能。

# 3.10.4.7 Matmul 高阶 API 使能 L2 Cache 切分

# 案例介绍

本案例呈现了在Matmul计算过程中，输入和输出的数据总量超过L2 Cache大小时，通过L2 Cache数据切分对算子性能的提升效果。使能L2 Cache切分的完整样例请参考L2Cache切分的算子样例。

本案例使用的AI处理器的L2 Cache大小为192MB，L2 Cache纯读带宽约为GM的3到4倍，两者之间存在较大差距。在搬入或搬出相同数据量的情况下，访问L2 Cache内的数据比访问GM更快。若数据无法命中L2 Cache，即需要访问的数据不在L2 Cache内，导致需要访问GM进行读写，带宽利用效率较低，最终算子搬入或搬出数据成为算子整个运行过程的性能瓶颈。

使能L2 Cache切分的适用场景

输入和输出的数据量超过L2 Cache的大小。

本案例的算子规格如下：


表3-38 算子规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>a</td><td>30720, 1024</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>4096, 1024</td><td>float16</td><td>ND</td></tr></table>

# 获取性能数据

使用msProf工具获取算子仿真流水图和上板Profiling数据。因为L2 Cache切分功能主要利用带宽更大的L2 Cache，减少MTE2数据搬运开销，所以重点分析MTE2的流水。

# 分析主要瓶颈点

当前案例基于Tiling全量常量化进一步优化，Tiling全量常量化请参考3.10.4.6Matmul高阶API使能Tiling全量常量化案例。优化前的Profiling数据如下，C列的aic_time是867us，K列的aic_mte2_time是861.9us，MTE2占比为99%，MTE2数据搬运是当前算子性能的瓶颈。

<table><tr><td></td><td>A</td><td>B</td><td>C</td><td>D</td><td>E</td><td>F</td><td>G</td><td>H</td><td>I</td><td>J</td><td>K</td><td>L</td><td>M</td><td>N</td><td>O</td><td>P</td></tr><tr><td>1</td><td>block_id</td><td>sub_block</td><td>aic_time(ua</td><td>aic_total_c</td><td>aic_cube_t</td><td>aic_cube_r</td><td>aic_scalar_r</td><td>aic_scalar_r</td><td>aic_mte1_t</td><td>aic_mte1_r</td><td>aic_mte2_t</td><td>aic_mte2_r</td><td>aic_mte3_t</td><td>aic_mte3_r</td><td>aic_fxpipe</td><td>aic_fxpipe</td></tr><tr><td>2</td><td>0</td><td>cube0</td><td>867.0195</td><td>1560635</td><td>743.8261</td><td>0.857912</td><td>275.6295</td><td>0.317905</td><td>553.735</td><td>0.638665</td><td>861.9689</td><td>0.994175</td><td>0.001111</td><td>0.000001</td><td>134.3511</td><td>0.154957</td></tr><tr><td>3</td><td>1</td><td>cube0</td><td>866.4894</td><td>1559681</td><td>744.5584</td><td>0.859281</td><td>276.6311</td><td>0.319255</td><td>555.375</td><td>0.640948</td><td>859.5522</td><td>0.991994</td><td>0.001111</td><td>0.000001</td><td>134.8006</td><td>0.155571</td></tr><tr><td>4</td><td>2</td><td>cube0</td><td>866.7955</td><td>1560232</td><td>743.9828</td><td>0.858314</td><td>276.9461</td><td>0.319506</td><td>553.6789</td><td>0.638765</td><td>861.8239</td><td>0.994264</td><td>0.001111</td><td>0.000001</td><td>134.7617</td><td>0.155471</td></tr><tr><td>5</td><td>3</td><td>cube0</td><td>869.3867</td><td>1564896</td><td>743.9955</td><td>0.855771</td><td>276.0161</td><td>0.317484</td><td>557.9583</td><td>0.641784</td><td>861.2433</td><td>0.990633</td><td>0.001111</td><td>0.000001</td><td>136.5989</td><td>0.157121</td></tr><tr><td>6</td><td>4</td><td>cube0</td><td>866.4155</td><td>1559548</td><td>742.9822</td><td>0.857536</td><td>276.0467</td><td>0.318608</td><td>558.6055</td><td>0.644732</td><td>857.1456</td><td>0.989301</td><td>0.001111</td><td>0.000001</td><td>133.7272</td><td>0.154345</td></tr><tr><td>7</td><td>5</td><td>cube0</td><td>867.6189</td><td>1561714</td><td>744.0605</td><td>0.857589</td><td>276.8322</td><td>0.319071</td><td>557.2839</td><td>0.642314</td><td>858.1094</td><td>0.98904</td><td>0.001111</td><td>0.000001</td><td>135.7422</td><td>0.156454</td></tr><tr><td>8</td><td>6</td><td>cube0</td><td>867.2844</td><td>1561112</td><td>742.9639</td><td>0.856655</td><td>275.4828</td><td>0.317638</td><td>560.2095</td><td>0.645935</td><td>856.4794</td><td>0.987542</td><td>0.001111</td><td>0.000001</td><td>133.0506</td><td>0.153411</td></tr><tr><td>9</td><td>7</td><td>cube0</td><td>866.1272</td><td>1559029</td><td>743.3489</td><td>0.858244</td><td>275.8489</td><td>0.318485</td><td>557.39</td><td>0.643543</td><td>853.5717</td><td>0.985504</td><td>0.001111</td><td>0.000001</td><td>133.0356</td><td>0.153598</td></tr><tr><td>10</td><td>8</td><td>cube0</td><td>867.3917</td><td>1561305</td><td>742.9006</td><td>0.856476</td><td>275.4478</td><td>0.317559</td><td>558.1</td><td>0.643423</td><td>856.1794</td><td>0.987074</td><td>0.001111</td><td>0.000001</td><td>135.6339</td><td>0.15637</td></tr><tr><td>11</td><td>9</td><td>cube0</td><td>868.9461</td><td>1564103</td><td>744.1939</td><td>0.856433</td><td>278.135</td><td>0.320083</td><td>551.4667</td><td>0.634638</td><td>864.5028</td><td>0.994887</td><td>0.001111</td><td>0.000001</td><td>134.5694</td><td>0.154865</td></tr><tr><td>12</td><td>10</td><td>cube0</td><td>866.6683</td><td>1560003</td><td>743.4911</td><td>0.857873</td><td>276.8239</td><td>0.319412</td><td>553.7267</td><td>0.638914</td><td>861.4861</td><td>0.994021</td><td>0.001111</td><td>0.000001</td><td>134.4906</td><td>0.155181</td></tr><tr><td>13</td><td>11</td><td>cube0</td><td>867.2733</td><td>1561092</td><td>743.6395</td><td>0.857445</td><td>277.1039</td><td>0.319512</td><td>553.0695</td><td>0.637711</td><td>862.9461</td><td>0.995011</td><td>0.001111</td><td>0.000001</td><td>135.2517</td><td>0.15595</td></tr><tr><td>14</td><td>12</td><td>cube0</td><td>867.4889</td><td>1561480</td><td>743.6367</td><td>0.857229</td><td>276.1606</td><td>0.318345</td><td>554.3283</td><td>0.639003</td><td>861.1934</td><td>0.992743</td><td>0.001111</td><td>0.000001</td><td>135.9372</td><td>0.156702</td></tr><tr><td>15</td><td>13</td><td>cube0</td><td>866.1033</td><td>1558986</td><td>743.5217</td><td>0.858468</td><td>276.5183</td><td>0.319267</td><td>553.9489</td><td>0.639588</td><td>861.5122</td><td>0.994699</td><td>0.001111</td><td>0.000001</td><td>135.82</td><td>0.156817</td></tr><tr><td>16</td><td>14</td><td>cube0</td><td>866.7022</td><td>1560064</td><td>744.0677</td><td>0.858505</td><td>276.3206</td><td>0.318818</td><td>555.5745</td><td>0.641021</td><td>859.2367</td><td>0.991386</td><td>0.001111</td><td>0.000001</td><td>133.2717</td><td>0.153769</td></tr><tr><td>17</td><td>15</td><td>cube0</td><td>869.4316</td><td>1564977</td><td>742.7806</td><td>0.854329</td><td>275.4756</td><td>0.316846</td><td>557.4161</td><td>0.641127</td><td>860.0016</td><td>0.989154</td><td>0.001111</td><td>0.000001</td><td>134.0194</td><td>0.154146</td></tr><tr><td>18</td><td>16</td><td>cube0</td><td>865.9283</td><td>1558671</td><td>743.1533</td><td>0.858216</td><td>274.7839</td><td>0.317329</td><td>557.99</td><td>0.644384</td><td>854.0439</td><td>0.986275</td><td>0.001111</td><td>0.000001</td><td>134.7578</td><td>0.155622</td></tr><tr><td>19</td><td>17</td><td>cube0</td><td>867.5294</td><td>1561553</td><td>743.7022</td><td>0.857265</td><td>276.2833</td><td>0.318471</td><td>555.9783</td><td>0.640875</td><td>858.4106</td><td>0.989489</td><td>0.001111</td><td>0.000001</td><td>135.6844</td><td>0.156403</td></tr><tr><td>20</td><td>18</td><td>cube0</td><td>867.3689</td><td>1561264</td><td>743.2472</td><td>0.856899</td><td>276.0567</td><td>0.318269</td><td>555.0605</td><td>0.639936</td><td>861.1278</td><td>0.992805</td><td>0.001111</td><td>0.000001</td><td>135.155</td><td>0.155822</td></tr><tr><td>21</td><td>19</td><td>cube0</td><td>866.1895</td><td>1559141</td><td>744.1061</td><td>0.859057</td><td>277.1489</td><td>0.319963</td><td>553.74</td><td>0.639283</td><td>861.4194</td><td>0.994493</td><td>0.001111</td><td>0.000001</td><td>135.1089</td><td>0.155981</td></tr><tr><td>22</td><td>20</td><td>cube0</td><td>867.2083</td><td>1560975</td><td>743.8272</td><td>0.857726</td><td>276.3544</td><td>0.318671</td><td>553.2839</td><td>0.638006</td><td>861.2522</td><td>0.993132</td><td>0.001111</td><td>0.000001</td><td>134.2833</td><td>0.154846</td></tr><tr><td>23</td><td>21</td><td>cube0</td><td>868.8367</td><td>1563906</td><td>744.0677</td><td>0.856395</td><td>276.9517</td><td>0.318761</td><td>554.5561</td><td>0.638274</td><td>862.7622</td><td>0.993009</td><td>0.001111</td><td>0.000001</td><td>134.3883</td><td>0.154676</td></tr><tr><td>24</td><td>22</td><td>cube0</td><td>866.3228</td><td>1559381</td><td>743.9667</td><td>0.858764</td><td>277.1939</td><td>0.319966</td><td>552.5367</td><td>0.637795</td><td>861.8406</td><td>0.994826</td><td>0.001111</td><td>0.000001</td><td>134.2789</td><td>0.154999</td></tr><tr><td>25</td><td>23</td><td>cube0</td><td>867.2939</td><td>1561129</td><td>744.1261</td><td>0.857986</td><td>276.6233</td><td>0.31895</td><td>554.9172</td><td>0.639826</td><td>861.7117</td><td>0.993564</td><td>0.001111</td><td>0.000001</td><td>134.7367</td><td>0.155353</td></tr></table>

# 设计优化方案

优化点一：调整切块大小和计算次数

优化前，输入数据不进行切分，所有核一次计算全部数据。如下图所示，图中数字表示核id，24个核一次计算A和B矩阵的所有数据。

优化后，输入数据被切分多次，所有核分多次计算，每个核单次计算只依赖切分后的数据量。L2 Cache切分方案确保单次计算的数据都在L2 Cache缓存中，搬运输入数据的效率更高。


图3-176 优化点一示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/450c70af9611fb57367e437f5caf5a05059a2ec4dc498fa2d1fc5f5a08ba3a7a.jpg)


优化点二：选择拖尾较小的L2 Cache切分方案

结合3.8.2.1 核间负载均衡的原理，AI处理器的物理核数固定，当数据进行L2Cache切分之后，可能出现部分核有计算拖尾的情况，即每次所有核总计算量除以每个核单次处理的数据量不能被核数整除，导致每次计算的最后需要部分尾核计算剩余数据。而在尾核计算时，部分核始终处于空闲状态，导致算子的整体性能变差。下图中标黄的数据块就是尾块数据，左边方案由于拖尾，每次计算中0、1、2、3核多执行一次处理剩余数据。为达到全局负载最优，调整拖尾核的位置，如右边方案所示，完成所有计算时，0到7核均多一次数据块的计算。

在实际场景中，满足切分后的数据量小于L2 Cache大小的前提下，拖尾越小越好。基于这个原则可以确定L2 Cache切分块数。


图3-177 优化点二示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/f894501475938e7096ea8affb97587c8d5f793e4fc0d5dc4767bff30f9a4ef29.jpg)


优化点三：错位分核，减少左右矩阵同地址冲突问题

同地址冲突：多核并发执行Matmul计算时，如果多核同时访问输入矩阵的相同地址，会导致地址冲突，影响性能。

在M和N方向，将矩阵数据L2 Cache切分为大数据块， 然后在数据块间错位分核，即将每个数据块依次沿对角线分配给不同的核处理，从而有效减少同地址冲突的问题。比如，在处理同一行的尾块数据0，1，2，3时，如果顺序分配执行的核，多核会同时读同一行左矩阵数据，导致读读冲突。若按照对角线方式分配执行的核，在对角线上的尾块数据被分配给核0，1，2，3计算，多核访问不同行的左矩阵数据，将减少同地址冲突的次数。


图 3-178 优化点三示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/da3e6c7550cfc7fe2dc4e4b5f13b5583ce41c2cfadec712f99caf434f0ff161a.jpg)


Matmul API使能L2 Cache切分的完整样例请参考L2 Cache切分的算子样例。实现L2Cache切分的关键步骤如下：

步骤1 判断是否需要进行L2 Cache切分。如果数据总量超过设定的L2 Cache大小，则计算L2Cache切分数目。

```txt
bool smallDim = mTileNum_ < L1_MIN_UST_DIM && nTileNum_ < L1_MIN_UST_DIM;
if (smallDim || (!EnableL2Tile())) { // 判断计算数据总量是否小于L2Cache阈值
    mL2TileNum_ = mTileNum_;
    nL2TileNum_ = nTileNum_;
    mL2BlockNum_ = 1;
    nL2BlockNum_ = 1;
    return; // 不需要切分，提前返回
```

```javascript
}
InitL2TileTail(); // 计算L2切分
```

步骤2 基于负载均衡原则，计算L2 Cache切分的份数，m方向L2 Cache切分数：

mL2TileNum_，n方向L2 Cache切分数：nL2TileNum_。

```cpp
int64_t mConflict = INT64_MAX;
int64_t nConflict = INT64_MAX;
constexpr bool isNMajor = l1N > l1M; // 根据shape大小，判断主维度
for (int64_t i = maxMajor; i >= L1_MIN_UST_DIM; i--) {
    for (int64_t j = maxMinor; j >= minMinor; j--) {
    if (GetTotalSize(j * l1M, i * l1N, k_) <= L2_TILE_THRESHOLD) { // 确保分块小于L2Cache阈值
    uint64_t mConflictTmp = AscendC::Ceil(blockNum_, mL2TileNumTailTmp); // 计算负载冲突值
    uint64_t nConflictTmp = AscendC::Ceil(blockNum_, nL2TileNumTailTmp);
    if (mConflict >= mConflictTmp && nConflict >= nConflictTmp) { // 若冲突值更小，更新分块数量
    mConflict = mConflictTmp;
    nConflict = nConflictTmp;
    mL2TileNum_ = curMajorDim;
    nL2TileNum_ = curMinorDim;
    }
    }
    }
}
```

步骤3 错位分核。输入当前数据块的下标，获取按对角线分配的核的下标。

```txt
__aicore__ inline BlockCoord GetBlockCoord(int64_t tileIdx) {
    GetCommonTileIndex(tileIdx);
    int64_t mTileIdx = newBlockIdx_% mL2TileNumTmp_;
    mTileIdx = mTileIdx + mL2Idx_* mL2TileNum_;
    int64_t nTileIdx = 0;
    if (mL2TileNumTmp_ != 0 && nL2TileNumTmp_ != 0) {
    int64_t tmp = newBlockIdx_/CalcLcm(mL2TileNumTmp_, nL2TileNumTmp_);
    nTileIdx = (newBlockIdx_ + tmp) % nL2TileNumTmp_;
    }
    nTileIdx = nTileIdx + nL2Idx_* nL2TileNum_;
    return {mTileIdx * l1M, nTileIdx * l1N, 0};
} 
```

步骤4 设置左右矩阵，根据前序步骤计算的L2 Cache切分数和执行核的下标，循环多次计算Matmul。

```txt
L2CacheOpt l2Opt(shapes, blockNum);
matmulObj.SetOrgShape(shapes.m, shapes.n, shapes.k);
for (int64_t tileIdx = curBlockIdx; tileIdx < l2Opt.GetTileNum(); tileIdx += blockNum) {
    auto blockShape = l2Opt.GetBlockShape(tileIdx); // 获取单次计算L2切分块大小
    if (Get<0>(blockShape) <= 0 ||
    Get<1>(blockShape) <= 0){
    return;
    }
    auto blockCoord = l2Opt.GetBlockCoord(tileIdx);
    // 获取当前执行计算的核的下标blockCoord
    matmulObj.SetTail(Get<0>(blockShape), Get<1>(blockShape), Get<2>(blockShape));
    const auto& offsetCoord = CalcOffset(shapes, blockCoord); // 基于下标计算矩阵偏移
    int64_t offsetA = Get<0>(offsetCoord);
    int64_t offsetB = Get<1>(offsetCoord);
    int64_t offsetC = Get<2>(offsetCoord);
    matmulObj.SetTensorA(aGlobal[offsetA], false);
    matmulObj.SetTensorB(bGlobal[offsetB], false);
    if (shapes.isBias) {
    matmulObj.SetBias.biasGlobal);
    }
    matmulObj.IterateAll(cGlobal[offsetC]); // 计算L2切分块
}
matmulObj.End();
```

----结束

# 验证优化方案性能收益

优化后的Profiling数据如下，C列的aic_time为805.6us，相比于优化前，总执行时间降低了约7.1%，MTE2搬运时间降低了约10.7%。

<table><tr><td></td><td>A</td><td>B</td><td>C</td><td>D</td><td>E</td><td>F</td><td>G</td><td>H</td><td>I</td><td>J</td><td>K</td><td>L</td><td>M</td><td>N</td><td>O</td><td>P</td></tr><tr><td>1</td><td>block_id</td><td>sub_block</td><td>aic_time(usa</td><td>aic_total_c</td><td>aic_cube_t</td><td>aic_cube_r</td><td>aic_scalar</td><td>aic_scalar</td><td>aic_mte1_t</td><td>aic_mte1_r</td><td>aic_mte2_t</td><td>aic_mte2_r</td><td>aic_mte3_t</td><td>aic_mte3_r</td><td>aic_fixpipe</td><td>aic_fixpipe_r</td></tr><tr><td>2</td><td>0</td><td>cube0</td><td>805.7633</td><td>1450374</td><td>739.6234</td><td>0.917916</td><td>284.7161</td><td>0.35335</td><td>571.9861</td><td>0.709869</td><td>760.5233</td><td>0.943855</td><td>0.001111</td><td>0.000001</td><td>126.0561</td><td>0.156443</td></tr><tr><td>3</td><td>1</td><td>cube0</td><td>802.1606</td><td>1443889</td><td>739.5</td><td>0.921885</td><td>284.6589</td><td>0.354865</td><td>572.6211</td><td>0.713849</td><td>757.5767</td><td>0.94442</td><td>0.001111</td><td>0.000001</td><td>125.6339</td><td>0.156619</td></tr><tr><td>4</td><td>2</td><td>cube0</td><td>799.1456</td><td>1438462</td><td>739.3761</td><td>0.925208</td><td>284.6972</td><td>0.356252</td><td>571.7355</td><td>0.715434</td><td>754.8494</td><td>0.944571</td><td>0.001111</td><td>0.000001</td><td>127.3744</td><td>0.159388</td></tr><tr><td>5</td><td>3</td><td>cube0</td><td>816.015</td><td>1468827</td><td>740.37</td><td>0.9073</td><td>284.8033</td><td>0.349017</td><td>567.6578</td><td>0.695646</td><td>795.1061</td><td>0.974377</td><td>0.001111</td><td>0.000001</td><td>125.8111</td><td>0.154177</td></tr><tr><td>6</td><td>4</td><td>cube0</td><td>807.84</td><td>1454112</td><td>740.4239</td><td>0.916548</td><td>284.7967</td><td>0.352541</td><td>568.8739</td><td>0.704191</td><td>783.5178</td><td>0.969892</td><td>0.001111</td><td>0.000001</td><td>126.3439</td><td>0.156397</td></tr><tr><td>7</td><td>5</td><td>cube0</td><td>804.7416</td><td>1448535</td><td>739.8639</td><td>0.919381</td><td>284.8022</td><td>0.353905</td><td>569.0039</td><td>0.707064</td><td>777.0356</td><td>0.965571</td><td>0.001111</td><td>0.000001</td><td>126.69</td><td>0.157429</td></tr><tr><td>8</td><td>6</td><td>cube0</td><td>818.2178</td><td>1472792</td><td>741.01</td><td>0.905639</td><td>285.4111</td><td>0.34882</td><td>568.1128</td><td>0.69433</td><td>796.1216</td><td>0.972995</td><td>0.001111</td><td>0.000001</td><td>126.7339</td><td>0.15489</td></tr><tr><td>9</td><td>7</td><td>cube0</td><td>814.1428</td><td>1465457</td><td>740.8483</td><td>0.909974</td><td>285.1306</td><td>0.350222</td><td>569.9772</td><td>0.700095</td><td>790.7106</td><td>0.971219</td><td>0.001111</td><td>0.000001</td><td>125.4344</td><td>0.154069</td></tr><tr><td>10</td><td>8</td><td>cube0</td><td>807.8306</td><td>1454095</td><td>740.3572</td><td>0.916476</td><td>285.1772</td><td>0.353016</td><td>568.4178</td><td>0.703635</td><td>782.9439</td><td>0.969193</td><td>0.001111</td><td>0.000001</td><td>126.5244</td><td>0.156622</td></tr><tr><td>11</td><td>9</td><td>cube0</td><td>803.1967</td><td>1445754</td><td>739.1906</td><td>0.920311</td><td>285.0422</td><td>0.354885</td><td>571.3561</td><td>0.711353</td><td>769.2961</td><td>0.957793</td><td>0.001111</td><td>0.000001</td><td>126.6961</td><td>0.15774</td></tr><tr><td>12</td><td>10</td><td>cube0</td><td>803.5383</td><td>1446369</td><td>738.8417</td><td>0.919485</td><td>284.7495</td><td>0.354369</td><td>571.6644</td><td>0.711434</td><td>760.8555</td><td>0.946881</td><td>0.001111</td><td>0.000001</td><td>126.62</td><td>0.157578</td></tr><tr><td>13</td><td>11</td><td>cube0</td><td>808.0139</td><td>1454425</td><td>739.0767</td><td>0.914683</td><td>285.2744</td><td>0.353056</td><td>571.0839</td><td>0.706775</td><td>766.0711</td><td>0.948092</td><td>0.001111</td><td>0.000001</td><td>124.3628</td><td>0.153912</td></tr><tr><td>14</td><td>12</td><td>cube0</td><td>805.0511</td><td>1449092</td><td>739.3517</td><td>0.918391</td><td>284.6683</td><td>0.353603</td><td>571.41</td><td>0.709781</td><td>764.96</td><td>0.950201</td><td>0.001111</td><td>0.000001</td><td>126.4333</td><td>0.15705</td></tr><tr><td>15</td><td>13</td><td>cube0</td><td>803.7078</td><td>1446674</td><td>739.2683</td><td>0.919822</td><td>284.4889</td><td>0.353971</td><td>570.6089</td><td>0.709971</td><td>767.9094</td><td>0.955459</td><td>0.001111</td><td>0.000001</td><td>126.0356</td><td>0.156818</td></tr><tr><td>16</td><td>14</td><td>cube0</td><td>803.0511</td><td>1445492</td><td>739.6194</td><td>0.921012</td><td>284.525</td><td>0.354305</td><td>571.5584</td><td>0.711733</td><td>774.99</td><td>0.965057</td><td>0.001111</td><td>0.000001</td><td>125.2233</td><td>0.155934</td></tr><tr><td>17</td><td>15</td><td>cube0</td><td>817.5378</td><td>1471568</td><td>741.0917</td><td>0.906492</td><td>285.6128</td><td>0.349357</td><td>566.67</td><td>0.693142</td><td>797.8073</td><td>0.975866</td><td>0.001111</td><td>0.000001</td><td>127.4817</td><td>0.155934</td></tr><tr><td>18</td><td>16</td><td>cube0</td><td>806.5061</td><td>1451711</td><td>739.1672</td><td>0.916505</td><td>285.1239</td><td>0.35353</td><td>571.2061</td><td>0.708248</td><td>769.6733</td><td>0.95433</td><td>0.001111</td><td>0.000001</td><td>126.5522</td><td>0.156914</td></tr><tr><td>19</td><td>17</td><td>cube0</td><td>800.7289</td><td>1441312</td><td>738.7955</td><td>0.922654</td><td>284.2606</td><td>0.355002</td><td>572.0061</td><td>0.714357</td><td>756.9067</td><td>0.945272</td><td>0.001111</td><td>0.000001</td><td>123.9328</td><td>0.154775</td></tr><tr><td>20</td><td>18</td><td>cube0</td><td>788.9984</td><td>1420197</td><td>738.9989</td><td>0.936629</td><td>284.01</td><td>0.359963</td><td>573.4628</td><td>0.726824</td><td>737.2867</td><td>0.934459</td><td>0.001111</td><td>0.000001</td><td>123.3733</td><td>0.156367</td></tr><tr><td>21</td><td>19</td><td>cube0</td><td>806.2889</td><td>1451320</td><td>739.1967</td><td>0.916789</td><td>284.4944</td><td>0.352844</td><td>571.7455</td><td>0.709108</td><td>768.0311</td><td>0.952551</td><td>0.001111</td><td>0.000001</td><td>125.0094</td><td>0.155043</td></tr><tr><td>22</td><td>20</td><td>cube0</td><td>805.7067</td><td>1450272</td><td>738.7505</td><td>0.916898</td><td>284.7761</td><td>0.353449</td><td>572.4617</td><td>0.710509</td><td>768.0133</td><td>0.953217</td><td>0.001111</td><td>0.000001</td><td>125.3339</td><td>0.155558</td></tr><tr><td>23</td><td>21</td><td>cube0</td><td>804.2167</td><td>1447590</td><td>739.4839</td><td>0.919508</td><td>284.6667</td><td>0.353968</td><td>571.2539</td><td>0.710323</td><td>773.0367</td><td>0.961229</td><td>0.001111</td><td>0.000001</td><td>125.2983</td><td>0.155802</td></tr><tr><td>24</td><td>22</td><td>cube0</td><td>797.9305</td><td>1436275</td><td>739.5328</td><td>0.926813</td><td>284.2545</td><td>0.35624</td><td>572.7439</td><td>0.717787</td><td>745.6906</td><td>0.934531</td><td>0.001111</td><td>0.000001</td><td>125.525</td><td>0.157313</td></tr><tr><td>25</td><td>23</td><td>cube0</td><td>805.1544</td><td>1449278</td><td>739.39</td><td>0.918321</td><td>284.7444</td><td>0.353652</td><td>572.1</td><td>0.710547</td><td>764.0517</td><td>0.94895</td><td>0.001111</td><td>0.000001</td><td>125.0522</td><td>0.155315</td></tr></table>

# 总结

在Matmul计算数据量超过L2 Cache大小的场景下，可以考虑使能L2 Cache切分，提高L2 Cache命中率，利用L2 Cache高带宽特性提升算子性能。

# 3.10.4.8 Matmul 高阶 API 使能多核切 K

# 案例介绍

本案例呈现了在矩阵乘算子场景中，使用Matmul高阶API进行矩阵乘法计算，使能多核切K功能对算子性能的提升效果。为了实现算子在多核上并行执行，提升计算效率，需要将矩阵数据进行切分，切分后的数据块被分配到不同的核上处理。通常情况下，切分矩阵数据时仅切分M、N轴，不切分K轴。若M和N较小，切分M和N轴较困难，此时需要考虑K轴切分；使能多核切K功能后，该场景下可以对矩阵的K轴进行切分，从而使算子在多核上并行执行。由于K轴较大，在该场景下不切分K轴通常会导致单核的输入数据量过大，使能K轴切分后，切分策略能够更有效地平衡输出带宽和输入带宽。

# 使能多核切K的适用场景

矩阵的K轴较大，M轴和N轴相比K轴较小，可以在K轴进行切分，使算子并行执行的核数更多。

矩阵的M轴、N轴和K轴均较大时，可以在K轴进行切分，使切分策略更好地平衡输入和输出带宽。

# 使能多核切K的约束条件

使能多核切K的场景，获取C矩阵结果时仅支持输出到Global Memory。

使能多核切K的场景，需在Kernel侧代码中首次将C矩阵分片的结果写入Global Memory之前，先对Global Memory进行清零，在获取C矩阵分片的结果时，开启AtomicAdd累加。如果不预先清零Global Memory，可能会因为累加Global Memory中的原始无效数据而产生精度问题。

使能多核切K的场景，不支持Bias参与矩阵乘计算。

本案例的算子规格如下：


表 3-39 算子规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>a</td><td>16, 1024</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>1024, 16</td><td>float16</td><td>ND</td></tr></table>

当前案例使用的AI处理器共24个核，算子中使能高阶API Matmul的纯Cube模式。Tiling参数如下：

原始shape：M=16, N= 16, K=1024。

单核shape：未开启多核切K时，singleCoreM=16，singleCoreN=16，singleCoreK=1024；开启多核切K后，singleCoreM=16，singleCoreN=16，singleCoreK=512。

# 获取性能数据

使用msProf工具获取算子仿真流水图和上板Profiling数据。

# 分析主要瓶颈点

优化前的流水图如下，由于未使能多核切K，且M和N非常小，原始矩阵数据未进行切分，所有数据在单核上进行计算。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/b7eb2d66098efba6cf06a96def40654820e66b0376dd1f77c7520f158a7650c4.jpg)


优化前的Profiling数据如下，可以看到算子只在单核上执行，aic_time耗时约19.60us，其中aic_mte2_time的平均耗时约为13.72us，aic_mte2_ratio占比较高。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/ef92ad5c287053540d774de18c8a4387282373ba724174eba13892cad4f0a599.jpg)


# 设计优化方案

使能多核切K后，矩阵的K方向数据可以进行切分。如下图所示，C矩阵中的R矩阵块，是通过A1*B1+A2*B2+A3*B3累加得到的，其中，A1*B1、A2*B2、A3*B3可在多个核上并行计算。


图 3-179 开启多核切 K


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/bc5a7f39f84c1bed5f1bfebcbccd8c4a9992694d179429ff4de915032febd06d.jpg)


使能多核切K功能的方式为：在GetTiling接口前调用EnableMultiCoreSplitK接口，使能多核切K，并在Kernel实现中，对C矩阵的Global Memory地址清零后开启AtomicAdd。使能多核切K的完整样例请参考多核切K场景的算子样例。具体步骤如下：

Tiling实现

通过GetTiling接口获取TCubeTiling结构体前，调用EnableMultiCoreSplitK接口且入参为true，使能多核切K。

```cpp
cubeTiling.SetOrgShape(M, N, K);
cubeTiling.SetShape(M, N, K);
cubeTiling.EnableBias(isBias);
cubeTiling.SetBufferSpace(-1, -1, -1);
// tiling enable split K
cubeTiling.EnableMultiCoreSplitK(true);
if (cubeTiling.GetTiling(tilingData) == -1) {
    std::cout << "gen tiling failed." << std::endl;
    return {};
} 
```

Kernel实现

调用Fill接口，对C矩阵的Global Memory地址清零。

```cpp
cGlobal.SetGlobalBuffer(reinterpret_cast<__gm__ cType*>(c), tiling.M * tiling.N);
// clear gm
Fill(cGlobal, tiling.M * tiling.N, (cType)0); 
```

调用IterateAll接口，开启AtomicAdd累加，完成矩阵乘操作。

```c
// set AtomicAdd
uint8_t enAtomic = 1;
matmulObj.IterateAll(cGlobal, enAtomic); 
```

# 验证优化方案性能收益

优化后的流水图如下，开启多核切K后，切分原始矩阵的K方向，单核处理K方向的数据量由原来的1024变为512，单核处理的数据量减半，MTE2流水变短。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/51f422a05f203f32b76432dc2862dfb7bc2c14ea281df8324703809c2b843812.jpg)


优化后的Profiling数据如下，可以看到算子在两个核上执行，aic_time平均耗时约为13.70us，较优化前的19.60us有较大提升。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/87867379d4164f2e049f01b5d4a4c53d7a95478583aa0f35c5513a946f5fd80e.jpg)


# 总结

当算子使用Matmul API完成矩阵计算时，原始矩阵的M和N方向无法进行有效切分，且结果输出到Global Memory时，可以考虑使能多核切K功能，实现多核并行，提升计算效率。

# 3.10.4.9 Matmul 高阶 API 使能多核 K 轴错峰访问内存

# 案例介绍

本案例呈现在矩阵乘算子场景中，使用Matmul高阶API进行矩阵乘法计算，使能多核K轴错峰访问Device内存对算子性能的提升效果。在多核并行执行Matmul计算时，如果输入矩阵A或B的内存位置位于GM，并且参与多核计算的矩阵相同，那么将出现多核同时访问相同GM地址的情况，导致地址访问冲突，从而影响算子性能。若使能多核K轴错峰访问Device内存，切分的矩阵K轴方向对应的不同核将尽量从不同的GM起始地址开始访问和搬运数据，缓解地址访问冲突，提升算子性能。


图3-180 访问地址冲突示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/d7e4802ddfd05b0a8801b042099cf605b8080405f2cf60ff6f89202e40a82aea.jpg)



图3-181 缓解地址冲突示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/af7379da47b9c7438199005d41c1521c1dab6c64d7bac77242d4c0c9a4c9f60e.jpg)


使能多核K轴错峰访问内存的适用场景：

多核执行Matmul，且输入矩阵的K轴较大。

使能多核K轴错峰访问内存的约束条件：

输入矩阵的K轴非全载，K轴非全载即矩阵的K方向数据不能同时搬入及保持在L1 Buffer中。

仅支持MDL模板。

在多核上执行Matmul计算。

A矩阵或B矩阵的内存位置位于GM。

本案例的算子规格如下：


表3-40 算子用例规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>a</td><td>768, 6144</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>6144, 2048</td><td>float16</td><td>ND</td></tr></table>

# 获取性能数据

使用msProf工具获取算子仿真流水图和上板Profiling数据，重点分析MTE2的流水。

# 分析主要瓶颈点

优化前的Profiling数据（PipeUtilization.csv）如下所示，aic_mte2_ratio平均达到0.93，MTE2在算子整体执行时长中占比较高，算子当前为MTE2 Bound。本案例中，矩阵按M和N方向切分，单核shape[singleCoreM，singleCoreN，singleCoreK]为[128, 512, 6144]，基本块shape[baseM，baseN，baseK]为[128, 256, 64]，每次加载A矩阵的数据时，多核有概率同时访问同一GM地址，引发地址冲突，导致MTE2搬运效率降低，MTE2执行耗时增加。

<table><tr><td>block_id</td><td>sub_block_id</td><td>aic_time(us)</td><td>aic_total_cycles</td><td>aic_cube_time(us)</td><td>aic_cube_ratio</td><td>aic_scalar_time(us)</td><td>aic_scalar_ratio</td><td>aic_mte1_time(us)</td><td>aic_mte1_ratio</td><td>aic_mte2_time(us)</td><td>aic_mte2_ratio</td></tr><tr><td>0</td><td>cube0</td><td>97.181084</td><td>179785</td><td>55.972973</td><td>0.575966</td><td>55.87027</td><td>0.574909</td><td>40.527027</td><td>0.417026</td><td>91.097298</td><td>0.937397</td></tr><tr><td>1</td><td>cube0</td><td>97.074051</td><td>179587</td><td>56.074596</td><td>0.577648</td><td>56.449188</td><td>0.581506</td><td>40.240002</td><td>0.414529</td><td>90.012436</td><td>0.927255</td></tr><tr><td>2</td><td>cube0</td><td>97.836754</td><td>180998</td><td>56.035675</td><td>0.572747</td><td>57.169189</td><td>0.584332</td><td>40.281082</td><td>0.411717</td><td>90.380539</td><td>0.923789</td></tr><tr><td>3</td><td>cube0</td><td>97.211891</td><td>179842</td><td>55.773514</td><td>0.573731</td><td>58.014053</td><td>0.596779</td><td>40.107025</td><td>0.412573</td><td>90.107025</td><td>0.926914</td></tr><tr><td>4</td><td>cube0</td><td>97.215675</td><td>179849</td><td>55.769188</td><td>0.573665</td><td>58.704323</td><td>0.603857</td><td>40.13081</td><td>0.412802</td><td>91.602699</td><td>0.942263</td></tr><tr><td>5</td><td>cube0</td><td>97.799461</td><td>180929</td><td>55.758919</td><td>0.570135</td><td>57.950272</td><td>0.592542</td><td>40.439999</td><td>0.413499</td><td>90.181625</td><td>0.922108</td></tr><tr><td>6</td><td>cube0</td><td>96.235672</td><td>178036</td><td>55.756218</td><td>0.579372</td><td>57.925404</td><td>0.601912</td><td>40.931892</td><td>0.42533</td><td>87.956215</td><td>0.913967</td></tr><tr><td>7</td><td>cube0</td><td>95.905945</td><td>177426</td><td>56.257298</td><td>0.586588</td><td>57.697838</td><td>0.601609</td><td>40.234596</td><td>0.419521</td><td>89.915138</td><td>0.937535</td></tr><tr><td>8</td><td>cube0</td><td>95.640541</td><td>176935</td><td>55.976215</td><td>0.585277</td><td>58.24054</td><td>0.608952</td><td>40.447567</td><td>0.422912</td><td>89.642159</td><td>0.937282</td></tr><tr><td>9</td><td>cube0</td><td>96.136757</td><td>177853</td><td>55.721622</td><td>0.579608</td><td>57.837296</td><td>0.601615</td><td>40.551891</td><td>0.421815</td><td>88.26973</td><td>0.918168</td></tr><tr><td>10</td><td>cube0</td><td>96.471352</td><td>178472</td><td>55.834595</td><td>0.578769</td><td>56.701622</td><td>0.587756</td><td>40.582703</td><td>0.420671</td><td>88.286484</td><td>0.915158</td></tr><tr><td>11</td><td>cube0</td><td>95.916214</td><td>177445</td><td>55.696217</td><td>0.580676</td><td>56.547028</td><td>0.589546</td><td>40.754593</td><td>0.424898</td><td>88.122704</td><td>0.918747</td></tr><tr><td>12</td><td>cube0</td><td>97.657837</td><td>180667</td><td>55.989189</td><td>0.57332</td><td>56.523243</td><td>0.578789</td><td>40.53838</td><td>0.415106</td><td>91.364326</td><td>0.935555</td></tr><tr><td>13</td><td>cube0</td><td>97.375679</td><td>180145</td><td>56.216217</td><td>0.577313</td><td>56.554596</td><td>0.580788</td><td>40.586487</td><td>0.416803</td><td>91.158379</td><td>0.936151</td></tr><tr><td>14</td><td>cube0</td><td>96.977295</td><td>179408</td><td>56.20108</td><td>0.579528</td><td>57.469189</td><td>0.592605</td><td>40.312431</td><td>0.415689</td><td>91.942703</td><td>0.948085</td></tr><tr><td>15</td><td>cube0</td><td>97.511353</td><td>180396</td><td>55.836758</td><td>0.572618</td><td>57.203243</td><td>0.586632</td><td>40.582703</td><td>0.416184</td><td>90.81189</td><td>0.931296</td></tr><tr><td>16</td><td>cube0</td><td>97.162704</td><td>179751</td><td>55.779461</td><td>0.574083</td><td>57.581081</td><td>0.592625</td><td>40.311893</td><td>0.414891</td><td>90.610268</td><td>0.932562</td></tr><tr><td>17</td><td>cube0</td><td>97.564323</td><td>180494</td><td>56.042702</td><td>0.574418</td><td>57.611351</td><td>0.590496</td><td>40.426487</td><td>0.414357</td><td>91.854057</td><td>0.941472</td></tr><tr><td>18</td><td>cube0</td><td>96.501625</td><td>178528</td><td>55.714054</td><td>0.577338</td><td>57.474052</td><td>0.595576</td><td>40.663784</td><td>0.421379</td><td>89.143784</td><td>0.923754</td></tr><tr><td>19</td><td>cube0</td><td>96.171349</td><td>177917</td><td>55.944324</td><td>0.581715</td><td>57.375675</td><td>0.596598</td><td>40.725407</td><td>0.423467</td><td>89.225403</td><td>0.927775</td></tr><tr><td>20</td><td>cube0</td><td>96.37027</td><td>178285</td><td>55.652973</td><td>0.577491</td><td>57.369728</td><td>0.595305</td><td>40.465946</td><td>0.419901</td><td>89.97081</td><td>0.933595</td></tr><tr><td>21</td><td>cube0</td><td>96.087029</td><td>177761</td><td>55.860542</td><td>0.581354</td><td>57.825405</td><td>0.601802</td><td>40.457836</td><td>0.421054</td><td>89.34919</td><td>0.929878</td></tr><tr><td>22</td><td>cube0</td><td>96.438377</td><td>178411</td><td>55.589188</td><td>0.576422</td><td>57.696758</td><td>0.598276</td><td>40.523243</td><td>0.420198</td><td>89.070267</td><td>0.923598</td></tr><tr><td>23</td><td>cube0</td><td>95.808647</td><td>177246</td><td>55.984863</td><td>0.58434</td><td>57.725945</td><td>0.602513</td><td>40.36108</td><td>0.421268</td><td>89.839996</td><td>0.937702</td></tr></table>

MTE2的搬运效率还可以通过查看其带宽利用率进行验证，如下图所示，通过分析Memory.csv，发现MTE2平均带宽利用率只有34.4%。

<table><tr><td>read_main_memory_datas(KB)</td><td>write_main_memory_datas(KB)</td><td>GM_to_L1_datas(KB)</td><td>GM_to_L1_bw_usage_rate(%)</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.257599</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.295368</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.028015</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.246742</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.245407</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.040989</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.594139</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.713074</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.809402</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.629734</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.509628</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.709358</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.090355</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.189137</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.329586</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.141567</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.264076</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.123032</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.498802</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.617279</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.545826</td></tr><tr><td>9216.75</td><td>256.125</td><td>9216</td><td>34.647655</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.521423</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>34.748329</td></tr></table>

查看OpBasicInfo.csv文件，优化前算子整体耗时为98.72us。

# 设计优化方案

使能K轴错峰访问内存：在创建Matmul对象时，将MatmulConfig中的enableKdimReorderLoad参数设置为true。enableKdimReorderLoad参数的详细介绍请参考MatmulConfig。

使能K轴错峰访问内存的完整样例请参考K轴错峰加载数据的算子样例。使能该功能的主要步骤如下：

步骤1 配置MDL模板参数，将其中的enableKdimReorderLoad参数设置为true，使能多核K轴错峰访问Device内存。

```javascript
constexpr MatmulConfig GetMDLKDimReorderConfig()
{
    auto CFG = CFG_MDL;
    CFG.enableKdimReorderLoad = true;
    return CFG;
}
constexpr static MatmulConfig MM_CFG = GetMDLKDimReorderConfig(); 
```

步骤2 基于自定义的MatmulConfig模板参数，创建Matmul对象。

```txt
AscendC::Matmul<AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, aType>, AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, bType>, AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, cType>, AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, biasType>, MM_CFG> matmulObj; 
```

----结束

# 验证优化方案性能收益

算子Tiling参数不变，优化后的Profiling数据（PipeUtilization.csv）如下所示。可以看到，MTE2耗时显著降低，MTE2的平均耗时从90us降低到69.87us，最大耗时从91.94us降低到75.82us。

<table><tr><td>block_id</td><td>sub_block_id</td><td>aic_time(us)</td><td>aic_total_cycles</td><td>aic_cube_time(us)</td><td>aic_cube_ratio</td><td>aic_scalar_time(us)</td><td>aic_scalar_ratio</td><td>aic_mte1_time(us)</td><td>aic_mte1_ratio</td><td>aic_mte2_time(us)</td><td>aic_mte2_ratio</td></tr><tr><td>0</td><td>cube0</td><td>78.071892</td><td>144433</td><td>53.95892</td><td>0.691144</td><td>58.42865</td><td>0.748395</td><td>43.446487</td><td>0.556493</td><td>69.223785</td><td>0.886667</td></tr><tr><td>1</td><td>cube0</td><td>77.507568</td><td>143389</td><td>54.162704</td><td>0.698805</td><td>57.375134</td><td>0.740252</td><td>43.763783</td><td>0.564639</td><td>68.304863</td><td>0.881267</td></tr><tr><td>2</td><td>cube0</td><td>78.468651</td><td>145167</td><td>53.987568</td><td>0.688015</td><td>56.552975</td><td>0.720708</td><td>43.652973</td><td>0.556311</td><td>68.991348</td><td>0.879222</td></tr><tr><td>3</td><td>cube0</td><td>79.827568</td><td>147681</td><td>53.952972</td><td>0.675869</td><td>57.167568</td><td>0.716138</td><td>43.250271</td><td>0.541796</td><td>69.38324</td><td>0.869164</td></tr><tr><td>4</td><td>cube0</td><td>80.668648</td><td>149237</td><td>53.994595</td><td>0.669338</td><td>58.288109</td><td>0.722562</td><td>43.816216</td><td>0.543163</td><td>69.952972</td><td>0.867164</td></tr><tr><td>5</td><td>cube0</td><td>84.896217</td><td>157058</td><td>54.560001</td><td>0.642667</td><td>58.983784</td><td>0.694775</td><td>42.615135</td><td>0.501967</td><td>75.561081</td><td>0.890041</td></tr><tr><td>6</td><td>cube0</td><td>78.974052</td><td>146102</td><td>53.936214</td><td>0.682961</td><td>58.467567</td><td>0.740339</td><td>43.268108</td><td>0.547877</td><td>68.018379</td><td>0.861275</td></tr><tr><td>7</td><td>cube0</td><td>78.737297</td><td>145664</td><td>54.020542</td><td>0.686086</td><td>58.810268</td><td>0.746918</td><td>43.918919</td><td>0.557791</td><td>67.791893</td><td>0.860988</td></tr><tr><td>8</td><td>cube0</td><td>78.758919</td><td>145704</td><td>53.978378</td><td>0.685362</td><td>58.65892</td><td>0.744791</td><td>43.52919</td><td>0.552689</td><td>68.038918</td><td>0.863888</td></tr><tr><td>9</td><td>cube0</td><td>79.076218</td><td>146291</td><td>53.92054</td><td>0.681881</td><td>58.251892</td><td>0.736655</td><td>43.400002</td><td>0.548838</td><td>67.408112</td><td>0.852445</td></tr><tr><td>10</td><td>cube0</td><td>79.098381</td><td>146332</td><td>54.211349</td><td>0.685366</td><td>58.585407</td><td>0.740665</td><td>43.184864</td><td>0.545964</td><td>69.188652</td><td>0.874716</td></tr><tr><td>11</td><td>cube0</td><td>82.610809</td><td>152830</td><td>54.591892</td><td>0.660832</td><td>59.13892</td><td>0.715874</td><td>42.374054</td><td>0.512936</td><td>74.741081</td><td>0.904737</td></tr><tr><td>12</td><td>cube0</td><td>78.178917</td><td>144631</td><td>53.951351</td><td>0.690101</td><td>56.943783</td><td>0.728378</td><td>43.131893</td><td>0.551707</td><td>69.227028</td><td>0.885495</td></tr><tr><td>13</td><td>cube0</td><td>77.577301</td><td>143518</td><td>53.989189</td><td>0.695941</td><td>56.585945</td><td>0.729414</td><td>43.57135</td><td>0.561651</td><td>68.375137</td><td>0.881381</td></tr><tr><td>14</td><td>cube0</td><td>79.400002</td><td>146890</td><td>54.136215</td><td>0.681816</td><td>57.812431</td><td>0.728116</td><td>44.243782</td><td>0.557226</td><td>68.981079</td><td>0.868779</td></tr><tr><td>15</td><td>cube0</td><td>79.317841</td><td>146738</td><td>54.033512</td><td>0.681228</td><td>57.177296</td><td>0.720863</td><td>43.816757</td><td>0.55242</td><td>68.676216</td><td>0.865836</td></tr><tr><td>16</td><td>cube0</td><td>80.390808</td><td>148723</td><td>54.009731</td><td>0.67184</td><td>57.611893</td><td>0.716648</td><td>43.364864</td><td>0.539426</td><td>70.575676</td><td>0.877907</td></tr><tr><td>17</td><td>cube0</td><td>84.824326</td><td>156925</td><td>54.648109</td><td>0.64425</td><td>58.284866</td><td>0.687124</td><td>42.815136</td><td>0.504751</td><td>76.032433</td><td>0.896352</td></tr><tr><td>18</td><td>cube0</td><td>78.860001</td><td>145891</td><td>54.025948</td><td>0.685087</td><td>57.82</td><td>0.733198</td><td>43.699459</td><td>0.55414</td><td>68.462166</td><td>0.868148</td></tr><tr><td>19</td><td>cube0</td><td>79.150269</td><td>146428</td><td>54.017296</td><td>0.682465</td><td>58.177296</td><td>0.735023</td><td>43.815674</td><td>0.553576</td><td>68.165947</td><td>0.861222</td></tr><tr><td>20</td><td>cube0</td><td>79.089188</td><td>146315</td><td>53.98811</td><td>0.682623</td><td>57.604324</td><td>0.728346</td><td>43.797298</td><td>0.553771</td><td>68.152435</td><td>0.861716</td></tr><tr><td>21</td><td>cube0</td><td>78.625946</td><td>145458</td><td>53.952972</td><td>0.686198</td><td>57.782703</td><td>0.734906</td><td>44.067028</td><td>0.560464</td><td>67.874054</td><td>0.863253</td></tr><tr><td>22</td><td>cube0</td><td>80.194595</td><td>148360</td><td>53.974052</td><td>0.673039</td><td>57.955677</td><td>0.722688</td><td>43.76865</td><td>0.545781</td><td>70.025406</td><td>0.873194</td></tr><tr><td>23</td><td>cube0</td><td>84.536758</td><td>156393</td><td>54.593513</td><td>0.645796</td><td>58.799461</td><td>0.695549</td><td>43.023785</td><td>0.508936</td><td>75.827026</td><td>0.896971</td></tr></table>

MTE2的带宽利用率（Memory.csv）如下所示，平均带宽利用率提升到41.7%。

<table><tr><td>read_main_memory_datas(KB)</td><td>write_main_memory_datas(KB)</td><td>GM_to_L1_datas(KB)</td><td>GM_to_L1_bw_usage_rate(%)</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.642628</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.953102</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.42701</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>41.704773</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>41.269943</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>39.214825</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.155495</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.282257</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.270645</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.101028</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.089233</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>40.299694</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.584248</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.91449</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>41.929348</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>41.972782</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>41.412575</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>39.248062</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.216465</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.061646</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.094128</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>42.342133</td></tr><tr><td>9216.875</td><td>256.25</td><td>9216</td><td>41.513901</td></tr><tr><td>9216.75</td><td>256.125</td><td>9216</td><td>39.381569</td></tr></table>

查看OpBasicInfo.csv文件，优化后算子整体耗时为85.68us，耗时从98.72us降低到85.68us，性能提升13.2%。

# 总结

在多核执行Matmul的场景，当输入矩阵K轴较大（一般大于4096）时，可以尝试使用MDL模板并开启K轴错峰访问内存的功能，缓解地址访问冲突，提升MTE2搬运效率，进而优化算子性能。

# 3.10.4.10 Matmul 高阶 API 使能 NBuffer33 模板

# 案例介绍

本案例呈现了在矩阵乘算子场景中，使用Matmul高阶API进行矩阵乘法计算，使能NBuffer33模板对算子性能的提升效果。NBuffer33模板的实现为单核计算的A矩阵切分为3x3个基本块，该3x3个A矩阵的基本块全载和保持在L1 Buffer中，每次与3x1个B矩阵的基本块计算矩阵乘，同时DoubleBuffer并行搬入下次计算所需的3x1个B矩阵基本块，直到singleCoreN方向的矩阵乘计算完成。针对MTE2 Bound场景，通过NBuffer33算法的切分数据方式，错开搬运流水，减少单次搬运的数据量，平衡MTE2和FixPipe的数据流量，让两者带宽均匀分布。NBuffer33模板的详细介绍请参考MatmulPolicy。

使能NBuffer33模板的适用场景

MTE2 Bound的场景，Tiling参数满足约束条件时，可以使能NBuffer33模板。

使能NBuffer33模板的约束条件

仅支持MatmulConfig为MDL模板。

A矩阵、B矩阵的内存逻辑位置只支持TPosition::GM。

仅支持纯Cube模式（只有矩阵计算），暂不支持MIX模式（包含矩阵计算和矢量计算）。

仅支持通过IterateAll接口获取Matmul的计算结果C矩阵。

stepM、stepKa、stepKb小于等于3，且满足：

stepKa=stepKb=Ceil(singleCoreK/baseK)。 

A矩阵全载的基本块大小与B矩阵载入的基本块大小之和不超过L1 Buffer的大小。

本案例的算子规格如下：


表 3-41 算子规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>a</td><td>256, 192</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>192, 512</td><td>float16</td><td>ND</td></tr></table>

当前案例使用的AI处理器共24个核，算子中使能高阶API Matmul的纯Cube模式，使用MDL模板，Tiling参数如下：

原始shape：M=256, N=512, K=192。

● 单核shape：singleCoreM=256，singleCoreN=256，singleCoreK=192。

基本块shape：baseM=128，baseN=256，baseK=64。

● L1缓存相关Tiling参数：stepM=2，stepN=1，stepKa=3，stepKb=3。

# 获取性能数据

使用msProf工具获取算子仿真流水图和上板Profiling数据，重点分析Cube、Fixpipe的流水情况。

# 分析主要瓶颈点

优化前的流水图如下，MatmulPolicy的默认模板下A、B矩阵全载，A、B矩阵都只搬运一次。此时MTE2执行时间较长，且流水整体呈串行。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/7b2a1273675569fa47d007fd2d9bfd1f678f2b59847339e027d6fb2b99374e9b.jpg)


优化前的Profiling数据如下，aic_time平均耗时34.01us。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/8c818632dfeb765bda73ec10b20db631f4a058364e554ec5305026a9d075e036.jpg)


# 设计优化方案

使能NBuffer33模板：在GetTiling接口前，调用SetMatmulConfigParams接口开启NBuffer33模式，使获取的Tiling满足要求；Kernel侧在创建Matmul对象时使能NBuffer33模板。使能NBuffer33模板的完整样例请参考使能NBuffer33模板策略的样例。具体步骤如下：

# Tiling实现

调用GetTiling接口获取TCubeTiling结构体前，开启NBuffer33模式。

```cpp
matmul_tiling::MatmulConfigParams matmulConfigParams(1, false,
    matmul_tiling::ScheduleType::N_BUFFER_33, /* NBuffer33模式 */
    matmul_tiling::MatrixTraverse::NOSET, false);
cubeTiling.SetMatmulConfigParams(matmulConfigParams);
if (cubeTiling.GetTiling(tilingData) == -1) {
    std::cout << "Generate tiling failed." << std::endl;
    return {};
}
```

# Kernel实现

设置模板参数MatmulPolicy为NBuffer33模板策略，创建Matmul对象。

```rust
AscendC::Matmullmpl<
AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, aType>,
AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, bType>,
AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, cType>,
AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, biasType>, CFG_MDL,
AscendC::MatmulCallBackFunc<nullptr, nullptr, nullptr>,
AscendC::Impl::Detail::NBuffer33MatmulPolicy> matmulObj; 
```

# 验证优化方案性能收益

优化后的流水图如下，Tiling参数不变，但由于stepM为2，NBuffer33模式会将左矩阵数据的搬运拆分为两次。可以看到，第一次MTE2结束后的计算过程（包括MTE1、MMAD和FIXPIPE）可以和第二次MTE2并行。分块搬运数据可以减少一次搬运数据导致的部分头开销，优化加载数据的性能。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/bcc8f5618b3b1e2b30051ee22b41e1370c965dbdc7a6f5348a448ceeabd15450.jpg)


优化后的Profiling数据如下，aic_time平均耗时32.66us，较优化前的34.01us有所提升。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/85541199aefbed0ea5a99f0e9efc0099059f70da696a86723e25d540e71c4fe3.jpg)


# 总结

MTE2 Bound的场景，Tiling参数满足stepM、stepKa、stepKb小于等于3的条件时，可以考虑使能NBuffer33模板，切分矩阵将搬运流水错开， 减少单次搬运的数据量，平衡MTE2和FixPipe的数据流量。

# 3.10.4.11 Matmul 高阶 API 使能 IBShare 模板共享 B 矩阵数据

# 案例介绍

本案例呈现了在矩阵乘算子场景中，使用Matmul高阶API进行矩阵乘法计算，B矩阵使能IBShare对算子性能的提升效果。IBShare功能通过共享L1 Buffer上相同的A矩阵或B矩阵数据，减少重复的MTE2数据搬运开销，提升算子性能。该功能支持A矩阵和B矩阵其中一个矩阵使能IBShare，也支持A矩阵和B矩阵同时使能IBShare。

使能IBShare的适用场景

MIX场景（包含矩阵计算和矢量计算）下，多个AIV的A矩阵或B矩阵GM地址相同，且多个AIV复用的A矩阵或B矩阵在L1 Buffer上全载。

使能IBShare的约束条件

A矩阵和B矩阵同时使能IBShare的场景，同一算子中其它Matmul对象的A矩阵和B矩阵也必须同时使能IBShare。

A矩阵和B矩阵同时使能IBShare的场景，获取矩阵计算结果时，只支持调用IterateAll接口，且只支持输出到Global Memory。

本案例的算子规格如下：


表3-42 算子规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>a</td><td>64, 384</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>384, 256</td><td>float16</td><td>ND</td></tr></table>

当前案例使用的AI处理器共20个核，每个核中包含1个AIC核和2个AIV核。因为输入shape较小，本案例以单核为示例，参考SetDim接口在MIX模式下的使用，在Tiling程序中设置参与运算的核数为2。Tiling参数如下：

原始shape：M=64, N= 256, K=384。

单核shape：singleCoreM=32，singleCoreN=256，singleCoreK=384。A矩阵拆成两半，一半在AIV0上处理，一半在AIV1上处理；AIV0和AIV1使用的B矩阵数据相同。

基本块shape：baseM=32，baseN=256，baseK=64。

● L1缓存相关Tiling参数：stepM=1，stepN=1，stepKa=6，stepKb=6。

# 获取性能数据

使用msProf工具获取算子仿真流水图和上板Profiling数据，因为IBShare功能主要是通过共享L1 Buffer上相同的A矩阵或B矩阵数据，减少重复的MTE2数据搬运开销，所以重点分析MTE2的流水情况。

# 分析主要瓶颈点

优化前的流水图如下，不使能IBShare模板，默认使用的Norm模板。黑框标识AIV0发起的MTE2搬运流水：MTE2总共搬运了12次，其中A矩阵搬运了6次（stepM*stepKa=6），B矩阵搬运了6次（stepN*stepKb=6）。红框标识的AIV1发起的MTE2搬运流水，跟AIV0基本一致。在该案例中，因为AIV1使用的B矩阵跟AIV0使用的B矩阵数据相同，且singleCoreN=baseN*stepN，singleCoreK=baseK*stepKb，即B矩阵可以在L1全载。考虑在AIV0搬入B矩阵到L1Buffer后，将B矩阵数据缓存在L1 Buffer上等待AIV1进行复用，进而节省B矩阵的MTE2重复搬运开销。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/0a9db3da9650673be00e30b6a97978693482e13874e6c10bcfccc55ec7226e0f.jpg)


优化前的Profiling数据如下，C列的aic_time是10.29us，K列的aic_mte2_time是5.56us。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/f86423a98efd6685f3f776899fb0d5d9322ea70c88b5a1edecc45ca74b2664fc.jpg)


# 设计优化方案

下图是不使能IBShare模板（默认使用Norm模板）的Matmul计算流水示意图。MTE2分多次从Global Memory搬运基本块到A1或B1，即使前后两次搬运的B矩阵基本块数据是相同的数据，也会重复搬运。


图 3-182 不使能 IBShare 模板的 Matmul 流水示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/398bd4bfa1165720652d9c6a7ca9d090b3de5e8ae0303cc1ffed57c8e53af55f.jpg)


下图是使能IBShare模板的Matmul计算流水示意图。MTE2分多次从Global Memory搬运基本块到A1或B1，若前后两次搬运的B矩阵基本块数据相同，不会重复搬运，第一次搬运到B1内的数据会被复用。


图 3-183 使能 IBShare 模板的 Matmul 流水示意图


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/a3d0dd9f419532194d7d63e031452c6fd8ffa7e8ec01ad4ad896cdde14cb777d.jpg)


Matmul API使能IBShare模板共享B矩阵的完整样例请参考仅B矩阵使能IBShare样例。使能IBShare功能的主要步骤如下：


步骤1 创建Matmul对象。


```cpp
#define ASCENDC_CUBE_ONLY
#include "lib/matmul_intf.h"

using A_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, AType>;
using B_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BType, false, LayoutMode::NONE, true>; // 设置B矩阵的IBSHARE参数为true
using C_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, CType>;
using BIAS_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BiasType>;
AscendC::Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, CFG_IBSHARE_NORM> matmulObj; // 使用默认的IBShare模板参数CFG_IBSHARE_NORM定义Matmul对象
```

# ----结束

# 验证优化方案性能收益

优化后的流水图如下，黑框标识的AIV0发起的MTE2搬运流水，与优化前一致。红框标识的AIV1发起的MTE2搬运流水，相较于优化前的A矩阵和B矩阵一共12次MTE2数据搬运，减少到了仅6次A矩阵的MTE2数据搬运，省去了B矩阵的6次MTE2数据搬运开销。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/b7dd3adeeaa53a59a6433324de4e62d6a86eecb71cc38d9014c0e1bd4abf1484.jpg)


优化后的Profiling数据如下，C列的aic_time是9.93us，较优化前的10.29us提升了3.55%。K列的aic_mte2_time是4.71us，较优化前的5.56us提升了15.46%。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/363ca5d9e232329faba589cb73e9eebc1d154878e8eb3b0f696b0929aba2004d.jpg)


# 总结

MIX场景（包含矩阵计算和矢量计算）下，若多个AIV的A矩阵或B矩阵GM地址相同，且多个AIV复用的A矩阵/B矩阵在L1 Buffer上全载。可以考虑使能IBShare模板，通过共享L1 Buffer上相同的A矩阵或B矩阵数据，减少重复的MTE2数据搬运开销，提升算子性能。

# 3.10.4.12 Matmul 高阶 API 使能 IBShare 模板共享 A 和 B 矩阵数据

# 案例介绍

本案例呈现了在融合算子场景中，使用Matmul高阶API进行矩阵乘法计算时，A矩阵和B矩阵同时启用IBShare对性能的提升效果。

该案例的关键优化措施包括：

分核逻辑：以Cube核视角分核，Matmul计算结果输出到GM，提供给Vector核进行后续计算。

开启IBShare：A矩阵和B矩阵同时开启IBShare。

本案例的算子规格如下：


表3-43 算子规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>x</td><td>128,384</td><td>float16</td><td>ND</td></tr><tr><td>y</td><td>384,256</td><td>float16</td><td>ND</td></tr></table>

开启IBShare和未开启IBShare的完整样例请参考A、B矩阵均使能IBShare样例和MatmulNoABshare样例。

# 获取性能数据

使用msProf工具获取算子的Profiling的数据，重点分析MTE2，Cube，Scalar的流水情况。

# 分析主要瓶颈点


图 3-184 优化前 Profiling 数据


<table><tr><td>Op Name</td><td>Task Type</td><td>Task Duration</td><td>Task</td><td>Viacore</td><td>aic_tota</td><td>aic_mac</td><td>aic_mai</td><td>aic_scalar</td><td>time</td><td>aic_sca</td><td>aic_mte</td><td>aic_mte</td><td>aic_mte2</td><td>aic_mte</td><td>aic_fipxi</td><td>aic_fipxi</td><td>aic_chact</td><td>aiv_time</td><td>ut_ai</td><td>total_cai</td><td>aiv_vec</td><td>tin_ai</td><td>aiv_vec</td><td>rat_ai</td><td>scalar</td><td>iav_scalar</td></tr><tr><td>matmul_noAbshare_custom</td><td>MIX_AIC</td><td>26.661</td><td>2.7</td><td>26.3</td><td>48589</td><td>2.75</td><td>0.105</td><td>25.753</td><td>0.981</td><td>4.953</td><td>0.189</td><td>9.854</td><td>0.375</td><td>15.018</td><td>0.572</td><td>0.005</td><td>20.74</td><td>76729</td><td>0.042</td><td>0.002</td><td>4.941</td><td>0.238</td><td></td><td></td><td></td><td></td></tr><tr><td>matmul_noAbshare_custom</td><td>MIX_AIC</td><td>27.341</td><td>2.98</td><td>26.9</td><td>49818</td><td>2.75</td><td>0.102</td><td>26.393</td><td>0.98</td><td>4.954</td><td>0.184</td><td>10.048</td><td>0.373</td><td>15.206</td><td>0.565</td><td>0.007</td><td>21.09</td><td>78033</td><td>0.042</td><td>0.002</td><td>4.934</td><td>0.234</td><td></td><td></td><td></td><td></td></tr><tr><td>matmul_noAbshare_custom</td><td>MIX_AIC</td><td>27.441</td><td>2.96</td><td>27.1</td><td>50123</td><td>2.75</td><td>0.102</td><td>26.615</td><td>0.982</td><td>4.953</td><td>0.183</td><td>10.152</td><td>0.375</td><td>15.333</td><td>0.566</td><td>0.005</td><td>21.05</td><td>77890</td><td>0.042</td><td>0.002</td><td>4.71</td><td>0.224</td><td></td><td></td><td></td><td></td></tr><tr><td>matmul_noAbshare.custom</td><td>MIX_AIC</td><td>27.48</td><td>2.86</td><td>27.1</td><td>50142</td><td>2.75</td><td>0.101</td><td>26.777</td><td>0.988</td><td>4.953</td><td>0.183</td><td>10.17</td><td>0.375</td><td>15.357</td><td>0.567</td><td>0.005</td><td>21.29</td><td>78788</td><td>0.042</td><td>0.002</td><td>4.884</td><td>0.229</td><td></td><td></td><td></td><td></td></tr><tr><td>matmul_noAbshare.custom</td><td>MIX_AIC</td><td>26.8</td><td>2.82</td><td>26.4</td><td>48870</td><td>2.751</td><td>0.104</td><td>26.122</td><td>0.989</td><td>4.955</td><td>0.188</td><td>9.935</td><td>0.376</td><td>15.059</td><td>0.57</td><td>0.006</td><td>20.91</td><td>77352</td><td>0.042</td><td>0.002</td><td>5.004</td><td>0.239</td><td></td><td></td><td></td><td></td></tr><tr><td>matmul_noAbshare_custom</td><td>MIX_AIC</td><td>26.701</td><td>2.9</td><td>26.2</td><td>48653</td><td>2.75</td><td>0.105</td><td>26.732</td><td>0.98</td><td>4.954</td><td>0.188</td><td>9.923</td><td>0.375</td><td>15.076</td><td>0.573</td><td>0.006</td><td>20.9</td><td>77834</td><td>0.042</td><td>0.002</td><td>4.85</td><td>0.239</td><td></td><td></td><td></td><td></td></tr><tr><td>matmul_noAbshare.custom</td><td>MIX_AIC</td><td>26.881</td><td>2.92</td><td>26.5</td><td>48939</td><td>2.75</td><td>0.104</td><td>25.956</td><td>0.981</td><td>4.953</td><td>0.187</td><td>10.169</td><td>0.384</td><td>15.336</td><td>0.58</td><td>0.005</td><td>20.85</td><td>77148</td><td>0.042</td><td>0.002</td><td>4.74</td><td>0.227</td><td></td><td></td><td></td><td></td></tr><tr><td>matmul_noAbshare_custom</td><td>MIX_AIC</td><td>27.041</td><td>2.78</td><td>26.6</td><td>49225</td><td>2.75</td><td>0.103</td><td>26.081</td><td>0.98</td><td>4.954</td><td>0.186</td><td>10.25</td><td>0.385</td><td>15.398</td><td>0.579</td><td>0.006</td><td>21.07</td><td>77959</td><td>0.042</td><td>0.002</td><td>4.748</td><td>0.225</td><td></td><td></td><td></td><td></td></tr><tr><td>matmul_noAbshare_custom</td><td>MIX_AIC</td><td>27.28</td><td>2.68</td><td>27</td><td>49693</td><td>2.75</td><td>0.102</td><td>26.488</td><td>0.982</td><td>4.954</td><td>0.184</td><td>10.119</td><td>0.375</td><td>15.292</td><td>0.567</td><td>0.005</td><td>21.15</td><td>78248</td><td>0.042</td><td>0.002</td><td>4.745</td><td>0.224</td><td></td><td></td><td></td><td></td></tr><tr><td>matmul_noAbshare_custom</td><td>MIX_AIC</td><td>27.52</td><td>3.02</td><td>27.2</td><td>50314</td><td>2.751</td><td>0.101</td><td>26.672</td><td>0.981</td><td>4.954</td><td>0.182</td><td>10.349</td><td>0.38</td><td>15.529</td><td>0.571</td><td>0.005</td><td>21.42</td><td>79243</td><td>0.042</td><td>0.002</td><td>4.774</td><td>0.223</td><td></td><td></td><td></td><td></td></tr></table>

通过分析以上Profiling数据可以看出，算子执行多次的平均耗时为27.11us，aic_scalar_time的平均耗时为26.27us，当前性能瓶颈点为Cube的Scalar流水。

# 设计优化方案

A矩阵和B矩阵均未开启IBShare时，数据需要根据K轴、M轴或N轴进行切分计算。这里以K轴切分为例，未开启IBShare之前，算子以AIV Block为视角进行tiling切分，AIV0发起A0*B0的计算，AIV1发起A1*B1的计算。


图 3-185 未开启 IBShare


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/0b9a4bb4dfcdcac35226caba15570edadacea52df6debfee7db0ba68c21b2622.jpg)


当A矩阵和B矩阵都启用IBShare时，可以一次性加载到L1 Buffer上，省去了切分，分开搬运的过程，同时Cube计算单元完全由AIV0单核驱动，发起一次计算，计算的结果由AIV0和AIV1共享，从而减少Cube响应的次数，减少Scalar计算。


图 3-186 开启 IBShare


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/de7ba3d593b0093cf0754dc0e38e4543145a3ea6a299b7aa8ec9772a17ae127e.jpg)


开启IBShare和不开启IBShare的数据交互对比示意图如下：

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/c84edcd68baa16e0c6cae1040999a0d4ca06eab635b20595d05ee9ddcfd6536b.jpg)


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/e3e81a4f94695ca33d12faeca128f553b95852e1915ee93c0f3efda318a029b2.jpg)


通过设置A和B矩阵MatmulType的IBShare均为true，开启该优化，具体代码如下：

```cpp
constexpr bool isABshare = true;
template <typename aType, typename bType, typename cType> class MatmulABshareKernel {
public:
    __aicore__ inline MatmulABshareKernel(){};
    __aicore__ inline void Init(GM_ADDR a, GM_ADDR b, GM_ADDR c, GM_ADDR workspace,
    const TCubeTiling &tiling, AscendC::TPipe *pipe);
    __aicore__ inline void Process(AscendC::TPipe *pipe);
    __aicore__ inline void CalcOffset(int32_t blockIdx, const TCubeTiling &tiling, int32_t &offsetA, int32_t
    &offsetB,
    int32_t &offsetC);
    AscendC::Matmul<AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, aType, false,
    LayoutMode::NONE, isABshare>,
    AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, bType, false, LayoutMode::NONE,
    isABshare>,
    AscendC::MatmulType<AscendC::TPosition::VECIN, CubeFormat::ND, cType>>
    matmulObj; 
```

```cpp
AscendC::GlobalTensor<aType> aGlobal;
AscendC::GlobalTensor<bType> bGlobal;
AscendC::GlobalTensor<cType> cGlobal;
TCubeTiling tiling;
};
template <typename aType, typename bType, typename cType>
__aicore__ inline void MatmulABshareKernel<aType, bType, cType>::Init(GM_ADDR a, GM_ADDR b, GM_ADDR c,
    GM_ADDR workspace,const TCubeTiling &tiling, AscendC::TPipe
*pipe)
{
    this->tiling = tiling;
    aGlobal.SetGlobalBuffer(reinterpret_cast<_gm__ aType *>(a), tiling.M * tiling.Ka);
    bGlobal.SetGlobalBuffer(reinterpret_cast<_gm__ bType *>(b), tiling.Kb * tiling.N);
    cGlobal.SetGlobalBuffer(reinterpret_cast<_gm__ cType *>(c), tiling.M * tiling.N);
    int32_t offsetA, offsetB, offsetC;
    CalcOffset(AscendC::GetBlockIdx(), tiling, offsetA, offsetB, offsetC); // calculate offset
    aGlobal = aGlobal[offsetA];
    bGlobal = bGlobal[offsetB];
    cGlobal = cGlobal[offsetC];
}
template <typename aType, typename bType, typename cType>
__aicore__ inline void
MatmulABshareKernel<aType, bType, cType>::CalcOffset(int32_t blockIdx, const TCubeTiling &tiling,
    int32_t &offsetA, int32_t &offsetB, int32_t &offsetC)
{
    offsetA = 0;
    offsetB = 0;
    offsetC = 0;
} 
```

# 验证优化方案性能收益

优化后执行多次的平均耗时：22.44us，较优化前有较大提升。


图 3-187 优化后 Profiling 数据


![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/45eb0c211de7e52ceadacf6c833088d8b51d059dc25fdba8cb575c18f919f40b.jpg)


# 总结

融合算子场景下，Matmul A矩阵和B矩阵同时开启IBShare，以Cube核视角分核，可以有效减少Cube侧的Scalar开销，提升性能。

# 3.10.4.13 AIV 核上的 ND2NZ 格式转换

# 案例介绍

本案例展示了在矩阵乘算子场景中，使用Matmul高阶API进行计算，对内轴（内轴即矩阵的行方向）非256字节对齐的输入矩阵，在AIV核上进行ND2NZ格式转换对算子性能提升的效果。为提升Cube单元的计算效率，ND格式的输入矩阵在执行Cube计算前会先转换为NZ格式，ND格式和NZ格式的具体内容可参考数据格式。Matmul API内部使用随路ND2NZ指令同时进行格式转换以及数据搬运。但在数据非256字节对齐时，随路ND2NZ指令存在带宽利用率低的问题。因此输入矩阵的内轴非256字节对齐时，在进行Matmul计算前，利用AIV核上Vector计算单元完成ND格式到NZ格式的转换，可以避免随路非对齐数据搬运存在的效率低的问题，从而提升算子性能。

AIV核上的ND2NZ格式转换的适用场景

输入矩阵内轴非256字节对齐，且数据量较大影响随路格式转换的效率。

本案例的算子规格如下：


表3-44 算子规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>a</td><td>1024, 1024</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>1024, 4095</td><td>float16</td><td>ND</td></tr></table>

当前案例使用的AI处理器共24个核，算子中使能高阶API Matmul的纯Cube模式。使用MDL模板，Tiling参数如下：

原始shape：M=1024, N= 4095, K=1024。

● 单核shape：singleCoreM=128，singleCoreN=1408，singleCoreK=1024。

基本块shape：baseM=128，baseN=256，baseK=64。

● L1缓存相关Tiling参数：stepM=1，stepN=1，stepKa=4，stepKb=4。

# 获取性能数据

使用msProf工具获取算子仿真流水图和上板Profiling数据，重点分析MTE2的流水。

# 分析主要瓶颈点

优化前的Cube流水图如下，由于使用了随路ND2NZ指令，在MTE2数据搬运过程中进行数据格式的转换，导致MTE2整体占比较高。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/f86c0843254e61b5e32b5ec4a2c9d1aed26cc46b76ae4f8366d51f18622f0529.jpg)


优化前的Profiling数据如下，可以看到只使用Cube单元执行计算，aic_time最大耗时149.04us，其中aic_mte2_ratio占比很高。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/b0f1f11b0eee09f96a06ff6699f3ec35f8db21b3654da1cfe3e709791eaa9102.jpg)


# 设计优化方案

对于ND格式的输入矩阵，不再使用随路ND2NZ指令进行格式转换，而是利用Vector计算单元的能力完成数据格式转换。首先使用DataCopyPad接口，将非对齐的矩阵数据搬入Unified Buffer，使用Duplicate接口填充需要补为对齐位置的数据，再逐行调用Copy接口实现数据从ND到NZ格式的重排，将重排后的NZ数据写入workspace内存，最后直接读取workspace上的NZ数据，进行Matmul计算。

AIV核上的ND2NZ格式转换的完整样例请参考Matmul输入矩阵ND到NZ格式转换的算子样例。实现AIV核上的ND2NZ格式转换的主要步骤如下：

步骤1 创建Matmul对象时，定义内轴非256字节对齐的B矩阵的Format为NZ格式。

```julia
using A_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, ATYPE, true>; // 使用CubeFormat::NZ定义矩阵B的类型信息
using B_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, AscendC::TPosition::GM, CubeFormat::NZ, BType, true>; using C_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, CType>; using BIAS_TYPE = AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, BiasType>; AscendC::Matmul<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, CFG_MDL> matmulObj;
```

步骤2 利用Vector计算单元实现ND2NZ格式转换。如下代码中MatrixBtoNZ为将B矩阵的ND格式转换为NZ格式的函数，该函数的具体实现请参考完整样例代码。

```cpp
// Vector ND2NZ
if ASCEND_IS_AIV {
    pipe->InitBuffer(ubBuf, TOTAL_UB_SIZE);
    MatrixBtoNZ<typename B_TYPE::T>(tempGM, bGMNZ, tiling, isTransB, ubBuf, tiling.baseK, tiling.baseN); // ND2NZ格式转换函数
    SyncAll();
    // CV SYNC
    NotifyEvent<PIPE_MTE3>(4);
    return;
}
if ASCEND_IS_AIC {
    WaitEvent(4); // 等待Vector完成ND2NZ格式转换
}
```

步骤3 设置左矩阵A、右矩阵B、Bias，完成矩阵乘操作。

```javascript
matmulObj.SetTail(tailM, tailN, shapes.k);
matmulObj.SetTensorA(aGlobal, false);
matmulObj.SetTensorB(bGlobal, false);
if (shapes.isBias) {
    matmulObj.SetBias(biasGlobal);
}
matmulObj.IterateAll(cGlobal); 
```

----结束

# 验证优化方案性能收益

优化后的Vector流水图如下所示，利用Vector计算单元的能力，完成B矩阵的数据格式转换。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/15fae585b485346ebb722593189c28641366051d96e4da1c5d631ffc7e585306.jpg)


优化后的Cube流水图如下所示，不使用随路ND2NZ指令对B矩阵进行格式转换后，MTE2的占比明显下降。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/8a3431ef7b48ab9b4ea194c0a58d513d4bb5a67a664b0bb930cc727944a1ff6e.jpg)


优化后的Profiling数据如下，可以看到同时使用Cube单元和Vector单元，aic_time最大耗时90.95us，其中aic_mte2_ratio占比明显降低。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/2ca1d10b87e1a20fd981941805c11cbdcc5b8daef6d3ddcc894d301cd0263232.jpg)



表3-45 端到端性能对比


<table><tr><td>优化方法</td><td>总耗时(us)</td><td>AIC_MTE2平均耗时(us)</td><td>AIV_MTE2平均耗时(us)</td></tr><tr><td>随路ND2NZ</td><td>149.82</td><td>130.77</td><td>0</td></tr><tr><td>Vector侧ND2NZ</td><td>93.76</td><td>22.85</td><td>10.31</td></tr></table>

从上表中执行时间的对比，可以看出：不使用随路ND2NZ指令后，总耗时大幅下降，端到端性能提升明显。

# 总结

对于矩阵乘计算中矩阵内轴非256字节对齐的场景，随路ND2NZ指令的带宽利用率低，影响算子性能，通过在AIV核上进行ND2NZ的数据重排，提升算子整体性能。值得注意的是，带宽利用率与数据量有关，如果矩阵数据总量太小，即使是在AIV核上进行的ND2NZ转换也无法明显提升有效带宽，反而会因为引入了多核同步，导致算子端到端的性能劣化。

# 3.10.4.14 Matmul 高阶 API 使能 MTE2 Preload

# 案例介绍

本案例呈现了在矩阵乘算子场景中，使用Matmul高阶API进行矩阵乘法计算，使能MTE2 Preload对算子性能的提升效果。通过MatmulConfig中的doMTE2Preload参数开启矩阵M或N方向的预加载功能，预加载即在MTE2间隙提前加载A矩阵/B矩阵数据，开启预加载功能后，可以减少MTE2间隙，提升算子性能。doMTE2Preload参数的详细介绍请参考MatmulConfig。

使能MTE2 Preload的适用场景

MTE2流水间隙较大，且M或N数值较大时。

使能MTE2 Preload的约束条件

仅在使用MDL模板和SpecialMDL模板时，MTE2 Preload有效。

开启M或N方向预加载功能时，需保证K方向数据全载，且M或N方向开启DoubleBuffer。

K方向数据全载的条件是 $\mathsf { s i n g l e K < = b a s e K ^ { \star } 5 t e p K _ { c } }$ 

M方向开启DoubleBuffer的条件是 $\mathsf { \Lambda } _ { : \mathsf { d e p t h A 1 } } = \mathsf { s t e p M } ^ { \star } \mathsf { s t e p K } ^ { \star } 2$ 。

N方向开启DoubleBuffer的条件是 $\mathsf { \Lambda } _ { : \mathsf { d e p t h B } 1 } = \mathsf { s t e p N } \vdash \mathsf { s t e p K } \vdash 2$ 。

本案例的算子规格如下：


表3-46 算子规格


<table><tr><td>输入</td><td>Shape</td><td>Data type</td><td>Format</td></tr><tr><td>a</td><td>128, 512</td><td>float16</td><td>ND</td></tr><tr><td>b</td><td>512, 24576</td><td>float16</td><td>ND</td></tr></table>

当前案例使用的AI处理器共24个核，算子中使能高阶API Matmul的纯Cube模式。使用MDL模板，Tiling参数如下：

原始shape：M=128, N= 24576, K=512。

单核shape：singleCoreM=128，singleCoreN=1024，singleCoreK=512。

基本块shape：baseM=128，baseN=128，baseK=64。

L1缓存相关Tiling参数：stepM=1，stepN=1，stepKa=8，stepKb=8，depthA1=8，depthB1=16。

# 获取性能数据

使用msProf工具获取算子仿真流水图和上板Profiling数据，重点分析Cube，Fixpipe的流水情况。

# 分析主要瓶颈点

优化前的流水图如下，M和K方向全载，因此A矩阵只搬运一次。由于N较大，B矩阵会搬运多次，可以看到单次MTE2间存在间隙。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/737963b8c91634cacd48e8163b53427f7ad7df2b74b670070edf3d9cf04f3a9a.jpg)


优化前的Profiling数据如下，aic_time平均耗时30.88us。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/f69f368c3d6390f46ae9c8c276db31b3323ac9a4804a5228e9bd217e4b92d855.jpg)


# 设计优化方案

使能MTE2 Preload功能：在创建Matmul对象时，开启doMTE2Preload开关。使能MTE2 Preload的完整样例请参考M方向预加载Matmul算子样例。具体步骤如下：

步骤1 配置MDL模板参数，将其中的doMTE2Preload参数设置为2，使能N方向Preload功能。

```txt
// preloadMode = 2
static constexpr MatmulConfig MM_CFG = GetMDLConfig(false, false, preloadMode); 
```

步骤2 基于自定义MatmulConfig模板参数，创建Matmul对象。

```txt
AscendC::Matmul<AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, aType>, AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, bType>, 
```

AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, cType>, AscendC::MatmulType<AscendC::TPosition::GM, CubeFormat::ND, biasType>, MM_CFG> matmulObj; 

# ----结束

# 验证优化方案性能收益

优化后的流水图如下，Tiling参数不变，可以看到，下一次计算使用的B矩阵数据提前加载，MTE2间的间隙缩短。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/2536ccb15134c39ad7f5ccdcf47b2ca8fb9f5bd8611c9d2acf7ff4c7381b4112.jpg)


优化后的Profiling数据如下，aic_time平均耗时28.50us，较优化前的30.88us有所下降。

![image](https://cdn-mineru.openxlab.org.cn/result/2026-05-11/67a17f70-7f6c-40cc-9c44-057874d977d6/c86a3d5e04232699e34245fc4dad2fc99ed4e8b9a97faff5b5b10b62ae710e27.jpg)


# 总结

当MTE2流水间隙较大，且M或N数值较大时，可以考虑使能MTE2 Preload功能，提前加载A矩阵或B矩阵数据。