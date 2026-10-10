/////////////////////////////////////////////////////////////////////////////
// Name:        curve.h
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_CURVE_H__
#define __VRV_CURVE_H__

#include "atts_shared.h"
#include "atts_usersymbols.h"
#include "controlelement.h"
#include "offsetinterface.h"
#include "timeinterface.h"

namespace vrv {

class Doc;

//----------------------------------------------------------------------------
// Curve
//----------------------------------------------------------------------------

/**
 * This class models the MEI <curve> element.
 * The curve connects its start and end (e.g., two notes), above or below them according to @curvedir. Its shape can
 * be given with @bulge. The Verovio customization also allows @lstartsym and @lendsym for drawing curved arrows.
 */
class Curve : public ControlElement,
              public OffsetSpanningInterface,
              public TimeSpanningInterface,
              public AttCurvature,
              public AttCurveLog,
              public AttLineRend,
              public AttLineRendBase {
public:
    /**
     * @name Constructors, destructors, and other standard methods
     * Reset method reset all attribute classes
     */
    ///@{
    Curve();
    virtual ~Curve();
    Object *Clone() const override { return new Curve(*this); }
    void Reset() override;
    std::string GetClassName() const override { return "curve"; }
    ///@}

    /**
     * @name Getter to interfaces
     */
    ///@{
    OffsetSpanningInterface *GetOffsetSpanningInterface() override { return vrv_cast<OffsetSpanningInterface *>(this); }
    const OffsetSpanningInterface *GetOffsetSpanningInterface() const override
    {
        return vrv_cast<const OffsetSpanningInterface *>(this);
    }
    TimePointInterface *GetTimePointInterface() override { return vrv_cast<TimePointInterface *>(this); }
    const TimePointInterface *GetTimePointInterface() const override
    {
        return vrv_cast<const TimePointInterface *>(this);
    }
    TimeSpanningInterface *GetTimeSpanningInterface() override { return vrv_cast<TimeSpanningInterface *>(this); }
    const TimeSpanningInterface *GetTimeSpanningInterface() const override
    {
        return vrv_cast<const TimeSpanningInterface *>(this);
    }
    ///@}

    /**
     * Return true if the curve is drawn below its end points (@curvedir="below").
     * The curve is drawn above by default.
     */
    bool IsBelow() const { return (this->GetCurvedir() == curvature_CURVEDIR_below); }

    /**
     * Calculate the two control points of the curve between p1 and p2.
     * The shape is given by the @bulge values when available, or by a default height otherwise.
     */
    void CalcControlPoints(const Doc *doc, int staffSize, const Point &p1, const Point &p2, Point &c1, Point &c2) const;

    //----------//
    // Functors //
    //----------//

    /**
     * Interface for class functor visitation
     */
    ///@{
    FunctorCode Accept(Functor &functor) override;
    FunctorCode Accept(ConstFunctor &functor) const override;
    FunctorCode AcceptEnd(Functor &functor) override;
    FunctorCode AcceptEnd(ConstFunctor &functor) const override;
    ///@}

private:
    //
public:
    //
private:
    //
};

} // namespace vrv

#endif
