#pragma once

// Research ("research=1" in multidiva.ini): finds where the game keeps a round's score data. At the result screen it
// logs the stage entry's result block (ReDIVA struc_662) and every other place in the game's static memory holding
// one of its values, with the memory around each: the live score, combo and hit counts sit next to each other there.
namespace md::finder {

void OnResultShown();

} // namespace md::finder
