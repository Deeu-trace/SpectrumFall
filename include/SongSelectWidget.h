#pragma once

#include <QWidget>
#include <QString>

class QListWidget;
class QPushButton;
class QLabel;

/// 歌曲选择页面：扫描目录音频文件列表，BPM 预览，可视化/游戏按钮
class SongSelectWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SongSelectWidget(QWidget* parent = nullptr);

    /// 设置歌曲列表
    void setSongs(const QStringList& paths);

    /// 获取当前选中的歌曲路径
    QString selectedSong() const;

    /// 显示 BPM
    void setBpmDisplay(float bpm);

    /// 扫描默认目录中的音频文件
    void scanDirectory(const QString& dirPath);

signals:
    void songSelected(const QString& path);
    void visualizeRequested();
    void gameRequested();
    void backRequested();

private slots:
    void onSongItemClicked();
    void onVisualizeClicked();
    void onGameClicked();

private:
    QListWidget* m_songList;       ///< 歌曲列表
    QPushButton* m_visualizeBtn;   ///< 可视化按钮
    QPushButton* m_gameBtn;        ///< 游戏按钮
    QPushButton* m_backBtn;        ///< 返回按钮
    QLabel* m_bpmLabel;            ///< BPM 显示标签
    QLabel* m_infoLabel;           ///< 信息标签
    QString m_selectedPath;        ///< 当前选中歌曲路径
};
