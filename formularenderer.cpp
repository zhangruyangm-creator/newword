#include "formularenderer.h"

#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QColor>
#include <QUrl>
#include <QtMath>

#include <functional>
#include <utility>

namespace {

struct Box
{
    qreal width = 0;
    qreal height = 0; // above baseline
    qreal depth = 0;  // below baseline
    std::function<void(QPainter &, qreal, qreal)> draw;
};

Box emptyBox()
{
    return {};
}

Box hbox(const QList<Box> &parts, qreal spacing = 0)
{
    Box out;
    for (const Box &b : parts) {
        out.width += b.width + spacing;
        out.height = qMax(out.height, b.height);
        out.depth = qMax(out.depth, b.depth);
    }
    if (!parts.isEmpty())
        out.width -= spacing;

    out.draw = [parts, spacing](QPainter &p, qreal x, qreal baseline) {
        qreal cx = x;
        for (const Box &b : parts) {
            if (b.draw)
                b.draw(p, cx, baseline);
            cx += b.width + spacing;
        }
    };
    return out;
}

Box vcenter(const Box &inner, qreal axisHeight)
{
    Box out = inner;
    const qreal mid = (inner.height - inner.depth) / 2.0;
    const qreal shift = axisHeight - mid;
    out.height = inner.height + shift;
    out.depth = inner.depth - shift;
    out.draw = [inner, shift](QPainter &p, qreal x, qreal baseline) {
        if (inner.draw)
            inner.draw(p, x, baseline - shift);
    };
    return out;
}

Box textBox(const QString &text, const QFont &font)
{
    QFontMetricsF fm(font);
    Box b;
    b.width = fm.horizontalAdvance(text);
    b.height = fm.ascent();
    b.depth = fm.descent();
    b.draw = [text, font](QPainter &p, qreal x, qreal baseline) {
        p.setFont(font);
        p.drawText(QPointF(x, baseline), text);
    };
    return b;
}

Box spaceBox(qreal w)
{
    Box b;
    b.width = w;
    return b;
}

class Parser
{
public:
    Parser(QString latex, QFont font)
        : m_src(std::move(latex))
        , m_font(std::move(font))
        , m_metrics(m_font)
    {
    }

    Box parse()
    {
        skipWs();
        Box result = parseList(false);
        return result.draw ? result : textBox(QStringLiteral("?"), m_font);
    }

private:
    QString m_src;
    int m_pos = 0;
    QFont m_font;
    QFontMetricsF m_metrics;

    bool atEnd() const { return m_pos >= m_src.size(); }
    QChar peek() const { return atEnd() ? QChar() : m_src.at(m_pos); }

    void skipWs()
    {
        while (!atEnd() && peek().isSpace())
            ++m_pos;
    }

    bool consume(QChar ch)
    {
        skipWs();
        if (!atEnd() && peek() == ch) {
            ++m_pos;
            return true;
        }
        return false;
    }

    QString readCommand()
    {
        if (atEnd() || peek() != QLatin1Char('\\'))
            return {};
        ++m_pos;
        if (atEnd())
            return {};
        QChar c = peek();
        if (!c.isLetter()) {
            ++m_pos;
            return QString(c);
        }
        QString name;
        while (!atEnd() && peek().isLetter())
            name.append(m_src.at(m_pos++));
        return name;
    }

    Box parseGroup()
    {
        if (!consume(QLatin1Char('{')))
            return parseAtom(false);
        Box inner = parseList(true);
        consume(QLatin1Char('}'));
        return inner;
    }

