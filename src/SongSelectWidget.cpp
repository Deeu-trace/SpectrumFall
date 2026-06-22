#include "SongSelectWidget.h"
#include "CacheManager.h"
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QProgressBar>
#include <QPainter>
#include <QTimer>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMenu>
#include <QDialog>
#include <QApplication>
#include <cmath>

// ── 转圈加载动画组件（无 Q_OBJECT，仅供 SongSelectWidget 内部使用）──
class SpinnerWidget : public QWidget
{
public:
    explicit SpinnerWidget(QWidget* parent = nullptr)
        : QWidget(parent), m_angle(0)
    {
        setFixedSize(28, 28);
        m_timer.setInterval(40);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_angle = (m_angle + 15) % 360;
            update();
        });
    }

    void start() { m_timer.start(); show(); }
    void stop()  { m_timer.stop(); m_angle = 0; hide(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::transparent);

        float cx = width()  / 2.0f;
        float cy = height() / 2.0f;
        float r  = qMin(cx, cy) - 3.0f;

        QPen pen(QColor(0, 255, 136), 2.5f);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(cx - r, cy - r, r * 2, r * 2),
                   m_angle * 16, 270 * 16);
    }

private:
    int m_angle;
    QTimer m_timer;
};

// ── SongSelectWidget 实现 ────────────────────────────────────

