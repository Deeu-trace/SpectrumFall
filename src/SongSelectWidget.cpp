#include "SongSelectWidget.h"
#include "CacheManager.h"
#include "LeaderboardManager.h"
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
#include <QSplitter>
#include <QFrame>
#include <cmath>

// -- SpinnerWidget (no Q_OBJECT, internal to SongSelectWidget) --
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

// == SongSelectWidget Implementation ================================

static const char* kCardStyle =
    "background: rgba(22, 33, 62, 160);"
    " border: 1px solid #0f3460; border-radius: 12px;";

SongSelectWidget::SongSelectWidget(QWidget* parent)
    : QWidget(parent)
    , m_analysisDone(false)
    , m_spinner(nullptr)
    , m_leaderboardMgr(nullptr)
    , m_laneCount(6)
    , m_spinnerDialog(nullptr)
{
    // -- Splitter: left | right --
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(2);
    m_splitter->setStyleSheet("QSplitter::handle { background: #0f3460; }");

    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(m_splitter);

    // ================================================================
    //  LEFT PANEL  (history + select file at bottom)
    // ================================================================
    m_leftPanel = new QWidget(m_splitter);
    m_leftPanel->setMinimumWidth(280);
    QVBoxLayout* leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(20, 20, 10, 20);
    leftLayout->setSpacing(10);

    // -- History title --
    m_historyTitle = new QLabel(QString::fromUtf8("\xe5\x8e\x86\xe5\x8f\xb2\xe8\xae\xb0\xe5\xbd\x95"), m_leftPanel);
    m_historyTitle->setStyleSheet("color: #bd93f9; font-size: 16px; font-weight: bold; background: transparent;");
    leftLayout->addWidget(m_historyTitle);

    // -- History list --
    m_historyList = new QListWidget(m_leftPanel);
    m_historyList->setObjectName("historyList");
    m_historyList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftLayout->addWidget(m_historyList, 1);

    // History context menu
    m_historyList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_historyMenu = new QMenu(this);
    m_historyMenu->setStyleSheet(
        "QMenu { background-color: #16213e; color: #e0e0e0; border: 1px solid #0f3460; }"
        "QMenu::item:selected { background-color: #0f3460; color: #00ff88; }");
    QAction* deleteAction = m_historyMenu->addAction(QString::fromUtf8("\xe5\x88\xa0\xe9\x99\xa4\xe6\xad\xa4\xe8\xae\xb0\xe5\xbd\x95"));
    connect(deleteAction, &QAction::triggered, this, &SongSelectWidget::onHistoryDeleteClicked);
    connect(m_historyList, &QListWidget::customContextMenuRequested,
            this, &SongSelectWidget::onHistoryContextMenu);

    // -- Select file button (at bottom) --
    m_selectFileBtn = new QPushButton(QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe6\x9c\xac\xe5\x9c\xb0\xe6\xad\x8c\xe6\x9b\xb2"), m_leftPanel);
    m_selectFileBtn->setObjectName("menuButton");
    m_selectFileBtn->setFixedHeight(45);
    leftLayout->addWidget(m_selectFileBtn);

    m_splitter->addWidget(m_leftPanel);

    // ================================================================
    //  RIGHT PANEL  (placeholder + 3 cards + action buttons)
    // ================================================================
    m_rightPanel = new QWidget(m_splitter);
    m_rightPanel->setMinimumWidth(400);
    QVBoxLayout* rightLayout = new QVBoxLayout(m_rightPanel);
    rightLayout->setContentsMargins(10, 20, 20, 20);
    rightLayout->setSpacing(12);

    // Placeholder (visible when no file selected)
    m_placeholderLabel = new QLabel(
        QString::fromUtf8("\xe8\xaf\xb7\xe4\xbb\x8e\xe5\xb7\xa6\xe4\xbe\xa7\xe9\x80\x89\xe6\x8b\xa9\xe6\xad\x8c\xe6\x9b\xb2"),
        m_rightPanel);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setStyleSheet(
        "color: #666666; font-size: 20px; background: transparent;");
    rightLayout->addWidget(m_placeholderLabel);

    // -- Cards container --
    m_cardsContainer = new QWidget(m_rightPanel);
    QVBoxLayout* cardsLayout = new QVBoxLayout(m_cardsContainer);
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setSpacing(10);

    // ================================================================
    //  CARD 1: File info (name, size, duration, BPM, analyze)
    // ================================================================
    m_infoCard = new QFrame(m_cardsContainer);
    m_infoCard->setObjectName("infoCard");
    m_infoCard->setFrameShape(QFrame::NoFrame);
    m_infoCard->setStyleSheet(QStringLiteral("#infoCard { %1 }").arg(kCardStyle));

    QVBoxLayout* infoLayout = new QVBoxLayout(m_infoCard);
    infoLayout->setSpacing(6);
    infoLayout->setContentsMargins(28, 22, 28, 22);

    m_fileNameLabel = new QLabel(m_infoCard);
    m_fileNameLabel->setStyleSheet("color: #e0e0e0; font-size: 20px; font-weight: bold; background: transparent;");
    infoLayout->addWidget(m_fileNameLabel);

    m_fileSizeLabel = new QLabel(m_infoCard);
    m_fileSizeLabel->setStyleSheet("color: #aaaaaa; font-size: 14px; background: transparent;");
    infoLayout->addWidget(m_fileSizeLabel);

    m_durationLabel = new QLabel(QString::fromUtf8("\xe6\x97\xb6\xe9\x95\xbf: --"), m_infoCard);
    m_durationLabel->setStyleSheet("color: #aaaaaa; font-size: 14px; background: transparent;");
    infoLayout->addWidget(m_durationLabel);

    m_bpmLabel = new QLabel(QStringLiteral("BPM: --"), m_infoCard);
    m_bpmLabel->setStyleSheet("color: #bd93f9; font-size: 18px; font-weight: bold; background: transparent;");
    infoLayout->addWidget(m_bpmLabel);

    infoLayout->addSpacing(8);

    // Analyze button row
    QHBoxLayout* analyzeRow = new QHBoxLayout();
    analyzeRow->setAlignment(Qt::AlignLeft);

    SpinnerWidget* spinner = new SpinnerWidget(m_infoCard);
    spinner->setObjectName("analysisSpinner");
    spinner->hide();
    m_spinner = spinner;
    analyzeRow->addWidget(spinner);

    m_analyzeBtn = new QPushButton(
        QString::fromUtf8("\xe5\xbc\x80\xe5\xa7\x8b\xe5\x88\x86\xe6\x9e\x90""BPM\xe5\xb9\xb6\xe7\x94\x9f\xe6\x88\x90\xe8\xb0\xb1\xe9\x9d\xa2"),
        m_infoCard);
    m_analyzeBtn->setMinimumSize(260, 45);
    m_analyzeBtn->setObjectName("actionButton");
    analyzeRow->addWidget(m_analyzeBtn);
    analyzeRow->addStretch();

    infoLayout->addLayout(analyzeRow);

    // Progress bar
    m_progressBar = new QProgressBar(m_infoCard);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    m_progressBar->setFixedHeight(20);
    m_progressBar->hide();
    infoLayout->addWidget(m_progressBar);

    // Error message
    m_errorLabel = new QLabel(m_infoCard);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setStyleSheet("color: #e94560; font-size: 14px; background: transparent;");
    m_errorLabel->hide();
    infoLayout->addWidget(m_errorLabel);

    cardsLayout->addWidget(m_infoCard);

    // ================================================================
    //  CARD 2: Score history
    // ================================================================
    m_scoreCard = new QFrame(m_cardsContainer);
    m_scoreCard->setObjectName("scoreCard");
    m_scoreCard->setFrameShape(QFrame::NoFrame);
    m_scoreCard->setStyleSheet(QStringLiteral("#scoreCard { %1 }").arg(kCardStyle));

    QVBoxLayout* scoreLayout = new QVBoxLayout(m_scoreCard);
    scoreLayout->setSpacing(4);
    scoreLayout->setContentsMargins(28, 18, 28, 18);

    m_scoreHistoryTitle = new QLabel(
        QString::fromUtf8("\xe5\x8e\x86\xe5\x8f\xb2\xe6\x88\x90\xe7\xbb\xa9"),
        m_scoreCard);
    m_scoreHistoryTitle->setStyleSheet(
        "color: #bd93f9; font-size: 14px; font-weight: bold; background: transparent;");
    scoreLayout->addWidget(m_scoreHistoryTitle);

    m_scoreHist1 = new QLabel(m_scoreCard);
    m_scoreHist1->setStyleSheet("color: #aaaaaa; font-size: 13px; background: transparent;");
    scoreLayout->addWidget(m_scoreHist1);

    m_scoreHist2 = new QLabel(m_scoreCard);
    m_scoreHist2->setStyleSheet("color: #aaaaaa; font-size: 13px; background: transparent;");
    scoreLayout->addWidget(m_scoreHist2);

    m_scoreHist3 = new QLabel(m_scoreCard);
    m_scoreHist3->setStyleSheet("color: #aaaaaa; font-size: 13px; background: transparent;");
    scoreLayout->addWidget(m_scoreHist3);

    cardsLayout->addWidget(m_scoreCard);

    // ================================================================
    //  CARD 3: Game mode (4K / 6K)
    // ================================================================
    m_modeCard = new QFrame(m_cardsContainer);
    m_modeCard->setObjectName("modeCard");
    m_modeCard->setFrameShape(QFrame::NoFrame);
    m_modeCard->setStyleSheet(QStringLiteral("#modeCard { %1 }").arg(kCardStyle));

    QHBoxLayout* modeLayout = new QHBoxLayout(m_modeCard);
    modeLayout->setSpacing(8);
    modeLayout->setContentsMargins(28, 16, 28, 16);

    QLabel* modeLabel = new QLabel(
        QString::fromUtf8("\xe6\xb8\xb8\xe6\x88\x8f\xe6\xa8\xa1\xe5\xbc\x8f"),
        m_modeCard);
    modeLabel->setStyleSheet(
        "color: #e0e0e0; font-size: 15px; font-weight: bold; background: transparent;");
    modeLayout->addWidget(modeLabel);

    modeLayout->addSpacing(12);

    m_4kBtn = new QPushButton(QStringLiteral("4K"), m_modeCard);
    m_4kBtn->setMinimumSize(70, 34);
    m_4kBtn->setObjectName("modeBtn4K");
    modeLayout->addWidget(m_4kBtn);

    m_6kBtn = new QPushButton(QStringLiteral("6K"), m_modeCard);
    m_6kBtn->setMinimumSize(70, 34);
    m_6kBtn->setObjectName("modeBtn6K");
    modeLayout->addWidget(m_6kBtn);

    modeLayout->addStretch();

    cardsLayout->addWidget(m_modeCard);

    m_cardsContainer->hide();
    rightLayout->addWidget(m_cardsContainer);
    rightLayout->addStretch(1);

    // -- Bottom action button row --
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(15);

    m_visualizeBtn = new QPushButton(QString::fromUtf8("\xe5\x8f\xaf\xe8\xa7\x86\xe5\x8c\x96\xe6\xa8\xa1\xe5\xbc\x8f"), m_rightPanel);
    m_visualizeBtn->setMinimumSize(150, 40);
    m_visualizeBtn->setEnabled(false);
    m_visualizeBtn->setObjectName("actionButton");
    btnLayout->addWidget(m_visualizeBtn);

    m_chartEditBtn = new QPushButton(QString::fromUtf8("\xe7\xbc\x96\xe8\xbe\x91\xe8\xb0\xb1\xe9\x9d\xa2"), m_rightPanel);
    m_chartEditBtn->setMinimumSize(150, 40);
    m_chartEditBtn->setEnabled(false);
    m_chartEditBtn->setObjectName("actionButton");
    btnLayout->addWidget(m_chartEditBtn);

    m_gameBtn = new QPushButton(QString::fromUtf8("\xe5\xbc\x80\xe5\xa7\x8b\xe6\xb8\xb8\xe6\x88\x8f"), m_rightPanel);
    m_gameBtn->setMinimumSize(150, 40);
    m_gameBtn->setEnabled(false);
    m_gameBtn->setObjectName("actionButton");
    btnLayout->addWidget(m_gameBtn);

    btnLayout->addStretch();

    m_backBtn = new QPushButton(QString::fromUtf8("\xe8\xbf\x94\xe5\x9b\x9e"), m_rightPanel);
    m_backBtn->setMinimumSize(100, 40);
    m_backBtn->setObjectName("backButton");
    btnLayout->addWidget(m_backBtn);

    rightLayout->addLayout(btnLayout);

    m_splitter->addWidget(m_rightPanel);

    // -- Splitter initial sizes (4:6) --
    m_splitter->setSizes({360, 540});

    // ================================================================
    //  Signal connections
    // ================================================================
    connect(m_selectFileBtn, &QPushButton::clicked,
            this, &SongSelectWidget::onSelectFileClicked);
    connect(m_analyzeBtn, &QPushButton::clicked,
            this, &SongSelectWidget::onAnalyzeClicked);
    connect(m_visualizeBtn, &QPushButton::clicked,
            this, &SongSelectWidget::onVisualizeClicked);
    connect(m_gameBtn, &QPushButton::clicked,
            this, &SongSelectWidget::onGameClicked);
    connect(m_chartEditBtn, &QPushButton::clicked,
            this, &SongSelectWidget::chartEditRequested);
    connect(m_backBtn, &QPushButton::clicked,
            this, &SongSelectWidget::backRequested);
    connect(m_historyList, &QListWidget::itemClicked,
            this, &SongSelectWidget::onHistoryItemClicked);

    // Lane count buttons
    connect(m_4kBtn, &QPushButton::clicked,
            this, &SongSelectWidget::onLaneToggled);
    connect(m_6kBtn, &QPushButton::clicked,
            this, &SongSelectWidget::onLaneToggled);

    updateLaneButtons();
}

// == Public interface ================================================

QString SongSelectWidget::selectedSong() const
{
    return m_selectedPath;
}

int SongSelectWidget::selectedLaneCount() const
{
    return m_laneCount;
}

void SongSelectWidget::setAnalysisProgress(int percent)
{
    m_progressBar->setValue(qBound(0, percent, 100));
}

void SongSelectWidget::setDurationDisplay(qint64 ms)
{
    m_durationLabel->setText(
        QString::fromUtf8("\xe6\x97\xb6\xe9\x95\xbf: %1").arg(formatDuration(ms)));
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
    m_chartEditBtn->setEnabled(true);

    updateLaneButtons();
    updateScoreHistory();

    m_selectFileBtn->setText(QString::fromUtf8("\xe9\x87\x8d\xe6\x96\xb0\xe9\x80\x89\xe6\x8b\xa9\xe6\x96\x87\xe4\xbb\xb6"));
    m_selectFileBtn->setEnabled(true);
    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(QString::fromUtf8("\xe9\x87\x8d\xe6\x96\xb0\xe5\x88\x86\xe6\x9e\x90"));
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
    m_analyzeBtn->setText(QString::fromUtf8("\xe9\x87\x8d\xe6\x96\xb0\xe5\x88\x86\xe6\x9e\x90"));
}

void SongSelectWidget::resetState()
{
    m_selectedPath.clear();
    m_analysisDone = false;

    m_laneCount = 6;
    updateLaneButtons();

    m_placeholderLabel->show();
    m_cardsContainer->hide();
    m_errorLabel->hide();
    m_progressBar->hide();
    m_bpmLabel->setText(QStringLiteral("BPM: --"));
    m_durationLabel->setText(QString::fromUtf8("\xe6\x97\xb6\xe9\x95\xbf: --"));
    m_fileNameLabel->clear();
    m_fileSizeLabel->clear();

    m_selectFileBtn->setText(QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe6\x9c\xac\xe5\x9c\xb0\xe6\xad\x8c\xe6\x9b\xb2"));
    m_selectFileBtn->setEnabled(true);
    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(
        QString::fromUtf8("\xe5\xbc\x80\xe5\xa7\x8b\xe5\x88\x86\xe6\x9e\x90""BPM\xe5\xb9\xb6\xe7\x94\x9f\xe6\x88\x90\xe8\xb0\xb1\xe9\x9d\xa2"));
    m_visualizeBtn->setEnabled(false);
    m_gameBtn->setEnabled(false);
    m_chartEditBtn->setEnabled(false);

    m_scoreHist1->clear();
    m_scoreHist2->clear();
    m_scoreHist3->clear();
}

void SongSelectWidget::refreshHistory(const QVector<CacheEntry>& entries)
{
    m_historyList->clear();

    if (entries.isEmpty()) {
        QListWidgetItem* emptyItem = new QListWidgetItem(
            QString::fromUtf8("  \xe6\x9a\x82\xe6\x97\xa0\xe5\x8e\x86\xe5\x8f\xb2\xe8\xae\xb0\xe5\xbd\x95\xef\xbc\x8c\xe9\x80\x89\xe6\x8b\xa9\xe6\xad\x8c\xe6\x9b\xb2\xe5\x88\x86\xe6\x9e\x90\xe5\x90\x8e\xe5\xb0\x86\xe8\x87\xaa\xe5\x8a\xa8\xe4\xbf\x9d\xe5\xad\x98"),
            m_historyList);
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        emptyItem->setData(Qt::UserRole, QString());
        return;
    }

    for (const CacheEntry& entry : entries) {
        QString text = QStringLiteral("%1  |  BPM: %2  |  %3  |  %4")
            .arg(entry.fileName)
            .arg(static_cast<int>(entry.bpm))
            .arg(formatDuration(entry.durationMs))
            .arg(formatDateTime(entry.analyzedAt));

        QListWidgetItem* item = new QListWidgetItem(text, m_historyList);
        item->setData(Qt::UserRole, entry.filePath);
        item->setData(Qt::UserRole + 1, static_cast<double>(entry.fileSize));

        if (!QFileInfo::exists(entry.filePath)) {
            item->setForeground(QColor(100, 100, 100));
            item->setText(text + QString::fromUtf8("  [\xe6\x96\x87\xe4\xbb\xb6\xe7\xbc\xba\xe5\xa4\xb1]"));
        }
    }
}

void SongSelectWidget::loadFromCache(const QString& filePath, float bpm, qint64 durationMs)
{
    m_selectedPath = filePath;
    m_analysisDone = true;

    m_placeholderLabel->hide();
    m_cardsContainer->show();

    QFileInfo fi(filePath);
    m_fileNameLabel->setText(fi.fileName());
    m_fileSizeLabel->setText(formatFileSize(fi.size()));
    m_durationLabel->setText(
        QString::fromUtf8("\xe6\x97\xb6\xe9\x95\xbf: %1").arg(formatDuration(durationMs)));
    m_bpmLabel->setText(QStringLiteral("BPM: %1").arg(static_cast<int>(bpm)));
    m_errorLabel->hide();
    m_progressBar->hide();

    m_selectFileBtn->setText(QString::fromUtf8("\xe9\x87\x8d\xe6\x96\xb0\xe9\x80\x89\xe6\x8b\xa9\xe6\x96\x87\xe4\xbb\xb6"));
    m_selectFileBtn->setEnabled(true);
    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(QString::fromUtf8("\xe9\x87\x8d\xe6\x96\xb0\xe5\x88\x86\xe6\x9e\x90"));
    m_visualizeBtn->setEnabled(true);
    m_gameBtn->setEnabled(true);
    m_chartEditBtn->setEnabled(true);

    updateLaneButtons();
    updateScoreHistory();
}

void SongSelectWidget::showLoadingState()
{
    QDialog* dlg = new QDialog(this);
    dlg->setWindowTitle(QString::fromUtf8("\xe5\x8a\xa0\xe8\xbd\xbd\xe4\xb8\xad"));
    dlg->setFixedSize(200, 100);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setStyleSheet(
        "QDialog { background-color: #1a1a2e; border: 1px solid #0f3460; border-radius: 8px; }");
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(dlg);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(8);

    QLabel* label = new QLabel(QString::fromUtf8("\xe5\x8a\xa0\xe8\xbd\xbd\xe4\xb8\xad..."), dlg);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(
        "color: #00ff88; font-size: 16px; font-weight: bold; background: transparent;");
    layout->addWidget(label);

    dlg->show();
    dlg->raise();

    m_spinner = nullptr;
    m_spinnerDialog = dlg;

    m_historyList->setEnabled(false);
    m_backBtn->setEnabled(false);
}

void SongSelectWidget::hideLoadingState()
{
    if (m_spinnerDialog) {
        m_spinnerDialog->close();
        m_spinnerDialog = nullptr;
    }

    m_backBtn->setEnabled(true);
    m_historyList->setEnabled(true);
}

// == Private slots ===================================================

void SongSelectWidget::onSelectFileClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe9\x9f\xb3\xe9\xa2\x91\xe6\x96\x87\xe4\xbb\xb6"),
        QString(),
        QString::fromUtf8("\xe9\x9f\xb3\xe9\xa2\x91\xe6\x96\x87\xe4\xbb\xb6 (*.mp3 *.wav *.flac);;\xe6\x89\x80\xe6\x9c\x89\xe6\x96\x87\xe4\xbb\xb6 (*.*)")
    );

    if (path.isEmpty()) return;

    m_selectedPath = path;
    m_analysisDone = false;

    m_placeholderLabel->hide();
    m_cardsContainer->show();

    QFileInfo fi(path);
    m_fileNameLabel->setText(fi.fileName());
    m_fileSizeLabel->setText(formatFileSize(fi.size()));
    m_durationLabel->setText(QString::fromUtf8("\xe6\x97\xb6\xe9\x95\xbf: --"));
    m_bpmLabel->setText(QStringLiteral("BPM: --"));
    m_errorLabel->hide();
    m_progressBar->hide();

    m_analyzeBtn->setEnabled(true);
    m_analyzeBtn->setText(
        QString::fromUtf8("\xe5\xbc\x80\xe5\xa7\x8b\xe5\x88\x86\xe6\x9e\x90""BPM\xe5\xb9\xb6\xe7\x94\x9f\xe6\x88\x90\xe8\xb0\xb1\xe9\x9d\xa2"));
    m_visualizeBtn->setEnabled(false);
    m_gameBtn->setEnabled(false);
    m_chartEditBtn->setEnabled(false);

    m_selectFileBtn->setText(QString::fromUtf8("\xe9\x87\x8d\xe6\x96\xb0\xe9\x80\x89\xe6\x8b\xa9\xe6\x96\x87\xe4\xbb\xb6"));

    updateScoreHistory();
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
    if (filePath.isEmpty()) return;

    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this,
            QString::fromUtf8("\xe6\x96\x87\xe4\xbb\xb6\xe4\xb8\x8d\xe5\xad\x98\xe5\x9c\xa8"),
            QString::fromUtf8("\xe9\x9f\xb3\xe9\xa2\x91\xe6\x96\x87\xe4\xbb\xb6\xe5\xb7\xb2\xe8\xa2\xab\xe7\xa7\xbb\xe5\x8a\xa8\xe6\x88\x96\xe5\x88\xa0\xe9\x99\xa4\xef\xbc\x8c\xe8\xaf\xb7\xe9\x87\x8d\xe6\x96\xb0\xe9\x80\x89\xe6\x8b\xa9\xe6\x96\x87\xe4\xbb\xb6"));
        return;
    }

    emit historySelected(filePath);
}

