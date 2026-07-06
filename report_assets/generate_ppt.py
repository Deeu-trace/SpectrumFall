"""
SpectrumFall 课程设计答辩 PPT 生成器
- 风格：科技深色风
- 页数：17 页
- 重点：创新功能（生存模式、主题系统、谱面密度归一化、难度自动评级）
"""
import sys
sys.path.insert(0, r"E:\Lib\site-packages")

from pptx import Presentation
from pptx.util import Inches, Pt, Emu, Cm
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.oxml.ns import qn
from lxml import etree
import os

# ==================== 配色方案（科技深色风） ====================
class Color:
    BG_DARK = RGBColor(0x0A, 0x16, 0x28)       # 深蓝黑 主背景
    BG_MID = RGBColor(0x16, 0x21, 0x3E)        # 稍亮深蓝 卡片背景
    BG_LIGHT = RGBColor(0x1F, 0x2D, 0x4D)      # 更亮的深蓝 强调卡片
    CYAN = RGBColor(0x00, 0xD9, 0xFF)          # 青色 主色
    PURPLE = RGBColor(0x9D, 0x4E, 0xDD)        # 紫色 强调
    PINK = RGBColor(0xFF, 0x00, 0x6E)          # 粉色 警示
    GREEN = RGBColor(0x06, 0xFF, 0xA5)         # 绿色 成功
    ORANGE = RGBColor(0xFF, 0xB7, 0x03)        # 橙色 警告
    WHITE = RGBColor(0xFF, 0xFF, 0xFF)         # 白色 主文字
    GRAY = RGBColor(0xA8, 0xB2, 0xD1)          # 浅灰 次文字
    GRAY_DARK = RGBColor(0x64, 0x6E, 0x8C)     # 深灰 弱文字

# ==================== 创建演示文稿 ====================
prs = Presentation()
prs.slide_width = Inches(13.333)   # 16:9
prs.slide_height = Inches(7.5)

SW = prs.slide_width
SH = prs.slide_height

# ==================== 辅助函数 ====================
def add_bg(slide, color=Color.BG_DARK):
    """填充背景色"""
    bg = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, SW, SH)
    bg.fill.solid()
    bg.fill.fore_color.rgb = color
    bg.line.fill.background()
    bg.shadow.inherit = False
    # Move to back
    spTree = bg._element.getparent()
    spTree.remove(bg._element)
    spTree.insert(2, bg._element)
    return bg

def add_text(slide, left, top, width, height, text, size=18, color=Color.WHITE,
             bold=False, align=PP_ALIGN.LEFT, anchor=MSO_ANCHOR.TOP, font='微软雅黑'):
    """添加文本框"""
    tb = slide.shapes.add_textbox(left, top, width, height)
    tf = tb.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = anchor
    # Set margins to 0
    tf.margin_left = Emu(0)
    tf.margin_right = Emu(0)
    tf.margin_top = Emu(0)
    tf.margin_bottom = Emu(0)
    p = tf.paragraphs[0]
    p.alignment = align
    run = p.add_run()
    run.text = text
    run.font.size = Pt(size)
    run.font.color.rgb = color
    run.font.bold = bold
    run.font.name = font
    # Set East Asian font
    rPr = run._r.get_or_add_rPr()
    eaFont = rPr.find(qn('a:ea'))
    if eaFont is None:
        eaFont = etree.SubElement(rPr, qn('a:ea'))
    eaFont.set('typeface', font)
    return tb

def add_multiline(slide, left, top, width, height, lines, size=16, color=Color.WHITE,
                  align=PP_ALIGN.LEFT, font='微软雅黑', line_spacing=1.2):
    """添加多行文本（每行可独立设置颜色/粗细）lines=[(text, color, bold), ...]"""
    tb = slide.shapes.add_textbox(left, top, width, height)
    tf = tb.text_frame
    tf.word_wrap = True
    tf.margin_left = Emu(0)
    tf.margin_right = Emu(0)
    tf.margin_top = Emu(0)
    tf.margin_bottom = Emu(0)
    for i, line in enumerate(lines):
        if isinstance(line, tuple):
            text, c, bold = line
        else:
            text, c, bold = line, color, False
        if i == 0:
            p = tf.paragraphs[0]
        else:
            p = tf.add_paragraph()
        p.alignment = align
        p.line_spacing = line_spacing
        run = p.add_run()
        run.text = text
        run.font.size = Pt(size)
        run.font.color.rgb = c
        run.font.bold = bold
        run.font.name = font
        rPr = run._r.get_or_add_rPr()
        eaFont = rPr.find(qn('a:ea'))
        if eaFont is None:
            eaFont = etree.SubElement(rPr, qn('a:ea'))
        eaFont.set('typeface', font)
    return tb

def add_rect(slide, left, top, width, height, fill_color, line_color=None, line_width=0):
    """添加矩形"""
    sh = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, left, top, width, height)
    sh.fill.solid()
    sh.fill.fore_color.rgb = fill_color
    if line_color is None:
        sh.line.fill.background()
    else:
        sh.line.color.rgb = line_color
        sh.line.width = Pt(line_width)
    sh.shadow.inherit = False
    return sh

def add_round_rect(slide, left, top, width, height, fill_color, line_color=None, line_width=0):
    """添加圆角矩形"""
    sh = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, left, top, width, height)
    sh.fill.solid()
    sh.fill.fore_color.rgb = fill_color
    if line_color is None:
        sh.line.fill.background()
    else:
        sh.line.color.rgb = line_color
        sh.line.width = Pt(line_width)
    sh.shadow.inherit = False
    return sh

def add_title_bar(slide, title, subtitle="", page_num=None, total=17):
    """添加页面标题栏（顶部装饰条 + 标题）"""
    # 顶部装饰条
    add_rect(slide, 0, 0, SW, Inches(0.08), Color.CYAN)
    # 左侧竖条
    add_rect(slide, Inches(0.5), Inches(0.4), Inches(0.08), Inches(0.6), Color.CYAN)
    # 标题
    add_text(slide, Inches(0.7), Inches(0.35), Inches(10), Inches(0.7), title,
             size=28, color=Color.WHITE, bold=True, anchor=MSO_ANCHOR.MIDDLE)
    # 副标题
    if subtitle:
        add_text(slide, Inches(0.7), Inches(0.95), Inches(10), Inches(0.35), subtitle,
                 size=14, color=Color.GRAY)
    # 页码
    if page_num is not None:
        add_text(slide, Inches(12.0), Inches(7.0), Inches(1.2), Inches(0.4),
                 f"{page_num} / {total}", size=11, color=Color.GRAY_DARK, align=PP_ALIGN.RIGHT)

def add_section_title(slide, left, top, width, text, color=Color.CYAN, size=18):
    """添加小节标题（带色块前缀）"""
    add_rect(slide, left, top + Inches(0.05), Inches(0.15), Inches(0.3), color)
    add_text(slide, left + Inches(0.25), top, width, Inches(0.4), text,
             size=size, color=Color.WHITE, bold=True, anchor=MSO_ANCHOR.MIDDLE)

def add_card(slide, left, top, width, height, title, content_lines, accent_color=Color.CYAN):
    """添加卡片样式（标题 + 内容列表）"""
    # 卡片背景
    add_round_rect(slide, left, top, width, height, Color.BG_MID)
    # 顶部色条
    add_rect(slide, left, top, width, Inches(0.08), accent_color)
    # 标题
    add_text(slide, left + Inches(0.3), top + Inches(0.2), width - Inches(0.6), Inches(0.4),
             title, size=16, color=accent_color, bold=True)
    # 内容
    y = top + Inches(0.75)
    for line in content_lines:
        if isinstance(line, tuple):
            text, c = line
        else:
            text, c = line, Color.GRAY
        add_text(slide, left + Inches(0.3), y, width - Inches(0.6), Inches(0.35),
                 text, size=12, color=c)
        y += Inches(0.35)

