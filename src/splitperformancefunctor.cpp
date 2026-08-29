/////////////////////////////////////////////////////////////////////////////
// Name:        splitperformancefunctor.cpp
// Author:      Niels Pfeffer
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "splitperformancefunctor.h"

//----------------------------------------------------------------------------

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <ranges>
#include <utility>

//----------------------------------------------------------------------------

#include "beam.h"
#include "beamspan.h"
#include "doc.h"
#include "horizontalaligner.h"
#include "layer.h"
#include "layerelement.h"
#include "measure.h"
#include "staff.h"
#include "system.h"
#include "timeinterface.h"
#include "tuplet.h"
#include "vrv.h"

namespace vrv {

//----------------------------------------------------------------------------
// Helpers
//----------------------------------------------------------------------------

namespace {

    /** The alignment times of an element and of everything it contains */
    void CollectAlignmentTimes(const Object *object, std::vector<Fraction> &times)
    {
        if (object->IsLayerElement()) {
            const LayerElement *element = vrv_cast<const LayerElement *>(object);
            if (element->GetAlignment()) times.push_back(element->GetAlignment()->GetTime());
        }
        for (const Object *child : object->GetChildren()) {
            CollectAlignmentTimes(child, times);
        }
    }

    /**
     * The notated time an element of a layer reaches from and to, taken from the alignments it and
     * its content sit on. Empty for something that was never aligned, and a single point for a note
     * or a chord - only a container such as a beam or a tuplet reaches from one time to another.
     */
    std::optional<std::pair<Fraction, Fraction>> GetAlignmentSpan(const Object *object)
    {
        std::vector<Fraction> times;
        CollectAlignmentTimes(object, times);
        if (times.empty()) return std::nullopt;

        const auto [low, high] = std::ranges::minmax_element(times);

        return std::pair(*low, *high);
    }

    /** The mark carried by what a cut left on the far side of it, so that the merge finds it */
    const std::string PERFORMANCE_SPLIT = "perf-split";

    /** Where the mark stands among the words of a @type, npos when it is not one of them */
    size_t FindMark(const std::string &type, const std::string &mark)
    {
        for (size_t pos = type.find(mark); pos != std::string::npos; pos = type.find(mark, pos + 1)) {
            const size_t end = pos + mark.size();
            if (((pos == 0) || (type[pos - 1] == ' ')) && ((end == type.size()) || (type[end] == ' '))) return pos;
        }

        return std::string::npos;
    }

    /** Whether the element carries the given mark among the words of its @type */
    bool HasMark(const Object *object, const std::string &mark)
    {
        const AttTyped *typed = dynamic_cast<const AttTyped *>(object);

        return typed && (FindMark(typed->GetType(), mark) != std::string::npos);
    }

    void AddMark(Object *object, const std::string &mark)
    {
        AttTyped *typed = dynamic_cast<AttTyped *>(object);
        if (!typed) return;

        typed->SetType(typed->HasType() ? typed->GetType() + " " + mark : mark);
    }

    void RemoveMark(Object *object, const std::string &mark)
    {
        AttTyped *typed = dynamic_cast<AttTyped *>(object);
        if (!typed) return;

        std::string type = typed->GetType();
        const size_t pos = FindMark(type, mark);
        if (pos == std::string::npos) return;

        // The word goes with the space that separated it from the one before or after it
        type.erase(pos, mark.size());
        if (pos > 0) {
            type.erase(pos - 1, 1);
        }
        else if (!type.empty()) {
            type.erase(0, 1);
        }
        typed->SetType(type);
    }

