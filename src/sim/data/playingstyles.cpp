#include "playingstyles.hpp"

#include <algorithm>
#include <cctype>

namespace PlayingStyles {

namespace {

const char* const comNames[comCount] = {
    "trickster",        "mazing_run",  "speeding_bullet", "incisive_run",
    "long_ball_expert", "early_cross", "long_ranger",
};

// Lowercases and drops separators so "Mazing Run", "mazing_run" and "mazingrun" all resolve to the same card.
std::string Normalize(const std::string& name) {
  std::string result;
  result.reserve(name.size());
  for (char character : name) {
    const unsigned char raw = static_cast<unsigned char>(character);
    if (std::isalnum(raw))
      result += static_cast<char>(std::tolower(raw));
  }
  return result;
}

}  // namespace

Com GetComAt(int index) {
  return static_cast<Com>(1u << std::max(0, std::min(index, comCount - 1)));
}

bool Has(ComMask mask, Com card) {
  return (mask & static_cast<ComMask>(card)) != 0;
}

ComMask ParseCom(const std::string& list) {
  ComMask mask = comMaskNone;
  size_t start = 0;
  while (start <= list.size()) {
    const size_t separator = list.find(',', start);
    const size_t end = separator == std::string::npos ? list.size() : separator;
    const std::string key = Normalize(list.substr(start, end - start));
    for (int i = 0; i < comCount; i++) {
      if (!key.empty() && key == Normalize(comNames[i]))
        mask |= static_cast<ComMask>(GetComAt(i));
    }
    if (separator == std::string::npos)
      break;
    start = separator + 1;
  }
  return mask;
}

std::string SerializeCom(ComMask mask) {
  std::string result;
  for (int i = 0; i < comCount; i++) {
    if (!Has(mask, GetComAt(i)))
      continue;
    if (!result.empty())
      result += ",";
    result += comNames[i];
  }
  return result;
}

}  // namespace PlayingStyles