# ==================== 通用空白页 ====================
def new_slide():
    """新建空白页并填充背景"""
    slide = prs.slides.add_slide(prs.slide_layouts[6])  # blank
    add_bg(slide)
    return slide

# ==================== 第 1 页：封面 ====================
slide = new_slide()
# 装饰：顶部渐变条
add_rect(slide, 0, 0, SW, Inches(0.15), Color.CYAN)
add_rect(slide, 0, Inches(0.15), SW, Inches(0.05), Color.PURPLE)
# 装饰：右上角几何图形
add_rect(slide, Inches(11.5), Inches(0.3), Inches(1.5), Inches(1.5), Color.BG_MID)
add_rect(slide, Inches(12.0), Inches(0.6), Inches(1.0), Inches(1.0), Color.BG_LIGHT)
# 装饰：左下角几何图形
add_rect(slide, Inches(-0.3), Inches(6.0), Inches(2.0), Inches(2.0), Color.BG_MID)

# 主标题
add_text(slide, Inches(1.0), Inches(2.0), Inches(11), Inches(1.2),
         "SpectrumFall", size=72, color=Color.CYAN, bold=True, align=PP_ALIGN.CENTER)
# 副标题
add_text(slide, Inches(1.0), Inches(3.3), Inches(11), Inches(0.6),
         "实时音频频谱可视化与音乐节奏游戏引擎", size=28, color=Color.WHITE, align=PP_ALIGN.CENTER)
# 分隔线
add_rect(slide, Inches(5.0), Inches(4.1), Inches(3.333), Inches(0.04), Color.CYAN)
# 课程信息
add_text(slide, Inches(1.0), Inches(4.4), Inches(11), Inches(0.5),
         "《C++程序设计》课程设计答辩", size=20, color=Color.GRAY, align=PP_ALIGN.CENTER)
# 作者信息
add_multiline(slide, Inches(1.0), Inches(5.2), Inches(11), Inches(1.5), [
    ("学院：计算机科学与工程学院    专业：计算机科学与技术", Color.GRAY, False),
    ("学号：20254875    姓名：范中海", Color.WHITE, True),
    ("指导教师：（待填写）    完成日期：2026 年 7 月", Color.GRAY, False),
], size=16, align=PP_ALIGN.CENTER, line_spacing=1.5)

# ==================== 第 2 页：目录 ====================
slide = new_slide()
add_title_bar(slide, "目  录", "CONTENTS", page_num=2)

toc_items = [
    ("01", "项目背景与意义", "课题来源、知识综合、工程挑战", Color.CYAN),
    ("02", "需求分析", "基础功能 / 扩展功能 / 创新功能", Color.PURPLE),
    ("03", "系统设计", "分层 MVC 架构 / 模块划分 / UML", Color.CYAN),
    ("04", "核心算法", "手写 FFT / 节拍检测", Color.PURPLE),
    ("05", "创新功能亮点", "★ 4 项核心创新详述", Color.PINK),
    ("06", "运行演示与总结", "测试报告 / 个人总结", Color.GREEN),
]

y = Inches(1.6)
for num, title, desc, color in toc_items:
    # 编号色块
    add_round_rect(slide, Inches(1.0), y, Inches(0.9), Inches(0.7), color)
    add_text(slide, Inches(1.0), y, Inches(0.9), Inches(0.7), num,
             size=24, color=Color.BG_DARK, bold=True,
             align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)
    # 标题
    add_text(slide, Inches(2.1), y + Inches(0.05), Inches(5), Inches(0.4), title,
             size=20, color=Color.WHITE, bold=True)
    # 描述
    add_text(slide, Inches(2.1), y + Inches(0.4), Inches(8), Inches(0.3), desc,
             size=12, color=Color.GRAY)
    y += Inches(0.85)

# ==================== 第 3 页：项目背景 ====================
slide = new_slide()
add_title_bar(slide, "01  项目背景与意义", "课题来源与意义", page_num=3)

# 左侧：课题来源
add_section_title(slide, Inches(0.5), Inches(1.4), Inches(6), "课题来源")
add_multiline(slide, Inches(0.5), Inches(2.0), Inches(6), Inches(4), [
    ("• 音频信号处理与可视化已渗透到日常生活", Color.WHITE, False),
    ("  - 网易云音乐 / QQ 音乐的频谱动画", Color.GRAY, False),
    ("  - osu! / 节奏大师 / Cytus 等节奏游戏", Color.GRAY, False),
    ("", Color.WHITE, False),
    ("• 课题要求实现完整的音频处理与节奏游戏系统", Color.WHITE, False),
    ("  - 读取音频文件 → 实时 FFT 频谱分析", Color.GRAY, False),
    ("  - 多种可视化效果呈现", Color.GRAY, False),
    ("  - 节拍检测 → 自动生成谱面 → 节奏游戏", Color.GRAY, False),
], size=14, line_spacing=1.3)

# 右侧：课题意义（4 张小卡片）
add_section_title(slide, Inches(7.0), Inches(1.4), Inches(6), "课题意义")
cards = [
    ("知识综合", "C++ OOP / FFT / DSP / Qt / OpenGL", Color.CYAN),
    ("工程挑战", "手写 FFT 实时性能 + 节拍检测鲁棒性", Color.PURPLE),
    ("实用价值", "音频解码/FFT/节拍检测可迁移至真实产品", Color.GREEN),
    ("创新空间", "谱面编辑器/特效/着色器/主题系统", Color.ORANGE),
]
y = Inches(2.0)
for title, desc, c in cards:
    add_round_rect(slide, Inches(7.0), y, Inches(6), Inches(0.95), Color.BG_MID)
    add_rect(slide, Inches(7.0), y, Inches(0.1), Inches(0.95), c)
    add_text(slide, Inches(7.3), y + Inches(0.1), Inches(2), Inches(0.4), title,
             size=15, color=c, bold=True)
    add_text(slide, Inches(7.3), y + Inches(0.5), Inches(5.5), Inches(0.35), desc,
             size=11, color=Color.GRAY)
    y += Inches(1.05)

# ==================== 第 4 页：需求分析 ====================
slide = new_slide()
add_title_bar(slide, "02  需求分析", "功能需求总览", page_num=4)

# 三大功能类别卡片
add_section_title(slide, Inches(0.5), Inches(1.3), Inches(12), "功能分类")

# 基础功能（必做）
add_round_rect(slide, Inches(0.5), Inches(1.9), Inches(4.0), Inches(4.8), Color.BG_MID)
add_rect(slide, Inches(0.5), Inches(1.9), Inches(4.0), Inches(0.6), Color.CYAN)
add_text(slide, Inches(0.5), Inches(1.9), Inches(4.0), Inches(0.6),
         "基础功能（必做 6 项）", size=16, color=Color.BG_DARK, bold=True,
         align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)
