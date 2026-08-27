/////////////////////////////////////////////////////////////////////////////
// Name:        calcperformancexposfunctor.cpp
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "calcperformancexposfunctor.h"

//----------------------------------------------------------------------------

#include <algorithm>
#include <iterator>
#include <numeric>
#include <ranges>

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
// PerformanceMap
//----------------------------------------------------------------------------

PerformanceMap::PerformanceMap(std::vector<PerformedOnset> onsets, MapOfNoteEvents noteEvents)
    : m_noteEvents(std::move(noteEvents))
{
    std::sort(onsets.begin(), onsets.end(),
        [](const PerformedOnset &lhs, const PerformedOnset &rhs) { return lhs.notatedTime < rhs.notatedTime; });

    // Merge the onsets sharing a notated time into a single knot. Their mean is what the map
    // uses for everything that is not itself aligned; the individual onsets are still what the
    // aligned notes are drawn at, which is where asynchrony comes from.
    for (auto iter = onsets.begin(); iter != onsets.end();) {
        const auto rest = std::ranges::subrange(iter, onsets.end());
        const auto chunk = std::ranges::subrange(
            iter, std::ranges::upper_bound(rest, iter->notatedTime, {}, &PerformedOnset::notatedTime));
        const auto [earliest, latest] = std::ranges::minmax_element(chunk, {}, &PerformedOnset::onsetMs);

        const double sumMs = std::accumulate(chunk.begin(), chunk.end(), 0.0,
            [](double sum, const PerformedOnset &onset) { return sum + onset.onsetMs; });

        PerformanceKnot knot;
        knot.notatedTime = iter->notatedTime;
        knot.firstMs = earliest->onsetMs;
        knot.lastMs = latest->onsetMs;
        knot.minXRel = std::min(0, std::ranges::min_element(chunk, {}, &PerformedOnset::xRel)->xRel);
        knot.meanMs = sumMs / std::ranges::distance(chunk);
        // The map has to be monotonic to be usable for interpolation
        if (!m_knots.empty()) knot.meanMs = std::max(knot.meanMs, m_knots.back().meanMs);

        m_knots.push_back(knot);
        iter = chunk.end();
    }

    // Where the sound of the score ends, which is the only place a duration is laid out rather
    // than the distance to the next onset. A note held under a fermata reaches past every onset.
    for (const PerformedOnset &onset : onsets) {
        m_lastOffsetMs = std::max(m_lastOffsetMs, onset.offsetMs);
    }

    // The rate used before the first and after the last knot, in ms per whole note
    if (m_knots.size() >= 2) {
        const double notatedSpan = m_knots.back().notatedTime.ToDouble() - m_knots.front().notatedTime.ToDouble();
        const double performedSpan = m_knots.back().meanMs - m_knots.front().meanMs;
        if ((notatedSpan > 0.0) && (performedSpan > 0.0)) m_fallbackRate = performedSpan / notatedSpan;
    }
}

double PerformanceMap::NotatedToMs(const Fraction &notatedTime) const
{
    if (m_knots.empty()) return 0.0;

    const double time = notatedTime.ToDouble();
    if (notatedTime <= m_knots.front().notatedTime) {
        return m_knots.front().meanMs - (m_knots.front().notatedTime.ToDouble() - time) * m_fallbackRate;
    }
    if (notatedTime >= m_knots.back().notatedTime) {
        return m_knots.back().meanMs + (time - m_knots.back().notatedTime.ToDouble()) * m_fallbackRate;
    }

    const auto high = std::ranges::upper_bound(m_knots, notatedTime, {}, &PerformanceKnot::notatedTime);
    const PerformanceKnot &low = *(high - 1);

    const double span = high->notatedTime.ToDouble() - low.notatedTime.ToDouble();
    if (span <= 0.0) return low.meanMs;

    return low.meanMs + (time - low.notatedTime.ToDouble()) / span * (high->meanMs - low.meanMs);
}

