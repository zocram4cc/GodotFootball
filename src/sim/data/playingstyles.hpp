// PES 2021 COM Playing Styles - the "playing cards" that only steer AI-controlled players.
// The player's own playing style is not here: PES keeps it as a bit of the roster row's style word
// (editsave.hpp kRowStyleMask, pesstyle.hpp Bit).

#ifndef _HPP_PLAYINGSTYLES
#define _HPP_PLAYINGSTYLES

#include <string>

namespace PlayingStyles {

// GF's COM bit order (not PES's mask order, editsave.hpp kEditFlagMaskBit).
enum class Com : unsigned int {
  Trickster = 1u << 0,
  MazingRun = 1u << 1,
  SpeedingBullet = 1u << 2,
  IncisiveRun = 1u << 3,
  LongBallExpert = 1u << 4,
  EarlyCross = 1u << 5,
  LongRanger = 1u << 6,
};
using ComMask = unsigned int;
const ComMask comMaskNone = 0;
const int comCount = 7;
Com GetComAt(int index);
bool Has(ComMask mask, Com card);

// Tokens are snake_case ("mazing_run"); parsing is case- and separator-insensitive and ignores unknown entries.
ComMask ParseCom(const std::string& list);
std::string SerializeCom(ComMask mask);

}  // namespace PlayingStyles

#endif