    Box parseList(bool stopAtBrace)
    {
        QList<Box> parts;
        skipWs();
        while (!atEnd()) {
            if (stopAtBrace && peek() == QLatin1Char('}'))
                break;
            if (peek() == QLatin1Char('&') || peek() == QLatin1Char('\n')) {
                ++m_pos;
                continue;
            }
            const int posBefore = m_pos;
            Box atom = parseAtom(true);
            // attach scripts
            skipWs();
            Box sup;
            Box sub;
            bool hasSup = false;
            bool hasSub = false;
            while (true) {
                skipWs();
                if (consume(QLatin1Char('^'))) {
                    sup = parseScript();
                    hasSup = true;
                    continue;
                }
                if (consume(QLatin1Char('_'))) {
                    sub = parseScript();
                    hasSub = true;
                    continue;
                }
                break;
            }
            if (hasSup || hasSub)
                atom = attachScripts(std::move(atom), std::move(sup), hasSup,
                                     std::move(sub), hasSub);
            // Avoid infinite loop if nothing was consumed (e.g. stray '}').
            if (m_pos == posBefore) {
                if (stopAtBrace)
                    break;
                ++m_pos;
                continue;
            }
            parts.append(std::move(atom));
            skipWs();
        }
        if (parts.isEmpty())
            return emptyBox();
        if (parts.size() == 1)
            return parts.first();
        return hbox(parts, m_metrics.horizontalAdvance(QLatin1Char(' ')) * 0.08);
    }

    Box parseScript()
    {
        skipWs();
        const QFont saved = m_font;
        const QFontMetricsF savedMetrics = m_metrics;
        QFont scriptFont = m_font;
        scriptFont.setPointSizeF(m_font.pointSizeF() * 0.7);
        m_font = scriptFont;
        m_metrics = QFontMetricsF(m_font);
        Box b;
        if (peek() == QLatin1Char('{'))
            b = parseGroup();
        else
            b = parseAtom(false);
        m_font = saved;
        m_metrics = savedMetrics;
        return b;
    }

    Box attachScripts(Box base, Box sup, bool hasSup, Box sub, bool hasSub)
    {
        const qreal gap = m_font.pointSizeF() * 0.06;
        qreal scriptWidth = 0;
        qreal height = base.height;
        qreal depth = base.depth;
        if (hasSup) {
            scriptWidth = qMax(scriptWidth, sup.width);
            height = qMax(height, base.height + gap + sup.depth + sup.height * 0.35);
        }
        if (hasSub) {
            scriptWidth = qMax(scriptWidth, sub.width);
            depth = qMax(depth, base.depth + gap + sub.height + sub.depth * 0.2);
        }
        Box out;
        out.width = base.width + scriptWidth;
        out.height = height;
        out.depth = depth;
        out.draw = [base = std::move(base), hasSup, sup = std::move(sup), hasSub,
                    sub = std::move(sub), gap](QPainter &p, qreal x, qreal baseline) {
            if (base.draw)
                base.draw(p, x, baseline);
            const qreal sx = x + base.width;
            if (hasSup && sup.draw)
                sup.draw(p, sx, baseline - base.height - gap + sup.depth);
            if (hasSub && sub.draw)
                sub.draw(p, sx, baseline + base.depth + gap + sub.height);
        };
        return out;
    }

    Box parseAtom(bool allowEmpty)
    {
        skipWs();
        if (atEnd())
            return allowEmpty ? emptyBox() : textBox(QStringLiteral("?"), m_font);

        const QChar ch = peek();
        if (ch == QLatin1Char('{'))
            return parseGroup();

        if (ch == QLatin1Char('\\')) {
            const QString cmd = readCommand();
            return parseCommand(cmd);
        }

        if (ch == QLatin1Char('}') || ch == QLatin1Char('^') || ch == QLatin1Char('_'))
            return allowEmpty ? emptyBox() : textBox(QStringLiteral("?"), m_font);

        ++m_pos;
        if (ch == QLatin1Char('~'))
            return spaceBox(m_metrics.horizontalAdvance(QLatin1Char(' ')));

        QString mapped = mapSymbol(ch);
        QFont font = m_font;
        if (ch.isLetter())
            font.setItalic(true);
        return textBox(mapped, font);
    }

    static QString mapSymbol(QChar ch)
    {
        switch (ch.unicode()) {
        case '-': return QStringLiteral("−");
        case '*': return QStringLiteral("⋅");
        default: return QString(ch);
        }
    }

