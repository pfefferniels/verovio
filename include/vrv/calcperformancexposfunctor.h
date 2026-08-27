/////////////////////////////////////////////////////////////////////////////
// Name:        calcperformancexposfunctor.h
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_CALCPERFORMANCEXPOSFUNCTOR_H__
#define __VRV_CALCPERFORMANCEXPOSFUNCTOR_H__

#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

//----------------------------------------------------------------------------

#include "fraction.h"
#include "functor.h"
#include "performance.h"

namespace vrv {
class System;
class Tie;
} // namespace vrv

namespace vrv {

//----------------------------------------------------------------------------
// CalcPerformanceXPosFunctor
//----------------------------------------------------------------------------

/** The two passes the functor runs over the same content, see CalcPerformanceXPosFunctor */
enum PerformancePass { PERFORMANCE_PASS_collect = 0, PERFORMANCE_PASS_apply };

/**
 * What the recording says about one notated point in time.
 * The mean is what everything not aligned itself is interpolated against; the earliest and the
 * latest are what a barline is placed between, since a rolled chord reaches over its downbeat.
 */
struct PerformanceKnot {
    double notatedTime = 0.0;
    double meanMs = 0.0;
    double firstMs = 0.0;
    double lastMs = 0.0;
    /**
     * The furthest any of these notes is drawn to the left of its own position, a chord second or
     * an accidental being the usual reason. Zero or negative.
     */
    int minXRel = 0;
};

/**
 * This class re-positions a page that has already been laid out with the ordinary duration based
 * spacing, so that its horizontal axis becomes the performed time of a <recording>.
 *
 * The collect pass gathers every aligned element as a knot of a piecewise linear map from notated
 * to performed time. The apply pass then moves each Alignment to the time the map gives it, which
 * carries measures, barlines and control events along, and gives every note that is itself aligned
 * an absolute position at its own onset - which is what lets notes written as simultaneous be
 * drawn apart.
 *
 * Everything up to and including the left barline keeps the position ordinary spacing gave it, so
 * that the clef, key and meter signature opening a system keep the room they need.
 */
class CalcPerformanceXPosFunctor : public DocFunctor {
public:
    /**
     * @name Constructors, destructors
     */
    ///@{
    CalcPerformanceXPosFunctor(Doc *doc, const PerformedRecording *recording);
    virtual ~CalcPerformanceXPosFunctor() = default;
    ///@}

    /*
     * Abstract base implementation
     */
    bool ImplementsEndInterface() const override { return true; }

    /**
     * Select the pass to run. Call BuildMap() between the two.
     */
    void SetPass(PerformancePass pass);

    /**
     * Build the notated to performed time map from what the collect pass found.
     * Returns false when the recording does not align anything in the score, in which
     * case the document cannot be laid out in performed time.
     */
    bool BuildMap();

    /*
     * Functor interface
     */
    ///@{
    FunctorCode VisitAlignment(Alignment *alignment) override;
    FunctorCode VisitLayerElement(LayerElement *layerElement) override;
    FunctorCode VisitMeasure(Measure *measure) override;
    FunctorCode VisitMeasureEnd(Measure *measure) override;
    FunctorCode VisitSystem(System *system) override;
    FunctorCode VisitSystemEnd(System *system) override;
    ///@}

private:
    /** Convert a notated time (in whole notes, from the start of the score) to performed ms */
    double NotatedToMs(double notatedTime) const;

    /** Convert a performed time in ms to a drawing position */
    int MsToX(double ms) const;

    /**
     * The position of the barline at a notated time. A barline has no performed time of its own,
     * so it goes between the last note before it and the first note after it - which matters
     * because a chord rolled across the downbeat starts before the beat it belongs to.
     */
    int CalcBarLineX(double notatedTime) const;

    /**
     * The event another note of the same pitch at the same notated time was aligned to, or NULL.
     * A doubled note is often aligned only once, and both copies belong at that onset.
     */
    const PerformedEvent *GetDuplicateEvent(const Note *note, double notatedTime) const;

    /** The notated time of an element, from the start of the score, in whole notes */
    double GetNotatedTime(const LayerElement *layerElement) const;

public:
    //
private:
    /** The recording being laid out */
    const PerformedRecording *m_recording;
    /** The current pass */
    PerformancePass m_pass;
    /** Drawing units per millisecond */
    double m_unitsPerMs;
    /** The performed time of the start of the recording */
    double m_originMs;
    /** The performed time drawn at the left edge of the current system */
    double m_systemOriginMs;
    /** The knots of the notated to performed time map, sorted by notated time */
    std::vector<PerformanceKnot> m_map;
    /** Collected onsets, with their offset within their alignment, before BuildMap() merges them */
    std::vector<std::tuple<double, double, int>> m_collected;
    /** The onset of each aligned note, by notated time and pitch, for resolving doubled notes */
    std::map<std::pair<double, int>, PerformedEvent> m_byPitch;
    /** Notes with no alignment of their own, kept until BuildMap() can look for their double */
    std::vector<std::tuple<double, int, int>> m_pending;
    /** The notes that end a tie - they are not sounded again, so they are never aligned */
    std::set<std::string> m_tieEnds;
    /** The rate used outside the extent of the map, in ms per whole note */
    double m_fallbackRate;
    /** The notated time at the start of the current measure, in whole notes */
    Fraction m_measureTime;
    /** The system being visited */
    System *m_currentSystem;
    /** The drawing X of the current system */
    int m_systemX;
    /** The drawing X at which performed time starts, i.e. after the opening signatures */
    int m_timeOriginX;
    /** Whether the measure being visited opens its system */
    bool m_isFirstMeasureInSystem;
    /** The drawing X of the measure being visited */
    int m_measureX;
    /** The extent of the current system, used to give it its width */
    int m_systemMaxX;
};

} // namespace vrv

#endif // __VRV_CALCPERFORMANCEXPOSFUNCTOR_H__
