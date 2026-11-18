# ImgViewPro

ImgViewPro 是一个基于 **Qt 6** 和 **OpenCV** 的 Windows 多图片查看器，支持常见图片格式、CSV 数据图、高精度缩放、像素值查看以及多窗口联动浏览。

## 主要功能

- 同时打开多张图片，左侧 Dock 显示图片列表。
- 支持一次选择多个文件，或将多个文件直接拖入窗口。
- 支持滚轮缩放、鼠标拖动平移、适应窗口、1:1 原始大小。
- 高倍放大时自动显示真实像素块，并可标注每个像素的 RGB / 单通道数值。
- 支持“视图联动 (像素精确)”：多个图片以相同像素密度缩放、按相同图片像素数平移。
- 使用 OpenCV 解码部分 Qt 处理不理想的格式，例如压缩 TIFF、高位深或浮点图像。
- 可将逗号分隔的 CSV 当作图片显示；非数字单元按 `0` 处理。
- 点击底部状态栏的坐标标签，输入 X/Y 整数坐标，可将图片放大到最大并把该像素居中显示。
- 支持设置 / 取消图片文件关联，并支持单实例打开：从资源管理器双击图片时会在已运行的窗口中打开。

## 技术栈

- C++20
- Qt 6（Widgets / Gui / Core / Network）
- OpenCV 4（core / imgcodecs / imgproc）
- CMake + Ninja
- vcpkg manifest 模式

## 构建

项目使用 CMake Presets。当前 `CMakePresets.json` 中的 Qt 与 vcpkg 路径是本机路径，换机器时请按实际情况修改：

```json
"CMAKE_TOOLCHAIN_FILE": "E:/tool/vcpkg/scripts/buildsystems/vcpkg.cmake",
"VCPKG_CHAINLOAD_TOOLCHAIN_FILE": "E:/qt/6.9.3/msvc2022_64/lib/cmake/Qt6/qt.toolchain.cmake",
"VCPKG_TARGET_TRIPLET": "x64-windows"
```

在项目根目录执行：

```powershell
cmake --preset default --fresh
cmake --build --preset default
```

Debug 构建：

```powershell
cmake --preset debug --fresh
cmake --build --preset debug
```

vcpkg 依赖在 `vcpkg.json` 中声明，首次配置时会自动安装 OpenCV 及 JPEG / PNG / TIFF / WebP 支持。

发布打包时，将 `ImgViewPro.exe` 与 Qt、OpenCV 的运行库放到同一目录即可。可使用 `windeployqt` 部署 Qt 依赖，并额外复制 OpenCV DLL。

## CSV 图片格式

CSV 文件按行和列描述一张单通道图片：

- 每行对应图片的一行像素。
- 每个单元对应一个像素值，使用 `,` 分隔。
- 行数 = 图片高度，列数 = 图片宽度。
- 非数字或解析失败的值会被当作 `0`。

示例：

```csv
1,2,3
4,5,6
```

## 目录结构

```text
ImgViewPro/
├── src/                  # C++ 源码
├── .vscode/              # VS Code 配置
├── .clang-format         # 代码格式配置
├── .clang-tidy           # 静态检查配置
├── .clangd               # clangd 配置
├── CMakeLists.txt        # 构建脚本
├── CMakePresets.json     # CMake Presets
├── vcpkg.json            # vcpkg 依赖清单
├── build/                # 构建输出（已忽略）
├── release/              # 发布输出（已忽略）
├── vcpkg_installed/      # vcpkg 安装目录（已忽略）
└── res/                  # 测试用图片 / CSV（已忽略）
```
