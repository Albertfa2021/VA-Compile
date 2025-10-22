# VA2022a 快速编译指南

> **重要提示**: 这是**已修复版本**，所有8个编译问题都已解决，可以直接编译！无需再应用修复脚本。

## 环境要求

### 必需软件
- **Visual Studio 2022** (MSVC 19.43+)
  - 安装时选择"Desktop development with C++"
  - 包含CMake工具
- **CMake** 3.25+
- **Matlab** R2024a (或其他R20xx版本)
- **Git** (用于克隆代码)
- **Python** 3.x (已安装在系统PATH中)

### 硬盘空间
- 源码: ~500MB
- 构建目录: ~2GB
- 发行版输出: ~50MB
- **建议预留**: 3GB

## 三步编译

### 第1步: 克隆代码
```bash
git clone https://github.com/Albertfa2021/VA-Compile.git
cd VA-Compile
```

**说明**: 这个仓库已包含所有修复，源码可直接编译！

### 第2步: 配置CMake
```bash
configure.bat
```

这个脚本会：
- 创建build/目录
- 配置CMake (使用va_toolchain.cmake)
- 下载所有依赖 (sndfile, eigen, gRPC等)
- 生成Visual Studio 2022项目

**预计时间**: 5-15分钟 (取决于网速)

**可能需要**:
- 如果Matlab不在 `C:\Program Files\MATLAB\R2024a`，请修改 `va_toolchain.cmake` 第23行

### 第3步: 编译
```bash
build.bat
```

或者分别编译关键组件：
```bash
cd build
cmake --build . --config Release --target VACore -j8
cmake --build . --config Release --target VAServer -j8
cmake --build . --config Release --target VAMatlab -j8
cmake --build . --config Release --target INSTALL
```

**预计时间**: 10-20分钟 (首次编译)

### 第4步: 检查输出
编译成功后，检查 `dist/` 目录：

```
dist/
├── VAServer_v2022a/
│   ├── bin/
│   │   ├── VACore.dll        (1.8MB)
│   │   ├── VAServer.exe      (121KB)
│   │   └── ... (38个DLL)
│   ├── conf/                  (10个配置文件)
│   └── data/                  (HRIR、方向性数据)
│
└── VAMatlab_v2022a/
    ├── @VA/private/
    │   ├── VAMatlab.mexw64   (616KB)  ← Matlab MEX文件
    │   └── *.dll
    └── VA_example_*.m        (9个示例)
```

## 验证编译结果

### 测试VAMatlab
```matlab
cd D:\Project\VA-build\dist\VAMatlab_v2022a
addpath(genpath(pwd))
VA                              % 打开VA设置GUI
VA_example_simple               % 运行简单示例
VA_example_outdoor_acoustics    % 测试OutdoorNoise渲染器
```

### 测试VAServer
```bash
cd D:\Project\VA-build\dist\VAServer_v2022a
run_VAServer.bat
```

应该看到服务器启动并监听端口的日志输出。

## 常见问题 (FAQ)

### Q1: CMake找不到Matlab
**症状**: `Could NOT find Matlab (missing: Matlab_ROOT_DIR)`

**解决**: 编辑 `va_toolchain.cmake` 第23行:
```cmake
set(Matlab_ROOT_DIR "C:/Program Files/MATLAB/R2024a" CACHE PATH "Matlab root directory")
```
改为您的Matlab实际安装路径。

### Q2: 编译时出现NatNetSDK错误
**症状**: `Cannot find NatNetSDK library`

**解决**: 确认 `dependencies/NatNetSDK/` 目录存在且包含:
```
dependencies/NatNetSDK/
├── include/
│   └── NatNetTypes.h
└── lib/x64/
    └── NatNetLibStatic.lib
```

如果缺失，从官方下载NatNetSDK 2.10并解压到此目录。

### Q3: Python脚本无法运行
**症状**: `python: command not found`

**解决**:
1. 确认Python已安装: `python --version`
2. 将Python添加到系统PATH
3. 或直接使用: `py apply_fixes.py` (Windows)

### Q4: 增量编译失败
**症状**: 修改源码后重新编译报错

