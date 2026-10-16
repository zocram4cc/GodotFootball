#include <gtest/gtest.h>

#include <cstring>
#include <new>

#include "gamedefines.hpp"

// Every command the controllers queue is copied into an Anim (Humanoid::SelectAnim), and the
// copy reads every flag. A command built in storage that still holds an old frame's bytes must
// still start with its flags clear: valgrind and UBSAN flagged a stale byte here as an invalid
// bool, so the flag this test reads is the one the constructor has to set.
TEST(SimCommand, FlagsAreClearWhenBuiltOverStaleBytes) {
  constexpr unsigned char kStaleByte = 0xBE;
  alignas(PlayerCommand) unsigned char storage[sizeof(PlayerCommand)];
  // volatile: a plain memset here is dead to the compiler once a new object is placed over it.
  volatile unsigned char* stale = storage;
  for (size_t i = 0; i < sizeof storage; ++i) stale[i] = kStaleByte;
  PlayerCommand* command = new (storage) PlayerCommand();
  const unsigned char useTouchInfo = *reinterpret_cast<const unsigned char*>(&command->useTouchInfo);
  EXPECT_EQ(useTouchInfo, 0);
  command->~PlayerCommand();
}
