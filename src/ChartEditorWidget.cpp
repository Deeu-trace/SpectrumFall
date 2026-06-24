#include "ChartEditorWidget.h"
#include "ChartManager.h"
#include "ChartTimelineWidget.h"
#include "AudioEngine.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QSpinBox>
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
#include <QModelIndexList>
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

// ── ChartEditorWidget 实现 ────────────────────────────────────

ChartEditorWidget::ChartEditorWidget(AudioEngine* audio, ChartManager* chartMgr, QWidget* parent)
    : QWidget(parent)
    , m_audio(audio)
    , m_chartMgr(chartMgr)
    , m_songFileSize(0)
    , m_songDurationMs(0)
    , m_bpm(0.0f)
    , m_loading(false)
{
    setupUI();
}

void ChartEditorWidget::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 30, 40, 30);
    layout->setSpacing(16);

    // ── 标题 ──
    QLabel* titleLabel = new QLabel(QStringLiteral("谱面编辑器"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(36);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #00ff88; background: transparent;");
    layout->addWidget(titleLabel);

    // ── 信息标签 ──
    m_infoLabel = new QLabel(this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setStyleSheet("color: #bd93f9; font-size: 16px; background: transparent;");
    layout->addWidget(m_infoLabel);

    // ── 位置标签 ──
    m_posLabel = new QLabel(this);
    m_posLabel->setAlignment(Qt::AlignCenter);
    m_posLabel->setStyleSheet("color: #8be9fd; font-size: 15px; background: transparent;");
    layout->addWidget(m_posLabel);

    // ── 可视化时间轴 ──
    m_timeline = new ChartTimelineWidget(this);
    m_timeline->setAudioEngine(m_audio);
    layout->addWidget(m_timeline, 0);

    // ── 表格 ──
    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("时间 (ms)") << QStringLiteral("轨道"));
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked);
    m_table->setAlternatingRowColors(true);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_table->setStyleSheet(
        "QTableWidget { background-color: #0d0d1f; alternate-background-color: #12122a; "
        "color: #e0e0e0; gridline-color: #1f1f3a; border: 1px solid #0f3460; border-radius: 8px; "
        "font-size: 15px; }"
        "QHeaderView::section { background-color: #16213e; color: #00ff88; "
        "font-weight: bold; padding: 6px; border: none; border-bottom: 1px solid #0f3460; }"
        "QTableWidget::item { padding: 4px 8px; }"
        "QTableWidget::item:selected { background-color: #0f3460; color: #ffffff; }");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 200);  // 时间列

    // 挂载 delegate：限制编辑输入范围，防止非法值
    m_table->setItemDelegateForColumn(0, new SpinBoxDelegate(0, 9999999, this));  // 时间
    m_table->setItemDelegateForColumn(1, new SpinBoxDelegate(0, 5, this));        // 轨道

    layout->addWidget(m_table, 1);

    // ── 按钮行 1：增删播放跳转 ──
    QHBoxLayout* btnRow1 = new QHBoxLayout();
    btnRow1->setSpacing(10);

    m_addBtn = new QPushButton(QStringLiteral("添加音符"), this);
    m_addBtn->setMinimumSize(110, 36);
    m_addBtn->setObjectName("actionButton");
    btnRow1->addWidget(m_addBtn);

    m_delBtn = new QPushButton(QStringLiteral("删除选中"), this);
    m_delBtn->setMinimumSize(110, 36);
    m_delBtn->setObjectName("actionButton");
    btnRow1->addWidget(m_delBtn);

    m_playBtn = new QPushButton(QStringLiteral("播放"), this);
    m_playBtn->setMinimumSize(110, 36);
    m_playBtn->setObjectName("actionButton");
    btnRow1->addWidget(m_playBtn);

    m_seekBtn = new QPushButton(QStringLiteral("跳到音符"), this);
    m_seekBtn->setMinimumSize(110, 36);
    m_seekBtn->setObjectName("actionButton");
    btnRow1->addWidget(m_seekBtn);

    btnRow1->addStretch();
    layout->addLayout(btnRow1);

    // ── 按钮行 2：保存游戏返回 ──
    QHBoxLayout* btnRow2 = new QHBoxLayout();
    btnRow2->setSpacing(10);

    m_saveBtn = new QPushButton(QStringLiteral("保存谱面"), this);
    m_saveBtn->setMinimumSize(110, 36);
    m_saveBtn->setObjectName("actionButton");
    btnRow2->addWidget(m_saveBtn);

    m_playGameBtn = new QPushButton(QStringLiteral("开始游戏"), this);
    m_playGameBtn->setMinimumSize(110, 36);
    m_playGameBtn->setObjectName("actionButton");
    btnRow2->addWidget(m_playGameBtn);

    btnRow2->addStretch();

    m_backBtn = new QPushButton(QStringLiteral("返回"), this);
    m_backBtn->setMinimumSize(100, 36);
    m_backBtn->setObjectName("backButton");
    btnRow2->addWidget(m_backBtn);

    layout->addLayout(btnRow2);

    // ── 位置轮询定时器 ──
    m_posTimer = new QTimer(this);
    m_posTimer->setInterval(50);
    connect(m_posTimer, &QTimer::timeout, this, &ChartEditorWidget::onPositionUpdate);

    // ── 连接信号 ──
    connect(m_addBtn, &QPushButton::clicked, this, &ChartEditorWidget::onAddNote);
    connect(m_delBtn, &QPushButton::clicked, this, &ChartEditorWidget::onDeleteNotes);
    connect(m_playBtn, &QPushButton::clicked, this, &ChartEditorWidget::onPlayPause);
    connect(m_seekBtn, &QPushButton::clicked, this, &ChartEditorWidget::onSeekToNote);
    connect(m_saveBtn, &QPushButton::clicked, this, &ChartEditorWidget::onSaveChart);
    connect(m_playGameBtn, &QPushButton::clicked, this, &ChartEditorWidget::onPlayGame);
    connect(m_backBtn, &QPushButton::clicked, this, &ChartEditorWidget::backRequested);
    connect(m_table, &QTableWidget::itemChanged, this, &ChartEditorWidget::onItemChanged);
    connect(m_timeline, &ChartTimelineWidget::noteAdded, this, &ChartEditorWidget::onTimelineNoteAdded);
    connect(m_timeline, &ChartTimelineWidget::noteSelected, this, &ChartEditorWidget::onTimelineNoteSelected);
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

        QTableWidgetItem* timeItem = new QTableWidgetItem;
        timeItem->setData(Qt::DisplayRole, QVariant(static_cast<int>(note.timestampMs)));
        timeItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, timeItem);

        QTableWidgetItem* laneItem = new QTableWidgetItem;
        laneItem->setData(Qt::DisplayRole, QVariant(note.lane));
        laneItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 1, laneItem);
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
        if (!timeItem || !laneItem) continue;

        qint64 ts = static_cast<qint64>(timeItem->data(Qt::DisplayRole).toInt());
        int lane = laneItem->data(Qt::DisplayRole).toInt();
        notes.append(GameNote(ts, lane));
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

