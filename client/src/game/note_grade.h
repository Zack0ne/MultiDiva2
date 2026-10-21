#pragma once
#include <cstdint>

namespace md {

// Inferred from score deltas, so holds make "Bad", "Wrong Bad" and "Wrong Safe" untrackable.
enum class NoteGrade : int32_t { Cool, Good, Safe, CoolWrong, GoodWrong, Count };

const char* ToString(NoteGrade g);

// Port of the legacy GetFrameScore heuristic: feed it the live score/combo every frame
// and it reports a grade whenever the score delta matches a known note value.
class NoteGradeTracker {
public:
    void Reset();

    // Returns true and sets `grade` when this frame's score change maps to a note grade.
    bool Update(int32_t score, int32_t combo, NoteGrade& grade);

    uint32_t Count(NoteGrade g) const { return m_counts[(int)g]; }
    int32_t LastDelta() const { return m_lastDelta; }

private:
    int32_t m_oldScore = 0;
    int32_t m_lastCombo = -1;
    int32_t m_lastDelta = 0;
    uint32_t m_counts[(int)NoteGrade::Count] = {};
};

} // namespace md
