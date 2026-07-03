#include "ChartEditorWidget.h"
#include "ChartManager.h"
#include "ChartTimelineWidget.h"
#include "AudioEngine.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QSpinBox>
#include <QComboBox>
#include <QStyledItemDelegate>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QMessageBox>
#include <QFileInfo>
#include <QShowEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMap>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QSlider>
#include <QModelIndexList>
#include <QDebug>
#include <algorithm>

// ── SpinBoxDelegate：限制表格编辑只能输入指定范围内的整数 ──
// 非 Q_OBJECT 类，仅重写虚函数，不需要 MOC 注册
class SpinBoxDelegate : public QStyledItemDelegate
{
public:
    explicit SpinBoxDelegate(int minVal, int maxVal, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_min(minVal), m_max(maxVal) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&,
                          const QModelIndex&) const override
    {
        QSpinBox* sb = new QSpinBox(parent);
        sb->setRange(m_min, m_max);
        sb->setFrame(false);
        return sb;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        QSpinBox* sb = qobject_cast<QSpinBox*>(editor);
        if (!sb) return;
        sb->setValue(index.data().toInt());
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override
    {
        QSpinBox* sb = qobject_cast<QSpinBox*>(editor);
        if (!sb) return;
        sb->interpretText();
        model->setData(index, sb->value());
    }

private:
    int m_min;
    int m_max;
};

/// ComboBoxDelegate：音符类型列，下拉选择 TAP / HOLD
/// 只读写 DisplayRole 字符串，不使用 EditRole（避免 EditRole 覆盖 DisplayRole 的问题）
class ComboBoxDelegate : public QStyledItemDelegate
{
public:
    explicit ComboBoxDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&,
                          const QModelIndex&) const override
    {
        QComboBox* cb = new QComboBox(parent);
        cb->addItem(QStringLiteral("TAP"));
        cb->addItem(QStringLiteral("HOLD"));
        return cb;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        QComboBox* cb = qobject_cast<QComboBox*>(editor);
        if (!cb) return;
        QString text = index.data(Qt::DisplayRole).toString();
        cb->setCurrentIndex(text == QStringLiteral("HOLD") ? 1 : 0);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override
    {
        QComboBox* cb = qobject_cast<QComboBox*>(editor);
        if (!cb) return;
        model->setData(index, cb->currentText(), Qt::DisplayRole);
    }
};

// ── ChartEditorWidget 实现 ────────────────────────────────────

ChartEditorWidget::ChartEditorWidget(AudioEngine* audio, ChartManager* chartMgr, QWidget* parent)
    : QWidget(parent)
    , m_audio(audio)
    , m_chartMgr(chartMgr)
    , m_songFileSize(0)
    , m_songDurationMs(0)
    , m_bpm(0.0f)
    , m_laneCount(6)
    , m_loading(false)
    , m_syncPending(false)
{
    setupUI();
}

void ChartEditorWidget::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(10);

    // ── 标题 ──
    QLabel* titleLabel = new QLabel(QStringLiteral("谱面编辑器"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(32);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #00ff88; background: transparent;");
    layout->addWidget(titleLabel);

    // ── 信息标签 ──
    m_infoLabel = new QLabel(this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setStyleSheet("color: #bd93f9; font-size: 14px; background: transparent;");
    layout->addWidget(m_infoLabel);

    // ── 位置标签 ──
    m_posLabel = new QLabel(this);
    m_posLabel->setAlignment(Qt::AlignCenter);
    m_posLabel->setStyleSheet("color: #8be9fd; font-size: 13px; background: transparent;");
    layout->addWidget(m_posLabel);

    // ── QSplitter：左表格 + 右时间轴 ──
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    // 左侧：音符表格（4列：时间, 轨道, 类型, 持续时间）
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("时间 (ms)") << QStringLiteral("轨道")
                      << QStringLiteral("类型") << QStringLiteral("持续(ms)"));
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked);
    m_table->setAlternatingRowColors(true);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_table->setStyleSheet(
        "QTableWidget { background-color: #0d0d1f; alternate-background-color: #12122a; "
        "color: #e0e0e0; gridline-color: #1f1f3a; border: 1px solid #0f3460; border-radius: 8px; "
        "font-size: 14px; }"
        "QHeaderView::section { background-color: #16213e; color: #00ff88; "
        "font-weight: bold; padding: 5px; border: none; border-bottom: 1px solid #0f3460; }"
        "QTableWidget::item { padding: 3px 6px; }"
        "QTableWidget::item:selected { background-color: #0f3460; color: #ffffff; }");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 120);
    m_table->setColumnWidth(1, 60);
    m_table->setColumnWidth(2, 70);
    m_table->setItemDelegateForColumn(0, new SpinBoxDelegate(0, 9999999, this));
    m_table->setItemDelegateForColumn(1, new SpinBoxDelegate(0, 5, this));
    m_table->setItemDelegateForColumn(2, new ComboBoxDelegate(this));
    m_table->setItemDelegateForColumn(3, new SpinBoxDelegate(0, 10000, this));
    m_table->setMinimumWidth(350);
    splitter->addWidget(m_table);