SongSelectWidget::SongSelectWidget(QWidget* parent)
    : QWidget(parent)
    , m_analysisDone(false)
    , m_spinner(nullptr)
    , m_spinnerDialog(nullptr)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(30, 30, 30, 30);

    // ── 标题 ──
    QLabel* titleLabel = new QLabel(QStringLiteral("选择歌曲"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(32);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #00ff88; background: transparent;");
    layout->addWidget(titleLabel);

    // ── 历史记录区域 ──
    m_historyTitle = new QLabel(QStringLiteral("历史记录"), this);
    m_historyTitle->setStyleSheet("color: #bd93f9; font-size: 16px; font-weight: bold; background: transparent;");
    layout->addWidget(m_historyTitle);

    m_historyList = new QListWidget(this);
    m_historyList->setObjectName("historyList");
    m_historyList->setMaximumHeight(200);
    m_historyList->setMinimumHeight(60);
    m_historyList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_historyList);

    layout->addSpacing(5);

    layout->addStretch(1);

    // ── 选择文件按钮 ──
    m_selectFileBtn = new QPushButton(QStringLiteral("选择本地歌曲"), this);
    m_selectFileBtn->setMinimumSize(260, 55);
    m_selectFileBtn->setObjectName("menuButton");
    layout->addWidget(m_selectFileBtn, 0, Qt::AlignCenter);

    QLabel* formatHint = new QLabel(
        QStringLiteral("支持 MP3、WAV、FLAC 格式音频文件"), this);
    formatHint->setAlignment(Qt::AlignCenter);
    formatHint->setStyleSheet("color: #888888; font-size: 14px; background: transparent;");
    layout->addWidget(formatHint, 0, Qt::AlignCenter);

    layout->addSpacing(10);

    // ── 文件信息 + 分析区域（初始隐藏）──
    m_fileInfoPanel = new QWidget(this);
    m_fileInfoPanel->setObjectName("statsPanel");
    QVBoxLayout* infoLayout = new QVBoxLayout(m_fileInfoPanel);
    infoLayout->setSpacing(8);
    infoLayout->setContentsMargins(20, 16, 20, 16);

    m_fileNameLabel = new QLabel(this);
    m_fileNameLabel->setStyleSheet("color: #e0e0e0; font-size: 16px; font-weight: bold; background: transparent;");
    infoLayout->addWidget(m_fileNameLabel);

    m_fileSizeLabel = new QLabel(this);
    m_fileSizeLabel->setStyleSheet("color: #aaaaaa; font-size: 14px; background: transparent;");
    infoLayout->addWidget(m_fileSizeLabel);

    m_durationLabel = new QLabel(QStringLiteral("时长: --"), this);
    m_durationLabel->setStyleSheet("color: #aaaaaa; font-size: 14px; background: transparent;");
    infoLayout->addWidget(m_durationLabel);

    infoLayout->addSpacing(8);

    // 分析按钮行（Spinner + 按钮）
    QHBoxLayout* analyzeRow = new QHBoxLayout();
    analyzeRow->setAlignment(Qt::AlignCenter);

    SpinnerWidget* spinner = new SpinnerWidget(m_fileInfoPanel);
    spinner->setObjectName("analysisSpinner");
    spinner->hide();
    m_spinner = spinner;
    analyzeRow->addWidget(spinner);

    m_analyzeBtn = new QPushButton(QStringLiteral("开始分析BPM并生成谱面"), this);
    m_analyzeBtn->setMinimumSize(260, 45);
    m_analyzeBtn->setObjectName("actionButton");
    analyzeRow->addWidget(m_analyzeBtn);
    analyzeRow->addStretch();

    infoLayout->addLayout(analyzeRow);

    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    m_progressBar->setFixedHeight(20);
    m_progressBar->hide();
    infoLayout->addWidget(m_progressBar);

    // BPM 显示
    m_bpmLabel = new QLabel(QStringLiteral("BPM: --"), this);
    m_bpmLabel->setStyleSheet("color: #bd93f9; font-size: 18px; font-weight: bold; background: transparent;");
    infoLayout->addWidget(m_bpmLabel);

    // 错误信息
    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setStyleSheet("color: #e94560; font-size: 14px; background: transparent;");
    m_errorLabel->hide();
    infoLayout->addWidget(m_errorLabel);

    m_fileInfoPanel->hide();
    layout->addWidget(m_fileInfoPanel);

    layout->addStretch(1);

    // ── 底部按钮行 ──
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(15);

    m_visualizeBtn = new QPushButton(QStringLiteral("可视化模式"), this);
    m_visualizeBtn->setMinimumSize(150, 40);
    m_visualizeBtn->setEnabled(false);
    m_visualizeBtn->setObjectName("actionButton");
    btnLayout->addWidget(m_visualizeBtn);

    m_gameBtn = new QPushButton(QStringLiteral("开始游戏"), this);
    m_gameBtn->setMinimumSize(150, 40);
    m_gameBtn->setEnabled(false);
    m_gameBtn->setObjectName("actionButton");
    btnLayout->addWidget(m_gameBtn);

    btnLayout->addStretch();

    m_backBtn = new QPushButton(QStringLiteral("返回"), this);
    m_backBtn->setMinimumSize(100, 40);
    m_backBtn->setObjectName("backButton");
    btnLayout->addWidget(m_backBtn);

    layout->addLayout(btnLayout);

    // ── 连接信号 ──
    connect(m_selectFileBtn, &QPushButton::clicked,
            this, &SongSelectWidget::onSelectFileClicked);
    connect(m_analyzeBtn,   &QPushButton::clicked,
            this, &SongSelectWidget::onAnalyzeClicked);
    connect(m_visualizeBtn, &QPushButton::clicked,
            this, &SongSelectWidget::onVisualizeClicked);
    connect(m_gameBtn,      &QPushButton::clicked,
            this, &SongSelectWidget::onGameClicked);
    connect(m_backBtn,      &QPushButton::clicked,
            this, &SongSelectWidget::backRequested);

    // 历史记录点击
    connect(m_historyList, &QListWidget::itemClicked,
            this, &SongSelectWidget::onHistoryItemClicked);

    // 历史记录右键菜单
    m_historyList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_historyMenu = new QMenu(this);
    m_historyMenu->setStyleSheet("QMenu { background-color: #16213e; color: #e0e0e0; border: 1px solid #0f3460; }"
                                  "QMenu::item:selected { background-color: #0f3460; color: #00ff88; }");
    QAction* deleteAction = m_historyMenu->addAction(QStringLiteral("删除此记录"));
    connect(deleteAction, &QAction::triggered, this, &SongSelectWidget::onHistoryDeleteClicked);
    connect(m_historyList, &QListWidget::customContextMenuRequested,
            this, &SongSelectWidget::onHistoryContextMenu);
}

// ── 公共接口 ──────────────────────────────────────────────────

QString SongSelectWidget::selectedSong() const
{
    return m_selectedPath;
}

void SongSelectWidget::setAnalysisProgress(int percent)
{
    m_progressBar->setValue(qBound(0, percent, 100));
}

void SongSelectWidget::setDurationDisplay(qint64 ms)
{
    m_durationLabel->setText(QStringLiteral("时长: %1").arg(formatDuration(ms)));
}

void SongSelectWidget::onAnalysisComplete(float bpm)
{
    m_analysisDone = true;
    exitAnalyzingState();

    m_bpmLabel->setText(QStringLiteral("BPM: %1").arg(static_cast<int>(bpm)));
    m_errorLabel->hide();
    m_progressBar->hide();

    m_visualizeBtn->setEnabled(true);
    m_gameBtn->setEnabled(true);
    m_selectFileBtn->setText(QStringLiteral("重新选择文件"));
    m_selectFileBtn->setEnabled(true);
    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(QStringLiteral("重新分析"));
}