    /**
     * Turn a beam the cut goes through into a beamSpan over the same elements.
     *
     * Two beams would each be drawn as a group closed in itself, and one left with a single note
     * would be drawn with neither a beam nor a flag. A beamSpan is drawn as one beam that runs to
     * the edge of the system and is taken up again on the next, which is how a beam broken by a
     * system break is written. The span carries the beam's identity, so that merging the measures
     * gives the beam back as it was encoded.
     */
    void ConvertBeamToSpan(Object *beam)
    {
        Object *parent = beam->GetParent();
        Measure *measure = vrv_cast<Measure *>(beam->GetFirstAncestor(MEASURE));
        assert(parent && measure);

        const ArrayOfObjects elements(beam->GetChildren());
        if (elements.empty()) return;

        const Beam *source = vrv_cast<const Beam *>(beam);
        BeamSpan *span = new BeamSpan();
        span->SetID(beam->GetID());
        span->SetType(source->HasType() ? source->GetType() + " " + PERFORMANCE_SPLIT : PERFORMANCE_SPLIT);
        span->SetColor(source->GetColor());
        span->SetLabel(source->GetLabel());
        span->SetBeamWith(source->GetBeamWith());
        span->SetForm(source->GetForm());
        span->SetPlace(source->GetPlace());
        span->SetSlash(source->GetSlash());
        span->SetSlope(source->GetSlope());
        // Whatever the beam was linked to is linked to the span while it stands for it
        *span->GetLinkingInterface() = *source->GetLinkingInterface();

        xsdAnyURI_List plist;
        std::ranges::transform(
            elements, std::back_inserter(plist), [](const Object *element) { return "#" + element->GetID(); });
        span->SetPlist(plist);
        span->SetStartid("#" + elements.front()->GetID());
        span->SetEndid("#" + elements.back()->GetID());
        span->SetBeamedElements(elements);

        // The elements take the place of the beam, which the span now stands for
        parent->MoveChildrenFrom(beam, parent->GetChildIndex(beam), true);
        beam->ClearRelinquishedChildren();
        parent->DeleteChild(beam);
        measure->AddChild(span);
    }

    /** The elements a beamSpan holds together, from what it kept or from its plist */
    ArrayOfObjects GetSpanElements(const BeamSpan *span)
    {
        if (!span->GetBeamedElements().empty()) return span->GetBeamedElements();

        const Object *system = span->GetFirstAncestor(SYSTEM);
        if (!system) return {};

        ArrayOfObjects elements;
        for (const std::string &uri : span->GetPlist()) {
            Object *element = const_cast<Object *>(system->FindDescendantByID(ExtractIDFragment(uri)));
            if (element) elements.push_back(element);
        }

        return elements;
    }

    /**
     * Put the beam a cut turned into a beamSpan back together, once the measures it reached across
     * have been merged and its elements stand next to each other again.
     */
    void RestoreBeamFromSpan(BeamSpan *span)
    {
        Object *measure = span->GetParent();
        const ArrayOfObjects elements = GetSpanElements(span);
        assert(measure);

        if (elements.empty()) {
            measure->DeleteChild(span);
            return;
        }

        Object *parent = elements.front()->GetParent();
        Beam *beam = new Beam();
        beam->SetID(span->GetID());
        beam->SetType(span->GetType());
        RemoveMark(beam, PERFORMANCE_SPLIT);
        beam->SetColor(span->GetColor());
        beam->SetLabel(span->GetLabel());
        beam->SetBeamWith(span->GetBeamWith());
        beam->SetForm(span->GetForm());
        beam->SetPlace(span->GetPlace());
        beam->SetSlash(span->GetSlash());
        beam->SetSlope(span->GetSlope());
        *beam->GetLinkingInterface() = *span->GetLinkingInterface();

        parent->InsertBefore(elements.front(), beam);
        for (Object *element : elements) {
            element->MoveItselfTo(beam);
        }
        parent->ClearRelinquishedChildren();
        measure->DeleteChild(span);
    }

    /**
     * Move everything of one element of a layer that sounds at or after the cut into the part of
     * the measure that follows.
     *
     * A note or a chord goes whole, and one held across the cut stays where it was struck. A beam
     * reaching across becomes a beamSpan and a tuplet is cut in two, so that a break point never
     * has to be moved to where the music happens to allow it. What ends up on the far side of the
     * cut is marked, so that merging the measure again puts the group back together as encoded.
     */
    void SplitAtTime(Object *object, Object *target, const Fraction &offset)
    {
        const std::optional<std::pair<Fraction, Fraction>> span = GetAlignmentSpan(object);
        // Something that was never aligned has no time of its own to be placed by
        if (!span) return;

        if (span->first >= offset) {
            object->MoveItselfTo(target);
            return;
        }
        // Anything the cut does not reach into stays whole on the system it began on
        if (span->second < offset) return;

        // A beam becomes one beam drawn across the break, and its notes are then moved as they
        // would be if they had never been beamed
        if (object->Is(BEAM)) {
            const ArrayOfObjects elements(object->GetChildren());
            ConvertBeamToSpan(object);
            for (Object *element : elements) {
                SplitAtTime(element, target, offset);
            }
            return;
        }

        // Anything else that cannot be taken apart stays whole as well
        if (!object->Is(TUPLET)) return;

        Object *copy = ObjectFactory::GetInstance().Create(object->GetClassId());
        if (!copy) return;
        object->CopyAttributesTo(copy);
        AddMark(copy, PERFORMANCE_SPLIT);
        // The two halves are one group of the same notes, so only the first of them is numbered
        vrv_cast<Tuplet *>(copy)->SetNumVisible(BOOLEAN_false);
        target->AddChild(copy);

        const ArrayOfObjects children(object->GetChildren());
        for (Object *child : children) {
            SplitAtTime(child, copy, offset);
        }
        object->ClearRelinquishedChildren();
    }

