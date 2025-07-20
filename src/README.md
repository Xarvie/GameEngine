Spartan 渲染库 - v0.1 严格技术规范

0.0 核心设计哲学 (Core Design Philosophy)
0.1 [核心] 斯巴达式设计哲学 (Spartan Philosophy): 本库严格遵循“最少的代码，最高的性能”原则。如果某个特性过于复杂，且对核心目标非绝对必要，则不予引入。
0.2 [原则] 按需引入 (On-Demand Complexity): 只有当一个功能被证明是绝对必要，且无法用更简单方式替代时，才会被引入库的核心。
0.3 [核心] Web一等公民 (Web First): 库的设计以Web平台（WebGL 2.0）为最高优先级，确保其在浏览器环境中拥有原生级的性能和行为。
0.4 [原则] 单线程至简 (Single-Threaded Simplicity): 严格遵循单线程模型，放弃多线程的复杂性、同步开销和不确定性，以追求性能极限和行为的可预测性（动画系统除外）。
0.5 [核心] 数据驱动 (Data-Oriented): 库的一切都围绕数据（组件）和数据的转换（系统）来设计。逻辑和数据严格分离，我们不关心“一个物体是什么”，只关心“它拥有哪些数据”。
0.6 [原则] 指令式渲染 (Command-based Rendering): CPU的核心任务是生成一个最优的渲染指令队列（RenderQueue），而非直接进行渲染。通过最大化批处理和实例化，将CPU到GPU的通信开销降至最低。
0.7 [原则] 最小化状态 (Statelessness): 底层渲染器本身尽可能无状态。所有渲染所需的信息都由渲染队列在每一帧动态提供，以减少模块间的隐性依赖。
0.8 [内部实现] ECS: 库的内部实现采用实体组件系统（ECS）来高效地管理数据。但库的公开API是独立的，不强制用户使用任何特定的架构。
0.9 [不支持] 禁止OpenGL扩展API (No Extensions): 包含调试扩展（如 KHR_debug），禁止使用任何非核心的OpenGL ES 3.0扩展，以保证绝对的跨平台一致性。

1.0 核心渲染管线 (Core Rendering Pipeline)
1.1 [支持] 统一的Forward渲染路径 (Unified Forward Rendering): 库的核心将采用单一的 Forward 渲染路径。
1.2 [不支持] Z-Prepass: 为确保在桌面、移动和Web端获得一致的架构和可预测的性能，库的渲染路径将不包含Z-Prepass。
1.3 [支持] 渲染目标管理 (MRT): API将支持创建和绑定多渲染目标（MRT），为后期处理提供基础。
1.4 [不支持] 自定义渲染管线/渲染图 (No Custom Pipeline): 库提供单一、固定的高效管线，不提供高层级的管线定制能力。
1.5 [不支持] GPU-based 遮挡剔除 (No GPU-based Occlusion Culling): 由于在目标平台 GLES 3.0 上的性能限制，不予支持。

2.0 光照与阴影 (Lighting & Shadow)
2.1 [支持] 基础光源类型 (Basic Light Types): 支持方向光 (Directional)、点光源 (Point)、聚光灯 (Spot)。
2.2 [不支持] 高级光源类型 (No Advanced Light Types): 舍弃复杂的区域光 (Area Lights) 和 IES 光源配置文件。
2.3 [支持] 动态阴影 (Dynamic Shadows):
为方向光提供简化的级联阴影贴图 (CSM)，建议2-3级级联。
为点光源提供立方体阴影贴图。
2.4 [支持] 软阴影 (Soft Shadows): 支持基础的 PCF (Percentage-Closer Filtering) 算法。
2.5 [不支持] 高级软阴影 (No Advanced Soft Shadows): 舍弃 PCSS 等性能开销巨大的软阴影算法。
2.6 [支持] 间接光照 (Indirect Lighting): 支持使用由引擎端提供的光照贴图 (Lightmaps) 和光照探针 (Light Probes) 数据。
2.7 [不支持] 实时全局光照 (No Real-time GI): 舍弃所有形式的实时GI和实时反射探针更新。

3.0 材质与着色系统 (Material & Shading System)
3.1 [支持] 完全可编程的自定义渲染风格 (Fully Programmable Shading): 通过createShader() API，用户可以提交任意GLSL代码，实现无限的视觉风格。
3.2 [支持] 内置着色器范例 (Built-in Shader Examples):
提供高质量的 PBR (Physically Based Rendering) Uber Shader。
提供高质量的卡通渲染 (Toon Shading) Shader，包含描边功能。
3.3 [不支持] 内置高级材质模型 (No Advanced Material Models): 舍弃对次表面散射(SSS)、头发、布料、各向异性等特化材质模型的内置支持。
3.4 [规范] 不支持透射与折射 (No Refraction/Transmission): 对于需要此类效果的材质，将统一按常规的半透明（Alpha Blending）材质处理。
3.5 [支持] 半透明渲染 (Alpha Blending): 通过基于排序键的机制，支持可靠的Alpha Blending渲染。
3.6 [支持] 材质热重载 (Material Hot-reloading): 在开发模式下，支持着色器和材质参数的运行时更新，以加速迭代和调试。

4.0 ozz动画与LOD (Animation & LOD)
4.1 [支持] 使用OZZ库GPU蒙皮 (OZZ GPU Skinning): 统一vtf骨骼变换矩阵，存放于单一4K纹理中，在GPU中完成蒙皮计算。
4.2 [不支持] 不支持变形目标动画 (Morph Target)。
4.3 [不支持] 高级动画系统 (No Advanced Animation System): IK、程序化动画等属于引擎职责。
4.4 [支持] 基础网格LOD (Basic Mesh LOD): 引擎可以通过提交不同的MeshHandle来实现LOD切换。
4.5 [不支持] 高级LOD系统 (No Advanced LOD System): 舍弃HLOD和自动LOD生成。
4.5 [支持] 强制离线期间把节点动画转换为骨骼动画。

