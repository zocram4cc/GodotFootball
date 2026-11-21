// The engine-backed trap world the tests share (AnimeLayer7 cut). A trap world's Trap-path calls run on the PES memory of an engine (PesFoxWorld):
// its window moves into that memory (BindMemory), and each player's anime chain is the engine player's own BodyAnimControl (BindAnimeChain).
// Open boots the engine from PES_EXE (the unpacked image) and PES21_ROOT (the install), loads its body and builds the players before any
// window binds, as the match does. BindWorld gives a trap world the state the match gives it (the install's image tables, the member
// vtables' slots, and P's constructor stores for `slot` when asked). Test-only.
#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <optional>

#include "onthepitch/pesfoxworld.hpp"
#include "onthepitch/pestrapactor.hpp"
#include "onthepitch/pestrapworld.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace pes_engine {

constexpr uint64_t kHeapBytes = 1ull << 30;
// The player block the engine's constructor fills (matchevents.cpp kPesAnimePlayerBytes): its BodyAnimControl is at +0xA08.
constexpr uint64_t kPlayerBytes = 0x3C00;

class EngineTrap {
 public:
  // Boots the engine, loads its body and builds `players` engine players. Null (and `error` set) when the install or the image is not there.
  static std::unique_ptr<EngineTrap> Open(const char* exe, const char* root, int players, std::string* error) {
    auto t = std::unique_ptr<EngineTrap>(new EngineTrap());
    t->vfs_ = std::make_unique<pes::PesVfs>();
    if (!t->vfs_->Mount(root, error)) return nullptr;
    auto pe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", error);
    if (!pe) return nullptr;
    t->actor_ = pes::LoadActorConstants(*pe, error);
    if (!t->actor_) return nullptr;
    t->tables_ = pes::LoadPesImageTables(*pe, error);
    if (!t->tables_) return nullptr;
    t->engine_ = std::make_unique<pes::PesFoxWorld>(exe, *t->vfs_, kHeapBytes);
    t->engine_->Boot();
    if (!t->engine_->LoadBody(error)) return nullptr;
    for (int k = 0; k < players; ++k) {
      const uint64_t p = t->engine_->Alloc(kPlayerBytes);
      if (p == 0) {
        *error = "the engine heap is full";
        return nullptr;
      }
      t->engine_->ConstructAnime(p, 0);
      t->players_.push_back(p);
    }
    pes::PesMemory& memory = t->engine_->Memory();
    t->ext_.image = memory.Image();
    t->ext_.imageBase = pes::PesMemory::kImageBase;
    t->ext_.imageBytes = memory.ImageBytes();
    t->ext_.borrowed = memory.SharedWindows();
    return t;
  }

  pes::PesFoxWorld& Engine() { return *engine_; }
  // The engine player k (constructed before any window bound).
  uint64_t Player(int k) const { return players_.at(static_cast<std::size_t>(k)); }
  // A new engine player, constructed as the others are: each capture takes its own chain, so no call sees another capture's chain state.
  uint64_t NewPlayer() {
    const uint64_t p = engine_->Alloc(kPlayerBytes);
    if (p == 0) throw std::runtime_error("the engine heap is full");
    engine_->ConstructAnime(p, 0);
    return p;
  }
  // A player's BodyAnimControl: the chain the trap path binds (BindAnimeChain).
  uint64_t ChainOf(uint64_t p) { return engine_->Read64(p + pes::kPScalePtrOffset); }
  pes::PesTrapExternals& Externals() { return ext_; }
  const pes::PesActorConstants& Actor() const { return *actor_; }

  // Gives `w` the match's state: bound to the engine's memory (its window moves there), the install's image tables, and the member vtables'
  // slots. With `slot` >= 0, P's constructor stores too (ConstructActorP: the class vtable the driver calls through, the member adjusters).
  void BindWorld(pes::PesWorld& w, int slot) {
    w.BindMemory(engine_->Memory());
    w.BindImageTables(*tables_);
    for (int k = 0; k < 3; ++k)
      w.AddImageSegment(actor_->memberVtable[k], actor_->memberVtableSlots[k].data(), actor_->memberVtableSlots[k].size());
    if (slot >= 0) pes::ConstructActorP(w, slot, *actor_);
  }

 private:
  EngineTrap() = default;
  std::unique_ptr<pes::PesVfs> vfs_;
  std::unique_ptr<pes::PesFoxWorld> engine_;
  std::optional<pes::PesActorConstants> actor_;
  std::optional<pes::PesImageTables> tables_;
  std::vector<uint64_t> players_;
  pes::PesTrapExternals ext_;
};

}  // namespace pes_engine