void SongSelectWidget::showAnalysisError(const QString& message)
{
    exitAnalyzingState();

    m_errorLabel->setText(message);
    m_errorLabel->show();
    m_progressBar->hide();
    m_bpmLabel->setText(QStringLiteral("BPM: --"));

    m_selectFileBtn->setEnabled(true);
    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(QStringLiteral("重新分析"));
}

void SongSelectWidget::resetState()
{
    m_selectedPath.clear();
    m_analysisDone = false;

    m_fileInfoPanel->hide();
    m_errorLabel->hide();
    m_progressBar->hide();
    m_bpmLabel->setText(QStringLiteral("BPM: --"));
    m_durationLabel->setText(QStringLiteral("时长: --"));
    m_fileNameLabel->clear();
    m_fileSizeLabel->clear();

    m_selectFileBtn->setText(QStringLiteral("选择本地歌曲"));
    m_selectFileBtn->setEnabled(true);
    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(QStringLiteral("开始分析BPM并生成谱面"));
    m_visualizeBtn->setEnabled(false);
    m_gameBtn->setEnabled(false);
}

void SongSelectWidget::refreshHistory(const QVector<CacheEntry>& entries)
{
    m_historyList->clear();

    if (entries.isEmpty()) {
        QListWidgetItem* emptyItem = new QListWidgetItem(
            QStringLiteral("  暂无历史记录，选择歌曲分析后将自动保存"), m_historyList);
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        // 存储空标记
        emptyItem->setData(Qt::UserRole, QString());
        return;
    }

    for (const CacheEntry& entry : entries) {
        // 构建显示文本：歌曲名 | BPM | 时长 | 分析时间
        QString text = QStringLiteral("%1  |  BPM: %2  |  %3  |  %4")
            .arg(entry.fileName)
            .arg(static_cast<int>(entry.bpm))
            .arg(formatDuration(entry.durationMs))
            .arg(formatDateTime(entry.analyzedAt));

        QListWidgetItem* item = new QListWidgetItem(text, m_historyList);
        item->setData(Qt::UserRole, entry.filePath);  // 存储路径用于点击/删除
        item->setData(Qt::UserRole + 1, static_cast<double>(entry.fileSize)); // 文件大小

        // 如果文件已不存在，显示为灰色
        if (!QFileInfo::exists(entry.filePath)) {
            item->setForeground(QColor(100, 100, 100));
            item->setText(text + QStringLiteral("  [文件缺失]"));
        }
    }
}

void SongSelectWidget::loadFromCache(const QString& filePath, float bpm, qint64 durationMs)
{
    m_selectedPath = filePath;
    m_analysisDone = true;

    QFileInfo fi(filePath);
    m_fileNameLabel->setText(fi.fileName());
    m_fileSizeLabel->setText(formatFileSize(fi.size()));
    m_durationLabel->setText(QStringLiteral("时长: %1").arg(formatDuration(durationMs)));
    m_bpmLabel->setText(QStringLiteral("BPM: %1").arg(static_cast<int>(bpm)));
    m_errorLabel->hide();
    m_progressBar->hide();

    m_selectFileBtn->setText(QStringLiteral("重新选择文件"));
    m_selectFileBtn->setEnabled(true);
    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(QStringLiteral("重新分析"));
    m_visualizeBtn->setEnabled(true);
    m_gameBtn->setEnabled(true);

    m_fileInfoPanel->show();
}

void SongSelectWidget::showLoadingState()
{
    // 创建一个独立的加载对话框（非模态，避免阻塞）
    // 注意：主线程随后会被 loadFile() 阻塞，所以不使用需要事件循环的动画组件
    QDialog* dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("加载中"));
    dlg->setFixedSize(200, 100);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setStyleSheet("QDialog { background-color: #1a1a2e; border: 1px solid #0f3460; border-radius: 8px; }");
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(dlg);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(8);

    QLabel* label = new QLabel(QStringLiteral("加载中..."), dlg);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("color: #00ff88; font-size: 16px; font-weight: bold; background: transparent;");
    layout->addWidget(label);

    dlg->show();
    dlg->raise();

    // 存储指针供 hideLoadingState 使用
    m_spinner = nullptr;
    m_spinnerDialog = dlg;

    // 禁用底层交互
    m_historyList->setEnabled(false);
    m_backBtn->setEnabled(false);
}

