#!/usr/bin/env python3
"""test_simslot.py: the sim-slot queue (tools/dev/simslot.sh) on a scratch lock with one or two slots.

  - waiters run in (priority, arrival) order: a priority-0 gate overtakes two earlier priority-5 agents;
  - no more commands run at once than GF_SIM_SLOTS, and the slots are all used;
  - no command starts while MemAvailable is below GF_SIM_MIN_AVAILABLE_MIB, and a killed waiter leaves no ticket;
  - a plain `flock` holder of slot 0 (an old script) is respected.

Run by ctest (simslot_queue_test): test_simslot.py <simslot.sh> <scratch dir>. Never takes the real gf-sim.lock."""
import fcntl
import os
import shutil
import subprocess
import sys
import time
import unittest

SIMSLOT = os.path.abspath(sys.argv[1])
SCRATCH = os.path.abspath(sys.argv[2])
sys.argv = sys.argv[:1]


class SimSlotQueue(unittest.TestCase):
    def setUp(self):
        self.dir = os.path.join(SCRATCH, "simslot", self._testMethodName)
        shutil.rmtree(self.dir, ignore_errors=True)
        os.makedirs(self.dir)
        self.lock = os.path.join(self.dir, "gf-sim.lock")
        self.log = os.path.join(self.dir, "order.log")

    def env(self, slots, priority, **extra):
        e = dict(os.environ, GF_SIM_LOCK=self.lock, GF_SIM_SLOTS=str(slots), GF_SIM_PRIORITY=str(priority))
        e.pop("GF_SIM_HOLD", None)
        e.pop("GF_SIM_PIN", None)
        e.update(extra)
        return e

    def run_slotted(self, command, env):
        return subprocess.Popen([SIMSLOT, *command], env=env)

    def queued(self):
        queue = self.lock + ".queue"
        return len(os.listdir(queue)) if os.path.isdir(queue) else 0

    def slot_held(self, path):
        fd = os.open(path, os.O_RDWR | os.O_CREAT)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return True
        finally:
            os.close(fd)
        return False

    def wait_for(self, condition, what, timeout=30):
        deadline = time.monotonic() + timeout
        while not condition():
            if time.monotonic() > deadline:
                self.fail("timed out waiting for: " + what)
            time.sleep(0.05)

    def test_priority_then_arrival(self):
        holder = self.run_slotted(["sleep", "1.5"], self.env(1, 5))
        self.wait_for(lambda: self.slot_held(self.lock), "the holder takes slot 0")
        waiters = []
        for name, priority in (("A", 5), ("B", 0), ("C", 5)):  # arrival order A, B, C
            waiters.append(self.run_slotted(["bash", "-c", f'echo {name} >> "{self.log}"'], self.env(1, priority)))
            self.wait_for(lambda n=len(waiters): self.queued() == n, f"{name} is queued")
        for p in [holder, *waiters]:
            self.assertEqual(p.wait(timeout=60), 0)
        self.assertEqual(open(self.log).read().split(), ["B", "A", "C"])

    def test_never_more_than_the_slot_count(self):
        procs = []
        for i in range(6):
            cmd = f'echo start{i} >> "{self.log}"; sleep 1.5; echo end{i} >> "{self.log}"'
            procs.append(self.run_slotted(["bash", "-c", cmd], self.env(2, 5)))
        for p in procs:
            self.assertEqual(p.wait(timeout=120), 0)
        events = open(self.log).read().split()
        self.assertEqual(len(events), 12)
        running = peak = 0
        for event in events:
            running += 1 if event.startswith("start") else -1
            peak = max(peak, running)
        self.assertEqual(peak, 2)

    def test_waits_while_memory_is_short_and_cleans_up(self):
        env = self.env(2, 5, GF_SIM_MIN_AVAILABLE_MIB=str(10**9))
        p = self.run_slotted(["bash", "-c", f'echo ran >> "{self.log}"'], env)
        self.wait_for(lambda: self.queued() == 1, "the waiter is queued")
        with self.assertRaises(subprocess.TimeoutExpired):
            p.wait(timeout=3)
        p.terminate()
        self.assertEqual(p.wait(timeout=30), 143)
        self.assertEqual(self.queued(), 0)
        self.assertFalse(os.path.exists(self.log))

    def test_respects_an_old_script_holding_slot_zero(self):
        started = time.time()
        holder = subprocess.Popen(["flock", self.lock, "sleep", "1.5"])  # the old scripts' form
        self.wait_for(lambda: self.slot_held(self.lock), "the old holder takes slot 0")
        p = self.run_slotted(["bash", "-c", f'date +%s%N >> "{self.log}"'], self.env(1, 5))
        self.assertEqual(p.wait(timeout=60), 0)
        self.assertEqual(holder.wait(timeout=30), 0)
        ran_at = int(open(self.log).read().split()[0]) / 1e9
        self.assertGreaterEqual(ran_at, started + 1.5 - 0.05)


if __name__ == "__main__":
    unittest.main(verbosity=2)
