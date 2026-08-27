/////////////////////////////////////////////////////////////////////////////
// Name:        performance.h
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_PERFORMANCE_H__
#define __VRV_PERFORMANCE_H__

#include <map>
#include <string>
#include <vector>

//----------------------------------------------------------------------------

#include "vrvdef.h"

namespace vrv {

/** The opacity given to the softest note when velocity is rendered as ink density */
#define PERFORMANCE_MIN_OPACITY 0.35

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
     * @name Constructors, destructors
     */
    ///@{
    PerformedRecording() = default;
    virtual ~PerformedRecording() = default;
    ///@}

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
     * @name Extent of the recording
     */
    ///@{
    bool IsEmpty() const { return m_events.empty(); }
    int GetEventCount() const { return static_cast<int>(m_events.size()); }
    /** Onset of the earliest event - what performed positions are normalised against */
    double GetFirstOnsetMs() const { return m_firstOnsetMs; }
    /** Offset of the latest event */
    double GetLastOffsetMs() const { return m_lastOffsetMs; }
    /**
     * The range of the encoded velocities, VRV_UNSET when none carries one.
     * This is what velocity is mapped to ink density against, so that the density means
     * something whatever range the source happens to use.
     */
    int GetMinVelocity() const { return m_minVelocity; }
    int GetMaxVelocity() const { return m_maxVelocity; }
    ///@}

private:
    //
public:
    //
private:
    /** From <recording>@xml:id */
    std::string m_id;
    /** From <recording>@source */
    std::string m_source;
    /** Events indexed by the xml:id of the score element they align to */
    std::map<std::string, PerformedEvent> m_events;
    /** Extent, maintained by AddEvent */
    double m_firstOnsetMs = 0.0;
    double m_lastOffsetMs = 0.0;
    bool m_hasEventExtent = false;
    int m_minVelocity = VRV_UNSET;
    int m_maxVelocity = VRV_UNSET;
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
    /**
     * @name Constructors, destructors
     */
    ///@{
    PerformanceData() = default;
    virtual ~PerformanceData() = default;
    ///@}

    /** Drop all recordings */
    void Reset();

    /** Append an empty recording and return it for filling */
    PerformedRecording *AddRecording();

    /**
     * @name Access to the recordings
     */
    ///@{
    bool HasRecordings() const { return !m_recordings.empty(); }
    int GetRecordingCount() const { return static_cast<int>(m_recordings.size()); }
    /**
     * Select a recording by 1-based index ("1", "2", ...) or by its @xml:id or @source.
     * An empty selector returns the first recording. Returns NULL when nothing matches.
     */
    const PerformedRecording *GetRecording(const std::string &selector) const;
    ///@}

    /**
     * Parse an MEI time value into milliseconds.
     * Accepts the "28618ms" and "12.5s" forms used with @abstype="smil", as well as a
     * bare number, which is read as milliseconds. Sets ok to false when unparsable.
     */
    static double ParseTimeToMs(const std::string &value, bool *ok = NULL);

private:
    //
public:
    //
private:
    std::vector<PerformedRecording> m_recordings;
};

} // namespace vrv

#endif // __VRV_PERFORMANCE_H__
