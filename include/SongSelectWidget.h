#pragma once

#include <QWidget>
#include <QString>
#include <QVector>

class QPushButton;
class QLabel;
class QProgressBar;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QDialog;
class QSplitter;
class QFrame;
class SpinnerWidget;
class LeaderboardManager;

struct CacheEntry;  // forward declaration

/// Song selection page: two-panel layout (left=history+select, right=3 cards)
class SongSelectWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SongSelectWidget(QWidget* parent = nullptr);

    /// Get currently selected song path
    QString selectedSong() const;

    /// Get currently selected lane count (4 or 6)
    int selectedLaneCount() const;

    /// Set analysis progress (0-100), manages analyzing-state UI
    void setAnalysisProgress(int percent);

    /// Update duration display
    void setDurationDisplay(qint64 ms);

    /// Analysis complete, update UI to success state
    void onAnalysisComplete(float bpm);

    /// Show analysis error, re-enable buttons
    void showAnalysisError(const QString& message);

    /// Reset to initial state (no file selected)
    void resetState();

    /// Refresh history list
    void refreshHistory(const QVector<CacheEntry>& entries);

    /// Load from cache, set UI to analyzed state
    void loadFromCache(const QString& filePath, float bpm, qint64 durationMs);

    /// Show loading state (disable all buttons + show spinner)
    void showLoadingState();

    /// Hide loading state (restore buttons)
    void hideLoadingState();

    /// Set leaderboard manager for score history display
    void setLeaderboardManager(LeaderboardManager* mgr);

    /// Refresh score history from leaderboard data
    void updateScoreHistory();

signals:
    void analyzeRequested(const QString& path);
    void visualizeRequested();
    void gameRequested();
    void chartEditRequested();
    void backRequested();
    void historySelected(const QString& filePath);
    void historyDeleteRequested(const QString& filePath);

private slots:
    void onSelectFileClicked();
    void onAnalyzeClicked();
    void onVisualizeClicked();
    void onGameClicked();
    void onHistoryItemClicked(QListWidgetItem* item);
    void onHistoryContextMenu(const QPoint& pos);
    void onHistoryDeleteClicked();
    void onLaneToggled();

private:
    void enterAnalyzingState();
    void exitAnalyzingState();
    void updateLaneButtons();
    void setCardsVisible(bool visible);
    QString formatFileSize(qint64 bytes) const;
    QString formatDuration(qint64 ms) const;
    QString formatDateTime(const QDateTime& dt) const;

    // Layout
    QSplitter* m_splitter;           ///< Left-right splitter

    // Left panel
    QWidget* m_leftPanel;            ///< Left panel container
    QPushButton* m_selectFileBtn;    ///< Select / re-select file button
    QLabel* m_historyTitle;          ///< History title label
    QListWidget* m_historyList;      ///< History list
    QMenu* m_historyMenu;            ///< Right-click menu

    // Right panel
    QWidget* m_rightPanel;           ///< Right panel container
    QLabel* m_placeholderLabel;      ///< Placeholder when no file selected
    QWidget* m_cardsContainer;       ///< Container for 3 cards

    // Card 1: File info
    QFrame* m_infoCard;              ///< Info card frame
    QLabel* m_fileNameLabel;         ///< File name
    QLabel* m_fileSizeLabel;         ///< File size
    QLabel* m_durationLabel;         ///< Duration
    QLabel* m_bpmLabel;              ///< BPM display
    QLabel* m_errorLabel;            ///< Error message
    QPushButton* m_analyzeBtn;       ///< Analyze BPM button
    QProgressBar* m_progressBar;     ///< Analysis progress bar
    SpinnerWidget* m_spinner;        ///< Spinner animation
    QDialog* m_spinnerDialog;        ///< Loading dialog (for history loading)

    // Card 2: Score history
    QFrame* m_scoreCard;             ///< Score history card frame
    QLabel* m_scoreHistoryTitle;     ///< Score history section title
    QLabel* m_scoreHist1;            ///< Recent score row 1
    QLabel* m_scoreHist2;            ///< Recent score row 2
    QLabel* m_scoreHist3;            ///< Recent score row 3

    // Card 3: Game mode
    QFrame* m_modeCard;              ///< Game mode card frame
    QPushButton* m_4kBtn;            ///< 4-key mode button
    QPushButton* m_6kBtn;            ///< 6-key mode button

    // Bottom action buttons
    QPushButton* m_visualizeBtn;     ///< Visualize mode button
    QPushButton* m_chartEditBtn;     ///< Chart editor button
    QPushButton* m_gameBtn;          ///< Start game button
    QPushButton* m_backBtn;          ///< Back button

    LeaderboardManager* m_leaderboardMgr; ///< Leaderboard data source
    QString m_selectedPath;          ///< Currently selected file path
    bool m_analysisDone;             ///< Whether analysis is complete
    int m_laneCount;                 ///< Selected lane count (4 or 6, default 6)
};