add_multiline(slide, Inches(0.7), Inches(2.7), Inches(3.6), Inches(4), [
    ("F-01 音频解码与播放", Color.WHITE, True),
    ("  MP3/WAV/FLAC 解码", Color.GRAY, False),
    ("F-02 FFT 频谱分析", Color.WHITE, True),
    ("  手写 Cooley-Tukey FFT", Color.GRAY, False),
    ("F-03 多种可视化效果", Color.WHITE, True),
    ("  柱状/圆形/波形/瀑布", Color.GRAY, False),
    ("F-04 节拍检测", Color.WHITE, True),
    ("  Spectral Flux 算法", Color.GRAY, False),
    ("F-05 节奏游戏", Color.WHITE, True),
    ("  4 键下落式 + 判定", Color.GRAY, False),
    ("F-06 用户界面", Color.WHITE, True),
    ("  5 大界面 + QSS 美化", Color.GRAY, False),
], size=11, line_spacing=1.2)

# 扩展功能（选做）
add_round_rect(slide, Inches(4.7), Inches(1.9), Inches(4.0), Inches(4.8), Color.BG_MID)
add_rect(slide, Inches(4.7), Inches(1.9), Inches(4.0), Inches(0.6), Color.PURPLE)
add_text(slide, Inches(4.7), Inches(1.9), Inches(4.0), Inches(0.6),
         "扩展功能（选做 5 项）", size=16, color=Color.WHITE, bold=True,
         align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)
add_multiline(slide, Inches(4.9), Inches(2.7), Inches(3.6), Inches(4), [
    ("E-01 谱面编辑器  ✓", Color.GREEN, True),
    ("  表格/时间轴/实时录入", Color.GRAY, False),
    ("E-02 更多游戏模式  ✓", Color.GREEN, True),
    ("  6 键模式 + HOLD 长按", Color.GRAY, False),
    ("E-03 音频特效  ✓", Color.GREEN, True),
    ("  回声/滤波/变调 DSP", Color.GRAY, False),
    ("E-04 着色器效果  ✓", Color.GREEN, True),
    ("  GLSL 霓虹可视化", Color.GRAY, False),
    ("E-05 在线排行榜  ✓", Color.GREEN, True),
    ("  本地存储 + 导入导出", Color.GRAY, False),
    ("", Color.WHITE, False),
    ("  全部 5 项已完成", Color.GREEN, True),
], size=11, line_spacing=1.2)

# 创新功能（自定）
add_round_rect(slide, Inches(8.9), Inches(1.9), Inches(4.0), Inches(4.8), Color.BG_MID)
add_rect(slide, Inches(8.9), Inches(1.9), Inches(4.0), Inches(0.6), Color.PINK)
add_text(slide, Inches(8.9), Inches(1.9), Inches(4.0), Inches(0.6),
         "★ 创新功能（8 项）", size=16, color=Color.WHITE, bold=True,
         align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)
add_multiline(slide, Inches(9.1), Inches(2.7), Inches(3.6), Inches(4), [
    ("I-01 生存模式  ⭐", Color.PINK, True),
    ("I-02 主题系统  ⭐", Color.PINK, True),
    ("I-03 缓存系统", Color.WHITE, True),
    ("I-04 谱面密度归一化  ⭐", Color.PINK, True),
    ("I-05 难度自动评级  ⭐", Color.PINK, True),
    ("I-06 双押与轨道配额", Color.WHITE, True),
    ("I-07 评级系统 φ~D", Color.WHITE, True),
    ("I-08 全局按钮动效", Color.WHITE, True),
    ("", Color.WHITE, False),
    ("  ⭐ = 重点创新亮点", Color.PINK, True),
    ("  后续单独详述", Color.GRAY, False),
], size=11, line_spacing=1.3)

# ==================== 第 5 页：系统架构 ====================
slide = new_slide()
add_title_bar(slide, "03  系统设计", "分层 MVC 架构", page_num=5)

# 用形状绘制三层架构
# View 层
add_round_rect(slide, Inches(0.8), Inches(1.5), Inches(11.7), Inches(1.4), Color.BG_MID)
add_rect(slide, Inches(0.8), Inches(1.5), Inches(0.15), Inches(1.4), Color.CYAN)
add_text(slide, Inches(1.1), Inches(1.6), Inches(2), Inches(0.4), "View 层",
         size=16, color=Color.CYAN, bold=True)
add_text(slide, Inches(1.1), Inches(1.95), Inches(2), Inches(0.3), "界面显示与交互",
         size=10, color=Color.GRAY)
# 8 个页面小方块
pages = ["主菜单", "歌曲选择", "可视化", "游戏", "结算", "排行榜", "谱面编辑", "主题编辑"]
for i, p in enumerate(pages):
    x = Inches(3.3 + i * 1.15)
    add_round_rect(slide, x, Inches(1.85), Inches(1.05), Inches(0.7), Color.BG_LIGHT)
    add_text(slide, x, Inches(1.85), Inches(1.05), Inches(0.7), p,
             size=11, color=Color.WHITE, align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)

# 箭头
add_text(slide, Inches(6.0), Inches(2.95), Inches(1.3), Inches(0.4), "▼ 信号槽路由 ▼",
         size=11, color=Color.GRAY, align=PP_ALIGN.CENTER)

# Controller 层
add_round_rect(slide, Inches(0.8), Inches(3.35), Inches(11.7), Inches(1.2), Color.BG_MID)
add_rect(slide, Inches(0.8), Inches(3.35), Inches(0.15), Inches(1.2), Color.PURPLE)
add_text(slide, Inches(1.1), Inches(3.45), Inches(3), Inches(0.4), "Controller 层",
         size=16, color=Color.PURPLE, bold=True)
add_text(slide, Inches(1.1), Inches(3.8), Inches(3), Inches(0.3), "页面导航 + 信号槽路由",
         size=10, color=Color.GRAY)
add_round_rect(slide, Inches(4.5), Inches(3.55), Inches(4.5), Inches(0.8), Color.BG_LIGHT)
add_text(slide, Inches(4.5), Inches(3.55), Inches(4.5), Inches(0.8),
         "MainWindow (game.h / game.cpp)", size=14, color=Color.WHITE, bold=True,
         align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)
add_text(slide, Inches(9.5), Inches(3.7), Inches(3), Inches(0.5),
         "持有 8 页面 + 8 管理器\n全局状态协调", size=10, color=Color.GRAY)

# 箭头
add_text(slide, Inches(6.0), Inches(4.6), Inches(1.3), Inches(0.4), "▼ 依赖注入 ▼",
         size=11, color=Color.GRAY, align=PP_ALIGN.CENTER)

# Model 层
add_round_rect(slide, Inches(0.8), Inches(5.0), Inches(11.7), Inches(2.0), Color.BG_MID)
add_rect(slide, Inches(0.8), Inches(5.0), Inches(0.15), Inches(2.0), Color.GREEN)
add_text(slide, Inches(1.1), Inches(5.1), Inches(3), Inches(0.4), "Model / Service 层",
         size=16, color=Color.GREEN, bold=True)
add_text(slide, Inches(1.1), Inches(5.45), Inches(3), Inches(0.3), "核心业务逻辑",
         size=10, color=Color.GRAY)