    /**
     * Put the half of a tuplet the cut left on the far side of it back into the one it was cut
     * from, which is the last thing in the layer the merge is filling. Returns false when the
     * child is not a continuation, i.e. when the caller has to move it itself.
     */
    bool MergeSplitChild(Object *child, Object *target)
    {
        if (!HasMark(child, PERFORMANCE_SPLIT)) return false;

        // The mark belongs to the cut and not to the score, so it goes whatever comes of it
        RemoveMark(child, PERFORMANCE_SPLIT);

        Object *previous = target->GetLast();
        if (!previous || !previous->Is(child->GetClassId())) return false;

        // Anything within the copy that the same cut went through goes back the same way
        const ArrayOfObjects children(child->GetChildren());
        for (Object *inner : children) {
            if (!MergeSplitChild(inner, previous)) inner->MoveItselfTo(previous);
        }
        child->ClearRelinquishedChildren();

        // The copy itself is left behind and goes with the measure it is deleted with
        return true;
    }

    /** The child of the given type carrying the given @n, NULL when there is none */
    Object *GetChildWithN(Object *parent, ClassId classId, int n)
    {
        for (Object *child : parent->GetChildren()) {
            if (!child->Is(classId)) continue;
            const AttNInteger *att = dynamic_cast<const AttNInteger *>(child);
            if (att && (att->GetN() == n)) return child;
        }

        return NULL;
    }

    /** The notated duration a measure covers, which a measure that was cut carries itself */
    Fraction GetMeasureDuration(const Measure *measure)
    {
        if (measure->GetPerformanceSegment()) return measure->GetPerformanceSegment()->duration;

        return measure->m_measureAligner.GetMaxTime();
    }

