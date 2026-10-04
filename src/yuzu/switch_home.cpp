// SPDX-FileCopyrightText: 2026 Torzu Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "yuzu/switch_home.h"
#include "yuzu/main.h"

#include <algorithm>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QUrl>

#include "yuzu/game_list_p.h"

// ============================================================================
// SwitchCircleButton Implementation
// ============================================================================
SwitchCircleButton::SwitchCircleButton(const QString& icon_text, const QString& label_text,
                                       const QColor& accent_color, QWidget* parent)
    : QPushButton(parent), button_icon_text(icon_text), button_label(label_text),
      accent_bg(accent_color) {
    setFixedSize(70, 78);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setToolTip(label_text);
}

void SwitchCircleButton::SetFocused(bool focused) {
    if (is_focused != focused) {
        is_focused = focused;
        update();
    }
}

void SwitchCircleButton::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int circle_size = 50;
    const int x = (width() - circle_size) / 2;
    const int y = 2;
    const QRect circle_rect(x, y, circle_size, circle_size);

    // Background circle
    if (underMouse() || is_focused) {
        p.setBrush(accent_bg);
        p.setPen(QPen(QColor(0, 212, 255), 3));
    } else {
        p.setBrush(QColor(52, 54, 56));
        p.setPen(QPen(QColor(75, 78, 82), 1.5));
    }
    p.drawEllipse(circle_rect);

    // Icon glyph/text
    p.setFont(QFont(QStringLiteral("sans-serif"), 16, QFont::Bold));
    p.setPen(QColor(255, 255, 255));
    p.drawText(circle_rect, Qt::AlignCenter, button_icon_text);

    // Label below circle
    p.setFont(QFont(QStringLiteral("sans-serif"), 8, QFont::Medium));
    if (underMouse() || is_focused) {
        p.setPen(QColor(0, 212, 255));
    } else {
        p.setPen(QColor(180, 184, 190));
    }
    const QRect label_rect(0, 54, width(), 22);
    p.drawText(label_rect, Qt::AlignCenter, button_label);
}

// ============================================================================
// SwitchGameCard Implementation
// ============================================================================
SwitchGameCard::SwitchGameCard(const SwitchGameEntry& entry, int index, QWidget* parent)
    : QWidget(parent), game_entry(entry), card_index(index) {
    setFixedSize(224, 224);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
}

void SwitchGameCard::SetSelected(bool selected) {
    if (is_selected != selected) {
        is_selected = selected;
        update();
    }
}

void SwitchGameCard::SetIcon(const QPixmap& icon) {
    game_entry.icon = icon;
    update();
}

void SwitchGameCard::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const int padding = 8;
    const QRect card_rect(padding, padding, width() - padding * 2, height() - padding * 2);
    const int radius = 14;

    QPainterPath path;
    path.addRoundedRect(card_rect, radius, radius);

    // Draw card background
    p.fillPath(path, QColor(35, 36, 38));

    // Draw game icon in high resolution
    if (!game_entry.icon.isNull()) {
        p.save();
        p.setClipPath(path);
        const qreal dpr = devicePixelRatioF();
        const QSize target_size(static_cast<int>(card_rect.width() * dpr),
                                static_cast<int>(card_rect.height() * dpr));
        const QPixmap scaled_icon = game_entry.icon.scaled(
            target_size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        p.drawPixmap(card_rect, scaled_icon);
        p.restore();
    } else {
        // Fallback placeholder
        p.save();
        p.setClipPath(path);
        p.setFont(QFont(QStringLiteral("sans-serif"), 12, QFont::Bold));
        p.setPen(QColor(200, 200, 200));
        p.drawText(card_rect, Qt::AlignCenter | Qt::TextWordWrap, game_entry.title);
        p.restore();
    }

    // Selected highlight (Switch glowing border)
    if (is_selected) {
        // Outer cyan glow
        p.setPen(QPen(QColor(0, 195, 227, 180), 5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(card_rect.adjusted(-2, -2, 2, 2), radius + 2, radius + 2);

        // Inner solid white ring
        p.setPen(QPen(QColor(255, 255, 255), 3.5));
        p.drawRoundedRect(card_rect, radius, radius);
    } else if (is_hovered) {
        p.setPen(QPen(QColor(255, 255, 255, 160), 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(card_rect, radius, radius);
    }
}

void SwitchGameCard::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit Clicked(card_index);
    }
    QWidget::mousePressEvent(event);
}

void SwitchGameCard::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit DoubleClicked(card_index);
    }
    QWidget::mouseDoubleClickEvent(event);
}

