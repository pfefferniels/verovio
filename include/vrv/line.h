/////////////////////////////////////////////////////////////////////////////
// Name:        line.h
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_LINE_H__
#define __VRV_LINE_H__

#include "atts_usersymbols.h"
#include "atts_visual.h"
#include "controlelement.h"
#include "offsetinterface.h"
#include "timeinterface.h"

namespace vrv {

//----------------------------------------------------------------------------
// Line
//----------------------------------------------------------------------------

/**
 * This class models the MEI <line> element.
 * The line is drawn horizontally above or below the staff (according to @place) between its start and end, for
 * example a wavy line over a passage or an arrow indicating a musical direction. It can be tilted with @startvo and
 * @endvo.
 */
class Line : public ControlElement,
             public OffsetSpanningInterface,
             public TimeSpanningInterface,
             public AttLineLog,
             public AttLineVis,
             public AttPlacementRelStaff {
public:
    /**
     * @name Constructors, destructors, and other standard methods
     * Reset method reset all attribute classes
     */
    ///@{
    Line();
    virtual ~Line();
    Object *Clone() const override { return new Line(*this); }
    void Reset() override;
    std::string GetClassName() const override { return "line"; }
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
