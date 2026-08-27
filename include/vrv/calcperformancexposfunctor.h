/////////////////////////////////////////////////////////////////////////////
// Name:        calcperformancexposfunctor.h
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_CALCPERFORMANCEXPOSFUNCTOR_H__
#define __VRV_CALCPERFORMANCEXPOSFUNCTOR_H__

#include <map>
#include <string>
#include <unordered_set>
#include <vector>

//----------------------------------------------------------------------------

#include "fraction.h"
#include "functor.h"
#include "performance.h"

namespace vrv {
class System;
} // namespace vrv

namespace vrv {

//----------------------------------------------------------------------------
// PerformanceMap
//----------------------------------------------------------------------------

/** One onset the recording gives to a notated point in time */
struct PerformedOnset {
    Fraction notatedTime;
    double onsetMs = 0.0;
    /** When the note stops sounding, i.e. its onset plus its duration */
    double offsetMs = 0.0;
    /** The offset of the element within its alignment, negative when it is displaced to the left */
    int xRel = 0;
};

/**
 * What the recording says about one notated point in time.
 * The mean is what everything not aligned itself is interpolated against; the earliest and the
 * latest are what a barline is placed between, since a rolled chord reaches over its downbeat.
 */
struct PerformanceKnot {
    Fraction notatedTime;
    double meanMs = 0.0;
    double firstMs = 0.0;
    double lastMs = 0.0;
    /**
     * The furthest any of these notes is drawn to the left of its own position, a chord second or
     * an accidental being the usual reason. Zero or negative.
     */
    int minXRel = 0;
};

/** The knots a barline falls between, see PerformanceMap::GetBarLineWindow */
struct PerformanceWindow {
    const PerformanceKnot *before = NULL;
    const PerformanceKnot *after = NULL;
};

/** A note of the score, identified by its notated time and its pitch */
struct NoteKey {
    Fraction notatedTime;
    int pitch = 0;

    auto operator<=>(const NoteKey &) const = default;
};

using MapOfNoteEvents = std::map<NoteKey, PerformedEvent>;

/**
 * This class is the piecewise linear map from notated to performed time that a recording gives to
 * a score, together with the events looked up by note, which is how a doubled note finds its twin.
 *
 * Its knots are sorted by notated time and their mean onset is non decreasing, which is what makes
 * them usable for interpolation. The earliest and the latest onset of a knot are not smoothed that
 * way: they are what was played, and the barlines are placed by them.
 */
class PerformanceMap {
public:
    /**
     * @name Constructors
     */
    ///@{
    PerformanceMap() = default;
    /** Merge the onsets sharing a notated time into knots. Takes them in any order. */
    PerformanceMap(std::vector<PerformedOnset> onsets, MapOfNoteEvents noteEvents);
    ///@}

    /** True when the recording aligned nothing in the score, so that nothing can be placed */
    bool IsEmpty() const { return m_knots.empty(); }

    /** Convert a notated time (in whole notes, from the start of the score) to performed ms */
    double NotatedToMs(const Fraction &notatedTime) const;

    /**
     * The performed time at which the last note of the map stops sounding.
     * Only the end of the score is placed by it: everywhere else a note is followed by another
     * one, and it is the next onset that says where the music went on, not how long the previous
     * note was held. Equal to the last onset when the recording encodes no duration.
     */
    double GetLastOffsetMs() const { return m_lastOffsetMs; }

    /**
     * The knots a barline at the given notated time falls between - the last onset before it and
     * the first at or after it. Either of them is NULL at the ends of the map.
     */
    PerformanceWindow GetBarLineWindow(const Fraction &notatedTime) const;

    /**
     * The event a note of the given pitch at the given notated time was aligned to, or NULL.
     * A doubled note is often aligned only once, and both copies belong at that onset.
     */
    const PerformedEvent *GetNoteEvent(const Fraction &notatedTime, int pitch) const;

private:
    /** The knots, sorted by notated time */
    std::vector<PerformanceKnot> m_knots;
    /** The onset of each aligned note, for resolving doubled notes */
    MapOfNoteEvents m_noteEvents;
    /** The rate used outside the extent of the map, in ms per whole note */
    double m_fallbackRate = 2000.0;
    /** When the last note still sounding stops, see GetLastOffsetMs */
    double m_lastOffsetMs = 0.0;
};

//----------------------------------------------------------------------------
// CalcPerformanceMapFunctor
//----------------------------------------------------------------------------

/**
 * This class collects the onsets that a recording gives to the elements of a page, from which the
 * notated to performed time map is built.
 */
class CalcPerformanceMapFunctor : public DocFunctor {
public:
    /**
     * @name Constructors, destructors
     */
    ///@{
    CalcPerformanceMapFunctor(Doc *doc, const PerformedRecording *recording);
    virtual ~CalcPerformanceMapFunctor() = default;
    ///@}

