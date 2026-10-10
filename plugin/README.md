# 🛠️ PoolAim 插件开发

[⬅ 返回项目首页](../README.md) · [⬇ 下载最新版](https://github.com/AuR3L1US07/PoolAim/releases/latest)

此目录包含插件源码、几何测试与单文件打包脚本。使用方法、效果截图与快捷键见项目首页。

## 🧩 源码导航

| 文件 | 职责 |
| :--- | :--- |
| [`src/plugin.cpp`](src/plugin.cpp) | 球对象读取、相机捕获与绘图挂接 |
| [`src/geometry.hpp`](src/geometry.hpp) | 双精度几何碰撞与路线计算 |
| [`src/identity.hpp`](src/identity.hpp) | 球实体名称与编号解析 |
| [`src/panel.hpp`](src/panel.hpp) | 缓存的中文面板绘制 |
| [`src/injector.cpp`](src/injector.cpp) | 32 位 DLL 加载器 |
| [`src/launcher.cpp`](src/launcher.cpp) | 单文件中文启动界面 |
| [`tests/geometry_test.cpp`](tests/geometry_test.cpp) | 薄切、相切、反弹与进袋测试 |
| [`PoolAim.ini`](PoolAim.ini) | 桌面尺寸配置 |

## 🔨 构建流程

在项目根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File plugin/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File plugin/package/build-single.ps1
```

先编译 DLL、加载器并运行测试，再生成 `PoolAim.exe`。工具链准备方式见 [首页构建说明](../README.md#build)。

## 🔍 运行说明

插件通过 CBall RTTI 与 Ogre getter 读取场景对象，支持 OpenGL 与 DirectX 9。对象布局依赖游戏版本。

初始化和识别诊断写入加载目录中的 `PoolAim.log`。插件驻留至游戏退出；更新前需完全退出游戏。

MinHook 许可证见 [`vendor/minhook/LICENSE.txt`](vendor/minhook/LICENSE.txt)。

