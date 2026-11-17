#pragma once

#include <QPushButton>
#include <QStyle>
#include <QStyleOptionButton>

#include <algorithm>

/**
 * @brief Action button with an uncached, content-derived size and minimum.
 * Full captions, icons and font-scaled padding are measured for the current style.
 */
class ContentSizedPushButton : public QPushButton {
public:
    using QPushButton::QPushButton;

    /**
     * @brief Measures the full label and lets the active style add its native chrome.
     * @return Natural size including font-scaled horizontal space on both sides.
     */
    QSize sizeHint() const override
    {
        ensurePolished();
        QStyleOptionButton option;
        initStyleOption(&option);
        QSize content = option.fontMetrics.size(Qt::TextShowMnemonic, option.text);
        content.setHeight(std::max(content.height(), option.fontMetrics.height()));
        if (!option.icon.isNull()) {
            constexpr int kQtIconTextGap = 4;
            content.rwidth() += option.iconSize.width() + (option.text.isEmpty() ? 0 : kQtIconTextGap);
            content.setHeight(std::max(content.height(), option.iconSize.height()));
        }
        option.rect.setSize(content);
        if (option.features.testFlag(QStyleOptionButton::HasMenu)) {
            content.rwidth() += style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &option, this);
        }
        // A font-height inset on each side scales with accessibility fonts and DPI.
        const int inset = std::max(option.fontMetrics.height(),
                                  style()->pixelMetric(QStyle::PM_ButtonMargin, &option, this));
        content.rwidth() += 2 * inset;
        return style()->sizeFromContents(QStyle::CT_PushButton, &option, content, this);
    }

    /**
     * @brief Prevents layouts from compressing the full caption and its padding.
     * @return The current content-derived natural size, without a fixed pixel width.
     */
    QSize minimumSizeHint() const override
    {
        return sizeHint();
    }
};
