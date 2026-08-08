#ifndef APPSTYLE_H
#define APPSTYLE_H

#include <QString>
#include <QVector>

/** Shared NewWord chrome styles — selectable built-in themes. */
namespace AppStyle {

enum class ThemeId {
    ClassicBlue = 0, //!< Cool slate + ink blue (default)
    Graphite,        //!< Neutral gray chrome
    Forest,          //!< Deep green accent
    Ocean,           //!< Teal accent
    Midnight,        //!< Dark chrome (document page stays white)
};

struct Palette {
    QString accent;
    QString accentSoft;
    QString accentHover;
    QString accentDeep; //!< Slider / pressed deeper accent
    QString surface;
    QString surfaceRaised;
    QString ribbon;
    QString ribbonTab;
    QString desk;
    QString border;
    QString borderSoft;
    QString text;
    QString textMuted;
    QString scrollHandle;
    QString scrollHandleHover;
    QString tooltipBg;
    QString tooltipFg;
    QString disabledText;
};

struct ThemeInfo {
    ThemeId id;
    const char *key; //!< Stable QSettings value
};

[[nodiscard]] const Palette &palette();
[[nodiscard]] ThemeId currentTheme();
void setTheme(ThemeId id);
void loadThemeFromSettings();
void saveThemeToSettings();

[[nodiscard]] QVector<ThemeInfo> availableThemes();
[[nodiscard]] QString themeKey(ThemeId id);
[[nodiscard]] ThemeId themeFromKey(const QString &key);
[[nodiscard]] QString themeTitle(ThemeId id); //!< English title; translate in UI

[[nodiscard]] inline QString accent() { return palette().accent; }
[[nodiscard]] inline QString accentSoft() { return palette().accentSoft; }
[[nodiscard]] inline QString accentHover() { return palette().accentHover; }
[[nodiscard]] inline QString surface() { return palette().surface; }
[[nodiscard]] inline QString surfaceRaised() { return palette().surfaceRaised; }
[[nodiscard]] inline QString text() { return palette().text; }
[[nodiscard]] inline QString textMuted() { return palette().textMuted; }
[[nodiscard]] inline QString border() { return palette().border; }
[[nodiscard]] inline QString desk() { return palette().desk; }

[[nodiscard]] QString applicationStyleSheet();
[[nodiscard]] QString ribbonExpandedStyleSheet();
[[nodiscard]] QString ribbonCollapsedStyleSheet();
[[nodiscard]] QString ribbonGroupStyleSheet();
[[nodiscard]] QString ribbonToolButtonStyleSheet();
[[nodiscard]] QString ribbonComboStyleSheet();
[[nodiscard]] QString documentTabsStyleSheet();
[[nodiscard]] QString pageScrollStyleSheet();
[[nodiscard]] QString pageFrameStyleSheet(bool showBorder, int borderWidthPx, const QString &borderColor);
[[nodiscard]] QString continuousHostStyleSheet(const QString &background);
[[nodiscard]] QString rulerStyleSheet();
[[nodiscard]] QString outlinePaneStyleSheet();
[[nodiscard]] QString statusBarStyleSheet();

} // namespace AppStyle

#endif // APPSTYLE_H