PerformanceWindow PerformanceMap::GetBarLineWindow(const Fraction &notatedTime) const
{
    const auto after = std::ranges::lower_bound(m_knots, notatedTime, {}, &PerformanceKnot::notatedTime);

    PerformanceWindow window;
    if (after != m_knots.end()) window.after = &*after;
    if (after != m_knots.begin()) window.before = &*(after - 1);

    return window;
}

const PerformedEvent *PerformanceMap::GetNoteEvent(const Fraction &notatedTime, int pitch) const
{
    auto iter = m_noteEvents.find({ notatedTime, pitch });
    if (iter == m_noteEvents.end()) return NULL;

    return &iter->second;
}

//----------------------------------------------------------------------------
// CalcPerformanceMapFunctor
//----------------------------------------------------------------------------

CalcPerformanceMapFunctor::CalcPerformanceMapFunctor(Doc *doc, const PerformedRecording *recording) : DocFunctor(doc)
{
    assert(recording);

    m_recording = recording;
}

PerformanceMap CalcPerformanceMapFunctor::BuildMap() const
{
    // A note doubled at the same notated time is often aligned only once. The copy is drawn at
    // its double's onset, so it belongs in the map too - the barlines have to see how far it reaches.
    std::vector<PerformedOnset> onsets = m_onsets;
    for (const PendingNote &pending : m_pending) {
        auto iter = m_noteEvents.find({ pending.notatedTime, pending.pitch });
        if (iter != m_noteEvents.end()) {
            onsets.push_back({ pending.notatedTime, iter->second.onsetMs, iter->second.GetOffsetMs(), pending.xRel });
        }
    }

    return PerformanceMap(std::move(onsets), m_noteEvents);
}

FunctorCode CalcPerformanceMapFunctor::VisitLayerElement(LayerElement *layerElement)
{
    const Alignment *alignment = layerElement->GetAlignment();
    if (!alignment) return FUNCTOR_CONTINUE;

    const Fraction notatedTime = m_measureTime + alignment->GetTime();
    const PerformedEvent *event = m_recording->GetEvent(layerElement->GetID());
    const bool isNote = layerElement->Is(NOTE);

    if (event) {
        m_onsets.push_back({ notatedTime, event->onsetMs, event->GetOffsetMs(), layerElement->GetDrawingXRel() });
        if (isNote) {
            const Note *note = vrv_cast<const Note *>(layerElement);
            m_noteEvents.insert({ { notatedTime, note->GetMIDIPitch() }, *event });
        }
    }
    else if (isNote) {
        const Note *note = vrv_cast<const Note *>(layerElement);
        m_pending.push_back({ notatedTime, note->GetMIDIPitch(), layerElement->GetDrawingXRel() });
    }

    return FUNCTOR_CONTINUE;
}

FunctorCode CalcPerformanceMapFunctor::VisitMeasureEnd(Measure *measure)
{
    m_measureTime = m_measureTime + measure->m_measureAligner.GetMaxTime();

    return FUNCTOR_CONTINUE;
}

//----------------------------------------------------------------------------
// CalcPerformanceXPosFunctor
//----------------------------------------------------------------------------

CalcPerformanceXPosFunctor::CalcPerformanceXPosFunctor(
    Doc *doc, const PerformedRecording *recording, const PerformanceMap &map)
    : DocFunctor(doc), m_recording(recording), m_map(map)
{
    assert(recording);

    m_systemOriginMs = recording->GetFirstOnsetMs();

    // The option is the width of one second in MEI units
    const double unitsPerSecond = doc->GetOptions()->m_performanceScale.GetValue();
    m_unitsPerMs = unitsPerSecond * doc->GetDrawingUnit(100) / 1000.0;

    // The second note of a tie is not attacked again, so the recording never aligns it - it is
    // placed by the map, but it is not a note the alignment failed on, and it sounds on with what
    // the note it is tied from was struck with. The lookup is on the whole document because after
    // the cast-off a tie can be on another page than the note it ends on.
    const ListOfObjects ties = doc->FindAllDescendantsByType(TIE);
    auto resolved = ties | std::views::transform([](Object *object) { return vrv_cast<const Tie *>(object); })
        | std::views::filter([](const Tie *tie) { return tie->GetStart() && tie->GetEnd(); });
    std::ranges::transform(resolved, std::inserter(m_tiedFrom, m_tiedFrom.end()),
        [](const Tie *tie) { return std::pair(tie->GetEnd()->GetID(), tie->GetStart()->GetID()); });

    // The map is built one page at a time, so every page ends with a barline that has nothing
    // after it. Only one of them closes the score, and the document is what knows which.
    const ListOfObjects measures = doc->FindAllDescendantsByType(MEASURE);
    if (!measures.empty()) m_lastMeasure = measures.back();
}