    Box parseCommand(const QString &cmd)
    {
        static const QHash<QString, QString> symbols = {
            {QStringLiteral("alpha"), QStringLiteral("α")},
            {QStringLiteral("beta"), QStringLiteral("β")},
            {QStringLiteral("gamma"), QStringLiteral("γ")},
            {QStringLiteral("delta"), QStringLiteral("δ")},
            {QStringLiteral("epsilon"), QStringLiteral("ε")},
            {QStringLiteral("varepsilon"), QStringLiteral("ε")},
            {QStringLiteral("zeta"), QStringLiteral("ζ")},
            {QStringLiteral("eta"), QStringLiteral("η")},
            {QStringLiteral("theta"), QStringLiteral("θ")},
            {QStringLiteral("iota"), QStringLiteral("ι")},
            {QStringLiteral("kappa"), QStringLiteral("κ")},
            {QStringLiteral("lambda"), QStringLiteral("λ")},
            {QStringLiteral("mu"), QStringLiteral("μ")},
            {QStringLiteral("nu"), QStringLiteral("ν")},
            {QStringLiteral("xi"), QStringLiteral("ξ")},
            {QStringLiteral("pi"), QStringLiteral("π")},
            {QStringLiteral("rho"), QStringLiteral("ρ")},
            {QStringLiteral("sigma"), QStringLiteral("σ")},
            {QStringLiteral("tau"), QStringLiteral("τ")},
            {QStringLiteral("phi"), QStringLiteral("φ")},
            {QStringLiteral("varphi"), QStringLiteral("ϕ")},
            {QStringLiteral("chi"), QStringLiteral("χ")},
            {QStringLiteral("psi"), QStringLiteral("ψ")},
            {QStringLiteral("omega"), QStringLiteral("ω")},
            {QStringLiteral("Gamma"), QStringLiteral("Γ")},
            {QStringLiteral("Delta"), QStringLiteral("Δ")},
            {QStringLiteral("Theta"), QStringLiteral("Θ")},
            {QStringLiteral("Lambda"), QStringLiteral("Λ")},
            {QStringLiteral("Xi"), QStringLiteral("Ξ")},
            {QStringLiteral("Pi"), QStringLiteral("Π")},
            {QStringLiteral("Sigma"), QStringLiteral("Σ")},
            {QStringLiteral("Phi"), QStringLiteral("Φ")},
            {QStringLiteral("Psi"), QStringLiteral("Ψ")},
            {QStringLiteral("Omega"), QStringLiteral("Ω")},
            {QStringLiteral("infty"), QStringLiteral("∞")},
            {QStringLiteral("pm"), QStringLiteral("±")},
            {QStringLiteral("mp"), QStringLiteral("∓")},
            {QStringLiteral("times"), QStringLiteral("×")},
            {QStringLiteral("cdot"), QStringLiteral("⋅")},
            {QStringLiteral("div"), QStringLiteral("÷")},
            {QStringLiteral("leq"), QStringLiteral("≤")},
            {QStringLiteral("geq"), QStringLiteral("≥")},
            {QStringLiteral("neq"), QStringLiteral("≠")},
            {QStringLiteral("approx"), QStringLiteral("≈")},
            {QStringLiteral("equiv"), QStringLiteral("≡")},
            {QStringLiteral("sim"), QStringLiteral("∼")},
            {QStringLiteral("propto"), QStringLiteral("∝")},
            {QStringLiteral("rightarrow"), QStringLiteral("→")},
            {QStringLiteral("leftarrow"), QStringLiteral("←")},
            {QStringLiteral("Rightarrow"), QStringLiteral("⇒")},
            {QStringLiteral("Leftrightarrow"), QStringLiteral("⇔")},
            {QStringLiteral("partial"), QStringLiteral("∂")},
            {QStringLiteral("nabla"), QStringLiteral("∇")},
            {QStringLiteral("forall"), QStringLiteral("∀")},
            {QStringLiteral("exists"), QStringLiteral("∃")},
            {QStringLiteral("in"), QStringLiteral("∈")},
            {QStringLiteral("notin"), QStringLiteral("∉")},
            {QStringLiteral("subset"), QStringLiteral("⊂")},
            {QStringLiteral("subseteq"), QStringLiteral("⊆")},
            {QStringLiteral("cup"), QStringLiteral("∪")},
            {QStringLiteral("cap"), QStringLiteral("∩")},
            {QStringLiteral("emptyset"), QStringLiteral("∅")},
            {QStringLiteral("ldots"), QStringLiteral("…")},
            {QStringLiteral("cdots"), QStringLiteral("⋯")},
            {QStringLiteral("degree"), QStringLiteral("°")},
            {QStringLiteral("angle"), QStringLiteral("∠")},
            {QStringLiteral("perp"), QStringLiteral("⊥")},
            {QStringLiteral("parallel"), QStringLiteral("∥")},
            {QStringLiteral("sum"), QStringLiteral("∑")},
            {QStringLiteral("prod"), QStringLiteral("∏")},
            {QStringLiteral("int"), QStringLiteral("∫")},
            {QStringLiteral("oint"), QStringLiteral("∮")},
            {QStringLiteral("ell"), QStringLiteral("ℓ")},
            {QStringLiteral("hbar"), QStringLiteral("ℏ")},
            {QStringLiteral("Re"), QStringLiteral("ℜ")},
            {QStringLiteral("Im"), QStringLiteral("ℑ")},
        };

        if (cmd == QLatin1String("frac")) {
            Box num = parseGroup();
            Box den = parseGroup();
            return makeFraction(num, den);
        }
        if (cmd == QLatin1String("sqrt")) {
            Box inner = parseGroup();
            return makeSqrt(inner);
        }
        if (cmd == QLatin1String("mathrm") || cmd == QLatin1String("text")
            || cmd == QLatin1String("textrm")) {
            const QFont saved = m_font;
            m_font.setItalic(false);
            m_metrics = QFontMetricsF(m_font);
            Box b = parseGroup();
            m_font = saved;
            m_metrics = QFontMetricsF(m_font);
            return b;
        }
        if (cmd == QLatin1String("mathbf") || cmd == QLatin1String("boldsymbol")) {
            const QFont saved = m_font;
            m_font.setBold(true);
            m_metrics = QFontMetricsF(m_font);
            Box b = parseGroup();
            m_font = saved;
            m_metrics = QFontMetricsF(m_font);
            return b;
        }
        if (cmd == QLatin1String("mathit")) {
            const QFont saved = m_font;
            m_font.setItalic(true);
            m_metrics = QFontMetricsF(m_font);
            Box b = parseGroup();
            m_font = saved;
            m_metrics = QFontMetricsF(m_font);
            return b;
        }
        if (cmd == QLatin1String("left") || cmd == QLatin1String("right")) {
            skipWs();
            if (!atEnd()) {
                QChar delim = peek();
                ++m_pos;
                if (delim == QLatin1Char('.'))
                    return emptyBox();
                return textBox(QString(delim), m_font);
            }
            return emptyBox();
        }
        if (cmd == QLatin1String(",") || cmd == QLatin1String("thinspace"))
            return spaceBox(m_font.pointSizeF() * 0.15);
        if (cmd == QLatin1String(";") || cmd == QLatin1String(" "))
            return spaceBox(m_font.pointSizeF() * 0.25);
        if (cmd == QLatin1String("quad"))
            return spaceBox(m_font.pointSizeF());
        if (cmd == QLatin1String("qquad"))
            return spaceBox(m_font.pointSizeF() * 2);
        if (cmd == QLatin1String("!"))
            return emptyBox(); // negative space omitted to avoid layout underflow
        if (cmd == QLatin1String("{") || cmd == QLatin1String("}"))
            return textBox(cmd, m_font);
        if (cmd == QLatin1String("\\"))
            return spaceBox(0);

        if (symbols.contains(cmd)) {
            QFont f = m_font;
            f.setItalic(false);
            Box sym = textBox(symbols.value(cmd), f);
            // Large ops a bit bigger
            if (cmd == QLatin1String("sum") || cmd == QLatin1String("prod")
                || cmd == QLatin1String("int") || cmd == QLatin1String("oint")) {
                QFont big = f;
                big.setPointSizeF(f.pointSizeF() * 1.35);
                sym = textBox(symbols.value(cmd), big);
            }
            return sym;
        }

        // Unknown command: show as text
        return textBox(QLatin1Char('\\') + cmd, m_font);
    }

