#include "MainAppUiBuilder.hpp"
#include "AppInfo.hpp"

#include "MainApp.hpp"
#include "MainAppEditActions.hpp"
#include "MainAppHelpActions.hpp"
#include "UiTranslator.hpp"
#include "Language.hpp"
#include "CategoryLanguage.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QDir>
#include <QDockWidget>
#include <QFileSystemModel>
#include <QItemSelectionModel>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLinearGradient>
#include <QComboBox>
#include <QFontMetrics>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QToolButton>
#include <QSize>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyleOption>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>

#include <algorithm>
#include <functional>

namespace {

class DisclosureToggleButton final : public QToolButton {
public:
    explicit DisclosureToggleButton(QWidget* parent = nullptr)
        : QToolButton(parent)
    {
        setCheckable(true);
        setToolButtonStyle(Qt::ToolButtonIconOnly);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setArrowType(Qt::NoArrow);
        setAutoRaise(true);
        setFixedSize(QSize(14, 14));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        QStyleOption option;
        option.initFrom(this);
        option.rect = rect();
        option.state |= QStyle::State_Children;
        if (isChecked()) {
            option.state |= QStyle::State_Open;
        }
        style()->drawPrimitive(QStyle::PE_IndicatorBranch, &option, &painter, this);
    }
};

using MenuIconPainter = std::function<void(QPainter&, const QRectF&, bool)>;

QColor with_enabled_alpha(QColor color, bool enabled, int disabled_alpha = 110)
{
    if (!enabled) {
        color.setAlpha(std::min(color.alpha(), disabled_alpha));
    }
    return color;
}

QIcon draw_menu_icon(MainApp& app, const MenuIconPainter& painter_fn)
{
    int target_size = app.style()->pixelMetric(QStyle::PM_SmallIconSize);
    if (target_size <= 0) {
        target_size = 16;
    }
    const int padding = std::max(4, target_size / 4);
    const QSize canvas_size(target_size + padding * 2, target_size + padding * 2);

    QIcon icon;
    for (QIcon::Mode mode : {QIcon::Normal, QIcon::Disabled}) {
        const bool enabled = mode != QIcon::Disabled;
        QPixmap canvas(canvas_size);
        canvas.fill(Qt::transparent);

        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter_fn(painter, QRectF(padding, padding, target_size, target_size), enabled);
        painter.end();

        icon.addPixmap(canvas, mode);
    }

    return icon;
}

QPainterPath sparkle_path(const QPointF& center, qreal outer_radius, qreal inner_radius)
{
    const qreal diagonal = inner_radius * 0.72;
    QPainterPath path;
    path.moveTo(center.x(), center.y() - outer_radius);
    path.lineTo(center.x() + diagonal, center.y() - diagonal);
    path.lineTo(center.x() + outer_radius, center.y());
    path.lineTo(center.x() + diagonal, center.y() + diagonal);
    path.lineTo(center.x(), center.y() + outer_radius);
    path.lineTo(center.x() - diagonal, center.y() + diagonal);
    path.lineTo(center.x() - outer_radius, center.y());
    path.lineTo(center.x() - diagonal, center.y() - diagonal);
    path.closeSubpath();
    return path;
}

void draw_card_label(QPainter& painter,
                     const QRectF& card_rect,
                     const QString& text,
                     const QColor& color,
                     qreal size_factor)
{
    QFont font = QApplication::font();
    font.setBold(true);
    font.setPixelSize(std::max(7, static_cast<int>(card_rect.height() * size_factor)));
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(card_rect, Qt::AlignCenter, text);
}

void draw_translation_cards(QPainter& painter,
                            const QRectF& rect,
                            bool enabled,
                            const QColor& front_start,
                            const QColor& front_end,
                            const QColor& front_label,
                            const QColor& top_start,
                            const QColor& top_end,
                            const QColor& top_label,
                            const QColor& bottom_start,
                            const QColor& bottom_end,
                            const QColor& bottom_label)
{
    const QRectF top_card(rect.left() + rect.width() * 0.42,
                          rect.top() + rect.height() * 0.03,
                          rect.width() * 0.50,
                          rect.height() * 0.50);
    const QRectF bottom_card(rect.left() + rect.width() * 0.36,
                             rect.top() + rect.height() * 0.47,
                             rect.width() * 0.50,
                             rect.height() * 0.50);
    const QRectF front_card(rect.left() + rect.width() * 0.04,
                            rect.top() + rect.height() * 0.22,
                            rect.width() * 0.50,
                            rect.height() * 0.50);

    const auto draw_card = [&](const QRectF& card,
                               const QColor& start,
                               const QColor& end,
                               const QString& label,
                               const QColor& label_color,
                               qreal label_size) {
        QLinearGradient gradient(card.topLeft(), card.bottomRight());
        gradient.setColorAt(0.0, with_enabled_alpha(start, enabled));
        gradient.setColorAt(1.0, with_enabled_alpha(end, enabled));
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        const qreal radius = std::max<qreal>(2.0, card.width() * 0.16);
        painter.drawRoundedRect(card, radius, radius);
        draw_card_label(painter, card, label, with_enabled_alpha(label_color, enabled), label_size);
    };

    draw_card(top_card,
              top_start,
              top_end,
              QString(QChar(0x3042)),
              top_label,
              0.44);
    draw_card(bottom_card,
              bottom_start,
              bottom_end,
              QString(QChar(0x6587)),
              bottom_label,
              0.40);
    draw_card(front_card,
              front_start,
              front_end,
              QStringLiteral("A"),
              front_label,
              0.54);
}

QIcon llm_menu_icon(MainApp& app)
{
    return draw_menu_icon(app, [](QPainter& painter, const QRectF& rect, bool enabled) {
        const QPointF main_center(rect.center().x() + rect.width() * 0.06, rect.center().y() - rect.height() * 0.02);
        const QPointF accent_center(rect.left() + rect.width() * 0.28, rect.top() + rect.height() * 0.28);
        const qreal main_outer = rect.width() * 0.27;
        const qreal main_inner = rect.width() * 0.12;
        const qreal accent_outer = rect.width() * 0.13;
        const qreal accent_inner = rect.width() * 0.06;

        QLinearGradient main_gradient(main_center.x() - main_outer,
                                      main_center.y() - main_outer,
                                      main_center.x() + main_outer,
                                      main_center.y() + main_outer);
        main_gradient.setColorAt(0.0, with_enabled_alpha(QColor("#ffd866"), enabled));
        main_gradient.setColorAt(1.0, with_enabled_alpha(QColor("#ff6b9f"), enabled));
        painter.setPen(Qt::NoPen);
        painter.setBrush(main_gradient);
        painter.drawPath(sparkle_path(main_center, main_outer, main_inner));

        painter.setBrush(with_enabled_alpha(QColor("#8a7cff"), enabled));
        painter.drawPath(sparkle_path(accent_center, accent_outer, accent_inner));
        painter.drawEllipse(QRectF(rect.right() - rect.width() * 0.17,
                                   rect.bottom() - rect.height() * 0.19,
                                   rect.width() * 0.09,
                                   rect.width() * 0.09));
    });
}

QIcon interface_language_menu_icon(MainApp& app)
{
    return draw_menu_icon(app, [](QPainter& painter, const QRectF& rect, bool enabled) {
        draw_translation_cards(painter,
                               rect,
                               enabled,
                               QColor("#1a59c9"),
                               QColor("#1847b7"),
                               QColor("#d8b4fe"),
                               QColor("#6677ff"),
                               QColor("#5160e8"),
                               QColor("#bcdcff"),
                               QColor("#4fc0ff"),
                               QColor("#37a6f5"),
                               QColor("#dff7ff"));
    });
}

QIcon category_language_menu_icon(MainApp& app)
{
    return draw_menu_icon(app, [](QPainter& painter, const QRectF& rect, bool enabled) {
        draw_translation_cards(painter,
                               rect,
                               enabled,
                               QColor("#1f2937"),
                               QColor("#374151"),
                               QColor("#f9fafb"),
                               QColor("#f3f4f6"),
                               QColor("#d7dbe3"),
                               QColor("#1f2937"),
                               QColor("#6b7280"),
                               QColor("#4b5563"),
                               QColor("#f9fafb"));
    });
}

QPainterPath tag_path(const QRectF& rect)
{
    QPainterPath path;
    path.moveTo(rect.left() + rect.width() * 0.14, rect.top() + rect.height() * 0.24);
    path.lineTo(rect.left() + rect.width() * 0.66, rect.top() + rect.height() * 0.24);
    path.lineTo(rect.right() - rect.width() * 0.06, rect.center().y());
    path.lineTo(rect.left() + rect.width() * 0.66, rect.bottom() - rect.height() * 0.24);
    path.lineTo(rect.left() + rect.width() * 0.14, rect.bottom() - rect.height() * 0.24);
    path.closeSubpath();
    return path;
}

QIcon whitelist_menu_icon(MainApp& app)
{
    return draw_menu_icon(app, [](QPainter& painter, const QRectF& rect, bool enabled) {
        const QRectF rear_tag = rect.adjusted(rect.width() * 0.12, rect.height() * 0.06,
                                              -rect.width() * 0.08, -rect.height() * 0.14);
        const QRectF front_tag = rect.adjusted(rect.width() * 0.02, rect.height() * 0.16,
                                               -rect.width() * 0.18, -rect.height() * 0.04);

        painter.setPen(Qt::NoPen);
        painter.setBrush(with_enabled_alpha(QColor("#f472b6"), enabled, 95));
        painter.drawPath(tag_path(rear_tag));

        QLinearGradient gradient(front_tag.topLeft(), front_tag.bottomRight());
        gradient.setColorAt(0.0, with_enabled_alpha(QColor("#ffe08a"), enabled));
        gradient.setColorAt(1.0, with_enabled_alpha(QColor("#ffc857"), enabled));
        painter.setBrush(gradient);
        painter.setPen(QPen(with_enabled_alpha(QColor("#1f2937"), enabled), 1.1,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(tag_path(front_tag));

        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::transparent);
        painter.drawEllipse(QRectF(front_tag.left() + front_tag.width() * 0.12,
                                   front_tag.center().y() - front_tag.height() * 0.09,
                                   front_tag.width() * 0.13,
                                   front_tag.width() * 0.13));
        painter.restore();

        QPainterPath check;
        check.moveTo(front_tag.left() + front_tag.width() * 0.40, front_tag.center().y());
        check.lineTo(front_tag.left() + front_tag.width() * 0.50, front_tag.center().y() + front_tag.height() * 0.12);
        check.lineTo(front_tag.left() + front_tag.width() * 0.68, front_tag.center().y() - front_tag.height() * 0.12);
        painter.setPen(QPen(with_enabled_alpha(QColor("#1f2937"), enabled), 1.5,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(check);
    });
}

} // namespace

void MainAppUiBuilder::build(MainApp& app) {
    build_central_panel(app);
    build_menus(app);
    app.analysis_in_progress_ = false;
    app.status_is_ready_ = true;
}

void MainAppUiBuilder::build_central_panel(MainApp& app) {
    app.setWindowTitle(app_display_name());
    app.resize(1000, 800);

    QWidget* central = new QWidget(&app);
    auto* main_layout = new QVBoxLayout(central);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(8);

    auto* path_layout = new QHBoxLayout();
    app.path_label = new QLabel(central);
    app.path_entry = new QLineEdit(central);
    app.browse_button = new QPushButton(central);
    path_layout->addWidget(app.path_label);
    path_layout->addWidget(app.path_entry, 1);
    path_layout->addWidget(app.browse_button);
    main_layout->addLayout(path_layout);

    auto* options_layout = new QHBoxLayout();
    app.use_subcategories_checkbox = new QCheckBox(central);
    app.categorize_files_checkbox = new QCheckBox(central);
    app.categorize_directories_checkbox = new QCheckBox(central);
    app.include_subdirectories_checkbox = new QCheckBox(central);
    app.categorize_files_checkbox->setChecked(true);
    options_layout->addWidget(app.use_subcategories_checkbox);
    options_layout->addWidget(app.categorize_files_checkbox);
    options_layout->addWidget(app.categorize_directories_checkbox);
    options_layout->addWidget(app.include_subdirectories_checkbox);
    options_layout->addStretch(1);
    main_layout->addLayout(options_layout);

    auto* document_options_layout = new QVBoxLayout();
    document_options_layout->setContentsMargins(0, 0, 0, 0);
    document_options_layout->setSpacing(4);
    auto* document_header_layout = new QHBoxLayout();
    document_header_layout->setContentsMargins(0, 0, 0, 0);
    document_header_layout->setSpacing(6);
    app.analyze_documents_checkbox = new QCheckBox(central);
    app.document_options_toggle_button = new DisclosureToggleButton(central);
    app.document_options_toggle_button->setChecked(false);
    document_header_layout->addWidget(app.analyze_documents_checkbox);
    document_header_layout->addWidget(app.document_options_toggle_button);
    document_header_layout->addStretch(1);
    document_options_layout->addLayout(document_header_layout);

    app.document_options_container = new QWidget(central);
    auto* document_rename_layout = new QVBoxLayout(app.document_options_container);
    document_rename_layout->setContentsMargins(24, 0, 0, 0);
    document_rename_layout->setSpacing(2);
    app.process_documents_only_checkbox = new QCheckBox(central);
    app.offer_rename_documents_checkbox = new QCheckBox(central);
    app.rename_documents_only_checkbox = new QCheckBox(central);
    app.add_document_date_to_category_checkbox = new QCheckBox(central);
    document_rename_layout->addWidget(app.process_documents_only_checkbox);
    document_rename_layout->addWidget(app.offer_rename_documents_checkbox);
    document_rename_layout->addWidget(app.rename_documents_only_checkbox);
    document_rename_layout->addWidget(app.add_document_date_to_category_checkbox);
    app.document_options_container->setVisible(false);
    document_options_layout->addWidget(app.document_options_container);
    main_layout->addLayout(document_options_layout);

    auto* image_options_layout = new QVBoxLayout();
    image_options_layout->setContentsMargins(0, 0, 0, 0);
    image_options_layout->setSpacing(4);
    auto* image_header_layout = new QHBoxLayout();
    image_header_layout->setContentsMargins(0, 0, 0, 0);
    image_header_layout->setSpacing(6);
    app.analyze_images_checkbox = new QCheckBox(central);
    app.image_options_toggle_button = new DisclosureToggleButton(central);
    app.image_options_toggle_button->setChecked(false);
    image_header_layout->addWidget(app.analyze_images_checkbox);
    image_header_layout->addWidget(app.image_options_toggle_button);
    image_header_layout->addStretch(1);
    image_options_layout->addLayout(image_header_layout);

    app.image_options_container = new QWidget(central);
    auto* image_rename_layout = new QVBoxLayout(app.image_options_container);
    image_rename_layout->setContentsMargins(24, 0, 0, 0);
    image_rename_layout->setSpacing(2);
    app.process_images_only_checkbox = new QCheckBox(central);
    app.add_image_date_to_category_checkbox = new QCheckBox(central);
    app.add_image_date_place_to_filename_checkbox = new QCheckBox(central);
    app.offer_rename_images_checkbox = new QCheckBox(central);
    app.rename_images_only_checkbox = new QCheckBox(central);
    image_rename_layout->addWidget(app.process_images_only_checkbox);
    image_rename_layout->addWidget(app.add_image_date_to_category_checkbox);
    image_rename_layout->addWidget(app.add_image_date_place_to_filename_checkbox);
    image_rename_layout->addWidget(app.offer_rename_images_checkbox);
    image_rename_layout->addWidget(app.rename_images_only_checkbox);
    app.image_options_container->setVisible(false);
    image_options_layout->addWidget(app.image_options_container);
    main_layout->addLayout(image_options_layout);

    app.add_audio_video_metadata_to_filename_checkbox = new QCheckBox(central);
    auto* audio_video_row = new QHBoxLayout();
    audio_video_row->setContentsMargins(0, 0, 0, 0);
    audio_video_row->addWidget(app.add_audio_video_metadata_to_filename_checkbox);
    audio_video_row->addStretch(1);
    main_layout->addLayout(audio_video_row);

    app.categorization_style_heading = new QLabel(central);
    app.categorization_style_refined_radio = new QRadioButton(central);
    app.categorization_style_consistent_radio = new QRadioButton(central);
    app.use_whitelist_checkbox = new QCheckBox(central);
    app.whitelist_selector = new QComboBox(central);
    app.whitelist_selector->setEnabled(false);
    app.whitelist_selector->setMinimumContentsLength(16);
    app.whitelist_selector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    QFontMetrics fm(app.whitelist_selector->font());
    app.whitelist_selector->setMinimumWidth(fm.horizontalAdvance(QString(16, QChar('W'))) + 5);

    app.analyze_button = new QPushButton(central);
    QIcon analyze_icon = QIcon::fromTheme(QStringLiteral("sparkle"));
    if (analyze_icon.isNull()) {
        analyze_icon = QIcon::fromTheme(QStringLiteral("applications-education"));
    }
    if (analyze_icon.isNull()) {
        analyze_icon = app.style()->standardIcon(QStyle::SP_MediaPlay);
    }
    app.analyze_button->setIcon(analyze_icon);
    app.analyze_button->setIconSize(QSize(20, 20));
    app.analyze_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    app.analyze_button->setMinimumWidth(160);
    auto* analyze_layout = new QHBoxLayout();
    auto* categorization_layout = new QVBoxLayout();
    auto* toggle_row = new QHBoxLayout();
    toggle_row->addWidget(app.categorization_style_refined_radio);
    toggle_row->addWidget(app.categorization_style_consistent_radio);
    toggle_row->addStretch();
    categorization_layout->addWidget(app.categorization_style_heading);
    categorization_layout->addLayout(toggle_row);

    auto* whitelist_row = new QHBoxLayout();
    whitelist_row->addWidget(app.use_whitelist_checkbox);
    whitelist_row->addWidget(app.whitelist_selector);
    whitelist_row->addStretch();

    auto* control_block = new QVBoxLayout();
    control_block->addLayout(categorization_layout);
    control_block->addSpacing(4);
    control_block->addLayout(whitelist_row);

    analyze_layout->addLayout(control_block);
    analyze_layout->addSpacing(12);
    analyze_layout->addWidget(app.analyze_button, 0, Qt::AlignBottom | Qt::AlignRight);
    main_layout->addLayout(analyze_layout);

    app.tree_model = new QStandardItemModel(0, 5, &app);

    app.results_stack = new QStackedWidget(central);

    app.tree_view = new QTreeView(app.results_stack);
    app.tree_view->setModel(app.tree_model);
    app.tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    app.tree_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    app.tree_view->header()->setSectionResizeMode(QHeaderView::Stretch);
    app.tree_view->setUniformRowHeights(true);
    app.tree_view_page_index_ = app.results_stack->addWidget(app.tree_view);

    app.folder_contents_model = new QFileSystemModel(app.results_stack);
    app.folder_contents_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    app.folder_contents_model->setRootPath(QDir::homePath());

    app.folder_contents_view = new QTreeView(app.results_stack);
    app.folder_contents_view->setModel(app.folder_contents_model);
    app.folder_contents_view->setRootIndex(app.folder_contents_model->index(QDir::homePath()));
    app.folder_contents_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    app.folder_contents_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    app.folder_contents_view->setRootIsDecorated(false);
    app.folder_contents_view->setUniformRowHeights(true);
    app.folder_contents_view->setSortingEnabled(true);
    app.folder_contents_view->sortByColumn(0, Qt::AscendingOrder);
    app.folder_contents_view->setAlternatingRowColors(true);
    app.folder_view_page_index_ = app.results_stack->addWidget(app.folder_contents_view);

    app.results_stack->setCurrentIndex(app.tree_view_page_index_);
    main_layout->addWidget(app.results_stack, 1);

    app.setCentralWidget(central);
}

UiTranslator::Dependencies MainAppUiBuilder::build_translator_dependencies(MainApp& app) const
{
    return UiTranslator::Dependencies{
        .window = app,
        .primary = UiTranslator::PrimaryControls{
            app.path_label,
            app.browse_button,
            app.analyze_button,
            app.use_subcategories_checkbox,
            app.categorization_style_heading,
            app.categorization_style_refined_radio,
            app.categorization_style_consistent_radio,
            app.use_whitelist_checkbox,
            app.whitelist_selector,
            app.categorize_files_checkbox,
            app.categorize_directories_checkbox,
            app.include_subdirectories_checkbox,
            app.analyze_images_checkbox,
            app.process_images_only_checkbox,
            app.add_image_date_to_category_checkbox,
            app.add_image_date_place_to_filename_checkbox,
            app.add_audio_video_metadata_to_filename_checkbox,
            app.offer_rename_images_checkbox,
            app.rename_images_only_checkbox,
            app.image_options_toggle_button,
            app.analyze_documents_checkbox,
            app.process_documents_only_checkbox,
            app.offer_rename_documents_checkbox,
            app.rename_documents_only_checkbox,
            app.add_document_date_to_category_checkbox,
            app.document_options_toggle_button},
        .tree_model = app.tree_model,
        .menus = UiTranslator::MenuControls{
            app.file_menu,
            app.edit_menu,
            app.view_menu,
            app.settings_menu,
            app.development_menu,
            app.development_settings_menu,
            app.language_menu,
            app.category_language_menu,
            app.help_menu},
        .actions = UiTranslator::ActionControls{
            app.file_quit_action,
            app.run_benchmark_action,
            app.copy_action,
            app.cut_action,
            app.undo_last_run_action,
            app.paste_action,
            app.delete_action,
            app.toggle_explorer_action,
            app.toggle_llm_action,
            app.manage_storage_plugins_action,
            app.manage_whitelists_action,
            app.development_prompt_logging_action,
            app.consistency_pass_action,
            app.english_action,
            app.dutch_action,
            app.french_action,
            app.german_action,
            app.italian_action,
            app.spanish_action,
            app.turkish_action,
            app.korean_action,
            app.category_language_english,
            app.category_language_french,
            app.category_language_german,
            app.category_language_italian,
            app.category_language_dutch,
            app.category_language_polish,
            app.category_language_portuguese,
            app.category_language_spanish,
            app.category_language_turkish,
            app.about_action,
            app.about_qt_action,
            app.about_agpl_action,
            app.support_project_action},
        .language = UiTranslator::LanguageControls{
            app.language_group,
            app.english_action,
            app.dutch_action,
            app.french_action,
            app.german_action,
            app.italian_action,
            app.spanish_action,
            app.turkish_action,
            app.korean_action},
        .category_language = UiTranslator::CategoryLanguageControls{
            app.category_language_group,
            app.category_language_dutch,
            app.category_language_english,
            app.category_language_french,
            app.category_language_german,
            app.category_language_italian,
            app.category_language_polish,
            app.category_language_portuguese,
            app.category_language_spanish,
            app.category_language_turkish},
        .file_explorer_dock = app.file_explorer_dock,
        .settings = app.settings,
        .translator = [](const char* source) {
            return QCoreApplication::translate("UiTranslator", source);
        }};
}

void MainAppUiBuilder::build_menus(MainApp& app) {
    build_file_menu(app);
    build_edit_menu(app);
    build_view_menu(app);
    build_settings_menu(app);
    if (app.is_development_mode()) {
        build_development_menu(app);
    }
    build_help_menu(app);
}

void MainAppUiBuilder::build_file_menu(MainApp& app) {
    app.file_menu = app.menuBar()->addMenu(QString());
    app.run_benchmark_action = app.file_menu->addAction(icon_for(app, "view-statistics", QStyle::SP_MediaPlay), QString());
    QObject::connect(app.run_benchmark_action, &QAction::triggered, &app, [&app]() {
        app.show_suitability_benchmark_dialog(false);
    });
    app.file_menu->addSeparator();
    app.file_quit_action = app.file_menu->addAction(icon_for(app, "application-exit", QStyle::SP_DialogCloseButton), QString());
    app.file_quit_action->setShortcut(QKeySequence::Quit);
    app.file_quit_action->setMenuRole(QAction::QuitRole);
    QObject::connect(app.file_quit_action, &QAction::triggered, qApp, &QApplication::quit);
}

void MainAppUiBuilder::build_edit_menu(MainApp& app) {
    app.edit_menu = app.menuBar()->addMenu(QString());

    app.undo_last_run_action = app.edit_menu->addAction(icon_for(app, "edit-undo", QStyle::SP_ArrowBack), QString());
    QObject::connect(app.undo_last_run_action, &QAction::triggered, &app, &MainApp::undo_last_run);

    app.copy_action = app.edit_menu->addAction(icon_for(app, "edit-copy", QStyle::SP_FileDialogContentsView), QString());
    QObject::connect(app.copy_action, &QAction::triggered, &app, [&app]() {
        MainAppEditActions::on_copy(app.path_entry);
    });
    app.copy_action->setShortcut(QKeySequence::Copy);

    app.cut_action = app.edit_menu->addAction(icon_for(app, "edit-cut", QStyle::SP_FileDialogDetailedView), QString());
    QObject::connect(app.cut_action, &QAction::triggered, &app, [&app]() {
        MainAppEditActions::on_cut(app.path_entry);
    });
    app.cut_action->setShortcut(QKeySequence::Cut);

    app.paste_action = app.edit_menu->addAction(icon_for(app, "edit-paste", QStyle::SP_FileDialogListView), QString());
    QObject::connect(app.paste_action, &QAction::triggered, &app, [&app]() {
        MainAppEditActions::on_paste(app.path_entry);
    });
    app.paste_action->setShortcut(QKeySequence::Paste);

    app.delete_action = app.edit_menu->addAction(icon_for(app, "edit-delete", QStyle::SP_TrashIcon), QString());
    QObject::connect(app.delete_action, &QAction::triggered, &app, [&app]() {
        MainAppEditActions::on_delete(app.path_entry);
    });
    app.delete_action->setShortcut(QKeySequence::Delete);
}

void MainAppUiBuilder::build_view_menu(MainApp& app) {
    app.view_menu = app.menuBar()->addMenu(QString());
    app.toggle_explorer_action = app.view_menu->addAction(icon_for(app, "system-file-manager", QStyle::SP_DirOpenIcon), QString());
    app.toggle_explorer_action->setCheckable(true);
    app.toggle_explorer_action->setChecked(app.settings.get_show_file_explorer());
    QObject::connect(app.toggle_explorer_action, &QAction::toggled, &app, [&app](bool checked) {
        if (app.file_explorer_dock) {
            app.file_explorer_dock->setVisible(checked);
        }
        app.settings.set_show_file_explorer(checked);
        app.update_results_view_mode();
    });
    app.file_explorer_menu_action = app.toggle_explorer_action;
}

void MainAppUiBuilder::build_settings_menu(MainApp& app) {
    app.settings_menu = app.menuBar()->addMenu(QString());
    app.toggle_llm_action = app.settings_menu->addAction(llm_menu_icon(app), QString());
    QObject::connect(app.toggle_llm_action, &QAction::triggered, &app, &MainApp::show_llm_selection_dialog);

    app.manage_storage_plugins_action = app.settings_menu->addAction(icon_for(app, "preferences-plugin", QStyle::SP_DriveFDIcon), QString());
    QObject::connect(app.manage_storage_plugins_action, &QAction::triggered, &app, &MainApp::show_storage_plugin_dialog);

    app.manage_whitelists_action = app.settings_menu->addAction(whitelist_menu_icon(app), QString());
    QObject::connect(app.manage_whitelists_action, &QAction::triggered, &app, &MainApp::show_whitelist_manager);

    app.language_menu = app.settings_menu->addMenu(QString());
    app.language_menu->setIcon(interface_language_menu_icon(app));
    if (app.language_menu->menuAction()) {
        app.language_menu->menuAction()->setIcon(interface_language_menu_icon(app));
    }
    app.language_group = new QActionGroup(&app);
    app.language_group->setExclusive(true);

    app.dutch_action = app.language_menu->addAction(QString());
    app.dutch_action->setCheckable(true);
    app.dutch_action->setData(static_cast<int>(Language::Dutch));
    app.language_group->addAction(app.dutch_action);

    app.english_action = app.language_menu->addAction(QString());
    app.english_action->setCheckable(true);
    app.english_action->setData(static_cast<int>(Language::English));
    app.language_group->addAction(app.english_action);

    app.french_action = app.language_menu->addAction(QString());
    app.french_action->setCheckable(true);
    app.french_action->setData(static_cast<int>(Language::French));
    app.language_group->addAction(app.french_action);
    app.german_action = app.language_menu->addAction(QString());
    app.german_action->setCheckable(true);
    app.german_action->setData(static_cast<int>(Language::German));
    app.language_group->addAction(app.german_action);
    app.italian_action = app.language_menu->addAction(QString());
    app.italian_action->setCheckable(true);
    app.italian_action->setData(static_cast<int>(Language::Italian));
    app.language_group->addAction(app.italian_action);
    app.korean_action = app.language_menu->addAction(QString());
    app.korean_action->setCheckable(true);
    app.korean_action->setData(static_cast<int>(Language::Korean));
    app.language_group->addAction(app.korean_action);
    app.spanish_action = app.language_menu->addAction(QString());
    app.spanish_action->setCheckable(true);
    app.spanish_action->setData(static_cast<int>(Language::Spanish));
    app.language_group->addAction(app.spanish_action);
    app.turkish_action = app.language_menu->addAction(QString());
    app.turkish_action->setCheckable(true);
    app.turkish_action->setData(static_cast<int>(Language::Turkish));
    app.language_group->addAction(app.turkish_action);

    QObject::connect(app.language_group, &QActionGroup::triggered, &app, [&app](QAction* action) {
        if (!action) {
            return;
        }
        const Language chosen = static_cast<Language>(action->data().toInt());
        app.on_language_selected(chosen);
    });

    app.category_language_menu = app.settings_menu->addMenu(QString());
    app.category_language_menu->setIcon(category_language_menu_icon(app));
    if (app.category_language_menu->menuAction()) {
        app.category_language_menu->menuAction()->setIcon(category_language_menu_icon(app));
    }
    app.category_language_group = new QActionGroup(&app);
    app.category_language_group->setExclusive(true);

    const auto add_cat_lang = [&](CategoryLanguage lang) {
        QAction* act = app.category_language_menu->addAction(QString());
        act->setCheckable(true);
        act->setData(static_cast<int>(lang));
        app.category_language_group->addAction(act);
        return act;
    };
    app.category_language_dutch = add_cat_lang(CategoryLanguage::Dutch);
    app.category_language_english = add_cat_lang(CategoryLanguage::English);
    app.category_language_french = add_cat_lang(CategoryLanguage::French);
    app.category_language_german = add_cat_lang(CategoryLanguage::German);
    app.category_language_italian = add_cat_lang(CategoryLanguage::Italian);
    app.category_language_polish = add_cat_lang(CategoryLanguage::Polish);
    app.category_language_portuguese = add_cat_lang(CategoryLanguage::Portuguese);
    app.category_language_spanish = add_cat_lang(CategoryLanguage::Spanish);
    app.category_language_turkish = add_cat_lang(CategoryLanguage::Turkish);

    QObject::connect(app.category_language_group, &QActionGroup::triggered, &app, [&app](QAction* action) {
        if (!action) {
            return;
        }
        const CategoryLanguage chosen = static_cast<CategoryLanguage>(action->data().toInt());
        app.on_category_language_selected(chosen);
    });
}

void MainAppUiBuilder::build_development_menu(MainApp& app) {
    app.development_menu = app.menuBar()->addMenu(QString());
    app.development_settings_menu = app.development_menu->addMenu(QString());
    app.development_prompt_logging_action = app.development_settings_menu->addAction(QString());
    app.development_prompt_logging_action->setCheckable(true);
    app.development_prompt_logging_action->setChecked(app.development_prompt_logging_enabled_);
    QObject::connect(app.development_prompt_logging_action, &QAction::toggled, &app, &MainApp::handle_development_prompt_logging);
}

void MainAppUiBuilder::build_help_menu(MainApp& app) {
    app.help_menu = app.menuBar()->addMenu(QString());
    if (app.help_menu && app.help_menu->menuAction()) {
        app.help_menu->menuAction()->setMenuRole(QAction::ApplicationSpecificRole);
    }

    app.about_action = app.help_menu->addAction(icon_for(app, "help-about", QStyle::SP_MessageBoxInformation), QString());
    app.about_action->setMenuRole(QAction::NoRole);
    QObject::connect(app.about_action, &QAction::triggered, &app, &MainApp::on_about_activate);

    app.about_qt_action = app.help_menu->addAction(icon_for(app, "help-about", QStyle::SP_MessageBoxInformation), QString());
    app.about_qt_action->setMenuRole(QAction::NoRole);
    QObject::connect(app.about_qt_action, &QAction::triggered, &app, [&app]() {
        QMessageBox::aboutQt(&app);
    });

    app.about_agpl_action = app.help_menu->addAction(icon_for(app, "help-about", QStyle::SP_MessageBoxInformation), QString());
    app.about_agpl_action->setMenuRole(QAction::NoRole);
    QObject::connect(app.about_agpl_action, &QAction::triggered, &app, [&app]() {
        MainAppHelpActions::show_agpl_info(&app);
    });

#ifdef _WIN32
    app.support_project_action = nullptr;
#else
    app.support_project_action = app.help_menu->addAction(icon_for(app, "help-donate", QStyle::SP_DialogHelpButton), QString());
    app.support_project_action->setMenuRole(QAction::NoRole);
    QObject::connect(app.support_project_action, &QAction::triggered, &app, []() {
        MainAppHelpActions::open_support_page();
    });
#endif
}

QIcon MainAppUiBuilder::icon_for(MainApp& app, const char* name, QStyle::StandardPixmap fallback) {
    QIcon icon = QIcon::fromTheme(QString::fromLatin1(name));
    if (icon.isNull()) {
        icon = app.style()->standardIcon(fallback);
    }
    if (!icon.isNull()) {
        const int targetSize = app.style()->pixelMetric(QStyle::PM_SmallIconSize);
        if (targetSize > 0) {
            QPixmap pixmap = icon.pixmap(targetSize, targetSize);
            if (!pixmap.isNull()) {
                const int padding = std::max(4, targetSize / 4);
                const QSize paddedSize(pixmap.width() + padding * 2, pixmap.height() + padding * 2);
                QPixmap padded(paddedSize);
                padded.fill(Qt::transparent);
                QPainter painter(&padded);
                painter.drawPixmap(padding, padding, pixmap);
                painter.end();
                QIcon paddedIcon;
                paddedIcon.addPixmap(padded, QIcon::Normal);
                paddedIcon.addPixmap(padded, QIcon::Disabled);
                icon = paddedIcon;
            }
        }
    }
    return icon;
}
