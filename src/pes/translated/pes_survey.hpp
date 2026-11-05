// pes_survey.hpp: survey mode at the untranslated-dispatch site (pes_dispatch_missing), GF_PES_SURVEY=<file>.
//
// Unset (the default) nothing here changes behaviour: a missing target runs the caller's recovery, if any, then aborts.
// Set, every hit on an untranslated target is counted, and the list is rewritten when a target first appears and at
// exit, one line per target: `target hits first_frame caller_offset`. first_frame is the sim's logic frame at the first
// hit (pes_survey_set_frame). caller_offset is the return address into the function that made the call, relative to the
// executable's load base: `nm -C -n <exe>` names the translated function (pes_fn_<VA>) at or below it.
// The run continues only through the caller's recovery (pes_dispatch_missing_handler, which must not return: the GF
// path that stands in for the translated call). With none, the survey ends at that target. Either way the process
// exits with kPesSurveyExitCode, so a survey run never passes as a smoke.
// ponytail: no locking; the sim's tick runs on one thread.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <link.h>
#include <map>

// Distinct from abort (134), timeout (124) and the ctest skip (77).
inline constexpr int kPesSurveyExitCode = 79;

namespace pes_survey_detail {

struct Hit {
  uint64_t hits = 0;
  uint64_t firstFrame = 0;
  uintptr_t firstCallerOffset = 0;
};

struct State {
  const char* path = nullptr;    // GF_PES_SURVEY; null when the survey is off
  uintptr_t exeBase = 0;         // load bias of the executable (dl_iterate_phdr's first object)
  uint64_t frame = 0;            // the sim's logic frame (pes_survey_set_frame)
  std::map<uint64_t, Hit> hits;  // by target VA, so the list is stable
};

inline void WriteList(const State& s) {
  std::FILE* f = std::fopen(s.path, "w");
  if (f == nullptr) {
    std::fprintf(stderr, "GF_PES_SURVEY: cannot write %s\n", s.path);
    return;
  }
  std::fprintf(f, "# untranslated targets reached; a survey run exits %d and never passes\n", kPesSurveyExitCode);
  std::fprintf(f, "# target hits first_frame caller_offset\n");
  for (const auto& [target, h] : s.hits)
    std::fprintf(f, "%#llx %llu %llu %#llx\n", static_cast<unsigned long long>(target),
                 static_cast<unsigned long long>(h.hits), static_cast<unsigned long long>(h.firstFrame),
                 static_cast<unsigned long long>(h.firstCallerOffset));
  std::fclose(f);
}

inline State& Get() {
  // Built once and never destroyed: the exit writer reads it during exit, after static destructors would have run.
  static State& s = [] () -> State& {
    State* st = new State();
    const char* path = std::getenv("GF_PES_SURVEY");
    if (path != nullptr && *path != '\0') {
      st->path = path;
      dl_iterate_phdr(
          [](dl_phdr_info* info, size_t, void* out) {
            *static_cast<uintptr_t*>(out) = info->dlpi_addr;
            return 1;  // the executable is the first object
          },
          &st->exeBase);
      std::atexit([] {
        WriteList(Get());
        std::fflush(nullptr);
        std::_Exit(kPesSurveyExitCode);  // _Exit: the status is ours, and the handlers do not run twice
      });
    }
    return *st;
  }();
  return s;
}

// Every translation unit that includes this header arms the survey at startup (Get is idempotent).
[[maybe_unused]] static const bool kArmedAtStartup = (Get(), true);

}  // namespace pes_survey_detail

inline bool pes_survey_armed() { return pes_survey_detail::Get().path != nullptr; }

// The sim's logic frame. A hit's first_frame is the frame set last.
inline void pes_survey_set_frame(uint64_t frame) { pes_survey_detail::Get().frame = frame; }

// Counts one hit on `target`. `caller` is the return address into the function that called pes_dispatch_missing.
inline void pes_survey_note(uint64_t target, const void* caller) {
  pes_survey_detail::State& s = pes_survey_detail::Get();
  if (s.path == nullptr) return;
  auto [it, fresh] = s.hits.try_emplace(target);
  pes_survey_detail::Hit& h = it->second;
  if (h.hits == 0) {
    h.firstFrame = s.frame;
    h.firstCallerOffset = reinterpret_cast<uintptr_t>(caller) - s.exeBase;
  }
  ++h.hits;
  if (fresh) pes_survey_detail::WriteList(s);
}

// Writes the list and exits with kPesSurveyExitCode. Never returns.
[[noreturn]] inline void pes_survey_end() {
  pes_survey_detail::WriteList(pes_survey_detail::Get());
  std::fflush(nullptr);
  std::_Exit(kPesSurveyExitCode);
}