    Box makeFraction(const Box &num, const Box &den)
    {
        const qreal pad = m_font.pointSizeF() * 0.12;
        const qreal bar = qMax<qreal>(1.0, m_font.pointSizeF() * 0.07);
        const qreal width = qMax(num.width, den.width) + pad * 2;
        Box out;
        out.width = width;
        out.height = num.height + num.depth + pad + bar;
        out.depth = den.height + den.depth + pad;
        out.draw = [num, den, width, pad, bar](QPainter &p, qreal x, qreal baseline) {
            const qreal numX = x + (width - num.width) / 2.0;
            const qreal denX = x + (width - den.width) / 2.0;
            if (num.draw)
                num.draw(p, numX, baseline - pad - bar - num.depth);
            p.save();
            QPen pen = p.pen();
            pen.setWidthF(bar);
            pen.setCapStyle(Qt::FlatCap);
            p.setPen(pen);
            const qreal y = baseline - pad - bar / 2.0;
            p.drawLine(QPointF(x + pad * 0.3, y), QPointF(x + width - pad * 0.3, y));
            p.restore();
            if (den.draw)
                den.draw(p, denX, baseline + pad + den.height);
        };
        return out;
    }

    Box makeSqrt(const Box &inner)
    {
        const qreal thick = qMax<qreal>(1.2, m_font.pointSizeF() * 0.08);
        const qreal pad = m_font.pointSizeF() * 0.15;
        const qreal radicalWidth = m_font.pointSizeF() * 0.55;
        Box out;
        out.width = radicalWidth + inner.width + pad;
        out.height = inner.height + pad;
        out.depth = inner.depth + pad * 0.3;
        out.draw = [inner, radicalWidth, pad, thick](QPainter &p, qreal x, qreal baseline) {
            const qreal top = baseline - inner.height - pad;
            const qreal bottom = baseline + inner.depth;
            p.save();
            QPen pen = p.pen();
            pen.setWidthF(thick);
            pen.setJoinStyle(Qt::RoundJoin);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            QPolygonF poly;
            poly << QPointF(x, baseline - inner.height * 0.2)
                 << QPointF(x + radicalWidth * 0.35, bottom)
                 << QPointF(x + radicalWidth * 0.55, top)
                 << QPointF(x + radicalWidth + inner.width + pad * 0.5, top);
            p.drawPolyline(poly);
            p.restore();
            if (inner.draw)
                inner.draw(p, x + radicalWidth, baseline);
        };
        return out;
    }
};

} // namespace

