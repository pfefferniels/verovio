/////////////////////////////////////////////////////////////////////////////
// Name:        performance.h
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_PERFORMANCE_H__
#define __VRV_PERFORMANCE_H__

#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

//----------------------------------------------------------------------------

#include "fraction.h"
#include "vrvdef.h"

namespace vrv {

/** The opacity given to the softest note when velocity is rendered as ink density */
constexpr double PERFORMANCE_MIN_OPACITY = 0.35;

//----------------------------------------------------------------------------
// PerformanceSegment
//----------------------------------------------------------------------------

/**
 * What a measure that came out of a cut by performed time knows about itself.
 *
 * Cutting the systems by time splits a measure into several of them, and each part has to keep
 * saying how long it is: the notated time the layout runs on is accumulated from one measure to
 * the next, so a part that ended at its last note rather than at the cut would put everything
 * after it out of step with the recording.
 */
struct PerformanceSegment {
    /** The notated duration of the segment, which is what pins the right barline of its aligner */
    Fraction duration;
    /** True when the segment continues a measure that was cut before it */
    bool isContinuation = false;
    /** True when a system break falls on the left edge of the segment */
    bool startsSystem = false;
    /**
     * @name The performed times the segment was cut at, which are what its edges are drawn at.
     * Set only where the edge is a cut: a segment opening a system knows the moment its system
     * starts at, and a segment cut at its end knows the moment the next one starts at. Drawing
     * both from the cut rather than from the music around it is what makes every system cover
     * exactly the span of the recording it was asked for.
     */
    ///@{
    std::optional<double> startMs;
    std::optional<double> endMs;
    ///@}
};

//----------------------------------------------------------------------------
// PerformedEvent
//----------------------------------------------------------------------------

/**
 * A single performed event - one <when> of a <recording> resolved to a score element.
 * Times are in milliseconds from the start of the recording (i.e. not yet normalised
 * against the lead-in; see PerformedRecording::GetFirstOnsetMs).
 */
struct PerformedEvent {
    /** Onset, from <when>@absolute */
    double onsetMs = 0.0;
    /** Duration, from <extData type="duration">; 0.0 when not encoded */
    double durationMs = 0.0;
    /** MIDI velocity from <extData type="velocity">; VRV_UNSET when not encoded */
    int velocity = VRV_UNSET;
    /**
     * False for events that were not encoded but interpolated during layout.
     * Only ever set by the layout functor, never by the importer.
     */
    bool matched = true;

    double GetOffsetMs() const { return onsetMs + durationMs; }
};

//----------------------------------------------------------------------------
// PerformedRecording
//----------------------------------------------------------------------------

/**
 * One <recording> element of a <performance>, holding its events indexed by the
 * xml:id of the score element they are aligned to.
 */
class PerformedRecording {
public:
    /**
     * @name Setters and getters for the identifiers
     */
    ///@{
    void SetID(const std::string &id) { m_id = id; }
    std::string GetID() const { return m_id; }
    void SetSource(const std::string &source) { m_source = source; }
    std::string GetSource() const { return m_source; }
    ///@}

    /**
     * Add an event for the given element xml:id.
     * When an id already has an event, the earlier onset is kept.
     */
    void AddEvent(const std::string &xmlId, const PerformedEvent &event);

    /**
     * Look up the event aligned to the given element xml:id.
     * Returns NULL when the element was not aligned in this recording.
     */
    const PerformedEvent *GetEvent(const std::string &xmlId) const;

    /**
     * @name Extent of the recording, 0.0 when it holds no event
     */
    ///@{
    bool IsEmpty() const { return m_events.empty(); }
    int GetEventCount() const { return static_cast<int>(m_events.size()); }
    /** Onset of the earliest event - what performed positions are normalised against */
    double GetFirstOnsetMs() const { return m_events.empty() ? 0.0 : m_firstOnsetMs; }
    /** Offset of the latest event */
    double GetLastOffsetMs() const { return m_events.empty() ? 0.0 : m_lastOffsetMs; }
    /**
     * The range of the encoded velocities, VRV_UNSET when none carries one.
     * This is what velocity is mapped to ink density against, so that the density means
     * something whatever range the source happens to use.
     */
    int GetMinVelocity() const { return this->HasVelocities() ? m_minVelocity : VRV_UNSET; }
    int GetMaxVelocity() const { return this->HasVelocities() ? m_maxVelocity : VRV_UNSET; }
    ///@}

private:
    bool HasVelocities() const { return (m_minVelocity <= m_maxVelocity); }

    /** From <recording>@xml:id */
    std::string m_id;
    /** From <recording>@source */
    std::string m_source;
    /** Events indexed by the xml:id of the score element they align to */
    std::map<std::string, PerformedEvent> m_events;
    /** Extent, maintained by AddEvent, and left at the identity while there is no event */
    double m_firstOnsetMs = std::numeric_limits<double>::max();
    double m_lastOffsetMs = std::numeric_limits<double>::lowest();
    int m_minVelocity = std::numeric_limits<int>::max();
    int m_maxVelocity = std::numeric_limits<int>::lowest();
};

//----------------------------------------------------------------------------
// PerformanceData
//----------------------------------------------------------------------------

/**
 * The content of the <performance> element of a document.
 *
 * This is deliberately not part of the Object tree: the data is only ever used as a
 * lookup table during layout, and adding Object subclasses would require entries in
 * FunctorInterface and an Accept/AcceptEnd pair for every visitor.
 */
class PerformanceData {
public:
    /** Drop all recordings */
    void Reset();

    /** Append a recording */
    void AddRecording(PerformedRecording recording);

    /**
     * @name Access to the recordings
     */
    ///@{
    bool HasRecordings() const { return !m_recordings.empty(); }
    int GetRecordingCount() const { return static_cast<int>(m_recordings.size()); }
    const std::vector<PerformedRecording> &GetRecordings() const { return m_recordings; }
    /**
     * Select a recording by 1-based index ("1", "2", ...) or by its @xml:id or @source.
     * An empty selector returns the first recording. Returns NULL when nothing matches.
     */
    const PerformedRecording *GetRecording(const std::string &selector) const;
    ///@}

    /**
     * Parse an MEI time value into milliseconds.
     * Accepts the "28618ms" and "12.5s" forms used with @abstype="smil", as well as a
     * bare number, which is read as milliseconds. Returns nothing when unparsable.
     */
    static std::optional<double> ParseTimeToMs(const std::string &value);

private:
    std::vector<PerformedRecording> m_recordings;
};

} // namespace vrv

#endif // __VRV_PERFORMANCE_H__
