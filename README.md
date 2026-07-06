# SpectrumFall — 音频处理与节奏游戏系统


基于 Qt 6 / C++17 的桌面节奏游戏，集音频解码、手写 FFT 频谱分析、多种实时可视化、节拍检测与下落式节奏游戏于一体。所有 FFT 与节拍检测算法均手写实现，不依赖第三方库。

---

## 功能概览

### 音频引擎
- 支持 WAV / MP3 / FLAC 格式加载与播放（QMediaPlayer + QAudioDecoder）
- 离线 PCM 解码供 FFT 与节拍检测使用
- 播放控制：播放/暂停、进度跳转、音量调节、倍速切换

### 频谱分析（手写 FFT）
- Cooley-Tukey 原地算法 + 预计算旋转因子表
- 窗口大小可配：512 / 1024 / 2048
- Hann 窗加窗 + 实时频域分解

### 四种可视化模式
- **频谱柱状图**（BarSpectrumVisualizer）
- **圆形频谱环**（CircularSpectrumVisualizer）
- **波形示波器**（WaveformVisualizer）
- **瀑布图**（WaterfallVisualizer）
- **OpenGL 3D 频谱**（GLSpectrumWidget，GLSL 着色器）

运行时可自由切换，实时响应音频数据。

### 节奏游戏
- 基于 Spectral Flux 的节拍检测 + 自适应阈值
- BPM 自动估计（直方图过滤 + 倍频校正）
- 自动生成下落式谱面：4 键（D/F/J/K）或 6 键（S/D/F/J/K/L）
- TAP 点击音符 + HOLD 长按音符（自动合并算法）
- Perfect / Good / Miss 三级判定（±60ms / ±120ms）
- 连击计数、实时分数、评级系统（φ/SSS/SS/S/A/B/C/D）
- 生存模式：血量条 + Miss 扣血 + 连击里程碑回血
- 密度归一化：根据歌曲时长自动调节音符密度（~100 音符/分钟）

### 谱面编辑器
- 可视化时间轴：波形 + 6 轨道色带 + 音符标记
- 表格编辑：SpinBox 限制输入，ComboBox 切换音符类型
- 实时录入：键盘 S/D/F/J/K/L 直接录入音符
- 时间轴交互：拖拽创建 HOLD、短点击创建 TAP

### 排行榜
- 4K / 6K 分榜，普通模式 / 生存模式分离
- 每首歌曲按分数降序保留前 50 条
- 卡片流布局，全屏查看对话框
- JSON 导入/导出（支持去重合并）

### 主题系统
- 5 种预设主题：激昂、欢快、舒缓、低沉、通用
- 双轨道调色板：菜单面板（QSS）+ 游戏绘制（运行时色值）
- 自定义调色器：9 色块实时预览
- JSON 导入/导出自定义预设
- 情绪自动分类：根据 BPM + 低频能量 + 整体能量匹配主题

### 音频特效
- 回声（Echo）：可调延迟、反馈、混合比
- 滤波器（Filter）：低通/高通，可调截止频率与共振
- 变调不变速（Pitch Shift）：半音调节

### 其他
- 全局按钮动效：悬停辉光 + 点击音效
- Qt 内置对话框中文翻译
- 异步缓存加载（QtConcurrent）
- 完整 QSS 深色主题

---

## 项目结构

```
SpectrumFall/
├── main.cpp                    # 应用入口
├── game.h / game.cpp           # MainWindow：页面导航 + 信号槽路由
├── game.vcxproj                # Visual Studio 项目文件（Qt VS Tools）
├── game.qrc                    # Qt 资源文件（QSS + SVG + click.wav 全部内嵌）
├── game.slnx                   # VS 解决方案
├── include/                    # 头文件（30 个）
│   ├── AudioEngine.h           # 音频引擎
│   ├── AudioEffectProcessor.h  # 音频特效处理器
│   ├── AudioEffectWidget.h     # 音频特效界面
│   ├── FFTAnalyzer.h           # FFT 频谱分析
│   ├── BeatDetector.h          # 节拍检测 + BPM 估计
│   ├── NoteGenerator.h         # 谱面生成（HOLD合并/密度归一化）
│   ├── ScoreManager.h          # 分数/判定/连击管理
│   ├── CacheManager.h          # 歌曲分析缓存
│   ├── LeaderboardManager.h    # 排行榜数据管理
│   ├── ThemeManager.h          # 主题调色板管理
│   ├── ChartManager.h          # 谱面文件管理（JSON v2）
│   ├── SpectrumProcessor.h     # 频谱数据处理
│   ├── GameWidget.h            # 游戏界面（判定/渲染/HUD）
│   ├── SongSelectWidget.h      # 歌曲选择界面
│   ├── VisualizationWidget.h   # 可视化模式界面
│   ├── ResultWidget.h          # 结算界面
│   ├── MainMenuWidget.h        # 主菜单界面
│   ├── LeaderboardWidget.h     # 排行榜界面
│   ├── ChartEditorWidget.h     # 谱面编辑器
│   ├── ChartTimelineWidget.h   # 谱面编辑时间轴
│   ├── ThemeEditorWidget.h     # 主题编辑器
│   ├── BarSpectrumVisualizer.h     # 柱状频谱
│   ├── CircularSpectrumVisualizer.h # 圆形频谱
│   ├── WaveformVisualizer.h        # 波形示波器
│   ├── WaterfallVisualizer.h       # 瀑布图
│   ├── GLSpectrumWidget.h          # OpenGL 3D 频谱
│   ├── VisualizerBase.h            # 可视化基类
│   ├── UIButtonEffects.h           # 按钮动效
│   └── dr_mp3.h / dr_flac.h       # 音频解码辅助
├── src/                        # 源文件（27 个，与 include/ 对应）
├── icons/                      # SVG 图标（13 个，Lucide 风格）
└── resources/
    └── style.qss               # 全局 QSS 样式表
```

