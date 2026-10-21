#include "note_grade.h"

namespace md {

const char* ToString(NoteGrade g)
{
    switch (g) {
    case NoteGrade::Cool: return "Cool";
    case NoteGrade::Good: return "Good";
    case NoteGrade::Safe: return "Safe";
    case NoteGrade::CoolWrong: return "Cool (wrong)";
    case NoteGrade::GoodWrong: return "Good (wrong)";
    default: return "?";
    }
}

void NoteGradeTracker::Reset()
{
    *this = NoteGradeTracker{};
}

// `delta` is in [base, base+maxExtra] and (delta - base) is a multiple of 10, for any base in `bases`.
// Covers the +10/combo-step and health bonuses the game stacks on top of a note's value.
template <size_t N>
static bool Matches(int32_t delta, const int32_t (&bases)[N], int32_t maxExtra)
{
    for (int32_t base : bases) {
        int32_t extra = delta - base;
        if (extra >= 0 && extra <= maxExtra && extra % 10 == 0)
            return true;
    }
    return false;
}

bool NoteGradeTracker::Update(int32_t score, int32_t combo, NoteGrade& grade)
{
    if (score == m_oldScore)
        return false;

    int32_t delta = score - m_oldScore;

    // Every 10 combo adds a 50 point bonus, capped at 50 combo.
    delta -= combo >= 50 ? 250 : (combo / 10) * 50;

    m_oldScore = score;
    m_lastDelta = delta;

    // Same value table as the legacy switch: 1-4 simultaneous notes, plus bonuses.
    static const int32_t cool[] = { 500, 1000, 1500, 2000 };      // +0..50
    static const int32_t good[] = { 300, 600, 900, 1200 };        // +0..50
    static const int32_t safe[] = { 100, 200, 400 };              // +0..40
    static const int32_t coolWrong[] = { 250, 750 };              // +0..40
    static const int32_t goodWrong[] = { 150, 450 };              // +0..40

    bool matched = false;
    if (Matches(delta, cool, 50)) {
        if (m_lastCombo < combo) { grade = NoteGrade::Cool; matched = true; }
    } else if (Matches(delta, good, 50)) {
        if (m_lastCombo < combo) { grade = NoteGrade::Good; matched = true; }
    } else if (Matches(delta, safe, 40)) {
        if (combo == 0) { grade = NoteGrade::Safe; matched = true; }
    } else if (Matches(delta, coolWrong, 40)) {
        if (combo == 0) { grade = NoteGrade::CoolWrong; matched = true; }
    } else if (Matches(delta, goodWrong, 40)) {
        if (combo == 0) { grade = NoteGrade::GoodWrong; matched = true; }
    }

    m_lastCombo = combo;
    if (matched)
        m_counts[(int)grade]++;
    return matched;
}

} // namespace md
