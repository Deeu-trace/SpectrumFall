#include "LeaderboardWidget.h"
#include "LeaderboardManager.h"

#include <QScrollArea>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QDialog>
#include <QFont>
#include <algorithm>

// -- card style constants --
static const QString CARD_BG = "rgba(22, 33, 62, 160)";
static const QString CARD_BORDER = "#0f3460";
static const QString ROW_EVEN = "rgba(15, 52, 96, 50)";

static QColor gradeColor(const QString& grade)
{
    if (grade == QString::fromUtf8("\xcf\x86")) return QColor(255, 215, 0);
    if (grade == "SSS") return QColor(255, 107, 157);
    if (grade == "SS")  return QColor(255, 159, 67);
    if (grade == "S")   return QColor(0, 255, 136);
    if (grade == "A")   return QColor(189, 147, 249);
    if (grade == "B")   return QColor(139, 233, 253);
    if (grade == "C")   return QColor(255, 255, 102);
    return QColor(233, 69, 96);
}

static QString formatNumber(int n)
{
    QString s = QString::number(n);
    int pos = s.length() - 3;
    while (pos > 0) {
        s.insert(pos, ',');
        pos -= 3;
    }
    return s;
}

static void clearLayout(QLayout* layout)
{
    while (layout->count() > 0) {
        QLayoutItem* item = layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        if (item->layout()) clearLayout(item->layout());
        delete item;
    }
}

// == Constructor ==
LeaderboardWidget::LeaderboardWidget(LeaderboardManager* lb, QWidget* parent)
    : QWidget(parent), m_lb(lb), m_cardsLayout(nullptr), m_hintLabel(nullptr), m_currentFilter(0)
{
    setupUI();
}

void LeaderboardWidget::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 30, 40, 30);
    layout->setSpacing(12);

    // Title
    QLabel* titleLabel = new QLabel(QString::fromUtf8("\xe6\x8e\x92\xe8\xa1\x8c\xe6\xa6\x9c"), this);
    QFont tf = titleLabel->font();
    tf.setPixelSize(36); tf.setBold(true);
    titleLabel->setFont(tf);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #00ff88; background: transparent;");
    layout->addWidget(titleLabel);

    // Filter buttons
    QHBoxLayout* filterRow = new QHBoxLayout();
    filterRow->setSpacing(8);
    filterRow->addStretch();

    auto makeFilterBtn = [&](const QString& text, int val) -> QPushButton* {
        QPushButton* btn = new QPushButton(text, this);
        btn->setMinimumSize(80, 34);
        connect(btn, &QPushButton::clicked, this, [this, val]() {
            m_currentFilter = val;
            updateFilterButtons();
            rebuildCards();
        });
        filterRow->addWidget(btn);
        return btn;
    };

    m_filterAllBtn = makeFilterBtn(QString::fromUtf8("\xe5\x85\xa8\xe9\x83\xa8"), 0);
    m_filter4KBtn  = makeFilterBtn("4K", 4);
    m_filter6KBtn  = makeFilterBtn("6K", 6);

    filterRow->addStretch();
    layout->addLayout(filterRow);
    updateFilterButtons();

    // Scroll area
    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: #1a1a2e; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: #0f3460; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    QWidget* scrollContent = new QWidget();
    scrollContent->setStyleSheet("background: transparent;");
    m_cardsLayout = new QVBoxLayout(scrollContent);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(16);
    m_cardsLayout->addStretch();

    scroll->setWidget(scrollContent);
    layout->addWidget(scroll, 1);

    // Empty hint
    m_hintLabel = new QLabel(this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet("color: #8888aa; font-size: 18px; background: transparent;");
    m_hintLabel->hide();
    layout->addWidget(m_hintLabel);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe8\xbf\x94\xe5\x9b\x9e\xe4\xb8\xbb\xe8\x8f\x9c\xe5\x8d\x95"), this);
    backBtn->setMinimumSize(160, 42);
    backBtn->setObjectName("backButton");
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addStretch(); btnRow->addWidget(backBtn); btnRow->addStretch();
    layout->addLayout(btnRow);
    connect(backBtn, &QPushButton::clicked, this, &LeaderboardWidget::backRequested);
}