# 管理器网格
managers = [
    ("AudioEngine", "音频引擎", Color.CYAN),
    ("FFTAnalyzer", "频谱分析", Color.CYAN),
    ("BeatDetector", "节拍检测", Color.CYAN),
    ("NoteGenerator", "谱面生成", Color.CYAN),
    ("ScoreManager", "分数判定", Color.PURPLE),
    ("CacheManager", "缓存系统", Color.PURPLE),
    ("ChartManager", "谱面文件", Color.PURPLE),
    ("LeaderboardManager", "排行榜", Color.PURPLE),
    ("ThemeManager", "主题系统", Color.PINK),
    ("AudioEffectProcessor", "音频特效", Color.GREEN),
]
for i, (cls, desc, c) in enumerate(managers):
    row = i // 5
    col = i % 5
    x = Inches(4.0 + col * 1.7)
    y = Inches(5.2 + row * 0.85)
    add_round_rect(slide, x, y, Inches(1.6), Inches(0.75), Color.BG_LIGHT)
    add_rect(slide, x, y, Inches(1.6), Inches(0.08), c)
    add_text(slide, x, y + Inches(0.12), Inches(1.6), Inches(0.35), cls,
             size=10, color=Color.WHITE, bold=True, align=PP_ALIGN.CENTER)
    add_text(slide, x, y + Inches(0.45), Inches(1.6), Inches(0.3), desc,
             size=9, color=Color.GRAY, align=PP_ALIGN.CENTER)

# ==================== 第 6 页：技术栈 ====================
slide = new_slide()
add_title_bar(slide, "03  系统设计", "技术栈与开发工具", page_num=6)

# 左：技术栈
add_section_title(slide, Inches(0.5), Inches(1.3), Inches(6), "技术栈", color=Color.CYAN)
tech_items = [
    ("C++17", "核心业务逻辑", "std::optional / 结构化绑定 / if constexpr", Color.CYAN),
    ("Qt 6.11.1", "GUI 框架", "多媒体/OpenGL/并发/SVG 模块", Color.CYAN),
    ("OpenGL 3.3", "GLSL 着色器", "霓虹柱状图 / 圆形频谱 / 粒子背景", Color.PURPLE),
    ("Visual Studio 2022", "IDE + 编译器", "v143 工具集 / x64 /O2 优化", Color.GREEN),
    ("dr_mp3 / dr_flac", "音频解码库", "单文件 C 解码（公共域许可）", Color.ORANGE),
]
y = Inches(1.85)
for name, role, detail, c in tech_items:
    add_round_rect(slide, Inches(0.5), y, Inches(6.0), Inches(0.9), Color.BG_MID)
    add_rect(slide, Inches(0.5), y, Inches(0.1), Inches(0.9), c)
    add_text(slide, Inches(0.7), y + Inches(0.08), Inches(2.5), Inches(0.4), name,
             size=14, color=c, bold=True)
    add_text(slide, Inches(3.2), y + Inches(0.08), Inches(3), Inches(0.4), role,
             size=12, color=Color.WHITE)
    add_text(slide, Inches(0.7), y + Inches(0.5), Inches(5.5), Inches(0.35), detail,
             size=10, color=Color.GRAY)
    y += Inches(1.0)

# 右：AI 辅助工具
add_section_title(slide, Inches(7.0), Inches(1.3), Inches(6), "AI 辅助工具", color=Color.PINK)
add_round_rect(slide, Inches(7.0), Inches(1.85), Inches(6.0), Inches(2.2), Color.BG_MID)
add_rect(slide, Inches(7.0), Inches(1.85), Inches(6.0), Inches(0.08), Color.PINK)
add_multiline(slide, Inches(7.3), Inches(2.05), Inches(5.5), Inches(2), [
    ("智谱 GLM-5.2", Color.CYAN, True),
    ("  代码补全 / 算法实现 / 调试辅助 / 文档撰写", Color.GRAY, False),
    ("", Color.WHITE, False),
    ("通义千问 Qwen3.7 Max", Color.PURPLE, True),
    ("  代码生成 / 架构设计 / 单元测试 / 代码评审", Color.GRAY, False),
    ("", Color.WHITE, False),
    ("原则：AI 辅助 + 人工把关", Color.PINK, True),
    ("核心算法必须理解后才采用，最终质量由开发者负责", Color.GRAY, False),
], size=12, line_spacing=1.3)

# 右下：项目规模
add_section_title(slide, Inches(7.0), Inches(4.4), Inches(6), "项目规模", color=Color.GREEN)
stats = [
    ("30+", "类", Color.CYAN),
    ("12000+", "代码行", Color.PURPLE),
    ("8", "界面", Color.PINK),
    ("6", "可视化", Color.GREEN),
]
for i, (num, label, c) in enumerate(stats):
    x = Inches(7.0 + i * 1.5)
    add_round_rect(slide, x, Inches(5.0), Inches(1.4), Inches(1.3), Color.BG_MID)
    add_text(slide, x, Inches(5.1), Inches(1.4), Inches(0.7), num,
             size=32, color=c, bold=True, align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)
    add_text(slide, x, Inches(5.85), Inches(1.4), Inches(0.4), label,
             size=12, color=Color.GRAY, align=PP_ALIGN.CENTER)

# ==================== 第 7 页：核心算法 - FFT ====================
slide = new_slide()
add_title_bar(slide, "04  核心算法", "手写 FFT（Cooley-Tukey 算法）", page_num=7)

# 左侧：算法说明
add_section_title(slide, Inches(0.5), Inches(1.3), Inches(6), "算法要点")
add_multiline(slide, Inches(0.5), Inches(1.85), Inches(6), Inches(4), [
    ("• 任务书明确要求：自行实现 FFT，不允许调用库", Color.WHITE, True),
    ("", Color.WHITE, False),
    ("• 采用 Cooley-Tukey 蝶形运算（原地算法）", Color.WHITE, False),
    ("  时间复杂度：O(N log N)", Color.GRAY, False),
    ("  空间复杂度：O(N)（原地）", Color.GRAY, False),
    ("", Color.WHITE, False),
    ("• 关键优化", Color.WHITE, True),
    ("  1. 预计算旋转因子表（避免重复计算）", Color.GRAY, False),
    ("  2. 位反转排列（bit-reversal permutation）", Color.GRAY, False),
    ("  3. Hann 窗函数（减少频谱泄漏）", Color.GRAY, False),
    ("  4. 原地蝶形运算（节省内存）", Color.GRAY, False),
    ("", Color.WHITE, False),
    ("• 窗口大小可配：512 / 1024 / 2048", Color.WHITE, False),
    ("• 实测性能：60Hz 刷新下 < 16ms/帧", Color.GREEN, True),
], size=13, line_spacing=1.3)

# 右侧：数据流图
add_section_title(slide, Inches(7.0), Inches(1.3), Inches(6), "数据流", color=Color.PURPLE)
# 5 个流程方块
flow = [
    ("PCM 音频数据", Color.CYAN),
    ("Hann 窗加窗", Color.PURPLE),
    ("位反转排列", Color.PURPLE),
    ("蝶形运算", Color.PINK),
    ("频域幅度谱", Color.GREEN),
]
y = Inches(1.9)
for i, (text, c) in enumerate(flow):
    add_round_rect(slide, Inches(7.5), y, Inches(5.0), Inches(0.6), Color.BG_MID)
    add_rect(slide, Inches(7.5), y, Inches(0.1), Inches(0.6), c)
    add_text(slide, Inches(7.5), y, Inches(5.0), Inches(0.6), text,
             size=14, color=Color.WHITE, bold=True,
             align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)
    if i < len(flow) - 1:
        add_text(slide, Inches(7.5), y + Inches(0.6), Inches(5.0), Inches(0.3), "↓",
                 size=14, color=Color.GRAY, align=PP_ALIGN.CENTER)
    y += Inches(0.95)

# 底部：性能对比
add_round_rect(slide, Inches(7.0), Inches(6.3), Inches(6.0), Inches(0.8), Color.BG_LIGHT)
add_text(slide, Inches(7.2), Inches(6.35), Inches(5.6), Inches(0.7),
         "对比：朴素 DFT 为 O(N²)，N=1024 时约慢 100 倍",
         size=12, color=Color.ORANGE, anchor=MSO_ANCHOR.MIDDLE)