**解决**:
```bash
# 清理build目录
cd build
cmake --build . --target clean

# 或完全删除重新配置
cd ..
rmdir /s /q build
configure.bat
build.bat
```

### Q5: 想要调试版本而非Release
**修改** `configure.bat` 和 `build.bat` 中的 `Release` 为 `Debug`:
```batch
cmake --build . --config Debug -j8
```

## 技术细节

### 已解决的8个编译问题
1. ✅ UNICODE编译错误 (ASIO/TBB字符类型冲突)
2. ✅ NatNetSDK链接错误 (静态库vs动态库)
3. ✅ ITASimulationScheduler未找到 (OutdoorNoise选项)
4. ✅ sprintf/sscanf链接错误 (CRT兼容性)
5. ✅ EVDLAlgorithm API变更 (24个文件)
6. ✅ FFTW m.lib错误 (Windows平台检查)
7. ✅ EVDLAlgorithm类型转换 (enum class)
8. ✅ 字符编码错误 (度数符号)

详细技术说明见 [CLAUDE.md](CLAUDE.md)

### 修改的源文件
本版本已对33个源文件应用修复，包括:
- `VACore/src/Rendering/Base/VAAudioRendererBase.h` (EVDLAlgorithm类型)
- `VACore/src/Rendering/Binaural/*/` (14个双耳渲染器)
- `VACore/src/directivities/VADirectivityDAFFEnergetic.cpp` (编码)
- 等27个其他文件

所有修复脚本 (`apply_fixes.py`, `fix_*.py`) 已包含在仓库中，但**无需手动运行** - 修复已应用到源码中。

### 依赖清单
自动下载的依赖 (通过CPM):
- sndfile 1.0.31
- samplerate 0.2.1
- eigen 3.4.0
- gRPC 1.49.0
- fftw 3.3.9
- portaudio 19.7.0
- tbb 2021.3.0
- 等25个库

手动包含的依赖:
- VistaCMakeCommon (Git submodule)
- NatNetSDK 2.10 (静态库)
- DAFF (openDAFF)

## 目录结构

```
VA-build/
├── source/                 # VA源码 (已修复，可直接编译)
├── dependencies/           # 手动依赖
│   ├── VistaCMakeCommon/
│   └── NatNetSDK/
├── build/                  # CMake构建目录 (gitignore)
├── dist/                   # 编译输出 (gitignore)
├── va_toolchain.cmake      # CMake工具链配置
├── configure.bat           # CMake配置脚本
├── build.bat               # 编译脚本
├── apply_fixes.py          # 修复脚本 (已应用，仅供参考)
├── fix_*.py                # 其他修复脚本 (已应用，仅供参考)
├── CLAUDE.md               # 完整技术文档
├── BUILD_GUIDE.md          # 本文档
└── README.md               # 项目说明
```

## 进阶使用

### 仅编译VAMatlab
如果您只需要Matlab绑定:
```bash
configure.bat
cd build
cmake --build . --config Release --target VAMatlab -j8
cmake --build . --config Release --target INSTALL
```

### 启用更多渲染器
编辑 `configure.bat`，取消注释相应选项:
```batch
-DITA_VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE=ON ^
-DITA_VACORE_WITH_RENDERER_AMBISONICS_VBAP=ON ^
```

### 修改编译器优化级别
编辑 `va_toolchain.cmake`:
```cmake
add_compile_options(/O2)  # 最大速度优化
# add_compile_options(/Od /Zi)  # 调试版本
```

## 获取帮助

- **技术细节**: 阅读 [CLAUDE.md](CLAUDE.md)
- **Issue**: https://github.com/Albertfa2021/VA-Compile/issues
- **原始VA仓库**: https://git.rwth-aachen.de/ita/VA

## 版本信息

- **VA版本**: v2022a
- **编译工具链**: MSVC 19.43 (VS2022)
- **目标平台**: Windows 10/11 x64
- **发布日期**: 2025-10-21
- **Release Tag**: v1.0.0-va2022a-ready-to-build

---

**祝编译顺利！** 🎉

如有问题，请先查看FAQ，或在GitHub提交Issue。