    // 右侧：缩放工具栏 + 可滚动时间轴
    QWidget* rightPanel = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);

    // 缩放工具栏
    QHBoxLayout* zoomRow = new QHBoxLayout();
    zoomRow->setSpacing(6);
    QLabel* zoomIcon = new QLabel(QStringLiteral("缩放:"), this);
    zoomIcon->setStyleSheet("color: #e0e0e0; font-size: 13px; background: transparent;");
    zoomRow->addWidget(zoomIcon);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(10, 100);   // 1.0x ~ 10.0x
    m_zoomSlider->setValue(10);         // 默认 1.0x
    m_zoomSlider->setFixedWidth(180);
    m_zoomSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 5px; background: #16213e; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #00ff88; width: 12px; height: 12px; "
        "margin: -4px 0; border-radius: 6px; }"
        "QSlider::sub-page:horizontal { background: #0f3460; border-radius: 2px; }");
    zoomRow->addWidget(m_zoomSlider);

    m_zoomLabel = new QLabel(QStringLiteral("1.0x"), this);
    m_zoomLabel->setStyleSheet("color: #00ff88; font-size: 13px; font-weight: bold; background: transparent;");
    m_zoomLabel->setFixedWidth(45);
    zoomRow->addWidget(m_zoomLabel);

    QLabel* zoomTip = new QLabel(QStringLiteral("  (Ctrl+滚轮)"), this);
    zoomTip->setStyleSheet("color: #666680; font-size: 11px; background: transparent;");
    zoomRow->addWidget(zoomTip);
    zoomRow->addStretch();
    rightLayout->addLayout(zoomRow);

    // 时间轴（包在 QScrollArea 中）
    m_timeline = new ChartTimelineWidget();
    m_timeline->setAudioEngine(m_audio);
    // Preferred: 按 sizeHint 宽度显示，超出 viewport 时出滚动条（缩放靠这个）
    m_timeline->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_timeline->setMinimumHeight(180);

    m_timelineScroll = new QScrollArea(this);
    m_timelineScroll->setWidget(m_timeline);
    m_timelineScroll->setWidgetResizable(false);  // 让 timeline 按 sizeHint 决定宽度
    m_timelineScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_timelineScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_timelineScroll->setStyleSheet(
        "QScrollArea { border: 1px solid #0f3460; border-radius: 6px; background: #0d0d1f; }"
        "QScrollBar:horizontal { background: #0d0d1f; height: 10px; border-radius: 5px; }"
        "QScrollBar::handle:horizontal { background: #0f3460; border-radius: 5px; min-width: 30px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }");
    rightLayout->addWidget(m_timelineScroll, 1);
    splitter->addWidget(rightPanel);

    // 分割比例：表格 ~25%, 时间轴 ~75%
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes(QList<int>() << 300 << 900);
    layout->addWidget(splitter, 1);

    // ── 底部按钮行 ──
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    m_keyModeBtn = new QPushButton(QStringLiteral("6K"), this);
    m_keyModeBtn->setMinimumSize(50, 34);
    m_keyModeBtn->setObjectName("actionButton");
    m_keyModeBtn->setStyleSheet(
        "QPushButton { background-color: #00ff88; color: #0d0d1f; font-weight: bold; "
        "border: none; border-radius: 6px; font-size: 14px; }"
        "QPushButton:hover { background-color: #33ffaa; }");
    btnRow->addWidget(m_keyModeBtn);

    m_saveBtn = new QPushButton(QStringLiteral("保存谱面"), this);
    m_saveBtn->setMinimumSize(100, 34);
    m_saveBtn->setObjectName("actionButton");
    btnRow->addWidget(m_saveBtn);

    m_playGameBtn = new QPushButton(QStringLiteral("开始游戏"), this);
    m_playGameBtn->setMinimumSize(100, 34);
    m_playGameBtn->setObjectName("actionButton");
    btnRow->addWidget(m_playGameBtn);

    btnRow->addStretch();

    // 角落灰色提示
    QLabel* hintLabel = new QLabel(
        QStringLiteral("左键添加  右键删除  空格播放  拖拽创建HOLD  滚轮滚动"), this);
    hintLabel->setStyleSheet("color: #555570; font-size: 11px; background: transparent;");
    btnRow->addWidget(hintLabel);

    btnRow->addSpacing(12);

    m_backBtn = new QPushButton(QStringLiteral("返回"), this);
    m_backBtn->setMinimumSize(80, 34);
    m_backBtn->setObjectName("backButton");
    btnRow->addWidget(m_backBtn);

    layout->addLayout(btnRow);

    // ── 定时器 ──
    m_posTimer = new QTimer(this);
    m_posTimer->setInterval(100);  // 100ms 够用，比 50ms 减少一半 repaint
    connect(m_posTimer, &QTimer::timeout, this, &ChartEditorWidget::onPositionUpdate);

    // 批量同步定时器（100ms 内合并多次 syncTimeline 请求）
    m_syncTimer = new QTimer(this);
    m_syncTimer->setSingleShot(true);
    m_syncTimer->setInterval(100);
    connect(m_syncTimer, &QTimer::timeout, this, [this]() {
        if (m_syncPending) {
            m_syncPending = false;
            syncTimeline();
        }
    });

    // ── 连接信号 ──
    connect(m_saveBtn, &QPushButton::clicked, this, &ChartEditorWidget::onSaveChart);
    connect(m_playGameBtn, &QPushButton::clicked, this, &ChartEditorWidget::onPlayGame);
    connect(m_backBtn, &QPushButton::clicked, this, &ChartEditorWidget::backRequested);
    connect(m_keyModeBtn, &QPushButton::clicked, this, &ChartEditorWidget::onKeyModeToggled);
    connect(m_table, &QTableWidget::itemChanged, this, &ChartEditorWidget::onItemChanged);
    connect(m_timeline, &ChartTimelineWidget::noteAdded, this, &ChartEditorWidget::onTimelineNoteAdded);
    connect(m_timeline, &ChartTimelineWidget::noteSelected, this, &ChartEditorWidget::onTimelineNoteSelected);
    connect(m_timeline, &ChartTimelineWidget::noteDeleted, this, &ChartEditorWidget::onTimelineNoteDeleted);
    connect(m_timeline, &ChartTimelineWidget::holdNoteAdded, this, &ChartEditorWidget::onTimelineHoldAdded);
    connect(m_timeline, &ChartTimelineWidget::scrollRequested, this, [this](int deltaPixels) {
        if (m_timelineScroll && m_timelineScroll->horizontalScrollBar()) {
            QScrollBar* hBar = m_timelineScroll->horizontalScrollBar();
            hBar->setValue(hBar->value() + deltaPixels / 2);
        }
    });
    connect(m_timeline, &ChartTimelineWidget::playheadDragged, this, [this](qint64 ms) {
        m_posLabel->setText(formatTime(ms) + QStringLiteral(" / ") + formatTime(m_songDurationMs));
    });

    // 缩放
    connect(m_zoomSlider, &QSlider::valueChanged, this, &ChartEditorWidget::applyZoom);
    connect(m_timeline, &ChartTimelineWidget::zoomChanged, this, [this](float factor) {
        m_zoomSlider->blockSignals(true);
        m_zoomSlider->setValue(static_cast<int>(factor * 10));
        m_zoomSlider->blockSignals(false);
        m_zoomLabel->setText(QStringLiteral("%1x").arg(factor, 0, 'f', 1));
    });
}