# ==================== 第 8 页：核心算法 - 节拍检测 ====================
slide = new_slide()
add_title_bar(slide, "04  核心算法", "节拍检测（Spectral Flux）", page_num=8)

# 三步流程
add_section_title(slide, Inches(0.5), Inches(1.3), Inches(12), "三步算法流程")
steps = [
    ("1", "Spectral Flux 计算", Color.CYAN,
     ["逐帧 FFT → 取幅度谱差分", "sum(|X[t] - X[t-1]|) 得到 SF 曲线", "正差分部分 = 能量增长点"]),
    ("2", "自适应阈值检测", Color.PURPLE,
     ["移动均值 + 标准差偏移", "threshold = mean + α × std", "超过阈值的峰值 = 候选节拍"]),
    ("3", "BPM 直方图估计", Color.PINK,
     ["统计峰值间隔直方图", "取众数作为 BPM 估计", "倍频校正（如 120 ↔ 60）"]),
]
for i, (num, title, c, details) in enumerate(steps):
    x = Inches(0.5 + i * 4.2)
    add_round_rect(slide, x, Inches(1.85), Inches(4.0), Inches(3.5), Color.BG_MID)
    add_rect(slide, x, Inches(1.85), Inches(4.0), Inches(0.7), c)
    # 编号圆
    add_oval = slide.shapes.add_shape(MSO_SHAPE.OVAL, x + Inches(0.2), Inches(1.95), Inches(0.5), Inches(0.5))
    add_oval.fill.solid()
    add_oval.fill.fore_color.rgb = Color.BG_DARK
    add_oval.line.color.rgb = c
    add_oval.line.width = Pt(2)
    add_oval.shadow.inherit = False
    add_text(slide, x + Inches(0.2), Inches(1.95), Inches(0.5), Inches(0.5), num,
             size=18, color=c, bold=True, align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)
    # 标题
    add_text(slide, x + Inches(0.8), Inches(1.95), Inches(3.0), Inches(0.5), title,
             size=14, color=Color.WHITE, bold=True, anchor=MSO_ANCHOR.MIDDLE)
    # 详情
    y = Inches(2.7)
    for d in details:
        add_text(slide, x + Inches(0.3), y, Inches(3.6), Inches(0.4), "• " + d,
                 size=11, color=Color.GRAY)
        y += Inches(0.5)

# 底部：创新轨道分配
add_section_title(slide, Inches(0.5), Inches(5.7), Inches(12), "★ 创新轨道分配策略", color=Color.PINK)
add_round_rect(slide, Inches(0.5), Inches(6.2), Inches(12.3), Inches(0.9), Color.BG_MID)
add_rect(slide, Inches(0.5), Inches(6.2), Inches(0.1), Inches(0.9), Color.PINK)
add_text(slide, Inches(0.7), Inches(6.25), Inches(12), Inches(0.85),
         "动态归一化 + 迟滞惩罚 + 饥饿配额 + 双押判定 → 避免连续同轨、保证轨道均衡、增加游戏变化性",
         size=13, color=Color.WHITE, anchor=MSO_ANCHOR.MIDDLE)

# ==================== 第 9-12 页：创新功能亮点 ====================
# 第 9 页：生存模式
slide = new_slide()
add_title_bar(slide, "05  创新功能亮点 (1/4)", "★ 生存模式（I-01）—— 游戏性深化", page_num=9)

# 左：设计思路
add_section_title(slide, Inches(0.5), Inches(1.3), Inches(6), "设计思路", color=Color.PINK)
add_multiline(slide, Inches(0.5), Inches(1.85), Inches(6), Inches(3), [
    ("传统节奏游戏缺乏失败惩罚", Color.WHITE, False),
    ("玩家压力感不足，刷分模式单一", Color.GRAY, False),
    ("", Color.WHITE, False),
    ("引入血量条机制", Color.PINK, True),
    ("将节奏游戏从「刷分」升级为「生存」", Color.WHITE, True),
    ("显著提升游戏紧张感与挑战性", Color.WHITE, False),
], size=14, line_spacing=1.4)

# 右：技术实现
add_section_title(slide, Inches(7.0), Inches(1.3), Inches(6), "技术实现", color=Color.CYAN)
add_multiline(slide, Inches(7.0), Inches(1.85), Inches(6), Inches(4), [
    ("• 血量初始值 = 100，最大值 = 100", Color.WHITE, False),
    ("• 每次 Miss 扣除 10 点血量", Color.WHITE, False),
    ("• 连击里程碑回血：", Color.WHITE, True),
    ("  50 连击 → 回血 5 点", Color.GREEN, False),
    ("  100 连击 → 回血 10 点", Color.GREEN, False),
    ("  200 连击 → 回血 15 点", Color.GREEN, False),
    ("• 血量归零 → 立即 Game Over", Color.PINK, True),
    ("  结算页显示「生存失败」", Color.GRAY, False),
    ("• 血条颜色动态变化：", Color.WHITE, True),
    ("  绿（>60%）→ 黄（30-60%）→ 红（<30%）", Color.ORANGE, False),
], size=13, line_spacing=1.3)

# 底部：效果卡片
add_round_rect(slide, Inches(0.5), Inches(5.5), Inches(12.3), Inches(1.4), Color.BG_LIGHT)
add_rect(slide, Inches(0.5), Inches(5.5), Inches(0.15), Inches(1.4), Color.PINK)
add_text(slide, Inches(0.8), Inches(5.6), Inches(3), Inches(0.4), "效果",
         size=16, color=Color.PINK, bold=True)
add_multiline(slide, Inches(0.8), Inches(6.0), Inches(12), Inches(0.9), [
    ("• 将「无脑刷分」变为「策略生存」，玩家需在风险与收益间权衡", Color.WHITE, False),
    ("• 特别适合硬核玩家，提升游戏重玩价值", Color.WHITE, False),
    ("• 血条颜色变化提供直观危险反馈，增强沉浸感", Color.WHITE, False),
], size=12, line_spacing=1.3)

# 第 10 页：主题系统
slide = new_slide()
add_title_bar(slide, "05  创新功能亮点 (2/4)", "★ 主题系统（I-02）—— 视觉个性化", page_num=10)

# 左：设计思路
add_section_title(slide, Inches(0.5), Inches(1.3), Inches(6), "设计思路", color=Color.PINK)
add_multiline(slide, Inches(0.5), Inches(1.85), Inches(6), Inches(3), [
    ("不同曲风的音频适合不同视觉风格", Color.WHITE, False),
    ("  电子乐 → 激昂炫酷", Color.GRAY, False),
    ("  古典乐 → 舒缓优雅", Color.GRAY, False),
    ("  摇滚乐 → 低沉暗黑", Color.GRAY, False),
    ("", Color.WHITE, False),
    ("实现「情绪自动分类 + 主题匹配」", Color.PINK, True),
    ("画面自动适配音乐氛围", Color.WHITE, True),
    ("无需用户手动调色", Color.GRAY, False),
], size=14, line_spacing=1.3)

