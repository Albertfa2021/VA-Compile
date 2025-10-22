# VA2022a Ready-to-Build 编译项目

> **🎉 这是已修复版本！clone下来即可直接编译，无需再处理编译问题！**

本项目提供Virtual Acoustics (VA) v2022a的**完整可编译版本**，所有已知编译问题已解决，开箱即用。

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![VA Version](https://img.shields.io/badge/VA-v2022a-blue)]()
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-lightgrey)]()
[![License](https://img.shields.io/badge/license-See%20VA%20License-orange)]()

## ✨ 特性

- ✅ **即拉即编译**: 所有33个源文件已修复，无需手动打补丁
- ✅ **完整工具链**: 包含CMake工具链、配置脚本、编译脚本
- ✅ **详细文档**: 8个编译问题的完整解决方案记录
- ✅ **OutdoorNoise渲染器**: 保留并可用（包含ITASimulationScheduler）
- ✅ **全平台DLL**: 生成38个DLL + VAMatlab.mexw64
- ✅ **测试通过**: 已验证VACore、VAServer、VAMatlab均可正常工作

## 🚀 快速开始

### 1. 克隆仓库
```bash
git clone https://github.com/Albertfa2021/VA-Compile.git
cd VA-Compile
```

### 2. 配置CMake
```bash
configure.bat
```

### 3. 编译
```bash
build.bat
```

### 4. 检查输出
```bash
cd dist\VAMatlab_v2022a
dir @VA\private\VAMatlab.mexw64  # 应该看到616KB的MEX文件
```

**完整指南**: 见 [BUILD_GUIDE.md](BUILD_GUIDE.md)

## 📋 环境要求

| 软件 | 版本要求 | 说明 |
|------|---------|------|
| Visual Studio | 2022 | 需包含"Desktop development with C++" |
| CMake | 3.25+ | 通常随VS2022安装 |
| Matlab | R2024a | 或其他R20xx版本 |
| Python | 3.x | 用于辅助脚本 |
| 硬盘空间 | 3GB | 包含源码、构建、输出 |

## 📁 项目结构

```
VA-build/
├── source/                    # ✅ VA源码 (已修复，可直接编译)
│   ├── VACore/                # 核心库
│   ├── VAServer/              # 服务器应用
│   ├── VAMatlab/              # Matlab绑定
│   └── ...
├── dependencies/              # ✅ 手动依赖 (必需)
│   ├── VistaCMakeCommon/      # CMake扩展
│   └── NatNetSDK/             # 运动追踪SDK
├── build/                     # CMake构建目录 (gitignore)
├── dist/                      # 编译输出 (gitignore)
│   ├── VAServer_v2022a/       # 服务器发行版
│   └── VAMatlab_v2022a/       # Matlab发行版
├── va_toolchain.cmake         # ✅ CMake工具链
├── configure.bat              # ✅ CMake配置脚本
├── build.bat                  # ✅ 编译脚本
├── apply_fixes.py             # 修复脚本 (已应用，仅供参考)
├── fix_*.py                   # 其他修复脚本 (已应用)
├── BUILD_GUIDE.md             # 📖 快速开始指南
├── CLAUDE.md                  # 📖 完整技术文档
└── README.md                  # 📖 本文档
```

## 🔧 已解决的编译问题

本版本解决了从零编译VA2022a时遇到的**8个关键技术问题**：

| # | 问题 | 根本原因 | 解决方案 |
|---|------|----------|----------|
| 1 | UNICODE编译错误 | UNICODE定义导致ASIO/TBB冲突 | 移除toolchain中的UNICODE定义 |
| 2 | NatNetSDK链接错误 | 动态库导入库vs静态库 | FindNatNetSDK.cmake使用静态库 |
| 3 | ITASimulationScheduler未找到 | OutdoorNoise选项未启用 | 添加cmake选项 |
| 4 | sprintf/sscanf链接错误 | 旧版CRT函数兼容性 | 链接legacy_stdio_definitions.lib |
| 5 | EVDLAlgorithm API变更 | ITACoreLibs API升级 | 批量修复24个文件 |
| 6 | FFTW m.lib错误 | Unix数学库在Windows不存在 | 添加平台检查 |
| 7 | EVDLAlgorithm类型错误 | int→enum class类型升级 | 修改3个文件的类型声明 |
| 8 | 字符编码错误 | 度数符号(°)在GBK编码下错误 | 替换为"degrees"文本 |

**详细技术说明**: 见 [CLAUDE.md](CLAUDE.md)

## 📦 编译输出

成功编译后，`dist/`目录包含：

### VAServer_v2022a (服务器应用)
- **VAServer.exe** (121KB) - VA服务器
- **VACore.dll** (1.8MB) - VA核心库
- **38个DLL** - 所有依赖库（ITACoreLibs、第三方库）
- **conf/** - 10个配置文件
- **data/** - HRIR数据、方向性数据、音频示例
- **run_VAServer.bat** - 启动脚本

### VAMatlab_v2022a (Matlab绑定)
- **VAMatlab.mexw64** (616KB) - Matlab MEX文件 ⭐
- **@VA/** - VA类和私有依赖
- **VA_example_*.m** - 9个示例脚本
- **VA_example_outdoor_acoustics.m** - OutdoorNoise渲染器示例 ⭐

### 其他
- **VACS_v2022a/** - C#绑定
- **VAUnity_v2022a/** - Unity插件

## 🎯 验证编译结果

### 测试Matlab绑定
```matlab
cd dist\VAMatlab_v2022a
addpath(genpath(pwd))
VA                              % 打开VA设置GUI
VA_example_simple               % 运行简单示例
```

### 测试VAServer
```bash
cd dist\VAServer_v2022a
run_VAServer.bat
```

应该看到服务器启动日志，无报错。

## 📚 文档

| 文档 | 内容 | 适合读者 |
|------|------|---------|
| [BUILD_GUIDE.md](BUILD_GUIDE.md) | 快速开始、FAQ、环境配置 | 所有用户 ⭐ |
| [CLAUDE.md](CLAUDE.md) | 完整编译过程、问题诊断、解决方案 | 开发者、排错 |
| [BUILD_PROGRESS.md](BUILD_PROGRESS.md) | 实时编译进度记录 | 了解历史 |

## 🏷️ 版本信息

- **Release版本**: v1.0.0-va2022a-ready-to-build
- **VA版本**: v2022a (RWTH Aachen University)
- **编译工具链**: MSVC 19.43 (Visual Studio 2022)
- **CMake版本**: 3.25
- **目标平台**: Windows 10/11 x64
- **Matlab版本**: R2024a
- **发布日期**: 2025-10-21
- **编译状态**: ✅ **100%完成，测试通过**

## 🔄 版本管理策略

### 推荐的开发工作流

1. **克隆此ready-to-build版本作为基线**
   ```bash
   git clone https://github.com/Albertfa2021/VA-Compile.git
   cd VA-Compile
   git checkout v1.0.0-va2022a-ready-to-build  # 锁定到stable版本
   ```

2. **创建您的开发分支**
   ```bash
   git checkout -b dev/your-feature-name
   ```

3. **开发和测试**
   - 修改 `source/` 中的源码
   - 运行 `build.bat` 增量编译
   - 测试您的修改

4. **出问题时回退到可编译状态**
   ```bash
   git stash                    # 暂存您的修改
   git checkout main            # 或 release/v1.0.0-va2022a
   git pull origin main
   build.bat                    # 重新编译基线版本
   ```

5. **合并您的修改**
   ```bash
   git checkout dev/your-feature-name
   git stash pop                # 恢复您的修改
   # 测试修改，确保能编译
   git commit -am "Your feature description"
   ```

### 分支策略

- **main**: 始终保持可编译状态（本release）
- **release/vX.X.X**: 稳定的release分支
- **dev/***: 您的开发分支
- **fix/***: 修复bug的分支

### Tag策略

- **v1.0.0-va2022a-ready-to-build**: 初始可编译版本（当前）
- **v1.1.0-va2022a-with-your-feature**: 您添加功能后的版本
- **v2.0.0-va2023a**: 升级到VA新版本时

## 🛠️ 进阶用法

### 仅编译特定组件
```bash
configure.bat
cd build
cmake --build . --config Release --target VAMatlab -j8
cmake --build . --config Release --target INSTALL
```

### 编译Debug版本
修改 `build.bat` 中的 `Release` 为 `Debug`

### 启用更多渲染器
编辑 `configure.bat`，取消注释：
```batch
-DITA_VACORE_WITH_RENDERER_BINAURAL_AIR_TRAFFIC_NOISE=ON ^
```

### 修改Matlab路径
编辑 `va_toolchain.cmake` 第23行

## ⚠️ 常见问题

### Q: CMake找不到Matlab
**A**: 编辑 `va_toolchain.cmake`，修改 `Matlab_ROOT_DIR` 为您的Matlab安装路径

### Q: 增量编译失败
**A**: 清理build目录后重新配置：
```bash
rmdir /s /q build
configure.bat
build.bat
```

### Q: 修复脚本还需要运行吗？
**A**: **不需要！** `apply_fixes.py` 等脚本的修复已应用到 `source/` 中，仅供参考

更多问题请查看 [BUILD_GUIDE.md](BUILD_GUIDE.md) 的FAQ章节

## 🤝 贡献

欢迎提交Issue和Pull Request！

- **报告问题**: [GitHub Issues](https://github.com/Albertfa2021/VA-Compile/issues)
- **提交改进**: Fork → 修改 → Pull Request
- **讨论**: [GitHub Discussions](https://github.com/Albertfa2021/VA-Compile/discussions)

## 📜 许可证

本项目遵循Virtual Acoustics原始许可证。详见 `COPYING` 文件。

- **VA源码**: RWTH Aachen University许可证
- **构建脚本和文档**: MIT许可证

## 🔗 相关链接

- **VA官方仓库**: https://git.rwth-aachen.de/ita/VA
- **VistaCMakeCommon**: https://github.com/VRGroup/VistaCMakeCommon
- **本项目GitHub**: https://github.com/Albertfa2021/VA-Compile

## 📧 联系

如有问题或建议，请通过以下方式联系：

- GitHub Issues: https://github.com/Albertfa2021/VA-Compile/issues
- Email: (您的联系方式)

---

**祝编译顺利！** 🎉

如果这个项目帮到了您，请给个Star ⭐
