/////////////////////////////////////////////////////////////////////////////
// Name:        splitperformancefunctor.h
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_SPLITPERFORMANCEFUNCTOR_H__
#define __VRV_SPLITPERFORMANCEFUNCTOR_H__

#include <vector>

//----------------------------------------------------------------------------

#include "calcperformancexposfunctor.h"
#include "fraction.h"
#include "functor.h"

namespace vrv {

class BeamSpan;
class Measure;

//----------------------------------------------------------------------------
// PerformanceBreakPoint
//----------------------------------------------------------------------------

/**
 * One place at which a system starts, in both notated and performed time.
 *
 * A break point is the moment the clock asked for, converted back into the notated time falling
 * on it. It rarely lands on a barline, and it does not have to: it can fall anywhere a measure
 * can be taken apart, which is anywhere at all - between two notes, inside a beam, or in the
 * middle of a note held across it.
 */
struct PerformanceBreakPoint {
    /** The measure the break point falls in */
    Measure *measure = NULL;
    /** Notated time of the start of that measure, from the start of the score */
    Fraction measureTime;
    /** Notated time of the break point itself, from the start of the score */
    Fraction time;
    /** The performed time asked for, which is what the system is drawn from */
    double ms = 0.0;
};

//----------------------------------------------------------------------------
// CalcPerformanceBreaksFunctor
//----------------------------------------------------------------------------

/**
 * This class finds where the systems of a page open when they are cut by the clock.
 *
 * The systems are wanted one interval apart in performed time, so each of them starts at the
 * notated time the map reads back from the moment asked for. Nothing in the music can veto it:
 * a beam reaching across a break point is drawn across it, a tuplet is cut in two, and a note
 * held across one simply goes on sounding into the system that follows.
 */
class CalcPerformanceBreaksFunctor : public DocFunctor {
public:
    /**
     * @name Constructors, destructors
     */
    ///@{
    CalcPerformanceBreaksFunctor(Doc *doc, const PerformanceMap &map, double firstMs, double intervalMs);
    virtual ~CalcPerformanceBreaksFunctor() = default;
    ///@}

    /*
     * Abstract base implementation
     */
    bool ImplementsEndInterface() const override { return true; }

    /** The break points found, ordered in time */
    const std::vector<PerformanceBreakPoint> &GetBreakPoints() const { return m_breakPoints; }

    /*
     * Functor interface
     */
    ///@{
    FunctorCode VisitMeasure(Measure *measure) override;
    FunctorCode VisitMeasureEnd(Measure *measure) override;
    ///@}

private:
    /** The notated to performed time map of the recording */
    const PerformanceMap &m_map;
    /** The performed time the first system opens at */
    double m_firstMs = 0.0;
    /** The performed time one system covers */
    double m_intervalMs = 0.0;
    /** The system being looked for, the first one needing no break point of its own */
    int m_system = 1;
    /** The break points found so far */
    std::vector<PerformanceBreakPoint> m_breakPoints;
    /** The notated time at the start of the current measure, in whole notes */
    Fraction m_measureTime;
};

//----------------------------------------------------------------------------
// MergePerformanceMeasuresFunctor
//----------------------------------------------------------------------------

/**
 * This class puts a measure that was cut by performed time back together, so that a second layout
 * cuts the score as it was encoded rather than the parts left over from the first one.
 *
 * The halves of a tuplet the cut went through become one tuplet again, and a beam it drew across
 * the break, which is a beamSpan while the score is cut, becomes the beam it was made from.
 */
class MergePerformanceMeasuresFunctor : public Functor {
public:
    /**
     * @name Constructors, destructors
     */
    ///@{
    MergePerformanceMeasuresFunctor() = default;
    virtual ~MergePerformanceMeasuresFunctor() = default;
    ///@}

    /*
     * Abstract base implementation
     */
    bool ImplementsEndInterface() const override { return false; }

    /**
     * Delete the parts that were merged back into the measure they were cut from.
     * Returns how many there were, i.e. zero when the page held nothing that was cut.
     */
    int DeleteMergedMeasures();

    /*
     * Functor interface
     */
    ///@{
    FunctorCode VisitMeasure(Measure *measure) override;
    ///@}

private:
    /** The measure the parts that follow it are merged into */
    Measure *m_contentMeasure = NULL;
    /** The parts that were merged back, to be deleted once the traversal is over */
    std::vector<Measure *> m_merged;
    /** The beams the cut turned into spans, to be made beams again once the parts are one measure */
    std::vector<BeamSpan *> m_beamSpans;
};

//----------------------------------------------------------------------------
// Cutting the measures
//----------------------------------------------------------------------------

/**
 * Cut the measures at the given break points, so that each of them becomes the start of a measure
 * and the cast-off can open a system there. Returns how many measures were cut in two.
 */
int CutPerformanceMeasures(const std::vector<PerformanceBreakPoint> &breakPoints);

} // namespace vrv

#endif // __VRV_SPLITPERFORMANCEFUNCTOR_H__
