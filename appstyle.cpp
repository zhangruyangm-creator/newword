#include "appstyle.h"

#include <QSettings>

namespace AppStyle {
namespace {

Palette makeClassicBlue()
{
    Palette p;
    p.accent = QStringLiteral("#1a5fb4");
    p.accentSoft = QStringLiteral("#e8f1fb");
    p.accentHover = QStringLiteral("#d6e6f7");
    p.accentDeep = QStringLiteral("#154a9a");
    p.surface = QStringLiteral("#f7f8fa");
    p.surfaceRaised = QStringLiteral("#ffffff");
    p.ribbon = QStringLiteral("#f3f5f8");
    p.ribbonTab = QStringLiteral("#e9ecf1");
    p.desk = QStringLiteral("#9aa3ad");
    p.border = QStringLiteral("#d0d5dd");
    p.borderSoft = QStringLiteral("#e2e5eb");
    p.text = QStringLiteral("#1f2328");
    p.textMuted = QStringLiteral("#6b7280");
    p.scrollHandle = QStringLiteral("#b0b8c1");
    p.scrollHandleHover = QStringLiteral("#8e98a3");
    p.tooltipBg = QStringLiteral("#2c333a");
    p.tooltipFg = QStringLiteral("#f5f5f5");
    p.disabledText = QStringLiteral("#a0a6ae");
    return p;
}

Palette g_palette = makeClassicBlue();
ThemeId g_theme = ThemeId::ClassicBlue;

Palette makeGraphite()
{
    Palette p;
    p.accent = QStringLiteral("#4a5568");
    p.accentSoft = QStringLiteral("#eef0f3");
    p.accentHover = QStringLiteral("#e2e5ea");
    p.accentDeep = QStringLiteral("#2d3748");
    p.surface = QStringLiteral("#f4f5f7");
    p.surfaceRaised = QStringLiteral("#ffffff");
    p.ribbon = QStringLiteral("#eceef2");
    p.ribbonTab = QStringLiteral("#e2e5ea");
    p.desk = QStringLiteral("#8b939c");
    p.border = QStringLiteral("#c9ced6");
    p.borderSoft = QStringLiteral("#dce0e6");
    p.text = QStringLiteral("#1a1f26");
    p.textMuted = QStringLiteral("#5c6570");
    p.scrollHandle = QStringLiteral("#a8b0ba");
    p.scrollHandleHover = QStringLiteral("#7f8894");
    p.tooltipBg = QStringLiteral("#2c333a");
    p.tooltipFg = QStringLiteral("#f5f5f5");
    p.disabledText = QStringLiteral("#9aa1aa");
    return p;
}

Palette makeForest()
{
    Palette p;
    p.accent = QStringLiteral("#2d6a4f");
    p.accentSoft = QStringLiteral("#e8f5ef");
    p.accentHover = QStringLiteral("#d5ebe0");
    p.accentDeep = QStringLiteral("#1b4332");
    p.surface = QStringLiteral("#f6f8f6");
    p.surfaceRaised = QStringLiteral("#ffffff");
    p.ribbon = QStringLiteral("#eef3ef");
    p.ribbonTab = QStringLiteral("#e3ebe5");
    p.desk = QStringLiteral("#8fa396");
    p.border = QStringLiteral("#c5d0c8");
    p.borderSoft = QStringLiteral("#d8e2db");
    p.text = QStringLiteral("#1c2b22");
    p.textMuted = QStringLiteral("#5a6b61");
    p.scrollHandle = QStringLiteral("#a3b3aa");
    p.scrollHandleHover = QStringLiteral("#7e9186");
    p.tooltipBg = QStringLiteral("#243028");
    p.tooltipFg = QStringLiteral("#f5f5f5");
    p.disabledText = QStringLiteral("#9aaba0");
    return p;
}

Palette makeOcean()
{
    Palette p;
    p.accent = QStringLiteral("#0e7490");
    p.accentSoft = QStringLiteral("#e6f6fa");
    p.accentHover = QStringLiteral("#d0eef5");
    p.accentDeep = QStringLiteral("#0c5c72");
    p.surface = QStringLiteral("#f5f9fa");
    p.surfaceRaised = QStringLiteral("#ffffff");
    p.ribbon = QStringLiteral("#edf4f6");
    p.ribbonTab = QStringLiteral("#e0ebef");
    p.desk = QStringLiteral("#8aa0a8");
    p.border = QStringLiteral("#c4d2d8");
    p.borderSoft = QStringLiteral("#d7e2e7");
    p.text = QStringLiteral("#1a2a30");
    p.textMuted = QStringLiteral("#5a6d75");
    p.scrollHandle = QStringLiteral("#a4b5bc");
    p.scrollHandleHover = QStringLiteral("#7f939c");
    p.tooltipBg = QStringLiteral("#1e2e34");
    p.tooltipFg = QStringLiteral("#f5f5f5");
    p.disabledText = QStringLiteral("#9aadb4");
    return p;
}

Palette makeMidnight()
{
    Palette p;
    p.accent = QStringLiteral("#6cb6ff");
    p.accentSoft = QStringLiteral("#2a3544");
    p.accentHover = QStringLiteral("#344255");
    p.accentDeep = QStringLiteral("#4d9de8");
    p.surface = QStringLiteral("#1a1f28");
    p.surfaceRaised = QStringLiteral("#232a35");
    p.ribbon = QStringLiteral("#1e2530");
    p.ribbonTab = QStringLiteral("#2a3340");
    p.desk = QStringLiteral("#3d4654");
    p.border = QStringLiteral("#3a4452");
    p.borderSoft = QStringLiteral("#2f3845");
    p.text = QStringLiteral("#e8ecf1");
    p.textMuted = QStringLiteral("#9aa3b0");
    p.scrollHandle = QStringLiteral("#5a6575");
    p.scrollHandleHover = QStringLiteral("#7a8696");
    p.tooltipBg = QStringLiteral("#0f1318");
    p.tooltipFg = QStringLiteral("#f0f2f5");
    p.disabledText = QStringLiteral("#6b7380");
    return p;
}

Palette paletteFor(ThemeId id)
{
    switch (id) {
    case ThemeId::Graphite:
        return makeGraphite();
    case ThemeId::Forest:
        return makeForest();
    case ThemeId::Ocean:
        return makeOcean();
    case ThemeId::Midnight:
        return makeMidnight();
    case ThemeId::ClassicBlue:
    default:
        return makeClassicBlue();
    }
}

} // namespace

const Palette &palette()
{
    return g_palette;
}

ThemeId currentTheme()
{
    return g_theme;
}

void setTheme(ThemeId id)
{
    g_theme = id;
    g_palette = paletteFor(id);
}

void loadThemeFromSettings()
{
    QSettings settings;
    const QString key = settings.value(QStringLiteral("ui/theme"),
                                       QStringLiteral("classic_blue")).toString();
    setTheme(themeFromKey(key));
}

void saveThemeToSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/theme"), themeKey(g_theme));
}

