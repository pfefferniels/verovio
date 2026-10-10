/////////////////////////////////////////////////////////////////////////////
// Name:        curve.cpp
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "curve.h"

//----------------------------------------------------------------------------

#include <algorithm>
#include <cassert>
#include <cmath>

//----------------------------------------------------------------------------

#include "doc.h"
#include "functor.h"
#include "vrv.h"

namespace vrv {

//----------------------------------------------------------------------------
// Curve
//----------------------------------------------------------------------------

static const ClassRegistrar<Curve> s_factory("curve", CURVE);

Curve::Curve()
    : ControlElement(CURVE)
    , OffsetSpanningInterface()
    , TimeSpanningInterface()
    , AttCurvature()
    , AttCurveLog()
    , AttLineRend()
    , AttLineRendBase()
{
    this->RegisterInterface(OffsetSpanningInterface::GetAttClasses(), OffsetSpanningInterface::IsInterface());
    this->RegisterInterface(TimeSpanningInterface::GetAttClasses(), TimeSpanningInterface::IsInterface());
    this->RegisterAttClass(ATT_CURVATURE);
    this->RegisterAttClass(ATT_CURVELOG);
    this->RegisterAttClass(ATT_LINEREND);
    this->RegisterAttClass(ATT_LINERENDBASE);

    this->Reset();
}

Curve::~Curve() {}

void Curve::Reset()
{
    ControlElement::Reset();
    OffsetSpanningInterface::Reset();
    TimeSpanningInterface::Reset();
    this->ResetCurvature();
    this->ResetCurveLog();
    this->ResetLineRend();
    this->ResetLineRendBase();
}

void Curve::CalcControlPoints(
    const Doc *doc, int staffSize, const Point &p1, const Point &p2, Point &c1, Point &c2) const
{
    assert(doc);

    const int unit = doc->GetDrawingUnit(staffSize);
    const double length = std::hypot(p2.x - p1.x, p2.y - p1.y);
    if (length == 0.0) {
        c1 = p1;
        c2 = p2;
        return;
    }

    // Unit vector along the chord and its normal to the left (MEI @bulge values are to the left when positive)
    const double dx = (p2.x - p1.x) / length;
    const double dy = (p2.y - p1.y) / length;
    const double nx = -dy;
    const double ny = dx;

    // With the control points at one and two thirds of the chord, the distance from the chord of the point at
    // lambda percent of it is a(lambda) * h1 + b(lambda) * h2, with h1 and h2 the heights of the control points
    auto a = [](double lambda) { return 3.0 * pow(1.0 - lambda, 2.0) * lambda; };
    auto b = [](double lambda) { return 3.0 * (1.0 - lambda) * pow(lambda, 2.0); };

    double h1 = 0.0;
    double h2 = 0.0;

    data_BULGE bulge = this->GetBulge();
    bulge.erase(
        std::remove_if(bulge.begin(), bulge.end(),
            [](const data_BULGE::value_type &entry) { return ((entry.second <= 0.0) || (entry.second >= 100.0)); }),
        bulge.end());

    if (bulge.size() == 1) {
        // Minimal solution, which is symmetrical for a bulge in the middle of the chord
        const double lambda = bulge.front().second / 100.0;
        const double distance = bulge.front().first * unit;
        const double norm = pow(a(lambda), 2.0) + pow(b(lambda), 2.0);
        h1 = distance * a(lambda) / norm;
        h2 = distance * b(lambda) / norm;
    }
    else if (bulge.size() > 1) {
        // Least squares solution of the constraints
        double aa = 0.0, ab = 0.0, bb = 0.0, ad = 0.0, bd = 0.0;
        for (const data_BULGE::value_type &entry : bulge) {
            const double lambda = entry.second / 100.0;
            const double distance = entry.first * unit;
            aa += a(lambda) * a(lambda);
            ab += a(lambda) * b(lambda);
            bb += b(lambda) * b(lambda);
            ad += a(lambda) * distance;
            bd += b(lambda) * distance;
        }
        const double det = aa * bb - ab * ab;
        if (std::abs(det) > 1e-9) {
            h1 = (ad * bb - bd * ab) / det;
            h2 = (bd * aa - ad * ab) / det;
        }
    }
    else {
        // Default height in the middle of the curve, proportional to its length
        const double height = std::clamp(length / 5.0, 1.5 * unit, 6.0 * unit);
        // The control points are higher than the curve itself
        h1 = h2 = height / 0.75;
        // Above or below on the page, independently from the direction of the chord
        const bool below = this->IsBelow();
        if ((ny < 0.0) != below) {
            h1 = -h1;
            h2 = -h2;
        }
    }

    c1 = Point(p1.x + dx * length / 3.0 + nx * h1, p1.y + dy * length / 3.0 + ny * h1);
    c2 = Point(p1.x + dx * length * 2.0 / 3.0 + nx * h2, p1.y + dy * length * 2.0 / 3.0 + ny * h2);
}

//----------------------------------------------------------------------------
// Curve functor methods
//----------------------------------------------------------------------------

FunctorCode Curve::Accept(Functor &functor)
{
    return functor.VisitCurve(this);
}

FunctorCode Curve::Accept(ConstFunctor &functor) const
{
    return functor.VisitCurve(this);
}

FunctorCode Curve::AcceptEnd(Functor &functor)
{
    return functor.VisitCurveEnd(this);
}

FunctorCode Curve::AcceptEnd(ConstFunctor &functor) const
{
    return functor.VisitCurveEnd(this);
}

} // namespace vrv