QString FormulaRenderer::stripMathDelimiters(const QString &latex)
{
    QString s = latex.trimmed();
    if (s.startsWith(QStringLiteral("$$")) && s.endsWith(QStringLiteral("$$")) && s.size() > 4)
        return s.mid(2, s.size() - 4).trimmed();
    if (s.startsWith(QLatin1Char('$')) && s.endsWith(QLatin1Char('$')) && s.size() > 2)
        return s.mid(1, s.size() - 2).trimmed();
    if (s.startsWith(QStringLiteral("\\[")) && s.endsWith(QStringLiteral("\\]")) && s.size() > 4)
        return s.mid(2, s.size() - 4).trimmed();
    if (s.startsWith(QStringLiteral("\\(")) && s.endsWith(QStringLiteral("\\)")) && s.size() > 4)
        return s.mid(2, s.size() - 4).trimmed();
    return s;
}

QImage FormulaRenderer::render(const QString &latex, qreal pointSize, qreal dpr)
{
    if (pointSize < 6.0)
        pointSize = 6.0;
    if (dpr < 0.5)
        dpr = 1.0;

    const QString body = stripMathDelimiters(latex);
    QFont font(QStringLiteral("Times New Roman"));
    if (!font.exactMatch())
        font = QFont(QStringLiteral("STIX Two Math"));
    if (!font.exactMatch())
        font = QFont(QStringLiteral("Latin Modern Math"));
    if (!font.exactMatch())
        font = QFont(QStringLiteral("Georgia"));
    font.setPointSizeF(pointSize);
    font.setStyleHint(QFont::Serif);

    Parser parser(body.isEmpty() ? QStringLiteral("?") : body, font);
    Box box = parser.parse();

    const qreal pad = pointSize * 0.25;
    const qreal logicalW = qMax<qreal>(1.0, box.width + pad * 2);
    const qreal logicalH = qMax<qreal>(1.0, box.height + box.depth + pad * 2);
    const int w = qBound(1, 8192, int(qCeil(logicalW * dpr)));
    const int h = qBound(1, 8192, int(qCeil(logicalH * dpr)));

    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return {};
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setPen(QColor(QStringLiteral("#1A1A1A")));
    const qreal baseline = pad + qMax<qreal>(0.0, box.height);
    if (box.draw)
        box.draw(painter, pad, baseline);
    painter.end();

    // The parser's box height can include phantom space (e.g. superscript font
    // metrics), leaving the glyphs stuck to the top of a tall image. Trim the
    // transparent margins so formulas hug their glyphs and center on the line.
    int minX = w;
    int minY = h;
    int maxX = -1;
    int maxY = -1;
    const uchar *bits = image.constBits();
    const int stride = image.bytesPerLine();
    for (int yy = 0; yy < h; ++yy) {
        const uchar *row = bits + yy * stride;
        for (int xx = 0; xx < w; ++xx) {
            if (row[xx * 4 + 3] > 8) {
                minX = qMin(minX, xx);
                maxX = qMax(maxX, xx);
                minY = qMin(minY, yy);
                maxY = qMax(maxY, yy);
            }
        }
    }
    if (maxX >= minX) {
        const int padPx = qMax(1, qRound(dpr * 2.0));
        const int x0 = qMax(0, minX - padPx);
        const int y0 = qMax(0, minY - padPx);
        const int x1 = qMin(w, maxX + 1 + padPx);
        const int y1 = qMin(h, maxY + 1 + padPx);
        QImage cropped = image.copy(x0, y0, x1 - x0, y1 - y0);
        cropped.setDevicePixelRatio(dpr);
        return cropped;
    }
    return image;
}

