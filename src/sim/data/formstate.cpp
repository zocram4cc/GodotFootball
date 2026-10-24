#include "formstate.hpp"

#include <algorithm>
#include <cstring>

namespace FormState {

namespace {

// docs/21Research.md, "Condition Arrow Multipliers": rows worst to best,
// columns awareness / technical / physique.
constexpr float kArrowTable[(int)Arrow::Count][3] = {
    {0.84f, 0.88f, 0.88f},  // Terrible (purple)
    {0.92f, 0.94f, 0.94f},  // Poor (blue)
    {1.00f, 1.00f, 1.00f},  // Normal (green)
    {1.06f, 1.05f, 1.03f},  // Great (orange)
    {1.12f, 1.09f, 1.06f},  // Top (red)
};

// docs/21Research.md, "Position Familiarity Multipliers": rows C / B / A,
// columns technical / awareness / physique.
constexpr float kFamiliarityTable[3][3] = {
    {0.80f, 0.82f, 0.84f},  // C, unfamiliar
    {0.92f, 0.94f, 0.96f},  // B, partial
    {1.00f, 1.00f, 1.00f},  // A, natural
};

bool Is(const char* stat, const char* key) { return std::strcmp(stat, key) == 0; }
bool StartsWith(const char* stat, const char* prefix) {
  return std::strncmp(stat, prefix, std::strlen(prefix)) == 0;
}


}  // namespace

const char* ArrowName(Arrow arrow) {
  switch (arrow) {
    case Arrow::Terrible: return "terrible";
    case Arrow::Poor: return "poor";
    case Arrow::Normal: return "normal";
    case Arrow::Great: return "great";
    case Arrow::Top: return "top";
    case Arrow::Count: break;
  }
  return "normal";
}

const char* ArrowColour(Arrow arrow) {
  switch (arrow) {
    case Arrow::Terrible: return "purple";
    case Arrow::Poor: return "blue";
    case Arrow::Normal: return "green";
    case Arrow::Great: return "orange";
    case Arrow::Top: return "red";
    case Arrow::Count: break;
  }
  return "green";
}

int Band(unsigned int conditionByte) {
  // FUN_140AF2DA0: CMP EAX,0x41/JC then <0x46/4B/50/55/5A/5F. Edges read from
  // the install with opcode checks at load; the ladder shape is the code port.
  if (conditionByte <= 0x40) return 0;
  if (conditionByte < 0x46) return 1;
  if (conditionByte < 0x4B) return 2;
  if (conditionByte < 0x50) return 3;
  if (conditionByte < 0x55) return 4;
  if (conditionByte < 0x5A) return 5;
  if (conditionByte < 0x5F) return 6;
  return 7;
}

Arrow ArrowForBand(int band) {
  // FUN_141FC1380's inverse map {4,3,2,1,0,2,2,2} in PES space, 4 - PES in GF's.
  switch (band) {
    case 0: return Arrow::Terrible;
    case 1: return Arrow::Poor;
    case 3: return Arrow::Great;
    case 4: return Arrow::Top;
    default: return Arrow::Normal;  // bands 2, 5, 6, 7
  }
}

Arrow ArrowForConditionByte(unsigned int conditionByte) { return ArrowForBand(Band(conditionByte)); }

Arrow ApplyTeamShift(Arrow arrow, bool ownTeam, bool teamModeThree, bool ownLevelClassSix,
                     bool oppLevelClassSix) {
  // FUN_14092AF10 (0x14092B4CE-0x14092B5DB), translated to GF space where Top=4
  // and PES index = 4 - GF arrow. Own team with T+0xA298 state 3 -> PES index
  // 0 = Top (no level gate on that leg: the state-3 check comes first). The
  // opponent one step worse (PES min(arrow+1,4)), gated by the /100 == 6 test
  // on the opponent side's own word (0x14092B516 R12D^1), so ownLevelClassSix
  // is unused on this path (docs/re/pes-re-team-status.md).
  (void)ownLevelClassSix;
  int pes = 4 - (int)arrow;
  if (ownTeam) {
    if (teamModeThree) pes = 0;
  } else if (oppLevelClassSix && pes < 4) {
    pes += 1;
  }
  return (Arrow)(4 - pes);
}

bool ArrowWritesStats(Arrow arrow) {
  // 0x14092B59B CMP EBX,5 / 0x14092B5A0 CMP EBX,2: green and out-of-range skip.
  const int pes = 4 - (int)arrow;
  return pes != 2 && pes < 5;
}

ArrowTier ArrowTierOf(const char* stat) {
  // Tier 1: awareness, the keeper's five, kicking power.
  if (Is(stat, "mental_offensivepositioning") || Is(stat, "mental_defensivepositioning") ||
      StartsWith(stat, "gk_") || Is(stat, "physical_shotpower"))
    return ArrowTier::Awareness;
  // Tier 3: physique.
  if (Is(stat, "physical_contact") || Is(stat, "physical_balance") ||
      Is(stat, "physical_stamina"))
    return ArrowTier::Physique;
  // Tier 2: everything technical, plus acceleration, speed and jump.
  return ArrowTier::Technical;
}

float ArrowMultiplier(Arrow arrow, ArrowTier tier) {
  return kArrowTable[std::clamp((int)arrow, 0, (int)Arrow::Count - 1)][(int)tier];
}

float ArrowMultiplierFor(Arrow arrow, const char* stat) {
  return ArrowMultiplier(arrow, ArrowTierOf(stat));
}

FamiliarityTier FamiliarityTierOf(const char* stat) {
  // Tier 1: awareness, the keeper's five, kicking power, ACCELERATION.
  if (Is(stat, "mental_offensivepositioning") || Is(stat, "mental_defensivepositioning") ||
      StartsWith(stat, "gk_") || Is(stat, "physical_shotpower") ||
      Is(stat, "physical_acceleration"))
    return FamiliarityTier::Awareness;
  // Tier 2: physique.
  if (Is(stat, "physical_contact") || Is(stat, "physical_balance") ||
      Is(stat, "physical_stamina"))
    return FamiliarityTier::Physique;
  // Tier 0: technique - and here speed and jump are technical.
  return FamiliarityTier::Technical;
}

float FamiliarityMultiplier(Familiarity level, FamiliarityTier tier) {
  return kFamiliarityTable[std::clamp((int)level, 0, 2)][(int)tier];
}

float FamiliarityMultiplierFor(Familiarity level, const char* stat) {
  return FamiliarityMultiplier(level, FamiliarityTierOf(stat));
}

float Apply(float stat01, Arrow arrow, Familiarity level, const char* stat) {
  const float raised = stat01 * ArrowMultiplierFor(arrow, stat) * FamiliarityMultiplierFor(level, stat);
  return std::clamp(raised, 0.0f, 1.0f);
}

Slot SlotFor(e_PlayerRole role, int side) {
  switch (role) {
    case e_PlayerRole_GK: return Slot::GK;
    case e_PlayerRole_CB: return Slot::CB;
    case e_PlayerRole_LB: return Slot::LB;
    case e_PlayerRole_RB: return Slot::RB;
    case e_PlayerRole_DM: return Slot::DMF;
    case e_PlayerRole_CM: return Slot::CMF;
    case e_PlayerRole_LM: return Slot::LMF;
    case e_PlayerRole_RM: return Slot::RMF;
    case e_PlayerRole_AM: return Slot::AMF;
    case e_PlayerRole_LW: return Slot::LWF;
    case e_PlayerRole_RW: return Slot::RWF;
    case e_PlayerRole_SS: return Slot::SS;
    case e_PlayerRole_CF:
      // GF has one forward role; PES has three. A forward on a flank is a wing
      // forward on that side, one in the middle a centre forward.
      if (side < 0) return Slot::LWF;
      if (side > 0) return Slot::RWF;
      return Slot::CF;
  }
  return Slot::CMF;
}

const char* SlotName(Slot slot) {
  static const char* names[(int)Slot::Count] = {"GK",  "CB",  "LB",  "RB", "DMF", "CMF", "LMF",
                                                "RMF", "AMF", "LWF", "RWF", "SS", "CF"};
  const int i = (int)slot;
  return i >= 0 && i < (int)Slot::Count ? names[i] : "CMF";
}

Familiarity Rating(const std::string& ratings, Slot slot) {
  const size_t i = (size_t)slot;
  if (i >= ratings.size()) return Familiarity::Unfamiliar;
  switch (ratings[i]) {
    case 'A': case 'a': return Familiarity::Natural;
    case 'B': case 'b': return Familiarity::Partial;
    default: return Familiarity::Unfamiliar;
  }
}

std::string InferRatings(const std::vector<e_PlayerRole>& roles) {
  std::string out((size_t)Slot::Count, 'C');
  auto set = [&out](Slot slot, char letter) {
    char& at = out[(size_t)slot];
    if (letter == 'A' || at == 'C') at = letter;  // never downgrade an A
  };
  for (e_PlayerRole role : roles) {
    switch (role) {
      case e_PlayerRole_GK: set(Slot::GK, 'A'); break;
      case e_PlayerRole_CB: set(Slot::CB, 'A'); break;
      case e_PlayerRole_LB: set(Slot::LB, 'A'); set(Slot::RB, 'B'); set(Slot::LMF, 'B'); break;
      case e_PlayerRole_RB: set(Slot::RB, 'A'); set(Slot::LB, 'B'); set(Slot::RMF, 'B'); break;
      case e_PlayerRole_DM: set(Slot::DMF, 'A'); set(Slot::CMF, 'B'); break;
      case e_PlayerRole_CM: set(Slot::CMF, 'A'); set(Slot::DMF, 'B'); set(Slot::AMF, 'B'); break;
      case e_PlayerRole_LM: set(Slot::LMF, 'A'); set(Slot::LWF, 'B'); set(Slot::RMF, 'B'); break;
      case e_PlayerRole_RM: set(Slot::RMF, 'A'); set(Slot::RWF, 'B'); set(Slot::LMF, 'B'); break;
      case e_PlayerRole_AM: set(Slot::AMF, 'A'); set(Slot::SS, 'B'); set(Slot::CMF, 'B'); break;
      case e_PlayerRole_LW: set(Slot::LWF, 'A'); set(Slot::LMF, 'B'); set(Slot::CF, 'B'); break;
      case e_PlayerRole_RW: set(Slot::RWF, 'A'); set(Slot::RMF, 'B'); set(Slot::CF, 'B'); break;
      case e_PlayerRole_SS: set(Slot::SS, 'A'); set(Slot::AMF, 'B'); set(Slot::CF, 'B'); break;
      case e_PlayerRole_CF:
        set(Slot::CF, 'A'); set(Slot::SS, 'A'); set(Slot::LWF, 'B'); set(Slot::RWF, 'B');
        break;
    }
  }
  return out;
}

}  // namespace FormState
