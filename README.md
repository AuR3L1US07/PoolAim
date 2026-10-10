# PoolAim 台球瞄准辅助

Windows x86 Poolians / Real Pool 3D 的瞄准辅助。运行根目录的 `PoolAim.exe`，进入游戏练习模式后点击“加载辅助”。单文件启动器内置插件、加载器、配置和 MinHook 许可证。

插件读取球坐标与相机矩阵，绘制精简的碰撞、反弹和传球预测路线。绿色高亮表示几何预测可进球；橙色表示极薄碰撞对误差敏感。预测不包含完整的力度、旋转和摩擦，不能保证进球。

| 按键 | 功能 |
|---|---|
| Insert | 显示／隐藏 |
| F5 | 重新识别球 |
| F6 | 切换反弹次数 |
| F7 | 切换白球编号 |
| F8 | 显示球编号 |
| F9 / F10 | 切换参考目标球／袋口 |
| F11 | 显示参考线 |
| + / - | 调整线宽 |

更新或移除已加载的插件，需要完全退出游戏。当前版本不包含自动开球和回合判断。

## 构建

在项目根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File plugin/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File plugin/package/build-single.ps1
```

第一步编译 DLL、加载器并运行几何测试；第二步生成根目录 `PoolAim.exe`。构建使用 `plugin/vendor/toolchain` 中的 x86 工具链；MinHook 源码及许可证保留在 `plugin/vendor/minhook`。桌面尺寸配置为 `plugin/PoolAim.ini`。

源码：`plugin/src`；测试：`plugin/tests`。`plugin/build` 和 `plugin/dist` 是可重新生成的构建目录。

## 界面与效果

### 中文启动界面

![PoolAim 中文启动界面](images/launcher.png)

### 游戏内路线与进球预测高亮

![游戏内瞄准路线和绿色预测高亮](images/aim-preview.png)

绿色表示当前几何模型预测可进，实际结果仍受力度、旋转和模型误差影响。

### 中文状态面板

![中文状态面板与快捷键](images/chinese-panel.png)

## 下载与使用范围

在 [Releases](https://github.com/AuR3L1US07/PoolAim/releases) 下载 `PoolAim.exe`。无需安装工具链；更新前完全退出游戏。

建议用于练习和几何学习。程序使用 DLL 注入与绘图挂接，只针对 Poolians.exe；其他游戏的反作弊兼容性未验证，请勿与带反作弊的游戏同时运行。

源码仓库不包含大型编译工具链。需要自行构建时，请从官方 [w64devkit](https://github.com/skeeto/w64devkit/releases/tag/v2.10.0) 获取 2.10.0 的 x86 包，将其中 w64devkit 文件夹放在 `plugin/vendor/toolchain/` 下，或通过 build.ps1 的 CompilerBin 参数指定编译器目录。