QVector<ThemeInfo> availableThemes()
{
    return {
        {ThemeId::ClassicBlue, "classic_blue"},
        {ThemeId::Graphite, "graphite"},
        {ThemeId::Forest, "forest"},
        {ThemeId::Ocean, "ocean"},
        {ThemeId::Midnight, "midnight"},
    };
}

QString themeKey(ThemeId id)
{
    for (const ThemeInfo &info : availableThemes()) {
        if (info.id == id)
            return QLatin1String(info.key);
    }
    return QStringLiteral("classic_blue");
}

ThemeId themeFromKey(const QString &key)
{
    for (const ThemeInfo &info : availableThemes()) {
        if (key == QLatin1String(info.key))
            return info.id;
    }
    return ThemeId::ClassicBlue;
}

QString themeTitle(ThemeId id)
{
    switch (id) {
    case ThemeId::ClassicBlue:
        return QStringLiteral("Classic Blue");
    case ThemeId::Graphite:
        return QStringLiteral("Graphite");
    case ThemeId::Forest:
        return QStringLiteral("Forest");
    case ThemeId::Ocean:
        return QStringLiteral("Ocean");
    case ThemeId::Midnight:
        return QStringLiteral("Midnight");
    }
    return QStringLiteral("Classic Blue");
}

QString applicationStyleSheet()
{
    const Palette &p = g_palette;
    return QStringLiteral(
        "QMainWindow { background: %1; }"
        "QMenuBar {"
        "  background: %2;"
        "  border-bottom: 1px solid %3;"
        "  padding: 2px 4px;"
        "  color: %4;"
        "}"
        "QMenuBar::item { padding: 5px 10px; border-radius: 4px; }"
        "QMenuBar::item:selected { background: %5; color: %6; }"
        "QMenu {"
        "  background: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  padding: 4px;"
        "  color: %4;"
        "}"
        "QMenu::item { padding: 6px 28px 6px 12px; border-radius: 4px; }"
        "QMenu::item:selected { background: %5; color: %6; }"
        "QMenu::separator { height: 1px; background: %3; margin: 4px 8px; }"
        "QToolTip {"
        "  background: %7;"
        "  color: %8;"
        "  border: none;"
        "  padding: 5px 8px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 10px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %9;"
        "  border-radius: 5px;"
        "  min-height: 32px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: %10; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal {"
        "  background: transparent;"
        "  height: 10px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: %9;"
        "  border-radius: 5px;"
        "  min-width: 32px;"
        "}"
        "QScrollBar::handle:horizontal:hover { background: %10; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QSplitter::handle { background: %3; }"
        "QSplitter::handle:hover { background: %6; }")
        .arg(p.surface, p.surfaceRaised, p.border, p.text, p.accentSoft, p.accent,
             p.tooltipBg, p.tooltipFg, p.scrollHandle, p.scrollHandleHover);
}