int CalcPerformanceXPosFunctor::MsToX(double ms) const
{
    return m_timeOriginX + static_cast<int>((ms - m_systemOriginMs) * m_unitsPerMs);
}

int CalcPerformanceXPosFunctor::CalcBarLineX(const Fraction &notatedTime, bool closesScore) const
{
    // The first note of the measure the barline opens, and the last one before it
    const PerformanceWindow window = m_map.GetBarLineWindow(notatedTime);
    if (!window.before && !window.after) return this->MsToX(m_map.NotatedToMs(notatedTime));

    const int unit = m_doc->GetDrawingUnit(100);
    if (!window.after) {
        // The barline closing the score announces no downbeat, so it goes after the sound has
        // stopped and not after the last attack - which is what leaves a final chord held under
        // a fermata the room it was played for. Anywhere else the duration says nothing about
        // where the barline belongs, since it is the next onset that carries the music on.
        const double ms
            = closesScore ? std::max(m_map.GetLastOffsetMs(), window.before->lastMs) : window.before->lastMs;
        return this->MsToX(ms) + unit * 2;
    }

    // A chord rolled across the downbeat starts before the beat it belongs to, so the barline
    // goes by the earliest of those onsets, never by their mean. A notehead displaced to the
    // left within its own alignment - a chord second, say - reaches further still.
    const int afterX = this->MsToX(window.after->firstMs) + window.after->minXRel;
    if (!window.before) return afterX - unit * 2;

    const int beforeX = this->MsToX(window.before->lastMs);

    // Halfway between the two where there is room, and just clear of the downbeat where there is
    // more than enough. Where the playing overlapped the barline, it still goes before the
    // downbeat it announces and the note held over simply reaches past it.
    int x = (beforeX + afterX) / 2;
    x = std::max(x, afterX - unit * 2);
    x = std::min(x, afterX - unit / 2);

    return x;
}

void CalcPerformanceXPosFunctor::CalcSystemTimeOrigin(int leftBarLineXRel)
{
    // Each system starts at its own performed time, so that the systems stack from the left edge
    // whatever point of the recording they cover
    m_systemOriginMs = m_map.NotatedToMs(m_measureTime);
    m_timeOriginX = m_systemX + leftBarLineXRel;
    // The brace, the line opening the system and the labels are all drawn against the left edge
    // of the system, so the first measure has to begin exactly there. Its left barline closes the
    // gutter and is pinned to the end of it, which the whole system is shifted by - a barline
    // stands ahead of the downbeat it announces, so without this the measure would reach back
    // past the edge of the system.
    m_timeOriginX += (m_systemX + leftBarLineXRel) - this->CalcBarLineX(m_measureTime, false);
    if (!m_currentSystem) return;

    // The time the ruler reads at the left edge of the content is the one that falls on the
    // barline, which is a little before the first onset the system holds
    const double gutterMs = m_systemOriginMs - (m_timeOriginX - m_systemX - leftBarLineXRel) / m_unitsPerMs;
    m_currentSystem->SetPerformanceOrigin(gutterMs, leftBarLineXRel);
}

const PerformedEvent *CalcPerformanceXPosFunctor::GetTieStartEvent(const std::string &xmlId) const
{
    // In a chain of ties only the first note is struck, so the walk goes on until an aligned note
    // is found. It is bounded so that data with a tie looping back on itself cannot spin here.
    std::string id = xmlId;
    for (size_t step = 0; step < m_tiedFrom.size(); ++step) {
        const auto tied = m_tiedFrom.find(id);
        if (tied == m_tiedFrom.end()) return NULL;

        id = tied->second;
        if (const PerformedEvent *event = m_recording->GetEvent(id)) return event;
    }

    return NULL;
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
    const int rightPadding = m_doc->GetDrawingUnit(100) * 4;
    system->m_drawingTotalWidth = m_systemMaxX - m_systemX + rightPadding;
    system->m_drawingJustifiableWidth = 0;

    return FUNCTOR_CONTINUE;
}

