/////////////////////////////////////////////////////////////////////////////
// Name:        calcperformancexposfunctor.cpp
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "calcperformancexposfunctor.h"

//----------------------------------------------------------------------------

#include <algorithm>

//----------------------------------------------------------------------------

#include "doc.h"
#include "horizontalaligner.h"
#include "layerelement.h"
#include "measure.h"
#include "note.h"
#include "system.h"
#include "tie.h"
#include "vrv.h"

namespace vrv {

//----------------------------------------------------------------------------
// CalcPerformanceXPosFunctor
//----------------------------------------------------------------------------

CalcPerformanceXPosFunctor::CalcPerformanceXPosFunctor(Doc *doc, const PerformedRecording *recording) : DocFunctor(doc)
{
    m_recording = recording;
    m_pass = PERFORMANCE_PASS_collect;
    m_originMs = recording ? recording->GetFirstOnsetMs() : 0.0;
    m_systemOriginMs = m_originMs;
    m_fallbackRate = 2000.0;
    m_measureTime = 0;
    m_currentSystem = NULL;
    m_systemX = 0;
    m_timeOriginX = 0;
    m_systemMaxX = 0;
    m_isFirstMeasureInSystem = true;

    // The option is the width of one second in MEI units
    const double unitsPerSecond = doc->GetOptions()->m_performanceScale.GetValue();
    m_unitsPerMs = unitsPerSecond * doc->GetDrawingUnit(100) / 1000.0;

    // The second note of a tie is not attacked again, so the recording never aligns it - it is
    // placed by the map, but it is not a note the alignment failed on. The lookup is on the whole
    // document because after the cast-off a tie can be on another page than the note it ends on.
    for (Object *object : doc->FindAllDescendantsByType(TIE)) {
        const Tie *tie = vrv_cast<const Tie *>(object);
        if (tie->GetEnd()) m_tieEnds.insert(tie->GetEnd()->GetID());
    }
}

void CalcPerformanceXPosFunctor::SetPass(PerformancePass pass)
{
    m_pass = pass;
    // The notated time is accumulated over the whole score, so it restarts with each pass
    m_measureTime = 0;
    m_isFirstMeasureInSystem = true;
}

bool CalcPerformanceXPosFunctor::BuildMap()
{
    m_map.clear();

    // A note doubled at the same notated time is often aligned only once. The copy is drawn at
    // its double's onset, so it belongs in the map too - the barlines have to see how far it reaches.
    for (const auto &[notatedTime, pitch, xRel] : m_pending) {
        auto iter = m_byPitch.find({ notatedTime, pitch });
        if (iter != m_byPitch.end()) {
            m_collected.push_back({ notatedTime, iter->second.onsetMs, xRel });
        }
    }
    m_pending.clear();

    if (m_collected.empty()) {
        LogWarning("The selected recording does not align any element of the score");
        return false;
    }

    std::sort(m_collected.begin(), m_collected.end(),
        [](const std::tuple<double, double, int> &lhs, const std::tuple<double, double, int> &rhs) {
            return std::get<0>(lhs) < std::get<0>(rhs);
        });

    // Merge the events sharing a notated time into a single knot. Their mean is what the map
    // uses for everything that is not itself aligned; the individual onsets are still what the
    // aligned notes are drawn at, which is where asynchrony comes from.
    for (auto iter = m_collected.begin(); iter != m_collected.end();) {
        auto next = iter;
        double sum = 0.0;
        int count = 0;
        PerformanceKnot knot;
        knot.notatedTime = std::get<0>(*iter);
        knot.firstMs = std::get<1>(*iter);
        knot.lastMs = std::get<1>(*iter);
        while ((next != m_collected.end()) && (std::get<0>(*next) == knot.notatedTime)) {
            sum += std::get<1>(*next);
            knot.firstMs = std::min(knot.firstMs, std::get<1>(*next));
            knot.lastMs = std::max(knot.lastMs, std::get<1>(*next));
            knot.minXRel = std::min(knot.minXRel, std::get<2>(*next));
            ++count;
            ++next;
        }
        knot.meanMs = sum / count;
        // The map has to be monotonic to be usable for interpolation
        if (!m_map.empty() && (knot.meanMs < m_map.back().meanMs)) knot.meanMs = m_map.back().meanMs;
        m_map.push_back(knot);
        iter = next;
    }

    // The rate used before the first and after the last knot, in ms per whole note
    if (m_map.size() >= 2) {
        const double notatedSpan = m_map.back().notatedTime - m_map.front().notatedTime;
        const double performedSpan = m_map.back().meanMs - m_map.front().meanMs;
        if ((notatedSpan > 0.0) && (performedSpan > 0.0)) m_fallbackRate = performedSpan / notatedSpan;
    }

    return true;
}

double CalcPerformanceXPosFunctor::NotatedToMs(double notatedTime) const
{
    if (m_map.empty()) return m_originMs;

    if (notatedTime <= m_map.front().notatedTime) {
        return m_map.front().meanMs - (m_map.front().notatedTime - notatedTime) * m_fallbackRate;
    }
    if (notatedTime >= m_map.back().notatedTime) {
        return m_map.back().meanMs + (notatedTime - m_map.back().notatedTime) * m_fallbackRate;
    }

    auto upper = std::upper_bound(m_map.begin(), m_map.end(), notatedTime,
        [](double value, const PerformanceKnot &knot) { return value < knot.notatedTime; });
    const PerformanceKnot &high = *upper;
    const PerformanceKnot &low = *(upper - 1);

    const double span = high.notatedTime - low.notatedTime;
    if (span <= 0.0) return low.meanMs;

    return low.meanMs + (notatedTime - low.notatedTime) / span * (high.meanMs - low.meanMs);
}

int CalcPerformanceXPosFunctor::MsToX(double ms) const
{
    return m_timeOriginX + static_cast<int>((ms - m_systemOriginMs) * m_unitsPerMs);
}

int CalcPerformanceXPosFunctor::CalcBarLineX(double notatedTime) const
{
    if (m_map.empty()) return this->MsToX(this->NotatedToMs(notatedTime));

    // The first note of the measure the barline opens, and the last one before it
    auto after = std::lower_bound(m_map.begin(), m_map.end(), notatedTime,
        [](const PerformanceKnot &knot, double value) { return knot.notatedTime < value; });

    const bool hasAfter = (after != m_map.end());
    const bool hasBefore = (after != m_map.begin());
    if (!hasAfter && !hasBefore) return this->MsToX(this->NotatedToMs(notatedTime));

    const int unit = m_doc->GetDrawingUnit(100);
    if (!hasAfter) return this->MsToX((after - 1)->lastMs) + unit * 2;

    // A chord rolled across the downbeat starts before the beat it belongs to, so the barline
    // goes by the earliest of those onsets, never by their mean. A notehead displaced to the
    // left within its own alignment - a chord second, say - reaches further still.
    const int afterX = this->MsToX(after->firstMs) + after->minXRel;
    if (!hasBefore) return afterX - unit * 2;

    const int beforeX = this->MsToX((after - 1)->lastMs);

    // Halfway between the two where there is room, and just clear of the downbeat where there is
    // more than enough. Where the playing overlapped the barline, it still goes before the
    // downbeat it announces and the note held over simply reaches past it.
    int x = (beforeX + afterX) / 2;
    x = std::max(x, afterX - unit * 2);
    x = std::min(x, afterX - unit / 2);

    return x;
}

const PerformedEvent *CalcPerformanceXPosFunctor::GetDuplicateEvent(const Note *note, double notatedTime) const
{
    auto iter = m_byPitch.find({ notatedTime, note->GetMIDIPitch() });
    if (iter == m_byPitch.end()) return NULL;

    return &iter->second;
}

double CalcPerformanceXPosFunctor::GetNotatedTime(const LayerElement *layerElement) const
{
    const Alignment *alignment = layerElement->GetAlignment();
    if (!alignment) return m_measureTime.ToDouble();

    return (m_measureTime + alignment->GetTime()).ToDouble();
}

FunctorCode CalcPerformanceXPosFunctor::VisitSystem(System *system)
{
    m_currentSystem = system;
    m_systemX = system->GetDrawingX();
    m_timeOriginX = m_systemX;
    m_systemMaxX = m_systemX;
    m_isFirstMeasureInSystem = true;

    return FUNCTOR_CONTINUE;
}

FunctorCode CalcPerformanceXPosFunctor::VisitSystemEnd(System *system)
{
    if (m_pass == PERFORMANCE_PASS_apply) {
        const int rightPadding = m_doc->GetDrawingUnit(100) * 4;
        system->m_drawingTotalWidth = m_systemMaxX - m_systemX + rightPadding;
        system->m_drawingJustifiableWidth = 0;
    }

    return FUNCTOR_CONTINUE;
}

FunctorCode CalcPerformanceXPosFunctor::VisitMeasure(Measure *measure)
{
    if (m_pass == PERFORMANCE_PASS_apply) {
        // Everything up to and including the left barline keeps the position that the ordinary
        // spacing gave it, so performed time starts only after that gutter.
        const int leftBarLineXRel = measure->GetLeftBarLineXRel();
        if (m_isFirstMeasureInSystem) {
            // Each system starts at its own performed time, so that the systems stack from the
            // left edge whatever point of the recording they cover
            m_systemOriginMs = this->NotatedToMs(m_measureTime.ToDouble());
            m_timeOriginX = m_systemX + leftBarLineXRel;
            // The brace, the line opening the system and the labels are all drawn against the
            // left edge of the system, so the first measure has to begin exactly there. Its left
            // barline closes the gutter and is pinned to the end of it, which the whole system
            // is shifted by - a barline stands ahead of the downbeat it announces, so without
            // this the measure would reach back past the edge of the system.
            m_timeOriginX += (m_systemX + leftBarLineXRel) - this->CalcBarLineX(m_measureTime.ToDouble());
            if (m_currentSystem) {
                // The time the ruler reads at the left edge of the content is the one that falls
                // on the barline, which is a little before the first onset the system holds
                const double gutterMs = m_systemOriginMs - (m_timeOriginX - m_systemX - leftBarLineXRel) / m_unitsPerMs;
                m_currentSystem->SetPerformanceOrigin(gutterMs, leftBarLineXRel);
            }
        }

        // The left barline of the measure goes between the last note before it and the first
        // note of its downbeat
        const int barLineX = this->CalcBarLineX(m_measureTime.ToDouble());
        m_measureX = barLineX - leftBarLineXRel;
        measure->SetDrawingXRel(m_measureX - m_systemX);

        // The aligners are not reached by the page traversal
        measure->m_measureAligner.Process(*this);
    }

    return FUNCTOR_CONTINUE;
}

FunctorCode CalcPerformanceXPosFunctor::VisitMeasureEnd(Measure *measure)
{
    if (m_pass == PERFORMANCE_PASS_apply) {
        m_systemMaxX = std::max(m_systemMaxX, m_measureX + measure->GetWidth());
    }

    m_measureTime = m_measureTime + measure->m_measureAligner.GetMaxTime();
    m_isFirstMeasureInSystem = false;

    return FUNCTOR_CONTINUE;
}

FunctorCode CalcPerformanceXPosFunctor::VisitAlignment(Alignment *alignment)
{
    if (m_pass != PERFORMANCE_PASS_apply) return FUNCTOR_CONTINUE;

    // The gutter of the measure keeps the ordinary spacing
    if (alignment->GetType() <= ALIGNMENT_MEASURE_LEFT_BARLINE) return FUNCTOR_CONTINUE;

    const double notatedTime = (m_measureTime + alignment->GetTime()).ToDouble();

    // The closing barline is placed the same way as the opening one of the next measure, so that
    // the two meet and the staff lines run on without a gap
    const bool isBarLine = (alignment->GetType() >= ALIGNMENT_MEASURE_RIGHT_BARLINE);
    const int x = isBarLine ? this->CalcBarLineX(notatedTime) : this->MsToX(this->NotatedToMs(notatedTime));
    alignment->SetXRel(x - m_measureX);

    // Grace notes are stacked to the left of the note they belong to
    const MapOfIntGraceAligners &graceAligners = alignment->GetGraceAligners();
    for (const auto &[key, value] : graceAligners) {
        value->SetGraceAlignmentXPos(m_doc);
    }

    return FUNCTOR_CONTINUE;
}

FunctorCode CalcPerformanceXPosFunctor::VisitLayerElement(LayerElement *layerElement)
{
    const Alignment *alignment = layerElement->GetAlignment();
    if (!alignment) return FUNCTOR_CONTINUE;

    const PerformedEvent *event = m_recording->GetEvent(layerElement->GetID());

    const double notatedTime = this->GetNotatedTime(layerElement);

    if (m_pass == PERFORMANCE_PASS_collect) {
        if (event) {
            m_collected.push_back({ notatedTime, event->onsetMs, layerElement->GetDrawingXRel() });
            if (layerElement->Is(NOTE)) {
                const Note *note = vrv_cast<const Note *>(layerElement);
                m_byPitch.insert({ { notatedTime, note->GetMIDIPitch() }, *event });
            }
        }
        else if (layerElement->Is(NOTE)) {
            const Note *note = vrv_cast<const Note *>(layerElement);
            m_pending.push_back({ notatedTime, note->GetMIDIPitch(), layerElement->GetDrawingXRel() });
        }
        return FUNCTOR_CONTINUE;
    }

    // A note doubled at the same notated time is often aligned only once - the copy belongs at
    // the same onset, and is no more unaligned than the note it doubles
    if (!event && layerElement->Is(NOTE)) {
        event = this->GetDuplicateEvent(vrv_cast<const Note *>(layerElement), notatedTime);
    }

    if (event) {
        // An aligned element is drawn at its own performed onset, which is what makes notes
        // written as simultaneous but played apart come out apart
        layerElement->m_perfEvent = *event;
        layerElement->m_drawingPerfX = this->MsToX(event->onsetMs);
        m_systemMaxX = std::max(m_systemMaxX, layerElement->m_drawingPerfX);
    }
    else {
        // Everything else stays on the Alignment the map has already moved, and only keeps a
        // record of that, so that the drawing can mark it as unaligned - which the second note of
        // a tie is not, since it is never attacked.
        PerformedEvent interpolated;
        interpolated.onsetMs = this->NotatedToMs(notatedTime);
        interpolated.durationMs = 0.0;
        interpolated.velocity = VRV_UNSET;
        interpolated.matched = (m_tieEnds.count(layerElement->GetID()) > 0);
        layerElement->m_perfEvent = interpolated;
    }

    return FUNCTOR_CONTINUE;
}

} // namespace vrv