void ChartEditorWidget::onAddNote()
{
    qint64 timeMs = 0;
    if (m_audio && m_audio->isLoaded()) {
        timeMs = m_audio->position();
    }

    int row = m_table->rowCount();
    m_loading = true;
    m_table->insertRow(row);

    QTableWidgetItem* timeItem = new QTableWidgetItem;
    timeItem->setData(Qt::DisplayRole, QVariant(static_cast<int>(timeMs)));
    timeItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 0, timeItem);

    QTableWidgetItem* laneItem = new QTableWidgetItem;
    laneItem->setData(Qt::DisplayRole, QVariant(0));
    laneItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 1, laneItem);
    m_loading = false;

    sortTable();
    updateInfoLabel();
    syncTimeline();

    // 选中刚添加的音符（排序后位置可能变了，按时间值查找）
    for (int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* it = m_table->item(i, 0);
        if (it && it->data(Qt::DisplayRole).toInt() == static_cast<int>(timeMs)) {
            m_table->selectRow(i);
            break;
        }
    }
}

void ChartEditorWidget::onDeleteNotes()
{
    QModelIndexList rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;

    // 从后往前删除，避免索引偏移
    std::sort(rows.begin(), rows.end(),
              [](const QModelIndex& a, const QModelIndex& b) {
                  return a.row() > b.row();
              });
    for (const QModelIndex& idx : rows) {
        m_table->removeRow(idx.row());
    }

    updateInfoLabel();
    syncTimeline();
}