QString ribbonExpandedStyleSheet()
{
    const Palette &p = g_palette;
    return QStringLiteral(
        "QTabWidget::pane {"
        "  border: none;"
        "  border-bottom: 1px solid %1;"
        "  background: %2;"
        "}"
        "QTabBar {"
        "  background: %3;"
        "  border-bottom: 1px solid %1;"
        "}"
        "QTabBar::tab {"
        "  background: transparent;"
        "  color: %4;"
        "  padding: 9px 20px;"
        "  margin: 0 1px;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "  min-width: 52px;"
        "}"
        "QTabBar::tab:hover {"
        "  color: %5;"
        "  background: %6;"
        "}"
        "QTabBar::tab:selected {"
        "  color: %5;"
        "  font-weight: 600;"
        "  background: transparent;"
        "  border-bottom: 2px solid %5;"
        "}")
        .arg(p.border, p.ribbon, p.surfaceRaised, p.textMuted, p.accent, p.accentSoft);
}

QString ribbonCollapsedStyleSheet()
{
    const Palette &p = g_palette;
    return QStringLiteral(
        "QTabWidget::pane {"
        "  border: none;"
        "  background: %1;"
        "  max-height: 0px;"
        "}"
        "QTabBar {"
        "  background: %1;"
        "  border-bottom: 1px solid %2;"
        "}"
        "QTabBar::tab {"
        "  background: transparent;"
        "  color: %3;"
        "  padding: 8px 18px;"
        "  margin: 0 1px;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "  min-width: 52px;"
        "}"
        "QTabBar::tab:hover { color: %4; background: %5; }"
        "QTabBar::tab:selected {"
        "  color: %4;"
        "  font-weight: 600;"
        "  border-bottom: 2px solid %4;"
        "}")
        .arg(p.surfaceRaised, p.border, p.textMuted, p.accent, p.accentSoft);
}

QString ribbonGroupStyleSheet()
{
    return QStringLiteral(
        "QFrame#ribbonGroup {"
        "  background: transparent;"
        "  border: none;"
        "  border-right: 1px solid %1;"
        "}")
        .arg(g_palette.borderSoft);
}

QString ribbonToolButtonStyleSheet()
{
    const Palette &p = g_palette;
    return QStringLiteral(
        "QToolButton {"
        "  color: %1;"
        "  background: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 5px;"
        "  padding: 4px 6px;"
        "}"
        "QToolButton:hover {"
        "  background: %2;"
        "  border: 1px solid %3;"
        "}"
        "QToolButton:pressed, QToolButton:checked {"
        "  background: %4;"
        "  border: 1px solid %5;"
        "}"
        "QToolButton:disabled { color: %6; }")
        .arg(p.text, p.accentSoft, p.border, p.accentHover, p.accent, p.disabledText);
}

QString ribbonComboStyleSheet()
{
    const Palette &p = g_palette;
    return QStringLiteral(
        "QComboBox, QFontComboBox {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 4px;"
        "  padding: 3px 22px 3px 8px;"
        "  color: %3;"
        "  min-height: 22px;"
        "}"
        "QComboBox:hover, QFontComboBox:hover { border-color: %4; }"
        "QComboBox::drop-down, QFontComboBox::drop-down {"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "  width: 20px;"
        "  border: none;"
        "  border-left: 1px solid %2;"
        "  background: transparent;"
        "}"
        "QComboBox::down-arrow, QFontComboBox::down-arrow {"
        "  image: url(:/resources/icons/chevron-down.png);"
        "  width: 10px;"
        "  height: 10px;"
        "}"
        "QComboBox QAbstractItemView, QFontComboBox QAbstractItemView {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  color: %3;"
        "  selection-background-color: %5;"
        "  selection-color: %3;"
        "  outline: 0;"
        "}")
        .arg(p.surfaceRaised, p.border, p.text, p.accent, p.accentSoft);
}

