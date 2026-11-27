#pragma once

#include <deque>
#include <functional>
#include <vector>
#include <juce_data_structures/juce_data_structures.h>

namespace monolith::v2
{

/** Complete undoable edit state: every parameter's normalised value plus the control-plane routing tree. */
struct StateSnapshot
{
    std::vector<float> values;
    juce::ValueTree control;

    [[nodiscard]] bool sameAs (const StateSnapshot& o) const
    {
        return values == o.values
            && (control.isValid() == o.control.isValid())
            && (! control.isValid() || control.isEquivalentTo (o.control));
    }
};

/**
    Snapshot Undo / Redo (message thread only).

    Parameter edits are committed after they settle for debounceMs (one knob drag = one step); discrete
    actions (preset load, A/B swap, copy, routing edits) call commit() directly so they are undoable too.
    Restores go through the supplied callback, which glides the audio via the control plane's transition.
*/
class UndoHistory
{
public:
    using Capture = std::function<StateSnapshot()>;
    using Restore = std::function<void (const StateSnapshot&)>;

    UndoHistory (Capture c, Restore r, size_t maxSteps = 128, double debounceMs = 350.0)
        : capture (std::move (c)), restore (std::move (r)), maxDepth (maxSteps), debounce (debounceMs) {}

    void reset()
    {
        undoStack.clear();
        redoStack.clear();
        baseline = capture();
        lastSeen = baseline;
        pendingSinceMs = -1.0;
    }

    /** Timer: commits a settled edit. nowMs is any monotonic clock in milliseconds. */
    void poll (double nowMs)
    {
        auto cur = capture();
        if (cur.sameAs (baseline)) { pendingSinceMs = -1.0; lastSeen = std::move (cur); return; }
        if (pendingSinceMs < 0.0 || ! cur.sameAs (lastSeen)) { lastSeen = std::move (cur); pendingSinceMs = nowMs; return; }
        if (nowMs - pendingSinceMs >= debounce)
            push (std::move (cur));
    }

    /** Commits any pending difference immediately (call after programmatic, discrete changes). */
    void commit()
    {
        auto cur = capture();
        if (! cur.sameAs (baseline))
            push (std::move (cur));
    }

    bool undo()
    {
        commit();
        if (undoStack.empty())
            return false;
        redoStack.push_back (std::move (baseline));
        baseline = std::move (undoStack.back());
        undoStack.pop_back();
        apply();
        return true;
    }

    bool redo()
    {
        commit(); // a fresh edit after an undo invalidates the redo branch (push clears it)
        if (redoStack.empty())
            return false;
        undoStack.push_back (std::move (baseline));
        baseline = std::move (redoStack.back());
        redoStack.pop_back();
        apply();
        return true;
    }

    [[nodiscard]] bool canUndo() const noexcept { return ! undoStack.empty() || pendingSinceMs >= 0.0; }
    [[nodiscard]] bool canRedo() const noexcept { return ! redoStack.empty(); }
    [[nodiscard]] size_t getNumUndoSteps() const noexcept { return undoStack.size(); }
    [[nodiscard]] size_t getNumRedoSteps() const noexcept { return redoStack.size(); }

private:
    void push (StateSnapshot&& cur)
    {
        undoStack.push_back (std::move (baseline));
        while (undoStack.size() > maxDepth)
            undoStack.pop_front();
        redoStack.clear();
        baseline = std::move (cur);
        lastSeen = baseline;
        pendingSinceMs = -1.0;
    }

    void apply()
    {
        restore (baseline);
        baseline = capture(); // the restored state as the host sees it (normalise round-trips are not exact)
        lastSeen = baseline;
        pendingSinceMs = -1.0;
    }

    Capture capture;
    Restore restore;
    size_t maxDepth;
    double debounce;
    StateSnapshot baseline, lastSeen;
    std::deque<StateSnapshot> undoStack, redoStack;
    double pendingSinceMs = -1.0;
};

} // namespace monolith::v2