QString FormulaRenderer::resourceNameForLatex(const QString &latex, qreal pointSize)
{
    static int counter = 0;
    const QByteArray encoded =
        stripMathDelimiters(latex).toUtf8().toBase64(QByteArray::Base64UrlEncoding);
    // v1:<pointSize*10>:<uniqueId>:<payload> — payload has no ':' (base64url)
    return QStringLiteral("newword-formula:v1:%1:%2:%3")
        .arg(qRound(pointSize * 10))
        .arg(++counter)
        .arg(QString::fromLatin1(encoded));
}

bool FormulaRenderer::isFormulaResource(const QString &name)
{
    return name.startsWith(QStringLiteral("newword-formula:"));
}

QString FormulaRenderer::latexFromResourceName(const QString &name)
{
    if (!isFormulaResource(name))
        return {};

    // Current: newword-formula:v1:<size>:<id>:<base64>
    if (name.startsWith(QStringLiteral("newword-formula:v1:"))) {
        const int lastColon = name.lastIndexOf(QLatin1Char(':'));
        if (lastColon < 0)
            return {};
        const QByteArray encoded = name.mid(lastColon + 1).toLatin1();
        return QString::fromUtf8(QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding));
    }

    // Legacy: newword-formula://<base64>
    if (name.startsWith(QStringLiteral("newword-formula://"))) {
        const QByteArray encoded = name.mid(QStringLiteral("newword-formula://").size()).toLatin1();
        return QString::fromUtf8(QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding));
    }
    return {};
}