// == Filter button styles ==
void LeaderboardWidget::updateFilterButtons()
{
    QString sel = "QPushButton { background: #00ff88; color: #1a1a2e; border: none; "
                  "border-radius: 6px; font-weight: bold; font-size: 14px; }";
    QString unsel = "QPushButton { background: rgba(15,52,96,180); color: #8888aa; "
                    "border: 1px solid #0f3460; border-radius: 6px; font-size: 14px; }"
                    "QPushButton:hover { border-color: #00ff88; color: #00ff88; }";
    m_filterAllBtn->setStyleSheet(m_currentFilter == 0 ? sel : unsel);
    m_filter4KBtn->setStyleSheet(m_currentFilter == 4 ? sel : unsel);
    m_filter6KBtn->setStyleSheet(m_currentFilter == 6 ? sel : unsel);
}

// == Data loading ==
void LeaderboardWidget::loadEntries()
{
    m_groupedEntries.clear();
    if (!m_lb) return;
    QVector<LeaderboardEntry> all = m_lb->allEntries();
    for (const LeaderboardEntry& e : all)
        m_groupedEntries[e.songFileSize].append(e);
    for (auto it = m_groupedEntries.begin(); it != m_groupedEntries.end(); ++it) {
        std::sort(it.value().begin(), it.value().end(),
            [](const LeaderboardEntry& a, const LeaderboardEntry& b) { return a.score > b.score; });
    }
}

// == Card management ==
void LeaderboardWidget::clearCards()
{
    if (m_cardsLayout) clearLayout(m_cardsLayout);
}

void LeaderboardWidget::rebuildCards()
{
    clearCards();
    if (!m_cardsLayout) return;
    bool anyShown = false;
    for (auto it = m_groupedEntries.constBegin(); it != m_groupedEntries.constEnd(); ++it) {
        const QVector<LeaderboardEntry>& entries = it.value();
        if (entries.isEmpty()) continue;
        QWidget* card = buildCard(entries[0].songFileName, entries);
        if (card) { m_cardsLayout->addWidget(card); anyShown = true; }
    }
    m_cardsLayout->addStretch();
    m_hintLabel->setVisible(!anyShown);
    if (!anyShown)
        m_hintLabel->setText(QString::fromUtf8(
            "\xe6\x9a\x82\xe6\x97\xa0\xe4\xbb\xbb\xe4\xbd\x95\xe6\x8e\x92\xe8\xa1\x8c"
            "\xe6\xa6\x9c\xe8\xae\xb0\xe5\xbd\x95\xef\xbc\x8c\xe5\x85\x88\xe5\x8e\xbb"
            "\xe6\x89\x93\xe5\xae\x8c\xe4\xb8\x80\xe5\xb1\x80\xe5\x90\xa7\xef\xbc\x81"));
}