QString documentTabsStyleSheet()
{
    const Palette &p = g_palette;
    return QStringLiteral(
        "QTabWidget::pane { border: none; background: %1; }"
        "QTabBar::tab {"
        "  background: %2;"
        "  color: %3;"
        "  padding: 7px 16px;"
        "  margin-right: 1px;"
        "  border: none;"
        "  border-top: 2px solid transparent;"
        "}"
        "QTabBar::tab:hover { background: %4; color: %5; }"
        "QTabBar::tab:selected {"
        "  background: %6;"
        "  color: %5;"
        "  font-weight: 600;"
        "  border-top: 2px solid %5;"
        "}"
        "QTabBar::close-button { margin: 2px; }"
        "QTabBar::close-button:hover { background: %4; border-radius: 3px; }")
        .arg(p.desk, p.ribbonTab, p.textMuted, p.accentSoft, p.accent, p.surfaceRaised);
}

QString pageScrollStyleSheet()
{
    return QStringLiteral(
        "QScrollArea {"
        "  background: %1;"
        "  border: none;"
        "}")
        .arg(g_palette.desk);
}

QString pageFrameStyleSheet(bool showBorder, int borderWidthPx, const QString &borderColor)
{
    if (showBorder) {
        return QStringLiteral(
            "#pageFrame {"
            "  background: #ffffff;"
            "  border: %1px solid %2;"
            "}").arg(borderWidthPx).arg(borderColor);
    }
    return QStringLiteral(
        "#pageFrame {"
        "  background: #ffffff;"
        "  border: 1px solid #c8cdd4;"
        "}");
}

QString continuousHostStyleSheet(const QString &background)
{
    return QStringLiteral("background: %1;").arg(background);
}

QString rulerStyleSheet()
{
    const Palette &p = g_palette;
    return QStringLiteral(
        "RulerWidget, QWidget#rulerWidget {"
        "  background: %1;"
        "  border-bottom: 1px solid %2;"
        "}").arg(p.ribbon, p.border);
}

QString outlinePaneStyleSheet()
{
    const Palette &p = g_palette;
    return QStringLiteral(
        "#outlinePane {"
        "  background: %1;"
        "  border-right: 1px solid %2;"
        "}"
        "#outlineHeader {"
        "  background: %3;"
        "  border-bottom: 1px solid %2;"
        "}"
        "#outlineTitle {"
        "  font-weight: 600;"
        "  color: %4;"
        "  border: none;"
        "  background: transparent;"
        "  padding-left: 12px;"
        "  letter-spacing: 0.3px;"
        "}"
        "QToolButton#outlineClose {"
        "  border: none;"
        "  color: %5;"
        "  font-size: 15px;"
        "  padding: 0 10px;"
        "  border-radius: 4px;"
        "}"
        "QToolButton#outlineClose:hover {"
        "  color: %4;"
        "  background: %6;"
        "}"
        "QTreeWidget {"
        "  border: none;"
        "  background: %1;"
        "  outline: none;"
        "  color: %7;"
        "}"
        "QTreeWidget::item { padding: 6px 10px; border-radius: 4px; margin: 1px 6px; }"
        "QTreeWidget::item:hover { background: %6; }"
        "QTreeWidget::item:selected { background: %6; color: %4; font-weight: 500; }"
        "#outlineEmpty {"
        "  color: %5;"
        "  padding: 16px 14px;"
        "  background: transparent;"
        "  border: none;"
        "  line-height: 1.4;"
        "}")
        .arg(p.surface, p.border, p.surfaceRaised, p.accent, p.textMuted, p.accentSoft, p.text);
}

QString statusBarStyleSheet()
{
    const Palette &p = g_palette;
    return QStringLiteral(
        "QStatusBar {"
        "  background: %1;"
        "  border-top: 1px solid %2;"
        "  color: %3;"
        "  min-height: 26px;"
        "}"
        "QStatusBar::item { border: none; }"
        "QStatusBar QLabel {"
        "  color: %3;"
        "  padding: 0 8px;"
        "  border-left: 1px solid %2;"
        "}"
        "QSlider::groove:horizontal {"
        "  height: 4px;"
        "  background: %2;"
        "  border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  width: 12px;"
        "  height: 12px;"
        "  margin: -4px 0;"
        "  background: %4;"
        "  border-radius: 6px;"
        "}"
        "QSlider::handle:horizontal:hover { background: %5; }"
        "QSlider::sub-page:horizontal {"
        "  background: %4;"
        "  border-radius: 2px;"
        "}")
        .arg(p.surfaceRaised, p.border, p.textMuted, p.accent, p.accentDeep);
}

} // namespace AppStyle