void ChartEditorWidget::loadChart(const QVector<GameNote>& notes, const QString& songPath,
                                  qint64 songFileSize, qint64 durationMs, float bpm)
{
    m_songPath = songPath;
    m_songFileSize = songFileSize;
    m_songDurationMs = durationMs;
    m_bpm = bpm;

    QFileInfo fi(songPath);
    m_songFileName = fi.fileName();

    m_loading = true;
    populateTable(notes);
    m_loading = false;

    updateInfoLabel();
    m_posLabel->setText(formatTime(0) + QStringLiteral(" / ") + formatTime(durationMs));

    // 同步时间轴
    m_timeline->setDurationMs(durationMs);
    m_timeline->setNotes(notes);
}

void ChartEditorWidget::populateTable(const QVector<GameNote>& notes)
{
    m_table->setRowCount(0);

    for (const GameNote& note : notes) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        // 时间列
        QTableWidgetItem* timeItem = new QTableWidgetItem;
        timeItem->setData(Qt::DisplayRole, QVariant(static_cast<qint64>(note.timestampMs)));
        timeItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, timeItem);

        // 轨道列
        QTableWidgetItem* laneItem = new QTableWidgetItem;
        laneItem->setData(Qt::DisplayRole, QVariant(note.lane));
        laneItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 1, laneItem);

        // 类型列：只用 setText 存字符串，不使用 EditRole
        QTableWidgetItem* typeItem = new QTableWidgetItem;
        typeItem->setText(note.noteType == HOLD ? QStringLiteral("HOLD") : QStringLiteral("TAP"));
        typeItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 2, typeItem);

        // 持续时间列
        QTableWidgetItem* durItem = new QTableWidgetItem;
        durItem->setData(Qt::DisplayRole, QVariant(static_cast<qint64>(note.holdDurationMs)));
        durItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 3, durItem);
    }

    sortTable();
}