---

## 技术架构

### 分层 MVC

```
+---------------------------------------------+
|  View Layer (Widgets)                        |
|  MainMenu / SongSelect / Visualization /     |
|  Game / Result / Leaderboard / ChartEditor / |
|  ThemeEditor                                 |
+---------------------------------------------+
|  Controller Layer (MainWindow)               |
|  页面导航 + 信号槽路由 + 全局状态协调         |
+---------------------------------------------+
|  Model / Service Layer                       |
|  AudioEngine / FFTAnalyzer / BeatDetector /  |
|  NoteGenerator / ScoreManager / CacheManager |
|  LeaderboardManager / ThemeManager /         |
|  ChartManager / AudioEffectProcessor         |
+---------------------------------------------+
```

### 数据流
- **可视化**：AudioEngine (PCM) → FFTAnalyzer (频谱) → Visualizer (渲染)
- **游戏**：AudioEngine (PCM) → BeatDetector (节拍) → NoteGenerator (谱面) → GameWidget (判定 + 渲染)

### 核心算法
- **FFT**：Cooley-Tukey 原地蝶形运算，预计算旋转因子，Hann 窗
- **节拍检测**：Spectral Flux + 自适应阈值（移动均值 + 标准差偏移）
- **BPM 估计**：峰值间隔直方图 + 倍频校正
- **HOLD 合并**：多步算法（间隙阈值 + 中位数判断 + 同轨窗口清理）
- **密度归一化**：基于歌曲时长计算目标音符数，均匀剔除多余 TAP

---

## 环境要求

| 依赖 | 版本 |
|------|------|
| Qt | 6.11.1 (MSVC 2022 64-bit) |
| Visual Studio | 2022 (v143 工具集) |
| Qt VS Tools | 3.04+ |
| Windows SDK | 10.0+ |

Qt 模块依赖：`core` `gui` `widgets` `multimedia` `multimediawidgets` `concurrent` `opengl` `openglwidgets` `svg`

---

## 编译与运行

### 方式一：Visual Studio IDE（推荐）

1. 安装 [Qt 6.11.1](https://www.qt.io/download) 和 [Qt VS Tools](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools) 扩展
2. 确保 Qt VS Tools 中已配置 Qt 6.11.1 MSVC 2022 64-bit 套件
3. 双击 `game.slnx` 打开解决方案
4. 选择 **x64 / Release** 配置
5. 按 `Ctrl+F5` 编译并运行

### 方式二：MSBuild 命令行

```bat
:: 设置 Qt MSBuild 路径（根据实际安装位置调整）
set QtMsBuild=C:\Users\<用户名>\AppData\Local\QtMsBuild

:: Debug 编译
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ^
    game.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal

:: Release 编译
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ^
    game.vcxproj /p:Configuration=Release /p:Platform=x64 /v:minimal
```

编译输出位于 `x64\Debug\game.exe` 或 `x64\Release\game.exe`。

### 打包可执行程序（windeployqt）

```bat
:: 1. 先编译 Release 版本（见上）

:: 2. 创建发布目录
mkdir SpectrumFall_Release

:: 3. 复制可执行文件
copy x64\Release\game.exe SpectrumFall_Release\

:: 4. 运行 windeployqt 自动复制所有 Qt 依赖
windeployqt --release SpectrumFall_Release\game.exe

:: 5. 打包为 zip
powershell Compress-Archive -Path SpectrumFall_Release\* -DestinationPath SpectrumFall_Release.zip
```

打包后的 `SpectrumFall_Release/` 目录可直接在任意 Windows 64-bit 机器上运行，无需安装 Qt。

---

## 操作说明

### 游戏按键
| 模式 | 按键 |
|------|------|
| 4 键 | D / F / J / K |
| 6 键 | S / D / F / J / K / L |
| 暂停 | Esc |

### 判定窗口
| 判定 | 误差范围 | 得分 |
|------|---------|------|
| Perfect | ± 60ms | 100% |
| Good | ± 120ms | 50% |
| Miss | 超出范围 | 0% |

### 评级标准
| 评级 | 条件 |
|------|------|
| φ | 全 Perfect（AP） |
| SSS | ≥ 96% |
| SS | ≥ 88% |
| S | ≥ 78% |
| A | ≥ 65% |
| B | ≥ 50% |
| C | ≥ 25% |
| D | < 25% |

---

## 数据存储

应用数据保存在 `%LOCALAPPDATA%\game\` 目录下（即 `C:\Users\<用户名>\AppData\Local\game\`）：

- `cache.json` — 歌曲分析缓存（BPM、时长、音符数据、频谱特征）
- `leaderboard.json` — 排行榜数据
- `custom_themes.json` — 自定义主题预设
- `charts/` — 用户编辑的谱面文件（JSON v2 格式）