void SongSelectWidget::onHistoryDeleteClicked()
{
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
    if (filePath.isEmpty()) return;

    m_historyList->setCurrentItem(item);
    m_historyMenu->exec(m_historyList->mapToGlobal(pos));
}

// == Private methods =================================================

void SongSelectWidget::enterAnalyzingState()
{
    m_analyzeBtn->setEnabled(false);
    m_analyzeBtn->setText(
        QString::fromUtf8("\xe6\xad\xa3\xe5\x9c\xa8\xe5\x88\x86\xe6\x9e\x90\xe8\x8a\x82\xe6\x8b\x8d..."));
    m_selectFileBtn->setEnabled(false);
    m_visualizeBtn->setEnabled(false);
    m_gameBtn->setEnabled(false);
    m_chartEditBtn->setEnabled(false);
    m_errorLabel->hide();

    m_progressBar->setValue(0);
    m_progressBar->show();

    if (m_spinner) m_spinner->start();
}

void SongSelectWidget::exitAnalyzingState()
{
    if (m_spinner) m_spinner->stop();
}

void SongSelectWidget::setCardsVisible(bool visible)
{
    m_cardsContainer->setVisible(visible);
    m_placeholderLabel->setVisible(!visible);
}

void SongSelectWidget::updateLaneButtons()
{
    if (m_laneCount == 4) {
        m_4kBtn->setStyleSheet(
            "QPushButton { background-color: #00ff88; color: #1a1a2e; "
            "font-weight: bold; border: 2px solid #00ff88; border-radius: 6px; font-size: 14px; }");
        m_6kBtn->setStyleSheet(
            "QPushButton { background-color: transparent; color: #888888; "
            "border: 2px solid #333333; border-radius: 6px; font-size: 14px; }"
            "QPushButton:hover { border-color: #00ff88; color: #00ff88; }");
    } else {
        m_6kBtn->setStyleSheet(
            "QPushButton { background-color: #00ff88; color: #1a1a2e; "
            "font-weight: bold; border: 2px solid #00ff88; border-radius: 6px; font-size: 14px; }");
        m_4kBtn->setStyleSheet(
            "QPushButton { background-color: transparent; color: #888888; "
            "border: 2px solid #333333; border-radius: 6px; font-size: 14px; }"
            "QPushButton:hover { border-color: #00ff88; color: #00ff88; }");
    }
}

