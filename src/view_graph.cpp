/////////////////////////////////////////////////////////////////////////////
// Name:        view_graph.cpp
// Author:      Laurent Pugin
// Created:     2005
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "view.h"

//----------------------------------------------------------------------------

#include <cassert>
#include <cmath>
#include <sstream>

//----------------------------------------------------------------------------

#include "devicecontext.h"
#include "doc.h"
#include "graphic.h"
#include "options.h"
#include "svg.h"
#include "symboldef.h"
#include "vrv.h"

namespace vrv {

void View::DrawVerticalLine(DeviceContext *dc, int y1, int y2, int x1, int width, int dashLength, int gapLength)
{
    assert(dc);

    dc->SetPen(std::max(1, this->ToDeviceContextX(width)), PEN_SOLID, dashLength, gapLength);

    dc->DrawLine(
        this->ToDeviceContextX(x1), this->ToDeviceContextY(y1), this->ToDeviceContextX(x1), this->ToDeviceContextY(y2));

    dc->ResetPen();
    return;
}

void View::DrawHorizontalLine(DeviceContext *dc, int x1, int x2, int y1, int width, int dashLength, int gapLength)
{
    assert(dc);

    dc->SetPen(std::max(1, this->ToDeviceContextX(width)), PEN_SOLID, dashLength, gapLength);

    dc->DrawLine(
        this->ToDeviceContextX(x1), this->ToDeviceContextY(y1), this->ToDeviceContextX(x2), this->ToDeviceContextY(y1));

    dc->ResetPen();
    return;
}

void View::DrawObliqueLine(DeviceContext *dc, int x1, int x2, int y1, int y2, int width, int dashLength, int gapLength)
{
    assert(dc);

    dc->SetPen(std::max(1, this->ToDeviceContextX(width)), PEN_SOLID, dashLength, gapLength);

    dc->DrawLine(
        this->ToDeviceContextX(x1), this->ToDeviceContextY(y1), this->ToDeviceContextX(x2), this->ToDeviceContextY(y2));

    dc->ResetPen();
    return;
}

void View::DrawVerticalSegmentedLine(
    DeviceContext *dc, int x1, SegmentedLine &line, int width, int dashLength, int gapLength)
{
    int start, end;
    for (int i = 0; i < line.GetSegmentCount(); ++i) {
        std::tie(start, end) = line.GetStartEnd(i);
        this->DrawVerticalLine(dc, start, end, x1, width, dashLength, gapLength);
    }
}

void View::DrawHorizontalSegmentedLine(
    DeviceContext *dc, int y1, SegmentedLine &line, int width, int dashLength, int gapLength)
{
    int start, end;
    for (int i = 0; i < line.GetSegmentCount(); ++i) {
        std::tie(start, end) = line.GetStartEnd(i);
        this->DrawHorizontalLine(dc, start, end, y1, width, dashLength, gapLength);
    }
}

void View::DrawNotFilledEllipse(DeviceContext *dc, int x1, int y1, int x2, int y2, int lineThickness)
{
    assert(dc); // DC cannot be NULL

    std::swap(y1, y2);

    dc->SetPen(lineThickness, PEN_SOLID);
    dc->SetBrush(0.0);

    int width = x2 - x1;
    int height = y1 - y2;

    dc->DrawEllipse(this->ToDeviceContextX(x1), this->ToDeviceContextY(y1), width, height);

    dc->ResetPen();
    dc->ResetBrush();
}

void View::DrawNotFilledRectangle(DeviceContext *dc, int x1, int y1, int x2, int y2, int lineThickness, int radius = 0)
{
    assert(dc); // DC cannot be NULL

    std::swap(y1, y2);

    const int penWidth = lineThickness;
    dc->SetPen(penWidth, PEN_SOLID);
    dc->SetBrush(0.0);

    dc->DrawRoundedRectangle(this->ToDeviceContextX(x1), this->ToDeviceContextY(y1), this->ToDeviceContextX(x2 - x1),
        this->ToDeviceContextX(y1 - y2), radius);

    dc->ResetPen();
    dc->ResetBrush();

    return;
}

/* Draw a filled rectangle with horizontal and vertical sides. */
void View::DrawFilledRectangle(DeviceContext *dc, int x1, int y1, int x2, int y2)
{
    assert(dc);

    this->DrawFilledRoundedRectangle(dc, x1, y1, x2, y2, 0);

    return;
}

void View::DrawFilledRoundedRectangle(DeviceContext *dc, int x1, int y1, int x2, int y2, int radius)
{
    assert(dc);

    std::swap(y1, y2);

    dc->SetPen(0, PEN_SOLID);

    dc->DrawRoundedRectangle(this->ToDeviceContextX(x1), this->ToDeviceContextY(y1), this->ToDeviceContextX(x2 - x1),
        this->ToDeviceContextX(y1 - y2), radius);

    dc->ResetPen();

    return;
}

/* Draw an oblique quadrilateral: specifically, a parallelogram with vertical left
    and right sides, and with opposite vertices at (x1,y1) and (x2,y2). */
void View::DrawObliquePolygon(DeviceContext *dc, int x1, int y1, int x2, int y2, int height)
{
    Point p[4];

    dc->SetPen(0, PEN_SOLID);

    height = this->ToDeviceContextX(height);
    p[0].x = this->ToDeviceContextX(x1);
    p[0].y = this->ToDeviceContextY(y1);
    p[1].x = this->ToDeviceContextX(x2);
    p[1].y = this->ToDeviceContextY(y2);
    p[2].x = p[1].x;
    p[2].y = p[1].y - height;
    p[3].x = p[0].x;
    p[3].y = p[0].y - height;

    dc->DrawPolygon(4, p);

    dc->ResetPen();
}

/* Draw an empty ("void") diamond with its top lefthand point at (x1, y1). */

void View::DrawDiamond(DeviceContext *dc, int x1, int y1, int height, int width, bool fill, int linewidth)
{
    Point p[4];

    dc->SetPen(linewidth, PEN_SOLID);
    if (fill) {
        dc->SetBrush(1.0);
    }
    else {
        dc->SetBrush(0.0);
    }

    int dHeight = this->ToDeviceContextX(height);
    int dWidth = this->ToDeviceContextX(width);
    p[0].x = this->ToDeviceContextX(x1);
    p[0].y = this->ToDeviceContextY(y1);
    p[1].x = this->ToDeviceContextX(x1 + dWidth / 2);
    p[1].y = this->ToDeviceContextY(y1 + dHeight / 2);
    p[2].x = p[0].x + dWidth;
    p[2].y = p[0].y;
    p[3].x = this->ToDeviceContextX(x1 + dWidth / 2);
    p[3].y = this->ToDeviceContextY(y1 - dHeight / 2);

    dc->DrawPolygon(4, p);

    dc->ResetPen();
    dc->ResetBrush();
}

void View::DrawDot(DeviceContext *dc, int x, int y, int staffSize, bool dimin)
{
    int r = std::max(this->ToDeviceContextX(m_doc->GetDrawingDoubleUnit(staffSize) / 5), 2);
    if (dimin) r *= m_doc->GetOptions()->m_graceFactor.GetValue();

    dc->SetPen(0, PEN_SOLID);

    dc->DrawCircle(this->ToDeviceContextX(x), this->ToDeviceContextY(y), r);

    dc->ResetPen();
}

void View::DrawVerticalDots(DeviceContext *dc, int x, const SegmentedLine &line, int barlineWidth, int interval)
{
    if (line.GetSegmentCount() > 1) return;

    const auto [top, bottom] = line.GetStartEnd(0);
    const int radius = std::max(barlineWidth, 2);
    int drawingPosition = top - interval / 2;

    dc->SetPen(0, PEN_SOLID);

    while (drawingPosition > bottom) {
        dc->DrawCircle(this->ToDeviceContextX(x), this->ToDeviceContextY(drawingPosition), radius);
        drawingPosition -= interval;
    }

    dc->ResetPen();
}

void View::DrawSquareBracket(DeviceContext *dc, bool leftBracket, int x, int y, int height, int width,
    int horizontalThickness, int verticalThickness)
{
    assert(dc);

    const int sign = leftBracket ? 1 : -1;

    this->DrawFilledRectangle(dc, x, y - horizontalThickness / 2, x + sign * verticalThickness,
        y + height + horizontalThickness / 2); // vertical
    this->DrawFilledRectangle(
        dc, x, y - horizontalThickness / 2, x + sign * width, y + horizontalThickness / 2); // horizontal bottom
    this->DrawFilledRectangle(dc, x, y + height - horizontalThickness / 2, x + sign * width,
        y + height + horizontalThickness / 2); // horizontal top
}

void View::DrawEnclosingBrackets(DeviceContext *dc, int x, int y, int height, int width, int offset, int bracketWidth,
    int horizontalThickness, int verticalThickness)
{
    assert(dc);

    this->DrawSquareBracket(
        dc, true, x - offset, y - offset, height + 2 * offset, bracketWidth, horizontalThickness, verticalThickness);
    this->DrawSquareBracket(dc, false, x + width + offset, y - offset, height + 2 * offset, bracketWidth,
        horizontalThickness, verticalThickness);
}

/*
void View::DrawSmuflCodeWithCustomFont(DeviceContext *dc, const std::string &customFont, int x, int y, char32_t code,
    int staffSize, bool dimin, bool setBBGlyph)
{
    if (customFont.empty()) {
        this->DrawSmuflCode(dc, x, y, code, staffSize, dimin, setBBGlyph);
        return;
    }

    Resources &resources = m_doc->GetResourcesForModification();
    const std::string prevFont = resources.GetCurrentFont();

    resources.SetCurrentFont(customFont);

    this->DrawSmuflCode(dc, x, y, code, staffSize, dimin, setBBGlyph);

    resources.SetCurrentFont(prevFont);
}
*/

void View::DrawSmuflCode(DeviceContext *dc, int x, int y, char32_t code, int staffSize, bool dimin, bool setBBGlyph)
{
    assert(dc);

    if (code == 0) return;

    std::u32string str;
    str.push_back(code);

    dc->SetFont(m_doc->GetDrawingSmuflFont(staffSize, dimin));

    dc->DrawMusicText(str, this->ToDeviceContextX(x), this->ToDeviceContextY(y), setBBGlyph);

    dc->ResetFont();

    return;
}

void View::DrawSmuflLine(
    DeviceContext *dc, Point orig, int length, int staffSize, bool dimin, char32_t fill, char32_t start, char32_t end)
{
    assert(dc);

    if (length <= 0) return;

    const int startWidth = (start == 0) ? 0 : m_doc->GetGlyphAdvX(start, staffSize, dimin);
    const int endWidth = (end == 0) ? 0 : m_doc->GetGlyphAdvX(end, staffSize, dimin);
    int fillWidth = m_doc->GetGlyphAdvX(fill, staffSize, dimin);

    if (fillWidth == 0) fillWidth = m_doc->GetGlyphWidth(fill, staffSize, dimin);

    // We add half a fill length for an average shorter / longer line result
    const int count = (length + fillWidth / 2 - startWidth - endWidth) / fillWidth;

    dc->SetFont(m_doc->GetDrawingSmuflFont(staffSize, dimin));

    std::u32string str;

    if (start != 0) {
        str.push_back(start);
    }

    for (int i = 0; i < count; ++i) {
        str.push_back(fill);
    }

    if (end != 0) {
        str.push_back(end);
    }

    dc->DrawMusicText(str, this->ToDeviceContextX(orig.x), this->ToDeviceContextY(orig.y), false);

    dc->ResetFont();
}

void View::DrawEnclosure(
    DeviceContext *dc, int x1, int y1, int x2, int y2, data_ENCLOSURE enclosure, int staffSize, bool cueSize)
{
    assert(dc);

    int unit = m_doc->GetDrawingUnit(staffSize);
    if (cueSize) unit = m_doc->GetCueSize(unit);
    const int lineWidth = m_options->m_textEnclosureThickness.GetValue() * unit;
    const int margin = unit / 3;
    const int height = y2 - y1;

    switch (enclosure) {
        case ENCLOSURE_circle: {
            // The ellipse going through the corners of the enclosed area, with the same proportions
            const int cx = (x1 + x2) / 2;
            const int cy = (y1 + y2) / 2;
            const int rx = (x2 - x1) / 2 * std::sqrt(2.0) + margin;
            const int ry = (y2 - y1) / 2 * std::sqrt(2.0) + margin;
            this->DrawNotFilledEllipse(dc, cx - rx, cy - ry, cx + rx, cy + ry, lineWidth);
            break;
        }
        case ENCLOSURE_box: {
            this->DrawNotFilledRectangle(dc, x1 - margin, y1 - margin, x2 + margin, y2 + margin, lineWidth, 0);
            break;
        }
        case ENCLOSURE_brack: {
            const int hook = unit / 2;
            const int left = x1 - 2 * margin;
            const int right = x2 + 2 * margin;
            const int bottom = y1 - margin;
            const int top = y2 + margin;
            Point leftBracket[4]
                = { this->ToDeviceContext(Point(left + hook, top)), this->ToDeviceContext(Point(left, top)),
                      this->ToDeviceContext(Point(left, bottom)), this->ToDeviceContext(Point(left + hook, bottom)) };
            Point rightBracket[4]
                = { this->ToDeviceContext(Point(right - hook, top)), this->ToDeviceContext(Point(right, top)),
                      this->ToDeviceContext(Point(right, bottom)), this->ToDeviceContext(Point(right - hook, bottom)) };
            dc->SetPen(lineWidth * 3 / 2, PEN_SOLID, 0, 0, LINECAP_BUTT, LINEJOIN_MITER);
            dc->DrawPolyline(4, leftBracket);
            dc->DrawPolyline(4, rightBracket);
            dc->ResetPen();
            break;
        }
        case ENCLOSURE_paren: {
            const int bow = std::max(unit / 2, height / 8);
            const int left = x1 - margin;
            const int right = x2 + margin;
            const int bottom = y1 - margin;
            const int top = y2 + margin;
            Point leftParen[4] = { this->ToDeviceContext(Point(left, top)),
                this->ToDeviceContext(Point(left - bow, top - (top - bottom) / 4)),
                this->ToDeviceContext(Point(left - bow, bottom + (top - bottom) / 4)),
                this->ToDeviceContext(Point(left, bottom)) };
            Point rightParen[4] = { this->ToDeviceContext(Point(right, top)),
                this->ToDeviceContext(Point(right + bow, top - (top - bottom) / 4)),
                this->ToDeviceContext(Point(right + bow, bottom + (top - bottom) / 4)),
                this->ToDeviceContext(Point(right, bottom)) };
            dc->SetPen(lineWidth * 3 / 2, PEN_SOLID, 0, 0, LINECAP_ROUND);
            dc->SetBrush(0.0);
            dc->DrawCubicBezierPath(leftParen);
            dc->DrawCubicBezierPath(rightParen);
            dc->ResetBrush();
            dc->ResetPen();
            break;
        }
        default: break;
    }
}

int View::GetLineWidth(const data_LINEWIDTH &lineWidth, int defaultWidth, int staffSize) const
{
    if (lineWidth.GetType() == LINEWIDTHTYPE_lineWidthTerm) {
        switch (lineWidth.GetLineWithTerm()) {
            case LINEWIDTHTERM_narrow: return defaultWidth * LINEWIDTHTERM_factor_narrow;
            case LINEWIDTHTERM_medium: return defaultWidth * LINEWIDTHTERM_factor_medium;
            case LINEWIDTHTERM_wide: return defaultWidth * LINEWIDTHTERM_factor_wide;
            default: break;
        }
    }
    else if (lineWidth.GetType() == LINEWIDTHTYPE_measurementunsigned) {
        if (lineWidth.GetMeasurementunsigned().GetType() == MEASUREMENTTYPE_px) {
            return lineWidth.GetMeasurementunsigned().GetPx();
        }
        else {
            return lineWidth.GetMeasurementunsigned().GetVu() * m_doc->GetDrawingUnit(staffSize);
        }
    }
    return defaultWidth;
}

void View::DrawWavyLine(DeviceContext *dc, const Point &p1, const Point &p2, int lineWidth, int staffSize)
{
    assert(dc);

    const double length = hypot(p2.x - p1.x, p2.y - p1.y);
    if (length <= 0) return;

    const int unit = m_doc->GetDrawingUnit(staffSize);
    const double amplitude = 0.6 * unit;
    // Use a whole number of half waves so that the line ends on its axis
    const int halfWaves = std::max(1, (int)std::round(length / (1.5 * unit)));
    const double halfWave = length / halfWaves;
    // Unit vectors along and perpendicular to the line
    const double dx = (p2.x - p1.x) / length;
    const double dy = (p2.y - p1.y) / length;

    constexpr int samplesPerHalfWave = 8;
    const int count = halfWaves * samplesPerHalfWave + 1;
    std::vector<Point> points(count);
    for (int i = 0; i < count; ++i) {
        const double t = (double)i / samplesPerHalfWave;
        const double along = t * halfWave;
        const double across = amplitude * sin(M_PI * t);
        points.at(i) = Point(this->ToDeviceContextX(p1.x + along * dx - across * dy),
            this->ToDeviceContextY(p1.y + along * dy + across * dx));
    }

    dc->SetPen(lineWidth, PEN_SOLID, 0, 0, LINECAP_ROUND, LINEJOIN_ROUND);
    dc->DrawPolyline(count, points.data());
    dc->ResetPen();
}

int View::GetLineEndSymbolLength(data_LINESTARTENDSYMBOL symbol, int staffSize, int symbolSize) const
{
    switch (symbol) {
        case LINESTARTENDSYMBOL_arrow:
        case LINESTARTENDSYMBOL_arrowopen:
        case LINESTARTENDSYMBOL_arrowwhite:
        case LINESTARTENDSYMBOL_harpoonleft:
        case LINESTARTENDSYMBOL_harpoonright: {
            // @lendsym.size (or @endsym.size) ranges from 1 to 9, 5 being the default size
            const double sizeFactor = (symbolSize > 0) ? (0.5 + 0.1 * symbolSize) : 1.0;
            return 2.2 * sizeFactor * m_doc->GetDrawingUnit(staffSize);
        }
        default: return 0;
    }
}

int View::GetLineEndSymbolInset(data_LINESTARTENDSYMBOL symbol, int staffSize, int symbolSize) const
{
    switch (symbol) {
        // The line ends within the head so its end does not show through the tip
        case LINESTARTENDSYMBOL_arrow: return this->GetLineEndSymbolLength(symbol, staffSize, symbolSize) * 2 / 3;
        case LINESTARTENDSYMBOL_arrowwhite: return this->GetLineEndSymbolLength(symbol, staffSize, symbolSize);
        default: return 0;
    }
}

void View::DrawLineEndSymbol(DeviceContext *dc, data_LINESTARTENDSYMBOL symbol, const Point &tip, double angle,
    int lineWidth, int staffSize, int symbolSize)
{
    assert(dc);

    const int unit = m_doc->GetDrawingUnit(staffSize);
    const double length = this->GetLineEndSymbolLength(symbol, staffSize, symbolSize);
    const double halfWidth = length * 0.4;
    // Unit vector pointing backwards from the tip along the line and its perpendicular (to the left)
    const double bx = -cos(angle);
    const double by = -sin(angle);
    const double nx = -by;
    const double ny = bx;

    auto toDC = [this, &tip, bx, by, nx, ny](double back, double side) {
        return Point(this->ToDeviceContextX(tip.x + back * bx + side * nx),
            this->ToDeviceContextY(tip.y + back * by + side * ny));
    };

    switch (symbol) {
        case LINESTARTENDSYMBOL_arrow: {
            Point points[4] = { toDC(0, 0), toDC(length, halfWidth), toDC(length * 0.75, 0), toDC(length, -halfWidth) };
            dc->SetPen(lineWidth, PEN_SOLID, 0, 0, LINECAP_ROUND, LINEJOIN_MITER);
            dc->DrawPolygon(4, points);
            dc->ResetPen();
            break;
        }
        case LINESTARTENDSYMBOL_arrowwhite: {
            Point points[3] = { toDC(0, 0), toDC(length, halfWidth), toDC(length, -halfWidth) };
            dc->SetPen(lineWidth, PEN_SOLID, 0, 0, LINECAP_ROUND, LINEJOIN_MITER);
            dc->DrawPolyline(3, points, true);
            dc->ResetPen();
            break;
        }
        case LINESTARTENDSYMBOL_arrowopen: {
            Point points[3] = { toDC(length, halfWidth), toDC(0, 0), toDC(length, -halfWidth) };
            dc->SetPen(lineWidth, PEN_SOLID, 0, 0, LINECAP_ROUND, LINEJOIN_ROUND);
            dc->DrawPolyline(3, points);
            dc->ResetPen();
            break;
        }
        case LINESTARTENDSYMBOL_harpoonleft:
        case LINESTARTENDSYMBOL_harpoonright: {
            // The barb is on the left or right side when looking at the tip
            const double side = (symbol == LINESTARTENDSYMBOL_harpoonleft) ? halfWidth : -halfWidth;
            Point points[2] = { toDC(0, 0), toDC(length, side) };
            dc->SetPen(lineWidth, PEN_SOLID, 0, 0, LINECAP_ROUND);
            dc->DrawLine(points[0].x, points[0].y, points[1].x, points[1].y);
            dc->ResetPen();
            break;
        }
        case LINESTARTENDSYMBOL_angledown:
        case LINESTARTENDSYMBOL_angleleft:
        case LINESTARTENDSYMBOL_angleup:
        case LINESTARTENDSYMBOL_angleright: {
            // A vertical hook on the page, independently from the direction of the line
            const bool down = (symbol == LINESTARTENDSYMBOL_angledown) || (symbol == LINESTARTENDSYMBOL_angleleft);
            const int hookY = tip.y + (down ? -2 : 2) * unit;
            dc->SetPen(lineWidth, PEN_SOLID, 0, 0, LINECAP_SQUARE);
            dc->DrawLine(this->ToDeviceContextX(tip.x), this->ToDeviceContextY(tip.y), this->ToDeviceContextX(tip.x),
                this->ToDeviceContextY(hookY));
            dc->ResetPen();
            break;
        }
        default: break;
    }
}

void View::DrawSmuflString(DeviceContext *dc, int x, int y, std::u32string s, data_HORIZONTALALIGNMENT alignment,
    int staffSize, bool dimin, bool setBBGlyph)
{
    assert(dc);

    int xDC = this->ToDeviceContextX(x);

    dc->SetFont(m_doc->GetDrawingSmuflFont(staffSize, dimin));

    if (alignment == HORIZONTALALIGNMENT_center) {
        TextExtend extend;
        dc->GetSmuflTextExtent(s, &extend);
        xDC -= extend.m_width / 2;
    }
    else if (alignment == HORIZONTALALIGNMENT_right) {
        TextExtend extend;
        dc->GetSmuflTextExtent(s, &extend);
        xDC -= extend.m_width;
    }

    dc->DrawMusicText(s, xDC, this->ToDeviceContextY(y), setBBGlyph);

    dc->ResetFont();
}

void View::DrawThickBezierCurve(
    DeviceContext *dc, Point bezier[4], int thickness, int staffSize, int penWidth, PenStyle penStyle)
{
    assert(dc);

    Point bez1[4], bez2[4]; // filled array with control points and end point

    BoundingBox::CalcThickBezier(bezier, thickness, bez1, bez2);

    bez1[0] = this->ToDeviceContext(bez1[0]);
    bez1[1] = this->ToDeviceContext(bez1[1]);
    bez1[2] = this->ToDeviceContext(bez1[2]);
    bez1[3] = this->ToDeviceContext(bez1[3]);

    bez2[0] = this->ToDeviceContext(bez2[0]);
    bez2[1] = this->ToDeviceContext(bez2[1]);
    bez2[2] = this->ToDeviceContext(bez2[2]);
    bez2[3] = this->ToDeviceContext(bez2[3]);

    // Actually draw it
    if (penStyle == PEN_SOLID) {
        // Solid Thick Bezier Curves are made of two beziers, filled in.
        dc->SetPen(std::max(1, m_doc->GetDrawingStemWidth(staffSize) / 2), penStyle);
        dc->DrawCubicBezierPathFilled(bez1, bez2);
    }
    else {
        // Dashed or Dotted Thick Bezier Curves have a uniform line width.
        dc->SetPen(thickness, penStyle);
        dc->DrawCubicBezierPath(bez1);
    }
    dc->ResetPen();
}

void View::DrawSymbolDef(DeviceContext *dc, Object *parent, SymbolDef *symbolDef, int x, int y, int staffSize,
    bool dimin, data_HORIZONTALALIGNMENT alignment)
{
    assert(dc);
    assert(symbolDef);

    TextDrawingParams params;
    params.m_x = x;
    params.m_y = y;

    // Because image y coordinates are inverted we need to adjust the y position
    params.m_y += symbolDef->GetSymbolHeight(m_doc, staffSize, dimin);

    if (alignment != HORIZONTALALIGNMENT_left) {
        const int width = symbolDef->GetSymbolWidth(m_doc, staffSize, dimin);
        params.m_x -= (alignment == HORIZONTALALIGNMENT_center) ? (width / 2) : width;
    }

    // Because thg Svg is a child of symbolDef we need to temporarily change the parent for the bounding boxes
    // to be properly propagated in the device context
    symbolDef->SetTemporaryParent(parent);

    for (Object *current : symbolDef->GetChildren()) {
        if (current->Is(GRAPHIC)) {
            Graphic *graphic = vrv_cast<Graphic *>(current);
            assert(graphic);
            this->DrawGraphic(dc, graphic, params, staffSize, dimin);
        }
        if (current->Is(SVG)) {
            Svg *svg = vrv_cast<Svg *>(current);
            assert(svg);
            this->DrawSvg(dc, svg, params, staffSize, dimin);
        }
    }

    symbolDef->ResetTemporaryParent();
}

} // namespace vrv