void SwitchGameCard::contextMenuEvent(QContextMenuEvent* event) {
    emit ContextMenuRequested(card_index, event->globalPos());
}

void SwitchGameCard::enterEvent(QEnterEvent* event) {
    is_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void SwitchGameCard::leaveEvent(QEvent* event) {
    is_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

// ============================================================================
// SwitchHomeWidget Implementation
// ============================================================================
SwitchHomeWidget::SwitchHomeWidget(QWidget* parent) : QWidget(parent) {
    SetupUI();
}

SwitchHomeWidget::~SwitchHomeWidget() = default;

void SwitchHomeWidget::SetupUI() {
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(QStringLiteral("background-color: #2d2d2d; color: #ffffff;"));

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(36, 16, 36, 16);
    main_layout->setSpacing(12);

    // ------------------------------------------------------------------------
    // 1. Top Bar (Header)
    // ------------------------------------------------------------------------
    header_frame = new QFrame(this);
    header_frame->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto* header_layout = new QHBoxLayout(header_frame);
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(14);

    // Profile Avatar
    user_avatar_label = new QLabel(this);
    user_avatar_label->setFixedSize(38, 38);
    user_avatar_label->setStyleSheet(
        QStringLiteral("background-color: #e60012; border: 2px solid #ffffff; "
                       "border-radius: 19px; font-weight: bold; font-size: 16px; color: #ffffff;"));
    user_avatar_label->setAlignment(Qt::AlignCenter);

    QString username = QString::fromLocal8Bit(qgetenv("USER"));
    if (username.isEmpty()) {
        username = QString::fromLocal8Bit(qgetenv("USERNAME"));
    }
    if (username.isEmpty()) {
        username = QStringLiteral("Usuario");
    }
    user_avatar_label->setText(username.left(1).toUpper());

    user_name_label = new QLabel(username, this);
    user_name_label->setStyleSheet(
        QStringLiteral("font-size: 16px; font-weight: bold; color: #ffffff;"));

    // Cartridge / Logo status
    cartridge_label = new QLabel(QStringLiteral("▤ Torzu Switch"), this);
    cartridge_label->setStyleSheet(
        QStringLiteral("font-size: 14px; font-weight: bold; color: #00d4ff; padding-left: 10px;"));

    // Top Right Status Indicators
    clock_label = new QLabel(this);
    clock_label->setStyleSheet(
        QStringLiteral("font-size: 16px; font-weight: bold; color: #ffffff;"));

    wifi_label = new QLabel(QStringLiteral("📶 Wi-Fi"), this);
    wifi_label->setStyleSheet(
        QStringLiteral("font-size: 13px; font-weight: bold; color: #e0e0e0;"));

    battery_label = new QLabel(QStringLiteral("🔋 100%"), this);
    battery_label->setStyleSheet(
        QStringLiteral("font-size: 13px; font-weight: bold; color: #39d353;"));

    header_layout->addWidget(user_avatar_label);
    header_layout->addWidget(user_name_label);
    header_layout->addWidget(cartridge_label);
    header_layout->addStretch();
    header_layout->addWidget(clock_label);
    header_layout->addSpacing(12);
    header_layout->addWidget(wifi_label);
    header_layout->addSpacing(12);
    header_layout->addWidget(battery_label);

    main_layout->addWidget(header_frame);

    // Clock timer
    clock_timer = new QTimer(this);
    connect(clock_timer, &QTimer::timeout, this, &SwitchHomeWidget::UpdateClock);
    clock_timer->start(1000);
    UpdateClock();

    // ------------------------------------------------------------------------
    // Search Box (Overlay/Toggleable)
    // ------------------------------------------------------------------------
    search_container = new QWidget(this);
    search_container->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* search_layout = new QHBoxLayout(search_container);
    search_layout->setContentsMargins(0, 4, 0, 4);

    search_edit = new QLineEdit(this);
    search_edit->setPlaceholderText(tr("🔍 Buscar juegos... (Pulsa Esc para cerrar)"));
    search_edit->setStyleSheet(
        QStringLiteral("QLineEdit { background-color: #3d3e42; color: #ffffff; border: 2px solid "
                       "#00d4ff; border-radius: 8px; padding: 6px 14px; font-size: 14px; }"));
    connect(search_edit, &QLineEdit::textChanged, this, &SwitchHomeWidget::OnSearchTextChanged);

    search_close_btn = new QPushButton(QStringLiteral("✕"), this);
    search_close_btn->setFixedSize(32, 32);
    search_close_btn->setStyleSheet(
        QStringLiteral("QPushButton { background-color: #4a4b50; color: #ffffff; border: none; "
                       "border-radius: 16px; font-weight: bold; } "
                       "QPushButton:hover { background-color: #e60012; }"));
    connect(search_close_btn, &QPushButton::clicked, this, &SwitchHomeWidget::ToggleSearch);

    search_layout->addStretch();
    search_layout->addWidget(search_edit, 3);
    search_layout->addWidget(search_close_btn);
    search_layout->addStretch();
    search_container->setVisible(false);

    main_layout->addWidget(search_container);

    // ------------------------------------------------------------------------
    // 2. Active Game Title & Details Header
    // ------------------------------------------------------------------------
    auto* title_container = new QWidget(this);
    title_container->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* title_layout = new QVBoxLayout(title_container);
    title_layout->setContentsMargins(0, 10, 0, 6);
    title_layout->setSpacing(4);

    active_title_label = new QLabel(tr("Selecciona un juego"), this);
    active_title_label->setStyleSheet(
        QStringLiteral("font-size: 26px; font-weight: bold; color: #ffffff;"));
    active_title_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    active_details_label = new QLabel(QStringLiteral(""), this);
    active_details_label->setStyleSheet(
        QStringLiteral("font-size: 13px; color: #a6abb0; font-weight: medium;"));
    active_details_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    title_layout->addWidget(active_title_label);
    title_layout->addWidget(active_details_label);
    main_layout->addWidget(title_container);

    // ------------------------------------------------------------------------
    // 3. Carousel of Game Cards
    // ------------------------------------------------------------------------
    scroll_area = new QScrollArea(this);
    scroll_area->setStyleSheet(
        QStringLiteral("QScrollArea { background: transparent; border: none; } "
                       "QScrollBar:horizontal { height: 0px; }"));
    scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area->setWidgetResizable(true);

    carousel_container = new QWidget(scroll_area);
    carousel_container->setStyleSheet(QStringLiteral("background: transparent;"));
    carousel_layout = new QHBoxLayout(carousel_container);
    carousel_layout->setContentsMargins(8, 8, 8, 8);
    carousel_layout->setSpacing(18);
    carousel_layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    empty_label = new QLabel(
        tr("No hay juegos en la biblioteca.\nPulsa 🛍️ o 'Añadir Carpeta' para cargar tus juegos."),
        carousel_container);
    empty_label->setStyleSheet(
        QStringLiteral("font-size: 16px; color: #8f9499; padding: 40px; border: 2px dashed "
                       "#4e5055; border-radius: 12px;"));
    empty_label->setAlignment(Qt::AlignCenter);
    carousel_layout->addWidget(empty_label);

    scroll_area->setWidget(carousel_container);
    main_layout->addWidget(scroll_area, 1);

    // ------------------------------------------------------------------------
    // 4. System Dock Ribbon (Circular Buttons)
    // ------------------------------------------------------------------------
    dock_container = new QWidget(this);
    dock_container->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* dock_layout = new QHBoxLayout(dock_container);
    dock_layout->setContentsMargins(0, 6, 0, 6);
    dock_layout->setSpacing(16);
    dock_layout->setAlignment(Qt::AlignCenter);

    struct DockSpec {
        QString icon;
        QString name;
        QColor color;
    };

    const std::vector<DockSpec> dock_specs = {
        {QStringLiteral("🔴"), tr("Online"), QColor(230, 0, 18)},
        {QStringLiteral("📰"), tr("Noticias"), QColor(0, 153, 229)},
        {QStringLiteral("🛍️"), tr("eShop"), QColor(255, 107, 0)},
        {QStringLiteral("📸"), tr("Álbum"), QColor(0, 180, 216)},
        {QStringLiteral("🎮"), tr("Mandos"), QColor(80, 84, 90)},
        {QStringLiteral("⚙️"), tr("Ajustes"), QColor(80, 84, 90)},
        {QStringLiteral("⏻"), tr("Apagar"), QColor(80, 84, 90)},
    };

    for (const auto& spec : dock_specs) {
        auto* btn = new SwitchCircleButton(spec.icon, spec.name, spec.color, dock_container);
        dock_buttons.push_back(btn);
        dock_layout->addWidget(btn);
    }

    // Connect dock buttons
    connect(dock_buttons[0], &QPushButton::clicked, this, [this] { emit MultiplayerRequested(); });
    connect(dock_buttons[1], &QPushButton::clicked, this, [this] {
        if (current_selected_index >= 0 && current_selected_index < static_cast<int>(games.size())) {
            OpenSelectedGameOptions();
        }
    });
    connect(dock_buttons[2], &QPushButton::clicked, this, [this] { emit AddDirectoryRequested(); });
    connect(dock_buttons[3], &QPushButton::clicked, this, [this] { emit AlbumRequested(); });
    connect(dock_buttons[4], &QPushButton::clicked, this, [this] { emit ControllersRequested(); });
    connect(dock_buttons[5], &QPushButton::clicked, this, [this] { emit SettingsRequested(); });
    connect(dock_buttons[6], &QPushButton::clicked, this, [this] { emit ExitRequested(); });

    main_layout->addWidget(dock_container);

    // ------------------------------------------------------------------------
    // 5. Footer Guide
    // ------------------------------------------------------------------------
    footer_frame = new QFrame(this);
    footer_frame->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto* footer_layout = new QHBoxLayout(footer_frame);
    footer_layout->setContentsMargins(0, 4, 0, 0);

    footer_guide_label = new QLabel(
        tr("Ⓨ Buscar  •  ⨁ Opciones  •  Ⓐ Iniciar  •  [F11] Pantalla Completa"), footer_frame);
    footer_guide_label->setStyleSheet(
        QStringLiteral("font-size: 13px; color: #8e9398; font-weight: bold;"));

    classic_view_btn = new QPushButton(tr("🖥️ Cambiar a Vista Clásica"), footer_frame);
    classic_view_btn->setCursor(Qt::PointingHandCursor);
    classic_view_btn->setStyleSheet(
        QStringLiteral("QPushButton { background-color: #3c3d42; color: #ffffff; border: 1px solid "
                       "#5a5c62; border-radius: 6px; padding: 6px 14px; font-size: 12px; "
                       "font-weight: bold; } QPushButton:hover { background-color: #00d4ff; color: "
                       "#000000; }"));
    connect(classic_view_btn, &QPushButton::clicked, this,
            [this] { emit ToggleClassicViewRequested(); });

    footer_layout->addWidget(footer_guide_label);
    footer_layout->addStretch();
    footer_layout->addWidget(classic_view_btn);

    main_layout->addWidget(footer_frame);
}

void SwitchHomeWidget::UpdateClock() {
    const QString time_str = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm AP"));
    clock_label->setText(time_str);
}

void SwitchHomeWidget::PopulateFromModel(QStandardItemModel* model) {
    all_games.clear();
    if (!model) {
        FilterGames();
        return;
    }

    const int row_count = model->rowCount();
    for (int i = 0; i < row_count; ++i) {
        QStandardItem* folder = model->item(i, 0);
        if (!folder) {
            continue;
        }

        const int children_count = folder->rowCount();
        for (int j = 0; j < children_count; ++j) {
            QStandardItem* child = folder->child(j, 0);
            if (!child) {
                continue;
            }

            const auto type = child->data(GameListItem::TypeRole).value<GameListItemType>();
            if (type != GameListItemType::Game) {
                continue;
            }

            SwitchGameEntry entry;
            entry.title = child->data(GameListItemPath::TitleRole).toString();
            entry.full_path = child->data(GameListItemPath::FullPathRole).toString();
            entry.program_id = child->data(GameListItemPath::ProgramIdRole).toULongLong();
            entry.file_type = child->data(GameListItemPath::FileTypeRole).toString();

            // Additional metadata columns
            QStandardItem* addon_item = folder->child(j, GameList::COLUMN_ADD_ONS);
            if (addon_item) {
                entry.version = addon_item->data(Qt::DisplayRole).toString();
            }

            QStandardItem* compat_item = folder->child(j, GameList::COLUMN_COMPATIBILITY);
            if (compat_item) {
                entry.compatibility = compat_item->data(Qt::DisplayRole).toString();
            }

            QStandardItem* time_item = folder->child(j, GameList::COLUMN_PLAY_TIME);
            if (time_item) {
                entry.play_time = time_item->data(Qt::DisplayRole).toString();
            }

            // Check for external 1024x1024 cover in ~/.local/share/yuzu/covers/
            const QString covers_dir = QDir::homePath() + QStringLiteral("/.local/share/yuzu/covers");
            const QString pid_hex_upper =
                QStringLiteral("%1").arg(entry.program_id, 16, 16, QLatin1Char('0')).toUpper();
            const QString pid_hex_lower = pid_hex_upper.toLower();

            const QStringList candidate_cover_paths = {
                covers_dir + QStringLiteral("/") + pid_hex_upper + QStringLiteral(".jpg"),
                covers_dir + QStringLiteral("/") + pid_hex_upper + QStringLiteral(".png"),
                covers_dir + QStringLiteral("/") + pid_hex_lower + QStringLiteral(".jpg"),
                covers_dir + QStringLiteral("/") + pid_hex_lower + QStringLiteral(".png"),
                QFileInfo(entry.full_path).dir().filePath(pid_hex_upper + QStringLiteral(".jpg")),
                QFileInfo(entry.full_path).dir().filePath(pid_hex_upper + QStringLiteral(".png")),
                QFileInfo(entry.full_path).dir().filePath(
                    QFileInfo(entry.full_path).completeBaseName() + QStringLiteral(".jpg")),
                QFileInfo(entry.full_path).dir().filePath(
                    QFileInfo(entry.full_path).completeBaseName() + QStringLiteral(".png")),
            };

            bool found_hires_cover = false;
            for (const auto& path : candidate_cover_paths) {
                if (QFile::exists(path)) {
                    QPixmap hires_pix(path);
                    if (!hires_pix.isNull()) {
                        entry.icon = hires_pix;
                        found_hires_cover = true;
                        break;
                    }
                }
            }

            if (!found_hires_cover) {
                // Check HiResIconRole from ROM (256x256 unscaled)
                const QVariant hires = child->data(GameListItemPath::HiResIconRole);
                if (hires.canConvert<QPixmap>()) {
                    const QPixmap pix = hires.value<QPixmap>();
                    if (!pix.isNull()) {
                        entry.icon = pix;
                        found_hires_cover = true;
                    }
                }
            }

            if (!found_hires_cover) {
                const QVariant decor = child->data(Qt::DecorationRole);
                if (decor.canConvert<QPixmap>()) {
                    entry.icon = decor.value<QPixmap>();
                } else if (decor.canConvert<QIcon>()) {
                    entry.icon = decor.value<QIcon>().pixmap(256, 256);
                }
            }

            entry.model_index = child->index();
            all_games.push_back(entry);
        }
    }

    FilterGames();
}

void SwitchHomeWidget::FilterGames() {
    const QString filter_text = search_edit->text().trimmed().toLower();
    games.clear();

    for (const auto& game : all_games) {
        if (filter_text.isEmpty() || game.title.toLower().contains(filter_text) ||
            QString::number(game.program_id, 16).contains(filter_text)) {
            games.push_back(game);
        }
    }

    // Rebuild carousel widgets
    qDeleteAll(card_widgets);
    card_widgets.clear();

    if (games.empty()) {
        empty_label->setVisible(true);
        active_title_label->setText(tr("Ningún juego disponible"));
        active_details_label->setText(QStringLiteral(""));
        current_selected_index = -1;
    } else {
        empty_label->setVisible(false);
        for (int i = 0; i < static_cast<int>(games.size()); ++i) {
            auto* card = new SwitchGameCard(games[i], i, carousel_container);
            connect(card, &SwitchGameCard::Clicked, this, &SwitchHomeWidget::SelectGame);
            connect(card, &SwitchGameCard::DoubleClicked, this,
                    [this](int) { LaunchSelectedGame(); });
            connect(card, &SwitchGameCard::ContextMenuRequested, this,
                    [this](int idx, const QPoint& pos) {
                        if (idx >= 0 && idx < static_cast<int>(games.size())) {
                            SelectGame(idx);
                            emit OpenGameContextMenu(pos, games[idx].program_id,
                                                    games[idx].full_path.toStdString());
                        }
                    });

            card_widgets.push_back(card);
            carousel_layout->addWidget(card);
        }

        SelectGame(0);
    }
}

void SwitchHomeWidget::SelectGame(int index) {
    if (games.empty()) {
        current_selected_index = -1;
        UpdateActiveGameDetails();
        return;
    }

    current_selected_index = std::clamp(index, 0, static_cast<int>(games.size()) - 1);

    for (int i = 0; i < static_cast<int>(card_widgets.size()); ++i) {
        card_widgets[i]->SetSelected(i == current_selected_index);
    }

    UpdateActiveGameDetails();
    CenterOnSelectedCard();
}

void SwitchHomeWidget::SelectNextGame() {
    if (!games.empty()) {
        SelectGame((current_selected_index + 1) % games.size());
    }
}

void SwitchHomeWidget::SelectPreviousGame() {
    if (!games.empty()) {
        SelectGame((current_selected_index - 1 + games.size()) % games.size());
    }
}

void SwitchHomeWidget::CenterOnSelectedCard() {
    if (current_selected_index < 0 ||
        current_selected_index >= static_cast<int>(card_widgets.size())) {
        return;
    }

    auto* card = card_widgets[current_selected_index];
    scroll_area->ensureWidgetVisible(card, 150, 0);
}

void SwitchHomeWidget::UpdateActiveGameDetails() {
    if (current_selected_index < 0 || current_selected_index >= static_cast<int>(games.size())) {
        active_title_label->setText(tr("Selecciona un juego"));
        active_details_label->setText(QStringLiteral(""));
        return;
    }

    const auto& game = games[current_selected_index];
    active_title_label->setText(game.title);

    QString details;
    if (!game.file_type.isEmpty()) {
        details += QStringLiteral("Formato: %1").arg(game.file_type);
    }
    if (!game.version.isEmpty()) {
        if (!details.isEmpty())
            details += QStringLiteral("  •  ");
        details += QStringLiteral("Versión: %1").arg(game.version);
    }
    if (!game.compatibility.isEmpty()) {
        if (!details.isEmpty())
            details += QStringLiteral("  •  ");
        details += QStringLiteral("Compatibilidad: %1").arg(game.compatibility);
    }
    if (!game.play_time.isEmpty()) {
        if (!details.isEmpty())
            details += QStringLiteral("  •  ");
        details += QStringLiteral("Tiempo: %1").arg(game.play_time);
    }
    if (game.program_id != 0) {
        if (!details.isEmpty())
            details += QStringLiteral("  •  ");
        details +=
            QStringLiteral("ID: %1").arg(game.program_id, 16, 16, QLatin1Char('0')).toUpper();
    }

    active_details_label->setText(details);
}

void SwitchHomeWidget::LaunchSelectedGame() {
    if (current_selected_index >= 0 && current_selected_index < static_cast<int>(games.size())) {
        const auto& game = games[current_selected_index];
        emit BootGame(game.full_path, StartGameType::Normal);
    }
}

void SwitchHomeWidget::OpenSelectedGameOptions() {
    if (current_selected_index >= 0 && current_selected_index < static_cast<int>(games.size())) {
        const auto& game = games[current_selected_index];
        auto* card = card_widgets[current_selected_index];
        const QPoint global_pos = card->mapToGlobal(QPoint(card->width() / 2, card->height() / 2));
        emit OpenGameContextMenu(global_pos, game.program_id, game.full_path.toStdString());
    }
}

void SwitchHomeWidget::ToggleSearch() {
    const bool show = !search_container->isVisible();
    search_container->setVisible(show);
    if (show) {
        search_edit->setFocus();
        search_edit->selectAll();
    } else {
        search_edit->clear();
        setFocus();
    }
}

void SwitchHomeWidget::OnSearchTextChanged(const QString&) {
    FilterGames();
}

void SwitchHomeWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Left:
        if (current_button_focus >= 0) {
            dock_buttons[current_button_focus]->SetFocused(false);
            current_button_focus =
                std::max(0, current_button_focus - 1);
            dock_buttons[current_button_focus]->SetFocused(true);
        } else {
            SelectPreviousGame();
        }
        event->accept();
        return;
    case Qt::Key_Right:
        if (current_button_focus >= 0) {
            dock_buttons[current_button_focus]->SetFocused(false);
            current_button_focus =
                std::min(static_cast<int>(dock_buttons.size()) - 1, current_button_focus + 1);
            dock_buttons[current_button_focus]->SetFocused(true);
        } else {
            SelectNextGame();
        }
        event->accept();
        return;
    case Qt::Key_Down:
        if (current_button_focus == -1 && !dock_buttons.empty()) {
            current_button_focus = 0;
            dock_buttons[0]->SetFocused(true);
        }
        event->accept();
        return;
    case Qt::Key_Up:
        if (current_button_focus >= 0) {
            dock_buttons[current_button_focus]->SetFocused(false);
            current_button_focus = -1;
        }
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (current_button_focus >= 0) {
            dock_buttons[current_button_focus]->click();
        } else {
            LaunchSelectedGame();
        }
        event->accept();
        return;
    case Qt::Key_Y:
        ToggleSearch();
        event->accept();
        return;
    case Qt::Key_Plus:
        OpenSelectedGameOptions();
        event->accept();
        return;
    case Qt::Key_F11:
        emit ToggleFullscreenRequested();
        event->accept();
        return;
    case Qt::Key_Tab:
        emit ToggleClassicViewRequested();
        event->accept();
        return;
    case Qt::Key_Escape:
        if (search_container->isVisible()) {
            ToggleSearch();
            event->accept();
            return;
        }
        break;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void SwitchHomeWidget::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() > 0) {
        SelectPreviousGame();
    } else if (event->angleDelta().y() < 0) {
        SelectNextGame();
    }
    event->accept();
}

void SwitchHomeWidget::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    // Draw sleek Switch home gradient background
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(46, 48, 51));
    bg.setColorAt(0.5, QColor(41, 43, 46));
    bg.setColorAt(1.0, QColor(36, 38, 40));
    p.fillRect(rect(), bg);

    QWidget::paintEvent(event);
}

void SwitchHomeWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    CenterOnSelectedCard();
}