5.0 粒子与特效 (Particles & VFX)
5.1 [支持] GPU粒子系统 (GPU Particle System): 提供一套独立的API来高效渲染大规模粒子。
5.2 [不支持] 粒子物理模拟 (No Particle Physics): 舍弃基于Compute Shader或Transform Feedback的复杂粒子物理。

6.0 后期处理 (Post-Processing)
6.1 [支持] 核心后期效果 (Core Post-FX): 内置支持泛光 (Bloom)、色调映射 (Tonemapping)、颜色分级 (LUT) 和 FXAA。
6.2 [支持] 基础后期效果 (Basic Post-FX): 内置支持简单的景深效果。
6.3 [不支持] 高级后期效果 (No Advanced Post-FX): 舍弃TAA、屏幕空间反射(SSR)和复杂的体积效果。

7.0 资源与平台 (Resources & Platform)
7.1 [核心] 目标平台 (Target Platform): 强制使用 OpenGL ES 3.0 / WebGL 2.0 的API能力集，不使用任何其他版本的OpenGL API，以确保绝对的跨平台一致性。
7.2 [不支持] 硬件曲面细分 (No Hardware Tessellation): 因目标平台不支持而舍弃。
7.3 [支持] 移动端压缩纹理 (Mobile Compressed Textures): 支持加载 ETC2 / ASTC 格式。
7.4 [不支持] 高级资源加载 (No Advanced Resource Loading): 舍弃虚拟纹理(Virtual Texture)和内置的异步资源加载。
7.5 [支持] 渲染统计 (Render Stats): 提供用于获取单帧性能指标的API，如 getRenderStats()，可返回Draw Call数、三角形面数等信息，不需要GPU耗时（不支持扩展）。

8.0 渲染批处理系统 (合批)
8.1 [核心机制] 64位排序键 (Sort Key): 批处理系统完全由一个精心设计的64位排序键驱动，通过一次全局排序实现高效的批次聚合。
8.2 [核心原则] 按材质和Mesh合批 (Batch by Material & Mesh): 排序键的设计将确保使用相同材质和网格的物体被聚合，以最大化利用实例化渲染。
8.3 [执行流程] 固定执行流 (Fixed Flow): 严格遵循“填充渲染队列 -> 全局排序 -> 遍历与合批 -> 批处理提交”的流程。
8.4 [实例化] 最大化利用实例化 (Maximize Instancing): 最大化利用glDrawElementsInstanced进行渲染。
8.5 [支持] 静态批处理工具 (Static Batching Tool): 提供一个可选的工具函数，用于在资源加载阶段或编辑器中，将多个静态网格（Static Mesh）预先合并成一个单一的网格，从而在运行时从源头上减少Draw Call。

9.0 数据传输与缓冲区规范
9.1 [规范] 数据按频率和大小，分别使用 UBO (全局/每帧)、VTF (批次/实例)、Uniforms (材质)、VBO (顶点)进行传输。
9.2 [规范] UBO用法 (UBO Usage): UBO有尺寸限制，仅用于存储所有着色器在一帧内都需要访问的、统一的、小体积数据（如相机矩阵）。
9.3 [规范] 大数据传输 (Large Data Transfer): 所有需要按实例或批次变化的大数据块（如模型矩阵、骨骼矩阵），强制使用纹理传输（VTF）。
9.4 [不支持] SSBO: 由于目标平台不提供原生支持，为保证全平台统一，本库完全不使用SSBO。
9.5 [规范] 实例数据限制 (Instance Count Limit): 为保证缓冲区大小和性能的可控性，单次glDrawElementsInstanced调用所处理的实例数量建议设置上限（如1024个）。当运行时批次超过此上限时，将自动拆分为多个绘制指令。

10.0 坐标系与惯例约定
10.1 [约定] 统一使用右手坐标系，Y轴朝上。
10.2 [约定] 统一使用**逆时针（CCW）**作为正面绕序。
10.3 [约定] API角度单位统一为弧度 (Radians)。

11.0 线程模型规范
11.1 [核心] 库不是线程安全的，所有API调用必须在同一线程。

12.0 着色器接口规范
12.1 [规范] 固定的顶点属性绑定位置 (Vertex Attribute Locations)。
12.2 [不支持] gles3.0没有UBO绑定点 (e.g., binding = 0)。

13.0 明确排除的引擎级功能
13.1 [排除] 场景管理, 地形系统, 物理集成, 动画逻辑, GUI。
13.2 [排除] 游戏即编辑器: 此为上层引擎的设计哲学，本库不提供相关支持。
13.3 [排除] 脚本支持 (Lua等): 脚本系统属于引擎职责。

14.0 编码与命名规范
14.1 [规范] 文件名: 所有文件名采用大驼峰命名法 (UpperCamelCase)，如 RenderWorld.h。
14.2 [规范] 代码命名: 类命名使用大驼峰命名法，代码中的函数、变量等采用小驼峰命名法 (lowerCamelCase)，如 createMesh()。


其他：
显存（UMA）带宽：
手机档次	主要SoC型号	内存类型	预估带宽范围
旗舰级	骁龙8 Gen 3, 天玑9300+, A17 Pro	LPDDR5X / LPDDR5T	~68 GB/s - 77 GB/s
中端	骁龙7系列, 天玑8000系列	LPDDR5 / LPDDR4X	~30 GB/s - 50 GB/s
入门级	骁龙6/4系列, 天玑700/600系列	LPDDR4X	~15 GB/s - 25 GB/s