    /*
     * Abstract base implementation
     */
    bool ImplementsEndInterface() const override { return true; }

    /** The map built from what the traversal found, empty when nothing in the score was aligned */
    PerformanceMap BuildMap() const;

    /*
     * Functor interface
     */
    ///@{
    FunctorCode VisitLayerElement(LayerElement *layerElement) override;
    FunctorCode VisitMeasureEnd(Measure *measure) override;
    ///@}

private:
    /** A note left to be resolved once every aligned note is known */
    struct PendingNote {
        Fraction notatedTime;
        int pitch = 0;
        int xRel = 0;
    };

    /** The recording being laid out */
    const PerformedRecording *m_recording;
    /** The onsets found, before BuildMap merges them into knots */
    std::vector<PerformedOnset> m_onsets;
    /** The onset of each aligned note, for resolving doubled notes */
    MapOfNoteEvents m_noteEvents;
    /** Notes with no alignment of their own, kept until BuildMap can look for their double */
    std::vector<PendingNote> m_pending;
    /** The notated time at the start of the current measure, in whole notes */
    Fraction m_measureTime;
};

//----------------------------------------------------------------------------
// CalcPerformanceXPosFunctor
//----------------------------------------------------------------------------

/**
 * This class re-positions a page that has already been laid out with the ordinary duration based
 * spacing, so that its horizontal axis becomes the performed time of a <recording>.
 *
 * It moves each Alignment to the time the map gives it, which carries measures, barlines and
 * control events along, and gives every note that is itself aligned an absolute position at its
 * own onset - which is what lets notes written as simultaneous be drawn apart.
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
    CalcPerformanceXPosFunctor(Doc *doc, const PerformedRecording *recording, const PerformanceMap &map);
    virtual ~CalcPerformanceXPosFunctor() = default;
    ///@}

    /*
     * Abstract base implementation
     */
    bool ImplementsEndInterface() const override { return true; }

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
    /** Convert a performed time in ms to a drawing position */
    int MsToX(double ms) const;

    /**
     * The position of the barline at a notated time. A barline has no performed time of its own,
     * so it goes between the last note before it and the first note after it - which matters
     * because a chord rolled across the downbeat starts before the beat it belongs to.
     * The barline that closes the score is the one case with no note after it to go by, and is
     * placed at the moment the performance stopped sounding instead.
     */
    int CalcBarLineX(const Fraction &notatedTime, bool closesScore) const;

    /** Anchor the performed time of the system opened by the measure being visited */
    void CalcSystemTimeOrigin(int leftBarLineXRel);

    /** The recording being laid out */
    const PerformedRecording *m_recording;
    /** The notated to performed time map of the recording */
    const PerformanceMap &m_map;
    /** Drawing units per millisecond */
    double m_unitsPerMs = 0.0;
    /** The performed time drawn at the left edge of the current system */
    double m_systemOriginMs = 0.0;
    /** The notes that end a tie - they are not sounded again, so they are never aligned */
    std::unordered_set<std::string> m_tieEnds;
    /** The last measure of the document, the only one whose right barline closes the score */
    const Object *m_lastMeasure = NULL;
    /** The notated time at the start of the current measure, in whole notes */
    Fraction m_measureTime;
    /** The system being visited */
    System *m_currentSystem = NULL;
    /** The drawing X of the current system */
    int m_systemX = 0;
    /** The drawing X at which performed time starts, i.e. after the opening signatures */
    int m_timeOriginX = 0;
    /** Whether the measure being visited opens its system */
    bool m_isFirstMeasureInSystem = true;
    /** Whether the measure being visited is the one that closes the score */
    bool m_isLastMeasureInScore = false;
    /** The drawing X of the measure being visited */
    int m_measureX = 0;
    /** The extent of the current system, used to give it its width */
    int m_systemMaxX = 0;
};

} // namespace vrv

#endif // __VRV_CALCPERFORMANCEXPOSFUNCTOR_H__
