/////////////////////////////////////////////////////////////////////////////
// Name:        line.cpp
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "line.h"

//----------------------------------------------------------------------------

#include <cassert>

//----------------------------------------------------------------------------

#include "functor.h"
#include "vrv.h"

namespace vrv {

//----------------------------------------------------------------------------
// Line
//----------------------------------------------------------------------------

static const ClassRegistrar<Line> s_factory("line", LINE);

Line::Line()
    : ControlElement(LINE)
    , OffsetSpanningInterface()
    , TimeSpanningInterface()
    , AttLineLog()
    , AttLineVis()
    , AttPlacementRelStaff()
{
    this->RegisterInterface(OffsetSpanningInterface::GetAttClasses(), OffsetSpanningInterface::IsInterface());
    this->RegisterInterface(TimeSpanningInterface::GetAttClasses(), TimeSpanningInterface::IsInterface());
    this->RegisterAttClass(ATT_LINELOG);
    this->RegisterAttClass(ATT_LINEVIS);
    this->RegisterAttClass(ATT_PLACEMENTRELSTAFF);

    this->Reset();
}

Line::~Line() {}

void Line::Reset()
{
    ControlElement::Reset();
    OffsetSpanningInterface::Reset();
    TimeSpanningInterface::Reset();
    this->ResetLineLog();
    this->ResetLineVis();
    this->ResetPlacementRelStaff();
}

//----------------------------------------------------------------------------
// Line functor methods
//----------------------------------------------------------------------------

FunctorCode Line::Accept(Functor &functor)
{
    return functor.VisitLine(this);
}

FunctorCode Line::Accept(ConstFunctor &functor) const
{
    return functor.VisitLine(this);
}

FunctorCode Line::AcceptEnd(Functor &functor)
{
    return functor.VisitLineEnd(this);
}

FunctorCode Line::AcceptEnd(ConstFunctor &functor) const
{
    return functor.VisitLineEnd(this);
}

} // namespace vrv