    /**
     * Cut one measure in two at the given notated offset from its start, which is where the moment
     * given in ms falls.
     * The part that follows is added to the system right after it and holds everything that sounds
     * at or after the cut; the barline between the two is not drawn, since the music simply goes on.
     */
    void CutMeasure(Measure *measure, const Fraction &offset, double ms)
    {
        System *system = vrv_cast<System *>(measure->GetParent());
        assert(system);

        PerformanceSegment current;
        current.duration = GetMeasureDuration(measure);
        if (measure->GetPerformanceSegment()) current = *measure->GetPerformanceSegment();

        Measure *segment = new Measure(MEASURED);
        measure->CopyAttributesTo(segment);
        segment->SetLeft(BARRENDITION_invis);
        // A part that continues a measure carries no number of its own
        segment->SetN("");
        segment->SetMetcon(BOOLEAN_false);
        segment->SetType(measure->HasType() ? measure->GetType() + " perf-segment" : "perf-segment");
        system->InsertAfter(measure, segment);

        // Nothing is drawn where the measure was cut - the music goes on across the system break
        measure->SetRight(BARRENDITION_invis);

        ArrayOfObjects staves;
        std::ranges::copy_if(
            measure->GetChildren(), std::back_inserter(staves), [](const Object *child) { return child->Is(STAFF); });

        for (Object *object : staves) {
            Staff *staff = vrv_cast<Staff *>(object);
            Staff *segmentStaff = new Staff();
            staff->CopyAttributesTo(segmentStaff);
            // The staff is created even when nothing moves into it, so that its lines are still
            // drawn on the system that follows
            segment->AddChild(segmentStaff);

            ArrayOfObjects layers;
            std::ranges::copy_if(
                staff->GetChildren(), std::back_inserter(layers), [](const Object *child) { return child->Is(LAYER); });

            for (Object *layerObject : layers) {
                Layer *layer = vrv_cast<Layer *>(layerObject);
                Layer *segmentLayer = new Layer();
                layer->CopyAttributesTo(segmentLayer);
                segmentStaff->AddChild(segmentLayer);

                const ArrayOfObjects children(layer->GetChildren());
                for (Object *child : children) {
                    SplitAtTime(child, segmentLayer, offset);
                }
                layer->ClearRelinquishedChildren();

                // A layer whose note is held over the cut only resumes further on, and one with
                // nothing left at all reaches to the end of the part
                const std::optional<std::pair<Fraction, Fraction>> moved = GetAlignmentSpan(segmentLayer);
                segmentLayer->SetSegmentStartTime((moved ? moved->first : current.duration) - offset);
            }
        }

        // A control event goes with the note it starts on. One hanging on a timestamp stays where
        // it was, since the timestamp it was resolved against belongs to the measure it was in.
        ArrayOfObjects controlEvents;
        std::ranges::copy_if(measure->GetChildren(), std::back_inserter(controlEvents),
            [](const Object *child) { return !child->Is(STAFF); });

        for (Object *child : controlEvents) {
            // A span this or an earlier cut made knows the elements it holds together before
            // anything has resolved its references
            const Object *start = NULL;
            if (child->Is(BEAMSPAN)) {
                const ArrayOfObjects &elements = vrv_cast<BeamSpan *>(child)->GetBeamedElements();
                if (!elements.empty()) start = elements.front();
            }
            else if (TimePointInterface *interface = child->GetTimePointInterface(); interface) {
                start = interface->GetStart();
                if (start && start->Is(TIMESTAMP_ATTR)) start = NULL;
            }
            if (!start) continue;

            if (start->GetFirstAncestor(MEASURE) == segment) child->MoveItselfTo(segment);
        }
        measure->ClearRelinquishedChildren();

        // Each part is drawn between the moments it was cut at, and the one the cut made carries
        // the moment its system opens at
        PerformanceSegment first = current;
        first.duration = offset;
        first.endMs = ms;
        measure->SetPerformanceSegment(first);

        PerformanceSegment second;
        second.duration = current.duration - offset;
        second.isContinuation = true;
        second.startsSystem = true;
        second.startMs = ms;
        second.endMs = current.endMs;
        segment->SetPerformanceSegment(second);
    }

} // namespace

//----------------------------------------------------------------------------
// CalcPerformanceBreaksFunctor
//----------------------------------------------------------------------------

CalcPerformanceBreaksFunctor::CalcPerformanceBreaksFunctor(
    Doc *doc, const PerformanceMap &map, double firstMs, double intervalMs)
    : DocFunctor(doc), m_map(map), m_firstMs(firstMs), m_intervalMs(intervalMs)
{
}

FunctorCode CalcPerformanceBreaksFunctor::VisitMeasure(Measure *measure)
{
    if (m_intervalMs <= 0.0) return FUNCTOR_CONTINUE;

    const Fraction measureEnd = m_measureTime + measure->m_measureAligner.GetMaxTime();

    // Every moment the clock asks for that falls within this measure opens a system there. There
    // can be several of them in a long measure, and none at all in a short one.
    while (true) {
        const double ms = m_firstMs + m_system * m_intervalMs;
        // A system opening after the last note was struck would hold nothing but the sound still
        // dying away, which the system in front of it carries on to instead
        if (ms >= m_map.GetLastOnsetMs()) break;

        const Fraction time = m_map.MsToNotated(ms);
        // The measures that follow will be asked for it in turn
        if (time >= measureEnd) break;

        // A break point never falls before the one in front of it, whatever the recording does
        const Fraction previous = m_breakPoints.empty() ? m_measureTime : m_breakPoints.back().time;
        if (time <= previous) {
            ++m_system;
            continue;
        }

        m_breakPoints.push_back({ measure, m_measureTime, time, ms });
        ++m_system;
    }

    return FUNCTOR_CONTINUE;
}

FunctorCode CalcPerformanceBreaksFunctor::VisitMeasureEnd(Measure *measure)
{
    m_measureTime = m_measureTime + measure->m_measureAligner.GetMaxTime();

    return FUNCTOR_CONTINUE;
}

//----------------------------------------------------------------------------
// MergePerformanceMeasuresFunctor
//----------------------------------------------------------------------------

int MergePerformanceMeasuresFunctor::DeleteMergedMeasures()
{
    // The beams the cut drew across the breaks are made again now that the measures they reached
    // across are one measure and their notes stand next to each other
    for (BeamSpan *span : m_beamSpans) {
        RestoreBeamFromSpan(span);
    }
    m_beamSpans.clear();

    const int count = static_cast<int>(m_merged.size());
    for (Measure *measure : m_merged) {
        Object *system = measure->GetParent();
        assert(system);
        system->DeleteChild(measure);
    }
    m_merged.clear();

    return count;
}

FunctorCode MergePerformanceMeasuresFunctor::VisitMeasure(Measure *measure)
{
    // Collected wherever they stand, since a cut can have left one in a measure that is merged
    // into another one further on
    for (Object *child : measure->GetChildren()) {
        if (child->Is(BEAMSPAN) && HasMark(child, PERFORMANCE_SPLIT)) {
            m_beamSpans.push_back(vrv_cast<BeamSpan *>(child));
        }
    }

    if (!measure->IsPerformanceContinuation()) {
        // Where the systems open is decided anew every time, so nothing of it is kept
        measure->ResetPerformanceSegment();
        m_contentMeasure = measure;
        return FUNCTOR_SIBLINGS;
    }

    // Cannot happen with measures that came out of a cut, since a cut always leaves the part it
    // was made from in front of them
    if (!m_contentMeasure) return FUNCTOR_SIBLINGS;

    for (Object *staffObject : measure->GetChildren()) {
        if (!staffObject->Is(STAFF)) continue;
        Staff *staff = vrv_cast<Staff *>(staffObject);
        Object *targetStaff = GetChildWithN(m_contentMeasure, STAFF, staff->GetN());
        if (!targetStaff) continue;

        for (Object *layerObject : staff->GetChildren()) {
            if (!layerObject->Is(LAYER)) continue;
            Layer *layer = vrv_cast<Layer *>(layerObject);
            Layer *targetLayer = dynamic_cast<Layer *>(GetChildWithN(targetStaff, LAYER, layer->GetN()));
            if (!targetLayer) continue;

            const ArrayOfObjects children(layer->GetChildren());
            for (Object *child : children) {
                if (!MergeSplitChild(child, targetLayer)) child->MoveItselfTo(targetLayer);
            }
            layer->ClearRelinquishedChildren();
            // The measure runs from its own start again, so no layer of it begins anywhere else
            targetLayer->SetSegmentStartTime(0);
        }
    }

    ArrayOfObjects controlEvents;
    std::ranges::copy_if(measure->GetChildren(), std::back_inserter(controlEvents),
        [](const Object *child) { return !child->Is(STAFF); });

    for (Object *child : controlEvents) {
        child->MoveItselfTo(m_contentMeasure);
    }
    measure->ClearRelinquishedChildren();

    // The last part of a cut measure is the one carrying the barline that closes it
    m_contentMeasure->SetRight(measure->GetRight());
    m_merged.push_back(measure);

    return FUNCTOR_SIBLINGS;
}

//----------------------------------------------------------------------------
// Cutting the measures
//----------------------------------------------------------------------------

int CutPerformanceMeasures(const std::vector<PerformanceBreakPoint> &breakPoints)
{
    // A break point falling on a barline only says where a system opens; the others cut a measure
    std::map<Measure *, std::vector<std::pair<Fraction, double>>> cuts;
    for (const PerformanceBreakPoint &breakPoint : breakPoints) {
        const Fraction offset = breakPoint.time - breakPoint.measureTime;
        if (offset <= 0) {
            PerformanceSegment segment;
            segment.duration = GetMeasureDuration(breakPoint.measure);
            if (breakPoint.measure->GetPerformanceSegment()) segment = *breakPoint.measure->GetPerformanceSegment();
            segment.startsSystem = true;
            segment.startMs = breakPoint.ms;
            breakPoint.measure->SetPerformanceSegment(segment);
        }
        else {
            cuts[breakPoint.measure].push_back({ offset, breakPoint.ms });
        }
    }

    int count = 0;
    for (auto &[measure, offsets] : cuts) {
        // Several cuts in one measure are made from the last one backwards, so that every part is
        // added to the system in front of the one made before it and they come out in order
        std::ranges::sort(offsets, std::ranges::greater{});
        for (const auto &[offset, ms] : offsets) {
            CutMeasure(measure, offset, ms);
            ++count;
        }
    }

    return count;
}

} // namespace vrv
