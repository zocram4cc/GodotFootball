// pes_survey.hpp: survey mode at the untranslated-dispatch site, GF_PES_SURVEY=<file>.
//
// Unset (the default) nothing here changes behaviour: a target the table lacks runs the caller's recovery
// (pes_dispatch_missing_handler), if any, then aborts (PES_DISPATCH_UNTRANSLATED).
// Set, every such hit is counted and the run goes on. The recovery runs first when one is installed: it is GF's own
// pre-cutover path for that call, and it may not return (the FixDemo rig longjmps; a GF cutover throws). When it returns,
// or none is installed, the call has no effect. The list is written at each new target and at exit, one line per target:
// `target hits first_frame caller_offset`. first_frame is the logic frame (pes_survey_set_frame) at the first hit;
// caller_offset is the return address into the calling function, relative to the executable's load bias: `nm -n` of the
// same executable names the function. The process exits with kPesSurveyExitCode, so a survey run never passes as a smoke.
// ponytail: a skipped call changes what runs after it, so later hits are a lower bound for that run: survey several seeds.
// ponytail: no locking; the sim's tick runs on one thread.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <link.h>
#include <map>

#include "pes_dispatch.hpp"

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

// The call a survey makes in place of an untranslated indirect target: nothing.
inline void NoOpCall(PesState&) {}

inline State& Get() {
  // Built once and never destroyed: the exit writer reads it during exit, after static destructors would have run.
  static State& s = []() -> State& {
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

// Counts one hit on `target`. `caller` is the return address into the function that reached the missing target.
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

// An indirect call's lookup for a target the table lacks. Unarmed: no entry, so the caller aborts as before. Armed: the
// hit is counted, the recovery runs, and the call becomes a no-op.
inline PesDispatch pes_survey_absent(uint64_t target, const void* caller) {
  if (!pes_survey_armed()) return {nullptr};
  pes_survey_note(target, caller);
  if (pes_dispatch_missing_handler) pes_dispatch_missing_handler(target);
  return {&pes_survey_detail::NoOpCall};
}
