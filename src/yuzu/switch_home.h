// SPDX-FileCopyrightText: 2026 Torzu Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QModelIndex>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "common/common_types.h"
#include "yuzu/game_list.h"

class SwitchGameCard;
class SwitchCircleButton;

struct SwitchGameEntry {
    QString title;
    QString full_path;
    u64 program_id{0};
    QString file_type;
    QString version;
    QString compatibility;
    QString play_time;
    QPixmap icon;
    QModelIndex model_index;
};

class SwitchHomeWidget : public QWidget {
    Q_OBJECT

public:
    explicit SwitchHomeWidget(QWidget* parent = nullptr);
    ~SwitchHomeWidget() override;

    void PopulateFromModel(QStandardItemModel* model);
    void SelectGame(int index);
    void SelectNextGame();
    void SelectPreviousGame();
    void LaunchSelectedGame();
    void OpenSelectedGameOptions();

    int GetSelectedGameIndex() const {
        return current_selected_index;
    }

    bool HasGames() const {
        return !games.empty();
    }

signals:
    void BootGame(const QString& game_path, StartGameType type);
    void GameChosen(const QString& game_path, const u64 program_id = 0);
    void OpenGameContextMenu(const QPoint& pos, u64 program_id, const std::string& path);
    void ControllersRequested();
    void SettingsRequested();
    void AddDirectoryRequested();
    void AlbumRequested();
    void MultiplayerRequested();
    void ExitRequested();
    void ToggleClassicViewRequested();
    void ToggleFullscreenRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void UpdateClock();
    void OnSearchTextChanged(const QString& text);
    void ToggleSearch();

private:
    void SetupUI();
    void UpdateActiveGameDetails();
    void FilterGames();
    void CenterOnSelectedCard();

    // Data
    std::vector<SwitchGameEntry> all_games;
    std::vector<SwitchGameEntry> games;
    int current_selected_index{-1};
    int current_button_focus{-1}; // -1 for game carousel, 0-6 for bottom buttons

    // Header Widgets
    QFrame* header_frame = nullptr;
    QLabel* user_avatar_label = nullptr;
    QLabel* user_name_label = nullptr;
    QLabel* cartridge_label = nullptr;
    QLabel* clock_label = nullptr;
    QLabel* wifi_label = nullptr;
    QLabel* battery_label = nullptr;
    QTimer* clock_timer = nullptr;

    // Active Game Details
    QLabel* active_title_label = nullptr;
    QLabel* active_details_label = nullptr;

    // Search bar
    QWidget* search_container = nullptr;
    QLineEdit* search_edit = nullptr;
    QPushButton* search_close_btn = nullptr;

    // Carousel Area
    QScrollArea* scroll_area = nullptr;
    QWidget* carousel_container = nullptr;
    QHBoxLayout* carousel_layout = nullptr;
    std::vector<SwitchGameCard*> card_widgets;
    QLabel* empty_label = nullptr;

    // System Dock (Circular Buttons)
    QWidget* dock_container = nullptr;
    std::vector<SwitchCircleButton*> dock_buttons;

    // Footer Guide
    QFrame* footer_frame = nullptr;
    QLabel* footer_guide_label = nullptr;
    QPushButton* classic_view_btn = nullptr;
};

// Custom widget for a Switch Game Card in the carousel
class SwitchGameCard : public QWidget {
    Q_OBJECT

public:
    explicit SwitchGameCard(const SwitchGameEntry& entry, int index, QWidget* parent = nullptr);
    void SetSelected(bool selected);
    bool IsSelected() const {
        return is_selected;
    }
    int GetIndex() const {
        return card_index;
    }
    const SwitchGameEntry& GetEntry() const {
        return game_entry;
    }

signals:
    void Clicked(int index);
    void DoubleClicked(int index);
    void ContextMenuRequested(int index, const QPoint& global_pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    SwitchGameEntry game_entry;
    int card_index{0};
    bool is_selected{false};
    bool is_hovered{false};
};

// Custom circular button for the Switch bottom dock
class SwitchCircleButton : public QPushButton {
    Q_OBJECT

public:
    explicit SwitchCircleButton(const QString& icon_text, const QString& label_text,
                                const QColor& accent_color, QWidget* parent = nullptr);
    void SetFocused(bool focused);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString button_icon_text;
    QString button_label;
    QColor accent_bg;
    bool is_focused{false};
};