# 右上：5 种预设主题色块
add_section_title(slide, Inches(7.0), Inches(1.3), Inches(6), "5 种预设主题", color=Color.CYAN)
themes = [
    ("激昂", RGBColor(0xFF, 0x3D, 0x3D), "红橙"),
    ("欢快", RGBColor(0xFF, 0xD9, 0x3D), "黄绿"),
    ("舒缓", RGBColor(0x3D, 0xFF, 0xD9), "青蓝"),
    ("低沉", RGBColor(0x6B, 0x3D, 0xFF), "紫黑"),
    ("通用", RGBColor(0xA8, 0xB2, 0xD1), "灰色"),
]
for i, (name, color, desc) in enumerate(themes):
    x = Inches(7.0 + (i % 3) * 2.0)
    y = Inches(1.85 + (i // 3) * 1.0)
    add_round_rect(slide, x, y, Inches(1.8), Inches(0.85), Color.BG_MID)
    add_rect(slide, x, y, Inches(0.4), Inches(0.85), color)
    add_text(slide, x + Inches(0.5), y + Inches(0.1), Inches(1.3), Inches(0.35), name,
             size=14, color=Color.WHITE, bold=True)
    add_text(slide, x + Inches(0.5), y + Inches(0.45), Inches(1.3), Inches(0.3), desc,
             size=10, color=Color.GRAY)

# 右下：技术实现
add_section_title(slide, Inches(7.0), Inches(3.2), Inches(6), "技术实现", color=Color.PURPLE)
add_multiline(slide, Inches(7.0), Inches(3.75), Inches(6), Inches(3), [
    ("• 情绪分类三维特征：", Color.WHITE, True),
    ("  BPM（慢/中/快）+ 低频能量比 + 整体能量", Color.GRAY, False),
    ("• 双轨道调色板设计：", Color.WHITE, True),
    ("  菜单 QSS 渲染 + 游戏运行时色值", Color.GRAY, False),
    ("• ThemeManager 单例 + 信号槽全局通知", Color.WHITE, False),
    ("• 自定义调色器：9 色块实时预览", Color.WHITE, False),
    ("• JSON 导入/导出共享主题预设", Color.WHITE, False),
], size=12, line_spacing=1.3)

# 底部：效果
add_round_rect(slide, Inches(0.5), Inches(5.5), Inches(6.0), Inches(1.4), Color.BG_LIGHT)
add_rect(slide, Inches(0.5), Inches(5.5), Inches(0.15), Inches(1.4), Color.PINK)
add_text(slide, Inches(0.8), Inches(5.6), Inches(3), Inches(0.4), "效果",
         size=16, color=Color.PINK, bold=True)
add_multiline(slide, Inches(0.8), Inches(6.0), Inches(5.5), Inches(0.9), [
    ("• 系统自动为每首歌匹配最佳视觉风格", Color.WHITE, False),
    ("• 提升用户体验个性化与沉浸感", Color.WHITE, False),
], size=12, line_spacing=1.3)

# 第 11 页：谱面密度归一化
slide = new_slide()
add_title_bar(slide, "05  创新功能亮点 (3/4)", "★ 谱面密度归一化（I-04）—— 算法创新", page_num=11)

# 顶部：问题与方案
add_section_title(slide, Inches(0.5), Inches(1.3), Inches(12), "问题与方案", color=Color.PINK)
# 问题
add_round_rect(slide, Inches(0.5), Inches(1.85), Inches(6.0), Inches(1.5), Color.BG_MID)
add_rect(slide, Inches(0.5), Inches(1.85), Inches(6.0), Inches(0.08), Color.ORANGE)
add_text(slide, Inches(0.7), Inches(1.95), Inches(2), Inches(0.4), "问题",
         size=14, color=Color.ORANGE, bold=True)
add_multiline(slide, Inches(0.7), Inches(2.35), Inches(5.5), Inches(1), [
    ("• 歌曲时长差异巨大（30秒~10分钟）", Color.WHITE, False),
    ("• 固定密度 → 长歌过密、短歌过稀", Color.WHITE, False),
], size=11, line_spacing=1.3)
# 方案
add_round_rect(slide, Inches(6.8), Inches(1.85), Inches(6.0), Inches(1.5), Color.BG_MID)
add_rect(slide, Inches(6.8), Inches(1.85), Inches(6.0), Inches(0.08), Color.GREEN)
add_text(slide, Inches(7.0), Inches(1.95), Inches(2), Inches(0.4), "方案",
         size=14, color=Color.GREEN, bold=True)
add_multiline(slide, Inches(7.0), Inches(2.35), Inches(5.5), Inches(1), [
    ("• 目标密度：100 音符/分钟（经验值）", Color.WHITE, False),
    ("• 根据时长自适应调节音符数量", Color.WHITE, False),
], size=11, line_spacing=1.3)

# 中部：算法流程
add_section_title(slide, Inches(0.5), Inches(3.6), Inches(12), "算法流程", color=Color.CYAN)
flow_steps = [
    ("计算目标数", "目标音符数\n= 时长(秒) × 100 / 60", Color.CYAN),
    ("判断多寡", "实际 vs 目标\n比较音符数量", Color.PURPLE),
    ("剔除/补充", "多则均匀剔除 TAP\n少则在强拍补充", Color.PINK),
    ("保持 HOLD", "HOLD 音符不剔除\n保证长按体验", Color.GREEN),
]
for i, (title, detail, c) in enumerate(flow_steps):
    x = Inches(0.5 + i * 3.15)
    add_round_rect(slide, x, Inches(4.15), Inches(2.9), Inches(1.6), Color.BG_MID)
    add_rect(slide, x, Inches(4.15), Inches(2.9), Inches(0.08), c)
    add_text(slide, x, Inches(4.25), Inches(2.9), Inches(0.4), title,
             size=14, color=c, bold=True, align=PP_ALIGN.CENTER)
    add_text(slide, x, Inches(4.7), Inches(2.9), Inches(1.0), detail,
             size=11, color=Color.WHITE, align=PP_ALIGN.CENTER)
    if i < 3:
        add_text(slide, x + Inches(2.9), Inches(4.7), Inches(0.25), Inches(0.5), "→",
                 size=20, color=Color.GRAY, align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)

# 底部：效果
add_round_rect(slide, Inches(0.5), Inches(6.0), Inches(12.3), Inches(0.9), Color.BG_LIGHT)
add_rect(slide, Inches(0.5), Inches(6.0), Inches(0.15), Inches(0.9), Color.PINK)
add_text(slide, Inches(0.8), Inches(6.05), Inches(12), Inches(0.8),
         "效果：无论歌曲长短，谱面密度保持一致，游戏体验稳定可预测",
         size=13, color=Color.WHITE, anchor=MSO_ANCHOR.MIDDLE)

# 第 12 页：难度自动评级
slide = new_slide()
add_title_bar(slide, "05  创新功能亮点 (4/4)", "★ 难度自动评级（I-05）—— 数据驱动设计", page_num=12)

# 左：四维特征
add_section_title(slide, Inches(0.5), Inches(1.3), Inches(6), "四维特征提取", color=Color.CYAN)
features = [
    ("音符密度", "音符数 / 分钟", "0.30", Color.CYAN),
    ("短间隔占比", "< 200ms 连续音符比例", "0.25", Color.PURPLE),
    ("BPM", "音乐速度", "0.20", Color.PINK),
    ("最长密集段", "连续高密度音符秒数", "0.25", Color.GREEN),
]
y = Inches(1.85)
for name, desc, weight, c in features:
    add_round_rect(slide, Inches(0.5), y, Inches(6.0), Inches(0.85), Color.BG_MID)
    add_rect(slide, Inches(0.5), y, Inches(0.1), Inches(0.85), c)
    add_text(slide, Inches(0.7), y + Inches(0.1), Inches(2.5), Inches(0.35), name,
             size=14, color=c, bold=True)
    add_text(slide, Inches(0.7), y + Inches(0.45), Inches(3.5), Inches(0.35), desc,
             size=10, color=Color.GRAY)
    # 权重色块
    add_round_rect(slide, Inches(4.8), y + Inches(0.2), Inches(1.5), Inches(0.5), Color.BG_LIGHT)
    add_text(slide, Inches(4.8), y + Inches(0.2), Inches(1.5), Inches(0.5),
             "权重 " + weight, size=12, color=c, bold=True,
             align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)
    y += Inches(0.95)

# 公式
add_round_rect(slide, Inches(0.5), Inches(5.7), Inches(6.0), Inches(1.2), Color.BG_LIGHT)
add_text(slide, Inches(0.5), Inches(5.85), Inches(6.0), Inches(0.4),
         "加权评分公式", size=12, color=Color.GRAY, align=PP_ALIGN.CENTER)
add_text(slide, Inches(0.5), Inches(6.2), Inches(6.0), Inches(0.6),
         "difficulty = 0.3×密度 + 0.25×短间隔 + 0.2×BPM + 0.25×密集段",
         size=13, color=Color.WHITE, bold=True, align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)

# 右：难度星级
add_section_title(slide, Inches(7.0), Inches(1.3), Inches(6), "难度星级映射", color=Color.PURPLE)
levels = [
    ("1-2 ★", "入门", Color.GREEN),
    ("3-4 ★", "简单", Color.CYAN),
    ("5-6 ★", "中等", Color.ORANGE),
    ("7-8 ★", "困难", Color.PINK),
    ("9-10 ★", "极限", RGBColor(0xFF, 0x00, 0x00)),
]
y = Inches(1.85)
for stars, label, c in levels:
    add_round_rect(slide, Inches(7.0), y, Inches(6.0), Inches(0.85), Color.BG_MID)
    add_rect(slide, Inches(7.0), y, Inches(0.1), Inches(0.85), c)
    add_text(slide, Inches(7.3), y + Inches(0.1), Inches(2.5), Inches(0.35), stars,
             size=18, color=c, bold=True)
    add_text(slide, Inches(7.3), y + Inches(0.45), Inches(2.5), Inches(0.35), label,
             size=12, color=Color.WHITE)
    # 进度条
    bar_width = Inches(4.0 - int(stars[0]) * 0.4)
    add_rect(slide, Inches(10.0), y + Inches(0.3), Inches(2.5), Inches(0.25), Color.BG_DARK)
    add_rect(slide, Inches(10.0), y + Inches(0.3), bar_width, Inches(0.25), c)
    y += Inches(0.95)

# 底部效果
add_round_rect(slide, Inches(7.0), Inches(6.5), Inches(6.0), Inches(0.7), Color.BG_LIGHT)
add_text(slide, Inches(7.2), Inches(6.55), Inches(5.6), Inches(0.6),
         "玩家选歌前直观了解难度，避免选到超能力歌曲",
         size=12, color=Color.WHITE, anchor=MSO_ANCHOR.MIDDLE)

# ==================== 第 13 页：UML 类图 ====================
slide = new_slide()
add_title_bar(slide, "03  系统设计", "UML 类图（整体架构）", page_num=13)

# 嵌入 UML 类图 PNG
img_path = r"e:\SpectrumFall\report_assets\images\class-diagram-overall.png"
if os.path.exists(img_path):
    # 居中放置，保持比例
    pic = slide.shapes.add_picture(img_path, Inches(0.5), Inches(1.3), height=Inches(5.7))
    # 水平居中
    pic.left = int((SW - pic.width) / 2)
else:
    add_text(slide, Inches(0.5), Inches(3.0), Inches(12), Inches(1),
             "[UML 类图占位：class-diagram-overall.png 未找到]",
             size=16, color=Color.GRAY, align=PP_ALIGN.CENTER)

# 底部说明
add_text(slide, Inches(0.5), Inches(7.05), Inches(12), Inches(0.35),
         "完整类图涵盖 30+ 类，展示继承关系、持有关系、依赖关系",
         size=11, color=Color.GRAY, align=PP_ALIGN.CENTER)

# ==================== 第 14 页：运行演示 ====================
slide = new_slide()
add_title_bar(slide, "06  运行演示与总结", "运行截图（待替换）", page_num=14)

# 6 个截图占位框
screenshots = [
    ("主菜单", "深色主题 + 6 大按钮"),
    ("可视化效果", "柱状/圆形/波形/瀑布"),
    ("游戏界面", "下落音符 + 判定 + HUD"),
    ("生存模式", "血量条 + 危险反馈"),
    ("主题编辑器", "9 色块实时预览"),
    ("谱面编辑器", "时间轴 + 表格编辑"),
]
for i, (title, desc) in enumerate(screenshots):
    row = i // 3
    col = i % 3
    x = Inches(0.5 + col * 4.3)
    y = Inches(1.5 + row * 2.7)
    # 占位框
    add_round_rect(slide, x, y, Inches(4.0), Inches(2.3), Color.BG_MID)
    # 虚线边框效果（用矩形模拟）
    add_rect(slide, x, y, Inches(4.0), Inches(2.3), Color.BG_DARK)
    add_rect(slide, x + Inches(0.05), y + Inches(0.05), Inches(3.9), Inches(2.2), Color.BG_MID)
    # 标题
    add_text(slide, x, y + Inches(0.6), Inches(4.0), Inches(0.5), title,
             size=18, color=Color.CYAN, bold=True, align=PP_ALIGN.CENTER)
    # 描述
    add_text(slide, x, y + Inches(1.1), Inches(4.0), Inches(0.4), desc,
             size=12, color=Color.GRAY, align=PP_ALIGN.CENTER)
    # 占位标识
    add_text(slide, x, y + Inches(1.5), Inches(4.0), Inches(0.5), "[ 待插入截图 ]",
             size=11, color=Color.GRAY_DARK, align=PP_ALIGN.CENTER)

# 底部说明
add_text(slide, Inches(0.5), Inches(7.05), Inches(12), Inches(0.35),
         "提示：实际答辩时替换为真实运行截图（建议 1920×1080 PNG）",
         size=11, color=Color.ORANGE, align=PP_ALIGN.CENTER)

# ==================== 第 15 页：测试报告 ====================
slide = new_slide()
add_title_bar(slide, "06  运行演示与总结", "测试报告", page_num=15)

# 左：功能测试
add_section_title(slide, Inches(0.5), Inches(1.3), Inches(6), "功能测试结果", color=Color.CYAN)
test_items = [
    ("基础功能（6 项）", "100% 通过", Color.GREEN),
    ("扩展功能（5 项）", "100% 通过", Color.GREEN),
    ("创新功能（8 项）", "100% 通过", Color.GREEN),
    ("音频格式兼容", "MP3/WAV/FLAC", Color.CYAN),
    ("多曲风测试", "电子/摇滚/古典/人声", Color.CYAN),
]
y = Inches(1.85)
for name, result, c in test_items:
    add_round_rect(slide, Inches(0.5), y, Inches(6.0), Inches(0.7), Color.BG_MID)
    add_rect(slide, Inches(0.5), y, Inches(0.1), Inches(0.7), c)
    add_text(slide, Inches(0.7), y + Inches(0.05), Inches(3.5), Inches(0.6), name,
             size=13, color=Color.WHITE, anchor=MSO_ANCHOR.MIDDLE)
    add_text(slide, Inches(4.2), y + Inches(0.05), Inches(2.2), Inches(0.6), result,
             size=12, color=c, bold=True, anchor=MSO_ANCHOR.MIDDLE, align=PP_ALIGN.RIGHT)
    y += Inches(0.8)

# 右：性能测试
add_section_title(slide, Inches(7.0), Inches(1.3), Inches(6), "性能指标", color=Color.PURPLE)
perf_items = [
    ("FFT 计算耗时", "< 16 ms / 帧", "60Hz 实时", Color.GREEN),
    ("游戏帧率", "稳定 60 fps", "无卡顿", Color.GREEN),
    ("音频同步误差", "< 50 ms", "双时钟策略", Color.CYAN),
    ("内存占用", "~ 150 MB", "含 PCM 缓存", Color.CYAN),
    ("启动时间", "< 2 秒", "冷启动", Color.CYAN),
]
y = Inches(1.85)
for name, value, note, c in perf_items:
    add_round_rect(slide, Inches(7.0), y, Inches(6.0), Inches(0.7), Color.BG_MID)
    add_rect(slide, Inches(7.0), y, Inches(0.1), Inches(0.7), c)
    add_text(slide, Inches(7.2), y + Inches(0.05), Inches(2.5), Inches(0.6), name,
             size=13, color=Color.WHITE, anchor=MSO_ANCHOR.MIDDLE)
    add_text(slide, Inches(9.7), y + Inches(0.05), Inches(1.6), Inches(0.6), value,
             size=13, color=c, bold=True, anchor=MSO_ANCHOR.MIDDLE)
    add_text(slide, Inches(11.3), y + Inches(0.05), Inches(1.6), Inches(0.6), note,
             size=10, color=Color.GRAY, anchor=MSO_ANCHOR.MIDDLE)
    y += Inches(0.8)

# 底部：兼容性
add_section_title(slide, Inches(0.5), Inches(5.95), Inches(12), "兼容性测试", color=Color.GREEN)
add_round_rect(slide, Inches(0.5), Inches(6.5), Inches(12.3), Inches(0.7), Color.BG_MID)
add_text(slide, Inches(0.7), Inches(6.55), Inches(12), Inches(0.6),
         "Windows 10/11 ✓  |  Intel 集显/NVIDIA 独显 ✓  |  不同屏幕分辨率 ✓  |  windeployqt 打包可独立运行 ✓",
         size=12, color=Color.WHITE, anchor=MSO_ANCHOR.MIDDLE)

# ==================== 第 16 页：个人总结 ====================
slide = new_slide()
add_title_bar(slide, "06  运行演示与总结", "个人总结", page_num=16)

# 左：项目收获
add_section_title(slide, Inches(0.5), Inches(1.3), Inches(6), "项目收获", color=Color.CYAN)
add_multiline(slide, Inches(0.5), Inches(1.85), Inches(6), Inches(5), [
    ("• C++ 工程化能力显著提升", Color.WHITE, True),
    ("  OOP 设计 / 智能指针 / 模板 / STL", Color.GRAY, False),
    ("", Color.WHITE, False),
    ("• Qt 框架深入掌握", Color.WHITE, True),
    ("  信号槽 / 多媒体 / OpenGL / 并发", Color.GRAY, False),
    ("", Color.WHITE, False),
    ("• DSP 算法实战经验", Color.WHITE, True),
    ("  FFT / 节拍检测 / 音频特效 DSP", Color.GRAY, False),
    ("", Color.WHITE, False),
    ("• AI 协作能力提升", Color.WHITE, True),
    ("  智谱 GLM-5.2 + 通义千问 Qwen3.7 Max", Color.GRAY, False),
    ("  学会提问 + 审核 + 整合 AI 生成内容", Color.GRAY, False),
], size=13, line_spacing=1.3)

# 右：不足与改进
add_section_title(slide, Inches(7.0), Inches(1.3), Inches(6), "不足与改进方向", color=Color.PINK)
add_multiline(slide, Inches(7.0), Inches(1.85), Inches(6), Inches(5), [
    ("不足：", Color.PINK, True),
    ("• 跨平台支持不足（仅 Windows）", Color.WHITE, False),
    ("• 单元测试覆盖率有待提升", Color.WHITE, False),
    ("• 移动端适配未实现", Color.WHITE, False),
    ("• 在线排行榜功能限于本地", Color.WHITE, False),
    ("• 对 AI 生成代码理解深度待加强", Color.WHITE, False),
    ("", Color.WHITE, False),
    ("改进方向：", Color.GREEN, True),
    ("• 跨平台（macOS/Linux）", Color.WHITE, False),
    ("• 引入 GTest 单元测试框架", Color.WHITE, False),
    ("• 网络对战 + 在线排行榜", Color.WHITE, False),
    ("• 移动端 Qt Quick 实现", Color.WHITE, False),
    ("• 深化 AI 协作实践", Color.WHITE, False),
], size=13, line_spacing=1.3)

# 底部：核心感悟
add_round_rect(slide, Inches(0.5), Inches(6.5), Inches(12.3), Inches(0.7), Color.BG_LIGHT)
add_rect(slide, Inches(0.5), Inches(6.5), Inches(0.15), Inches(0.7), Color.PINK)
add_text(slide, Inches(0.8), Inches(6.55), Inches(12), Inches(0.6),
         "★ AI 是放大器，不是替代品 —— 核心知识必须自己掌握，提问能力即生产力",
         size=13, color=Color.WHITE, bold=True, anchor=MSO_ANCHOR.MIDDLE)

# ==================== 第 17 页：致谢 ====================
slide = new_slide()
# 装饰
add_rect(slide, 0, 0, SW, Inches(0.15), Color.CYAN)
add_rect(slide, 0, Inches(0.15), SW, Inches(0.05), Color.PURPLE)
add_rect(slide, 0, Inches(7.3), SW, Inches(0.15), Color.CYAN)
add_rect(slide, 0, Inches(7.25), SW, Inches(0.05), Color.PURPLE)

# 中央大字
add_text(slide, Inches(1.0), Inches(2.0), Inches(11.333), Inches(1.5),
         "THANKS", size=88, color=Color.CYAN, bold=True, align=PP_ALIGN.CENTER)
add_text(slide, Inches(1.0), Inches(3.5), Inches(11.333), Inches(0.6),
         "感谢各位老师的聆听", size=28, color=Color.WHITE, align=PP_ALIGN.CENTER)
add_rect(slide, Inches(5.0), Inches(4.3), Inches(3.333), Inches(0.04), Color.CYAN)
add_text(slide, Inches(1.0), Inches(4.5), Inches(11.333), Inches(0.5),
         "欢迎提问与指导", size=20, color=Color.GRAY, align=PP_ALIGN.CENTER)

# 项目信息
add_text(slide, Inches(1.0), Inches(5.5), Inches(11.333), Inches(0.4),
         "SpectrumFall — 实时音频频谱可视化与音乐节奏游戏引擎",
         size=14, color=Color.GRAY_DARK, align=PP_ALIGN.CENTER)
add_text(slide, Inches(1.0), Inches(6.0), Inches(11.333), Inches(0.4),
         "范中海  |  学号 20254875  |  2026 年 7 月",
         size=14, color=Color.GRAY_DARK, align=PP_ALIGN.CENTER)

# ==================== 保存 ====================
output_path = r"e:\SpectrumFall\SpectrumFall答辩.pptx"
prs.save(output_path)
print(f"PPT saved to: {output_path}")
print(f"Total slides: {len(prs.slides)}")
print(f"File size: {os.path.getsize(output_path)} bytes")