QVector<GameNote> ChartEditorWidget::collectNotes() const
{
    QVector<GameNote> notes;
    notes.reserve(m_table->rowCount());

    for (int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* timeItem = m_table->item(i, 0);
        QTableWidgetItem* laneItem = m_table->item(i, 1);
        QTableWidgetItem* typeItem = m_table->item(i, 2);
        QTableWidgetItem* durItem  = m_table->item(i, 3);
        if (!timeItem || !laneItem) continue;

        qint64 ts   = timeItem->data(Qt::DisplayRole).toLongLong();
        int lane    = laneItem->data(Qt::DisplayRole).toInt();
        int type    = (typeItem && typeItem->text() == QStringLiteral("HOLD")) ? HOLD : TAP;
        qint64 dur  = durItem ? durItem->data(Qt::DisplayRole).toLongLong() : 0;
        notes.append(GameNote(ts, lane, type, dur));
    }

    // 按时间升序排列
    std::sort(notes.begin(), notes.end(),
              [](const GameNote& a, const GameNote& b) {
                  return a.timestampMs < b.timestampMs;
              });

    return notes;
}

void ChartEditorWidget::sortTable()
{
    m_loading = true;
    m_table->sortItems(0, Qt::AscendingOrder);
    m_loading = false;
}

void ChartEditorWidget::updateInfoLabel()
{
    int count = m_table->rowCount();
    m_infoLabel->setText(QStringLiteral("%1  |  BPM: %2  |  音符数: %3  |  时长: %4")
        .arg(m_songFileName)
        .arg(static_cast<int>(m_bpm))
        .arg(count)
        .arg(formatTime(m_songDurationMs)));
}

QString ChartEditorWidget::formatTime(qint64 ms) const
{
    qint64 totalSec = ms / 1000;
    qint64 min = totalSec / 60;
    qint64 sec = totalSec % 60;
    return QStringLiteral("%1:%2")
        .arg(min, 2, 10, QLatin1Char('0'))
        .arg(sec, 2, 10, QLatin1Char('0'));
}

// ── 槽函数 ────────────────────────────────────────────────────

void ChartEditorWidget::onPlayPause()
{
    if (!m_audio || !m_audio->isLoaded()) return;

    if (m_audio->state() == QMediaPlayer::PlayingState) {
        m_audio->pause();
    } else {
        m_audio->play();
    }
}

void ChartEditorWidget::onSaveChart()
{
    QVector<GameNote> notes = collectNotes();
    if (notes.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("没有音符可保存"));
        return;
    }

    ChartEntry entry;
    entry.songFileName   = m_songFileName;
    entry.songFileSize   = m_songFileSize;
    entry.songDurationMs = m_songDurationMs;
    entry.bpm            = m_bpm;
    entry.laneCount      = m_laneCount;
    entry.editedAt       = QDateTime::currentDateTime();
    entry.notes          = notes;

    m_chartMgr->saveChart(entry);

    m_saveBtn->setText(QStringLiteral("已保存"));
    m_saveBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this]() {
        m_saveBtn->setText(QStringLiteral("保存谱面"));
        m_saveBtn->setEnabled(true);
    });
}

