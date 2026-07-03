#pragma once

class QApplication;

/// 全局按钮音效 + 视觉增强
/// - 悬停：霓虹辉光 + 微缩放动画 + hover 音效
/// - 点击：click 音效 + 回弹动画
/// - 释放/离开：平滑还原
/// 在 main() 中调用 install() 即可对所有 QPushButton 生效
namespace UIButtonEffects
{
    /// 初始化音效并安装全局事件过滤器（在 QApplication 创建后调用）
    void install(QApplication* app);
}