// == Build a song card ==
QWidget* LeaderboardWidget::buildCard(const QString& songName, const QVector<LeaderboardEntry>& entries)
{
    QVector<LeaderboardEntry> e4k, e6k;
    int max4k = 0, max6k = 0;
    int playCount = entries.size();
    for (const LeaderboardEntry& e : entries) {
        int ms = e.totalNotes * 300;
        if (e.laneCount == 4) { e4k.append(e); if (ms > max4k) max4k = ms; }
        else                   { e6k.append(e); if (ms > max6k) max6k = ms; }
    }
    if (m_currentFilter == 4 && e4k.isEmpty()) return nullptr;
    if (m_currentFilter == 6 && e6k.isEmpty()) return nullptr;

    QFrame* card = new QFrame();
    card->setStyleSheet(QStringLiteral("QFrame { background: %1; border: 1px solid %2; border-radius: 12px; }").arg(CARD_BG, CARD_BORDER));
    QVBoxLayout* cl = new QVBoxLayout(card);
    cl->setContentsMargins(20, 16, 20, 16);
    cl->setSpacing(10);

    // Header
    QHBoxLayout* hdr = new QHBoxLayout();
    QLabel* nm = new QLabel(songName);
    QFont nf = nm->font(); nf.setPixelSize(22); nf.setBold(true); nm->setFont(nf);
    nm->setStyleSheet("color: #00ff88; background: transparent; border: none;");
    hdr->addWidget(nm);
    hdr->addStretch();

    float bpm = entries[0].bpm;
    qint64 durMs = entries[0].songDurationMs;
    int durSec = static_cast<int>(durMs / 1000);
    QString meta = QStringLiteral("BPM: %1  |  %2:%3  |  %4: %5")
        .arg(static_cast<int>(bpm))
        .arg(durSec / 60).arg(durSec % 60, 2, 10, QLatin1Char('0'))
        .arg(QString::fromUtf8("\xe6\xb8\xb8\xe7\x8e\xa9")).arg(playCount);
    QLabel* ml = new QLabel(meta);
    ml->setStyleSheet("color: #bd93f9; font-size: 14px; background: transparent; border: none;");
    hdr->addWidget(ml);
    cl->addLayout(hdr);

    // Mode sections
    QString capName = songName;
    qint64 fsz = entries[0].songFileSize;

    auto addModeSection = [&](const QVector<LeaderboardEntry>& me, int maxS, int mode) {
        if (me.isEmpty()) {
            QLabel* empty = new QLabel(QStringLiteral("%1K  ").arg(mode) + QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe8\xae\xb0\xe5\xbd\x95"));
            empty->setStyleSheet("color: #555570; font-size: 14px; background: transparent; border: none; padding: 4px 0;");
            cl->addWidget(empty);
            return;
        }
        cl->addWidget(buildTop3(me, maxS, mode));
        if (me.size() > 3) {
            QString btnText = QString::fromUtf8("\xe6\x9f\xa5\xe7\x9c\x8b\xe5\x85\xa8\xe9\x83\xa8") +
                              QStringLiteral(" %1K ").arg(mode) +
                              QString::fromUtf8("\xe6\x8e\x92\xe8\xa1\x8c") + QStringLiteral(" \xe2\x86\x92");
            QPushButton* va = new QPushButton(btnText);
            va->setStyleSheet(
                "QPushButton { color: #8888aa; font-size: 13px; background: transparent; border: none; padding: 2px 8px; }"
                "QPushButton:hover { color: #00ff88; }");
            va->setCursor(Qt::PointingHandCursor);
            connect(va, &QPushButton::clicked, this, [this, capName, fsz, mode]() { showFullDialog(capName, fsz, mode); });
            QHBoxLayout* vaRow = new QHBoxLayout();
            vaRow->addStretch(); vaRow->addWidget(va);
            cl->addLayout(vaRow);
        }
    };

    if (m_currentFilter != 6) addModeSection(e4k, max4k, 4);
    if (m_currentFilter != 4) addModeSection(e6k, max6k, 6);

    return card;
}

// == Build top 3 compact list ==
QWidget* LeaderboardWidget::buildTop3(const QVector<LeaderboardEntry>& me, int maxScore, int mode)
{
    QWidget* c = new QWidget();
    c->setStyleSheet("background: transparent; border: none;");
    QVBoxLayout* vl = new QVBoxLayout(c);
    vl->setContentsMargins(0, 4, 0, 4);
    vl->setSpacing(3);

    QLabel* ml = new QLabel(QStringLiteral("%1K").arg(mode));
    QFont mf = ml->font(); mf.setPixelSize(16); mf.setBold(true); ml->setFont(mf);
    ml->setStyleSheet("color: #8be9fd; background: transparent; border: none;");
    vl->addWidget(ml);

    int cnt = qMin(3, me.size());
    for (int i = 0; i < cnt; ++i) {
        const LeaderboardEntry& e = me[i];
        int rank = i + 1;
        QWidget* row = new QWidget();
        row->setFixedHeight(30);
        row->setStyleSheet(QStringLiteral("background: %1; border: none; border-radius: 4px;").arg(i % 2 == 0 ? ROW_EVEN : "transparent"));
        QHBoxLayout* hl = new QHBoxLayout(row);
        hl->setContentsMargins(8, 0, 8, 0);
        hl->setSpacing(10);

        QLabel* rl = new QLabel(QStringLiteral("#%1").arg(rank));
        rl->setFixedWidth(35);
        if (rank == 1) rl->setStyleSheet("color: #ffd700; font-weight: bold; font-size: 15px; background: transparent; border: none;");
        else if (rank == 2) rl->setStyleSheet("color: #c0c0c0; font-weight: bold; font-size: 14px; background: transparent; border: none;");
        else rl->setStyleSheet("color: #cd7f32; font-weight: bold; font-size: 14px; background: transparent; border: none;");
        hl->addWidget(rl);

        QLabel* nl = new QLabel(e.playerName);
        nl->setFixedWidth(130);
        nl->setStyleSheet("color: #e0e0e0; font-size: 14px; background: transparent; border: none;");
        hl->addWidget(nl);

        QLabel* sl = new QLabel(QStringLiteral("%1 / %2").arg(formatNumber(e.score), formatNumber(maxScore)));
        sl->setFixedWidth(170);
        sl->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: bold; background: transparent; border: none;");
        hl->addWidget(sl);

        QLabel* gl = new QLabel(e.grade);
        gl->setFixedWidth(35);
        gl->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; font-size: 14px; background: transparent; border: none;").arg(gradeColor(e.grade).name()));
        hl->addWidget(gl);

        hl->addStretch();

        QLabel* dl = new QLabel(e.playedAt.toString(QStringLiteral("MM-dd HH:mm")));
        dl->setFixedWidth(90);
        dl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        dl->setStyleSheet("color: #666680; font-size: 13px; background: transparent; border: none;");
        hl->addWidget(dl);

        vl->addWidget(row);
    }
    return c;
}

// == Full leaderboard dialog ==
void LeaderboardWidget::showFullDialog(const QString& songName, qint64 fileSize, int mode)
{
    QVector<LeaderboardEntry> entries = m_lb->entriesForSong(fileSize, mode);
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("%1 - %2K").arg(songName).arg(mode));
    dlg.setMinimumSize(750, 500);
    dlg.setStyleSheet("QDialog { background: #0d0d1f; }");

    QVBoxLayout* dl = new QVBoxLayout(&dlg);
    dl->setContentsMargins(20, 16, 20, 16);
    dl->setSpacing(12);

    QLabel* dt = new QLabel(QStringLiteral("%1  %2K").arg(songName).arg(mode));
    QFont dtf = dt->font(); dtf.setPixelSize(22); dtf.setBold(true); dt->setFont(dtf);
    dt->setStyleSheet("color: #00ff88; background: transparent;");
    dl->addWidget(dt);

    QTableWidget* table = new QTableWidget(entries.size(), 9, &dlg);
    table->setHorizontalHeaderLabels(QStringList()
        << QString::fromUtf8("\xe6\x8e\x92\xe5\x90\x8d")
        << QString::fromUtf8("\xe7\x8e\xa9\xe5\xae\xb6")
        << QString::fromUtf8("\xe5\x88\x86\xe6\x95\xb0")
        << QString::fromUtf8("\xe6\xbb\xa1\xe5\x88\x86")
        << QString::fromUtf8("\xe8\xaf\x84\xe7\xba\xa7")
        << "Perfect" << "Good" << "Miss"
        << QString::fromUtf8("\xe6\x97\xa5\xe6\x9c\x9f"));
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setStyleSheet(
        "QTableWidget { background-color: #0d0d1f; alternate-background-color: #12122a; "
        "color: #e0e0e0; gridline-color: #1f1f3a; border: 1px solid #0f3460; border-radius: 8px; font-size: 14px; }"
        "QHeaderView::section { background-color: #16213e; color: #00ff88; "
        "font-weight: bold; padding: 6px; border: none; border-bottom: 1px solid #0f3460; }"
        "QTableWidget::item { padding: 4px 8px; }"
        "QTableWidget::item:selected { background-color: #0f3460; color: #ffffff; }");
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0, 55); table->setColumnWidth(1, 130);
    table->setColumnWidth(2, 90); table->setColumnWidth(3, 90);
    table->setColumnWidth(4, 55); table->setColumnWidth(5, 65);
    table->setColumnWidth(6, 55); table->setColumnWidth(7, 55);
    table->setColumnWidth(8, 140);

    auto makeItem = [](const QString& text) -> QTableWidgetItem* {
        QTableWidgetItem* it = new QTableWidgetItem(text);
        it->setTextAlignment(Qt::AlignCenter);
        return it;
    };

    for (int i = 0; i < entries.size(); ++i) {
        const LeaderboardEntry& e = entries[i];
        int rank = i + 1;
        int ms = e.totalNotes * 300;

        QTableWidgetItem* ri = makeItem(QStringLiteral("#%1").arg(rank));
        if (rank <= 3) { QFont rf = ri->font(); rf.setBold(true); ri->setFont(rf); }
        table->setItem(i, 0, ri);
        table->setItem(i, 1, makeItem(e.playerName));

        QTableWidgetItem* si = makeItem(formatNumber(e.score));
        QFont sf = si->font(); sf.setBold(true); si->setFont(sf);
        table->setItem(i, 2, si);
        table->setItem(i, 3, makeItem(formatNumber(ms)));

        QTableWidgetItem* gi = makeItem(e.grade);
        gi->setForeground(QBrush(gradeColor(e.grade)));
        QFont gf = gi->font(); gf.setBold(true); gi->setFont(gf);
        table->setItem(i, 4, gi);

        table->setItem(i, 5, makeItem(QString::number(e.perfect)));
        table->setItem(i, 6, makeItem(QString::number(e.good)));
        table->setItem(i, 7, makeItem(QString::number(e.miss)));
        table->setItem(i, 8, makeItem(e.playedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));

        if (rank == 1) {
            for (int c = 0; c < 9; ++c) {
                QTableWidgetItem* it = table->item(i, c);
                if (it) it->setForeground(QBrush(QColor(0, 255, 136)));
            }
        }
    }
    dl->addWidget(table, 1);

    QHBoxLayout* br = new QHBoxLayout();
    br->addStretch();
    QPushButton* cb = new QPushButton(QString::fromUtf8("\xe5\x85\xb3\xe9\x97\xad"), &dlg);
    cb->setMinimumSize(100, 36);
    cb->setStyleSheet(
        "QPushButton { background: #0f3460; color: #e0e0e0; border: 1px solid #1e3560; border-radius: 6px; font-size: 14px; padding: 8px 16px; }"
        "QPushButton:hover { border-color: #00ff88; color: #00ff88; }");
    br->addWidget(cb);
    br->addStretch();
    dl->addLayout(br);
    connect(cb, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

// == Public interface ==
void LeaderboardWidget::refresh(qint64 songFileSize, int laneCount)
{
    loadEntries();
    rebuildCards();
    if (laneCount == 4 || laneCount == 6) {
        m_currentFilter = laneCount;
        updateFilterButtons();
        rebuildCards();
    }
}