void ChartEditorWidget::onPlayGame()
{
    QVector<GameNote> notes = collectNotes();
    if (notes.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("没有音符，无法开始游戏"));
        return;
    }

    emit playRequested(notes);
}

void ChartEditorWidget::onItemChanged(QTableWidgetItem* item)
{
    if (m_loading) return;
    if (!item) return;

    updateInfoLabel();

    int col = item->column();

    // 时间列被修改后重新排序
    if (col == 0) {
        sortTable();
    }

    // 类型列：ComboBoxDelegate 已直接写入 DisplayRole 文本，无需额外处理
    scheduleSync();
}

void ChartEditorWidget::onPositionUpdate()
{
    if (!m_audio || !m_audio->isLoaded()) return;

    // 拖拽播放头时跳过位置更新，避免覆盖拖拽位置
    if (m_timeline->isDraggingPlayhead()) return;

    qint64 pos = m_audio->position();
    m_posLabel->setText(formatTime(pos) + QStringLiteral(" / ") + formatTime(m_songDurationMs));
    m_timeline->setPositionMs(pos);

    // 同步时间轴高度 = 滚动区域视口高度（消除底部留白）
    // 只在差异 > 2px 时才 resize，避免每 100ms 触发 resizeEvent → 波形重建
    if (m_timelineScroll) {
        int vpH = m_timelineScroll->viewport()->height();
        if (vpH > 100 && std::abs(m_timeline->height() - vpH) > 2) {
            m_timeline->resize(m_timeline->width(), vpH);
        }
    }

    // 播放时自动滚动时间轴，让播放头保持可见
    if (m_audio->state() == QMediaPlayer::PlayingState && m_timelineScroll) {
        int px = m_timeline->timeToX(pos);
        // 确保播放头在可视区域内（留 50px 余量）
        m_timelineScroll->ensureVisible(px, 0, 50, 0);
    }
}

// ── 时间轴交互 ────────────────────────────────────────────────

void ChartEditorWidget::syncTimeline()
{
    QVector<GameNote> notes = collectNotes();

    // 诊断：检查 HOLD 数量
    int holdCount = 0;
    for (const GameNote& n : notes) {
        if (n.noteType == HOLD) ++holdCount;
    }
    m_infoLabel->setText(QStringLiteral("%1  |  BPM: %2  |  TAP: %3  HOLD: %4  |  时长: %5")
        .arg(m_songFileName)
        .arg(static_cast<int>(m_bpm))
        .arg(notes.size() - holdCount)
        .arg(holdCount)
        .arg(formatTime(m_songDurationMs)));

    m_timeline->setNotes(notes);
}

void ChartEditorWidget::scheduleSync()
{
    m_syncPending = true;
    if (!m_syncTimer->isActive()) {
        m_syncTimer->start();
    }
}

void ChartEditorWidget::applyZoom(int sliderValue)
{
    float factor = sliderValue / 10.0f;
    m_timeline->setZoomFactor(factor);
    m_zoomLabel->setText(QStringLiteral("%1x").arg(factor, 0, 'f', 1));
}

void ChartEditorWidget::onTimelineNoteAdded(qint64 timeMs, int lane)
{
    m_loading = true;
    int row = m_table->rowCount();
    m_table->insertRow(row);

    QTableWidgetItem* timeItem = new QTableWidgetItem;
    timeItem->setData(Qt::DisplayRole, QVariant(timeMs));
    timeItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 0, timeItem);

    QTableWidgetItem* laneItem = new QTableWidgetItem;
    laneItem->setData(Qt::DisplayRole, QVariant(lane));
    laneItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 1, laneItem);

    QTableWidgetItem* typeItem = new QTableWidgetItem;
    typeItem->setText(QStringLiteral("TAP"));
    typeItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 2, typeItem);

    QTableWidgetItem* durItem = new QTableWidgetItem;
    durItem->setData(Qt::DisplayRole, QVariant(static_cast<qint64>(0)));
    durItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 3, durItem);

    m_loading = false;

    sortTable();
    updateInfoLabel();
    scheduleSync();

    // 选中刚添加的音符
    for (int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* it = m_table->item(i, 0);
        if (it && it->data(Qt::DisplayRole).toLongLong() == timeMs) {
            m_table->selectRow(i);
            break;
        }
    }
}