void SongSelectWidget::hideLoadingState()
{
    if (m_spinnerDialog) {
        m_spinnerDialog->close();  // WA_DeleteOnClose 会自动删除
        m_spinnerDialog = nullptr;
    }

    m_backBtn->setEnabled(true);
    m_historyList->setEnabled(true);
}

// ── 私有槽 ────────────────────────────────────────────────────

void SongSelectWidget::onSelectFileClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择音频文件"),
        QString(),
        QStringLiteral("音频文件 (*.mp3 *.wav *.flac);;所有文件 (*.*)")
    );

    if (path.isEmpty()) return;

    m_selectedPath = path;
    m_analysisDone = false;

    QFileInfo fi(path);
    m_fileNameLabel->setText(fi.fileName());
    m_fileSizeLabel->setText(formatFileSize(fi.size()));
    m_durationLabel->setText(QStringLiteral("时长: --"));
    m_bpmLabel->setText(QStringLiteral("BPM: --"));
    m_errorLabel->hide();
    m_progressBar->hide();

    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(QStringLiteral("开始分析BPM并生成谱面"));
    m_visualizeBtn->setEnabled(false);
    m_gameBtn->setEnabled(false);

    m_fileInfoPanel->show();
    m_selectFileBtn->setText(QStringLiteral("重新选择文件"));
}

void SongSelectWidget::onAnalyzeClicked()
{
    if (m_selectedPath.isEmpty()) return;
    enterAnalyzingState();
    emit analyzeRequested(m_selectedPath);
}

void SongSelectWidget::onVisualizeClicked()
{
    if (m_analysisDone && !m_selectedPath.isEmpty()) {
        emit visualizeRequested();
    }
}

void SongSelectWidget::onGameClicked()
{
    if (m_analysisDone && !m_selectedPath.isEmpty()) {
        emit gameRequested();
    }
}

void SongSelectWidget::onHistoryItemClicked(QListWidgetItem* item)
{
    if (!item) return;

    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty()) return; // 空占位项

    // 检查文件是否存在
    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, QStringLiteral("文件不存在"),
            QStringLiteral("音频文件已被移动或删除，请重新选择文件"));
        return;
    }

    emit historySelected(filePath);
}

void SongSelectWidget::onHistoryDeleteClicked()
{
    // 找到当前选中的项
    QListWidgetItem* item = m_historyList->currentItem();
    if (!item) return;

    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty()) return;

    emit historyDeleteRequested(filePath);
}

void SongSelectWidget::onHistoryContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_historyList->itemAt(pos);
    if (!item) return;

    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty()) return; // 空占位项

    // 选中该项并弹出右键菜单
    m_historyList->setCurrentItem(item);
    m_historyMenu->exec(m_historyList->mapToGlobal(pos));
}

// ── 私有方法 ──────────────────────────────────────────────────

void SongSelectWidget::enterAnalyzingState()
{
    m_analyzeBtn->setEnabled(false);
    m_analyzeBtn->setText(QStringLiteral("正在分析节拍..."));
    m_selectFileBtn->setEnabled(false);
    m_visualizeBtn->setEnabled(false);
    m_gameBtn->setEnabled(false);
    m_errorLabel->hide();

    m_progressBar->setValue(0);
    m_progressBar->show();

    if (m_spinner) m_spinner->start();
}

void SongSelectWidget::exitAnalyzingState()
{
    if (m_spinner) m_spinner->stop();
}

QString SongSelectWidget::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

QString SongSelectWidget::formatDuration(qint64 ms) const
{
    qint64 totalSec = ms / 1000;
    qint64 min = totalSec / 60;
    qint64 sec = totalSec % 60;
    if (min >= 60) {
        qint64 hr = min / 60;
        min = min % 60;
        return QStringLiteral("%1:%2:%3")
            .arg(hr).arg(min, 2, 10, QLatin1Char('0')).arg(sec, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(min).arg(sec, 2, 10, QLatin1Char('0'));
}

QString SongSelectWidget::formatDateTime(const QDateTime& dt) const
{
    if (!dt.isValid()) return QStringLiteral("--");
    // 今天的只显示时间，其他显示日期+时间
    QDateTime now = QDateTime::currentDateTime();
    if (dt.date() == now.date()) {
        return dt.toString(QStringLiteral("HH:mm"));
    }
    // 一年内的显示 月-日 时:分
    if (dt.date().year() == now.date().year()) {
        return dt.toString(QStringLiteral("MM-dd HH:mm"));
    }
    return dt.toString(QStringLiteral("yyyy-MM-dd"));
}