FunctorCode CalcPerformanceXPosFunctor::VisitMeasure(Measure *measure)
{
    m_isLastMeasureInScore = (measure == m_lastMeasure);

    // Everything up to and including the left barline keeps the position that the ordinary
    // spacing gave it, so performed time starts only after that gutter.
    const int leftBarLineXRel = measure->GetLeftBarLineXRel();
    if (m_isFirstMeasureInSystem) this->CalcSystemTimeOrigin(leftBarLineXRel);

    // The left barline of the measure goes between the last note before it and the first
    // note of its downbeat
    m_measureX = this->CalcBarLineX(m_measureTime, false) - leftBarLineXRel;
    measure->SetDrawingXRel(m_measureX - m_systemX);

    // The aligners are not reached by the page traversal
    measure->m_measureAligner.Process(*this);

    return FUNCTOR_CONTINUE;
}

FunctorCode CalcPerformanceXPosFunctor::VisitMeasureEnd(Measure *measure)
{
    m_systemMaxX = std::max(m_systemMaxX, m_measureX + measure->GetWidth());
    m_measureTime = m_measureTime + measure->m_measureAligner.GetMaxTime();
    m_isFirstMeasureInSystem = false;

    return FUNCTOR_CONTINUE;
}

FunctorCode CalcPerformanceXPosFunctor::VisitAlignment(Alignment *alignment)
{
    // The gutter of the measure keeps the ordinary spacing
    if (alignment->GetType() <= ALIGNMENT_MEASURE_LEFT_BARLINE) return FUNCTOR_CONTINUE;

    const Fraction notatedTime = m_measureTime + alignment->GetTime();

    // The closing barline is placed the same way as the opening one of the next measure, so that
    // the two meet and the staff lines run on without a gap
    const bool isBarLine = (alignment->GetType() >= ALIGNMENT_MEASURE_RIGHT_BARLINE);
    const int x = isBarLine ? this->CalcBarLineX(notatedTime, m_isLastMeasureInScore)
                            : this->MsToX(m_map.NotatedToMs(notatedTime));
    alignment->SetXRel(x - m_measureX);

    // Grace notes are stacked to the left of the note they belong to
    for (GraceAligner *graceAligner : alignment->GetGraceAligners() | std::views::values) {
        graceAligner->SetGraceAlignmentXPos(m_doc);
    }

    return FUNCTOR_CONTINUE;
}

FunctorCode CalcPerformanceXPosFunctor::VisitLayerElement(LayerElement *layerElement)
{
    const Alignment *alignment = layerElement->GetAlignment();
    if (!alignment) return FUNCTOR_CONTINUE;

    const Fraction notatedTime = m_measureTime + alignment->GetTime();
    const PerformedEvent *event = m_recording->GetEvent(layerElement->GetID());

    // A note doubled at the same notated time is often aligned only once - the copy belongs at
    // the same onset, and is no more unaligned than the note it doubles
    if (!event && layerElement->Is(NOTE)) {
        const Note *note = vrv_cast<const Note *>(layerElement);
        event = m_map.GetNoteEvent(notatedTime, note->GetMIDIPitch());
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
        interpolated.onsetMs = m_map.NotatedToMs(notatedTime);
        interpolated.matched = m_tiedFrom.contains(layerElement->GetID());
        // It sounds on with the velocity it was struck with, so that both halves of a tie are
        // drawn with the same ink density. Its duration is not, being measured from the attack.
        if (const PerformedEvent *struck = this->GetTieStartEvent(layerElement->GetID())) {
            interpolated.velocity = struck->velocity;
        }
        layerElement->m_perfEvent = interpolated;
    }

    return FUNCTOR_CONTINUE;
}

} // namespace vrv
