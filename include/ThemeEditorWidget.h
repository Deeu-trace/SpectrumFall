#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QMap>
#include <QVector>

struct ThemePalette;
class ToggleSwitch;

/// 主题编辑器：预设切换 + 自定义调色 + 保存/重置预设 + JSON 导入导出
class ThemeEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ThemeEditorWidget(QWidget* parent = nullptr);

signals:
    void backRequested();

private slots:
    void onPresetClicked(int index);
    void onCustomClicked();
    void onApplyCustom();
    void onSavePresetClicked();
    void onResetClicked();
    void onSavedPresetClicked(int index);
    void onDeletePreset(int index);
    void onExportClicked();
    void onImportClicked();
    void onColorPickClicked(const QString& key);
    void onAutoMoodToggled(bool checked);

private:
    void setupUI();
    void updateActiveButton(int index);
    void showCustomPanel(bool visible);

    // 自定义预设持久化
    struct CustomPreset {
        QString name;
        QMap<QString, QString> colors;  // key -> hex
    };
    QVector<CustomPreset> m_customPresets;
    QString m_presetFilePath;
    void loadCustomPresets();
    void saveCustomPresets();
    void refreshCustomPresetButtons();
    ThemePalette buildPaletteFromColors(const QMap<QString, QString>& colors) const;

    // 预设按钮
    QVector<QPushButton*> m_presetBtns;
    QPushButton* m_customBtn;
    int m_activeIndex = -1;

    // 自定义预设按钮行
    QWidget* m_customPresetRow;
    QHBoxLayout* m_customPresetLayout;
    QVector<QPushButton*> m_savedPresetBtns;

    // 自定义面板
    QWidget* m_customPanel;
    QMap<QString, QPushButton*> m_swatchBtns;
    QMap<QString, QColor> m_customColors;

    QPushButton* m_applyBtn;
    QPushButton* m_savePresetBtn;
    QPushButton* m_resetBtn;
    QPushButton* m_exportBtn;
    QPushButton* m_importBtn;
    QPushButton* m_backBtn;
    QLabel* m_currentMoodLabel;
    ToggleSwitch* m_autoMoodToggle;
};