void ChartEditorWidget::onTimelineNoteSelected(int noteIndex)
{
    if (noteIndex < 0 || noteIndex >= m_table->rowCount()) return;
    m_table->selectRow(noteIndex);
    m_timeline->setSelectedIndex(noteIndex);
}

void ChartEditorWidget::onTimelineNoteDeleted(int noteIndex)
{
    if (noteIndex < 0 || noteIndex >= m_table->rowCount()) return;
    m_table->removeRow(noteIndex);
    updateInfoLabel();
    scheduleSync();
}

void ChartEditorWidget::onTimelineHoldAdded(qint64 startTimeMs, qint64 durationMs, int lane)
{
    m_loading = true;

    // 删除同轨道上与新 HOLD 时间范围重叠的所有音符（TAP 和 HOLD）
    qint64 holdEnd = startTimeMs + durationMs;
    QVector<int> rowsToRemove;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* laneItem = m_table->item(i, 1);
        QTableWidgetItem* timeItem = m_table->item(i, 0);
        QTableWidgetItem* durItem  = m_table->item(i, 3);
        if (!laneItem || !timeItem) continue;
        if (laneItem->data(Qt::DisplayRole).toInt() != lane) continue;

        qint64 ts  = timeItem->data(Qt::DisplayRole).toLongLong();
        qint64 dur = durItem ? durItem->data(Qt::DisplayRole).toLongLong() : 0;
        qint64 noteEnd = ts + dur;

        // 时间范围重叠判断：[ts, noteEnd] 与 [startTimeMs, holdEnd] 有交集
        if (ts <= holdEnd && startTimeMs <= noteEnd) {
            rowsToRemove.append(i);
        }
    }
    // 从后往前删除，避免索引偏移
    for (int i = rowsToRemove.size() - 1; i >= 0; --i) {
        m_table->removeRow(rowsToRemove[i]);
    }

    // 插入 HOLD 音符行
    int row = m_table->rowCount();
    m_table->insertRow(row);

    QTableWidgetItem* timeItem = new QTableWidgetItem;
    timeItem->setData(Qt::DisplayRole, QVariant(startTimeMs));
    timeItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 0, timeItem);

    QTableWidgetItem* laneItem = new QTableWidgetItem;
    laneItem->setData(Qt::DisplayRole, QVariant(lane));
    laneItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 1, laneItem);

    QTableWidgetItem* typeItem = new QTableWidgetItem;
    typeItem->setText(QStringLiteral("HOLD"));
    typeItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 2, typeItem);

    QTableWidgetItem* durItem = new QTableWidgetItem;
    durItem->setData(Qt::DisplayRole, QVariant(durationMs));
    durItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 3, durItem);

    m_loading = false;

    sortTable();
    updateInfoLabel();
    scheduleSync();
}

void ChartEditorWidget::onKeyModeToggled()
{
    if (m_laneCount == 6) {
        m_laneCount = 4;
        m_keyModeBtn->setText(QStringLiteral("4K"));
    } else {
        m_laneCount = 6;
        m_keyModeBtn->setText(QStringLiteral("6K"));
    }

    // 更新时间轴的轨道数
    m_timeline->setLaneCount(m_laneCount);

    // 更新轨道列的 SpinBoxDelegate 范围
    m_table->setItemDelegateForColumn(1, new SpinBoxDelegate(0, m_laneCount - 1, this));

    // 重新同步时间轴显示
    syncTimeline();
}

// ── 键盘事件 ──────────────────────────────────────────────────

void ChartEditorWidget::keyPressEvent(QKeyEvent* event)
{
    // Space: 播放/暂停（仅在非表格编辑状态时）
    if (event->key() == Qt::Key_Space && !m_table->hasFocus()) {
        onPlayPause();
        return;
    }

    QWidget::keyPressEvent(event);
}

// ── 事件保护 ──────────────────────────────────────────────────

void ChartEditorWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_posTimer->start();

    // 初始高度同步（定时器首次触发前就要对齐）
    QTimer::singleShot(50, this, [this]() {
        if (m_timelineScroll) {
            int vpH = m_timelineScroll->viewport()->height();
            if (vpH > 100) {
                m_timeline->resize(m_timeline->width(), vpH);
            }
        }
    });
}

void ChartEditorWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_posTimer->stop();
    if (m_audio) m_audio->pause();
}