void SongSelectWidget::setLeaderboardManager(LeaderboardManager* mgr)
{
    m_leaderboardMgr = mgr;
}

void SongSelectWidget::onLaneToggled()
{
    QPushButton* sender = qobject_cast<QPushButton*>(QObject::sender());
    if (sender == m_4kBtn)
        m_laneCount = 4;
    else if (sender == m_6kBtn)
        m_laneCount = 6;
    updateLaneButtons();
    updateScoreHistory();
}

void SongSelectWidget::updateScoreHistory()
{
    if (!m_leaderboardMgr || m_selectedPath.isEmpty()) return;

    QFileInfo fi(m_selectedPath);
    QVector<LeaderboardEntry> entries =
        m_leaderboardMgr->entriesForSong(fi.size(), m_laneCount);

    if (entries.isEmpty()) {
        m_scoreHistoryTitle->setText(
            QString::fromUtf8("\xe5\x8e\x86\xe5\x8f\xb2\xe6\x88\x90\xe7\xbb\xa9 - \xe6\x9a\x82\xe6\x97\xa0\xe8\xae\xb0\xe5\xbd\x95"));
        m_scoreHist1->clear();
        m_scoreHist2->clear();
        m_scoreHist3->clear();
        return;
    }

    int maxShow = qMin(3, entries.size());
    m_scoreHistoryTitle->setText(
        QString::fromUtf8("\xe5\x8e\x86\xe5\x8f\xb2\xe6\x88\x90\xe7\xbb\xa9 (%1 \xe6\xac\xa1)")
        .arg(entries.size()));

    auto gradeColor = [](const QString& grade) -> QColor {
        if (grade == QString::fromUtf8("\xcf\x86")) return QColor(255, 215, 0);
        if (grade == "SSS") return QColor(255, 107, 157);
        if (grade == "SS")  return QColor(255, 159, 67);
        if (grade == "S")   return QColor(0, 255, 136);
        if (grade == "A")   return QColor(189, 147, 249);
        if (grade == "B")   return QColor(139, 233, 253);
        if (grade == "C")   return QColor(255, 255, 102);
        return QColor(200, 200, 200);
    };

    QLabel* labels[3] = {m_scoreHist1, m_scoreHist2, m_scoreHist3};
    for (int j = 0; j < 3; j++) {
        if (j < maxShow) {
            const auto& e = entries[j];
            int maxScore = e.totalNotes * 300;
            QString text = QStringLiteral("#%1  %2  %3/%4  %5")
                .arg(j + 1)
                .arg(e.grade)
                .arg(e.score)
                .arg(maxScore)
                .arg(e.playedAt.toString(QStringLiteral("M/d HH:mm")));
            labels[j]->setText(text);
            labels[j]->setStyleSheet(
                QStringLiteral("color: %1; font-size: 13px; font-weight: bold; background: transparent;")
                .arg(gradeColor(e.grade).name()));
        } else {
            labels[j]->clear();
        }
    }
}

QString SongSelectWidget::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(
            static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(
        static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
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
            .arg(hr).arg(min, 2, 10, QLatin1Char('0'))
            .arg(sec, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(min).arg(sec, 2, 10, QLatin1Char('0'));
}

QString SongSelectWidget::formatDateTime(const QDateTime& dt) const
{
    if (!dt.isValid()) return QStringLiteral("--");
    QDateTime now = QDateTime::currentDateTime();
    if (dt.date() == now.date()) {
        return dt.toString(QStringLiteral("HH:mm"));
    }
    if (dt.date().year() == now.date().year()) {
        return dt.toString(QString::fromUtf8("M\xe6\x9c\x88""d\xe6\x97\xa5 HH:mm"));
    }
    return dt.toString(QStringLiteral("yyyy/M/d HH:mm"));
}