void ChartEditorWidget::onPlayPause()
{
    if (!m_audio || !m_audio->isLoaded()) return;

    if (m_audio->state() == QMediaPlayer::PlayingState) {
        m_audio->pause();
        m_playBtn->setText(QStringLiteral("播放"));
    } else {
        m_audio->play();
        m_playBtn->setText(QStringLiteral("暂停"));
    }
}

void ChartEditorWidget::onSeekToNote()
{
    int row = m_table->currentRow();
    if (row < 0) return;

    QTableWidgetItem* timeItem = m_table->item(row, 0);
    if (!timeItem) return;

    qint64 timeMs = static_cast<qint64>(timeItem->data(Qt::DisplayRole).toInt());
    if (m_audio && m_audio->isLoaded()) {
        m_audio->seek(timeMs);
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
    entry.laneCount      = 6;
    entry.editedAt       = QDateTime::currentDateTime();

    entry.notes.reserve(notes.size());
    for (const GameNote& note : notes) {
        entry.notes.append({note.timestampMs, note.lane});
    }

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

    // 时间列被修改后重新排序
    if (item->column() == 0) {
        sortTable();
    }
    syncTimeline();
}

void ChartEditorWidget::onPositionUpdate()
{
    if (!m_audio || !m_audio->isLoaded()) return;

    qint64 pos = m_audio->position();
    m_posLabel->setText(formatTime(pos) + QStringLiteral(" / ") + formatTime(m_songDurationMs));
    m_timeline->setPositionMs(pos);

    // 同步播放按钮文字（处理自动停止的情况）
    if (m_audio->state() == QMediaPlayer::PlayingState) {
        m_playBtn->setText(QStringLiteral("暂停"));
    } else {
        m_playBtn->setText(QStringLiteral("播放"));
    }
}

// ── 时间轴交互 ────────────────────────────────────────────────

void ChartEditorWidget::syncTimeline()
{
    m_timeline->setNotes(collectNotes());
}

void ChartEditorWidget::onTimelineNoteAdded(qint64 timeMs, int lane)
{
    m_loading = true;
    int row = m_table->rowCount();
    m_table->insertRow(row);

    QTableWidgetItem* timeItem = new QTableWidgetItem;
    timeItem->setData(Qt::DisplayRole, QVariant(static_cast<int>(timeMs)));
    timeItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 0, timeItem);

    QTableWidgetItem* laneItem = new QTableWidgetItem;
    laneItem->setData(Qt::DisplayRole, QVariant(lane));
    laneItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, 1, laneItem);
    m_loading = false;

    sortTable();
    updateInfoLabel();
    syncTimeline();

    // 选中刚添加的音符
    for (int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* it = m_table->item(i, 0);
        if (it && it->data(Qt::DisplayRole).toInt() == static_cast<int>(timeMs)) {
            m_table->selectRow(i);
            break;
        }
    }
}

void ChartEditorWidget::onTimelineNoteSelected(int noteIndex)
{
    if (noteIndex < 0 || noteIndex >= m_table->rowCount()) return;
    m_table->selectRow(noteIndex);
}

// ── 事件保护 ──────────────────────────────────────────────────

void ChartEditorWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_posTimer->start();
}

void ChartEditorWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_posTimer->stop();
    if (m_audio) m_audio->pause();
    m_playBtn->setText(QStringLiteral("播放"));
}
