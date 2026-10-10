# 插件开发

使用方法、快捷键和构建命令见根目录 README.md。

- src/plugin.cpp：球对象读取、相机捕获与绘图挂接。
- src/geometry.hpp：双精度几何碰撞和路线计算。
- src/identity.hpp：球实体名称与编号解析。
- src/panel.hpp：缓存的中文面板绘制。
- src/injector.cpp：32 位 DLL 加载器。
- src/launcher.cpp：单文件中文启动界面。
- tests/geometry_test.cpp：薄切、相切、反弹及进袋几何测试。
- PoolAim.ini：桌面尺寸配置。

插件通过 CBall RTTI 与 Ogre getter 读取场景对象；对象布局依赖游戏版本。支持 OpenGL 与 DirectX 9。初始化与识别错误写入加载目录中的 PoolAim.log。插件驻留至游戏退出。
