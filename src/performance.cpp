/////////////////////////////////////////////////////////////////////////////
// Name:        performance.cpp
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "performance.h"

//----------------------------------------------------------------------------

#include <algorithm>
#include <cctype>
#include <cstdlib>

//----------------------------------------------------------------------------

#include "vrv.h"

namespace vrv {

//----------------------------------------------------------------------------
// PerformedRecording
//----------------------------------------------------------------------------

void PerformedRecording::AddEvent(const std::string &xmlId, const PerformedEvent &event)
{
    if (xmlId.empty()) return;

    auto [iter, inserted] = m_events.insert({ xmlId, event });
    if (!inserted) {
        // A score element aligned more than once - keep the earliest onset, since that is
        // where the element is drawn, but extend the duration to the latest offset.
        PerformedEvent &existing = iter->second;
        const double offset = std::max(existing.GetOffsetMs(), event.GetOffsetMs());
        if (event.onsetMs < existing.onsetMs) {
            existing.onsetMs = event.onsetMs;
            if (event.velocity != VRV_UNSET) existing.velocity = event.velocity;
        }
        existing.durationMs = offset - existing.onsetMs;
    }

    m_firstOnsetMs = std::min(m_firstOnsetMs, event.onsetMs);
    m_lastOffsetMs = std::max(m_lastOffsetMs, event.GetOffsetMs());

    if (event.velocity != VRV_UNSET) {
        m_minVelocity = std::min(m_minVelocity, event.velocity);
        m_maxVelocity = std::max(m_maxVelocity, event.velocity);
    }
}

const PerformedEvent *PerformedRecording::GetEvent(const std::string &xmlId) const
{
    auto iter = m_events.find(xmlId);
    if (iter == m_events.end()) return NULL;

    return &iter->second;
}

//----------------------------------------------------------------------------
// PerformanceData
//----------------------------------------------------------------------------

void PerformanceData::Reset()
{
    m_recordings.clear();
}

void PerformanceData::AddRecording(PerformedRecording recording)
{
    m_recordings.push_back(std::move(recording));
}

const PerformedRecording *PerformanceData::GetRecording(const std::string &selector) const
{
    if (m_recordings.empty()) return NULL;

    if (selector.empty()) return &m_recordings.front();

    // A 1-based index
    const bool isNumber = std::all_of(selector.begin(), selector.end(), [](unsigned char c) { return isdigit(c); });
    if (isNumber) {
        const int index = atoi(selector.c_str());
        if ((index < 1) || (index > this->GetRecordingCount())) {
            LogWarning("Recording '%s' is out of range, the document has %d recording(s)", selector.c_str(),
                this->GetRecordingCount());
            return NULL;
        }
        return &m_recordings.at(index - 1);
    }

    // An @xml:id or a @source, with or without a leading '#'
    const std::string id = (selector.at(0) == '#') ? selector.substr(1) : selector;
    const auto iter
        = std::find_if(m_recordings.begin(), m_recordings.end(), [&id](const PerformedRecording &recording) {
              return (recording.GetID() == id) || (recording.GetSource() == id);
          });
    if (iter != m_recordings.end()) return &*iter;

    LogWarning("No recording with an @xml:id or @source matching '%s'", selector.c_str());

    return NULL;
}

std::optional<double> PerformanceData::ParseTimeToMs(const std::string &value)
{
    if (value.empty()) return std::nullopt;

    const char *start = value.c_str();
    char *end = NULL;
    const double number = strtod(start, &end);
    if (end == start) return std::nullopt;

    // Skip whitespace between the number and its unit
    while ((*end != '\0') && isspace(static_cast<unsigned char>(*end))) ++end;

    std::string unit(end);
    std::transform(
        unit.begin(), unit.end(), unit.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });

    if (unit.empty() || (unit == "ms")) return number;
    if (unit == "s") return number * 1000.0;
    if (unit == "min") return number * 60000.0;

    LogWarning("Unsupported time unit '%s' in '%s', reading the value as milliseconds", unit.c_str(), value.c_str());

    return number;
}

} // namespace vrv
