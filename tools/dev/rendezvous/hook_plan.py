#!/usr/bin/env python3
"""PES hook plan (static): map each target VA to its function (.pdata), find direct callers, run the
window check (rv_hooks.displaced_window), and scan the whole code section for branches into each hook
window. Reads pes_full.bin only (flat image, file offset = VA - base). No live process is touched.

usage: hook_plan.py IMAGE OUT_DIR
writes OUT_DIR/plan.json (no instruction bytes) and OUT_DIR/pes_hooks.cfg (expected bytes: scratch only).
"""
import bisect
import json
import os
import re
import struct
import sys

import capstone

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rv_hooks as rh  # noqa: E402

TEXT_RVA, TEXT_END_RVA = 0x1000, 0x1000 + 0x252D800   # .trace (code) section, from the PE table

# (name, VA, note). Targets given in the brief; the caller of the FixDemo evaluator is found below.
TARGETS = [
    ("cut_create_141F57840", 0x141F57840, "cutscene CREATE/LOAD (brief)"),
    ("cut_load_141F36200", 0x141F36200, "cutscene CREATE/LOAD (brief)"),
    ("cut_load_141F5F330", 0x141F5F330, "cutscene CREATE/LOAD (brief); ledger: slot-list build"),
    ("fixdemo_eval_141F4D2F0", 0x141F4D2F0, "FixDemo evaluator (brief; ledger: condition evaluator)"),
    ("fixdemo_match_141F4B1D0", 0x141F4B1D0, "FixDemo matcher (scene-birth doc, not in brief)"),
    ("restart_44C880", 0x14044C880, "restart builder (brief; situation-sequencer runner)"),
    ("restart_446390", 0x140446390, "restart builder (brief; fan-in)"),
    ("camera_update_87F280", 0x14087F280, "in-play camera update (brief; vtable slot 4)"),
    ("camera_caller_886780", 0x140886780, "caller of the camera update (brief)"),
    ("hud_radar_update_E2910", 0x1420E2910, "HUD radar update (hud-ui doc §5/§9)"),
    ("mi_refresh_113B280", 0x14113B280, "per-tick MI refresh candidate; brief VA 0x14113B27F is int3 padding before it"),
    ("mi_refresh_113B520", 0x14113B520, "per-tick MI refresh candidate; brief VA 0x14113B51F is int3 padding before it"),
    ("type13_body_A115E9", 0x140A115E9, "type-13 body: its function (brief, for G+0x180)"),
    # Foul/offside stoppage functions that need real-call seeds (docs/ports/pes-port-FoulOffside.md).
    ("foul_sanction_47F170", 0x14047F170, "sanction -> scene decision tree"),
    ("foul_sanction_caller_480910", 0x140480910, "sanction tree's caller"),
    ("ref_involve_A3A690", 0x140A3A690, "officials' involvement"),
    ("ref_judge_A3A930", 0x140A3A930, "referee JUDGE action"),
    ("ref_latch_A3A870", 0x140A3A870, "one-signal-per-stop latch"),
    ("ref_restart_cmd_A39A80", 0x140A39A80, "restart command"),
    ("linesman_lookat_A38F20", 0x140A38F20, "linesman look-at"),
    ("restart_driver_445E90", 0x140445E90, "restart step-programme driver"),
    # Kick scatter and press command: real-call cases (docs/ports/pes-port-KickInputs.md, docs/re/pes-re-press-live.md).
    # Not hookable, so reached through the scatter entry: the quality 0x1408413C0 (rip-relative operand in the window)
    # and the scatter body at 0x1408398D5 (a call in the window).
    ("kick_scatter_83CC30", 0x14083CC30, "kick scatter entry"),
    ("kick_postscale_8414A0", 0x1408414A0, "kick quality post-scales"),
    ("press_cmd_writer_A85A70", 0x140A85A70, "press command words writer (class, useClassRate, kmh)"),
    # The carrier's Clear unit and its corridor angle (docs/ports/pes-port-EndClear2.md).
    ("carrier_clear_2109EB0", 0x142109EB0, "carrier Clear unit (kind 1)"),
    ("clear_angle_2108630", 0x142108630, "clear corridor angle"),
    # The Trap action's kind-0 control-touch chain (docs/re/pes-re-trap-kind0.md, docs/ports/pes-port-RecordMap.md).
    ("trap_kind0_79A470", 0x14079A470, "Trap action kind-0 control touch"),
    ("trap_commit_79AFD0", 0x14079AFD0, "Trap clip commit step"),
    # Clear chain inputs and the cutscene camera's cut offset (docs/ports/pes-port-MatchInfo.md, -FdcCamera4.md).
    ("clear_helper_63A490", 0x14063A490, "Clear chain helper"),
    ("danger_writer_699960", 0x140699960, "carrier danger flag writer"),
    ("danger_builder_2119390", 0x142119390, "imageA builder that stores the danger byte"),
    ("cut_offset_1F39CC0", 0x141F39CC0, "cutscene player: cut offset and the played camera key"),
    # FixDemo selection inputs for the GF-side request (docs/ports/pes-port-CutsceneHybrid2.md, CutsceneHybrid3).
    ("fixdemo_wrapper_141F4FDA0", 0x141F4FDA0, "FixDemo variant wrapper"),
    # The ctx setter 0x141F51630 is not hookable (a je at +3 falls inside the patch window).
    ("fixdemo_ctx_init_141F514D0", 0x141F514D0, "FixDemo ctx init"),
    ("goal_pick_140477400", 0x140477400, "goal cutscene pool pick"),
    # The carrier's unit walk: the forced-clear decision is inline here (docs/ports/pes-port-MatchInfo3.md).
    ("carrier_walk_210F110", 0x14210F110, "carrier unit walk (forced clear)"),
    # The danger writer's input pressure record (MatchInfo4): built right before 0x140699960 runs.
    ("pressure_build_69A500", 0x14069A500, "pressure record builder feeding the danger writer"),
    # FixDemo scene start (CutsceneCam): reads scene+0x110..0x120 and the step kind; goal/foul cut offsets.
    ("fixdemo_scene_start_141F5CD00", 0x141F5CD00, "FixDemo scene start (cut offset per scene)"),
    # Goal cutscene anchor (CutsceneCam): scorer position clamped to the pitch, then written to scene+0x110..0x120.
    ("cut_anchor_calc_474F00", 0x140474F00, "goal scene anchor from the scorer's position"),
    ("cut_anchor_write_47D280", 0x14047D280, "goal scene anchor writer (scene+0x110..0x120)"),
    # The foul judge's level decision (docs/re/pes-re-duels-fouls.md 2.2): contact -> points -> level -> pending foul (CardLevel).
    ("judge_decide_5B7470", 0x1405B7470, "judge level decision: points, thresholds, overrides, builds the pending foul"),
    ("judge_whistle_50E770", 0x14050E770, "whistle writer: JR restart/side/reason/spot/kind/fouler/victim"),
    ("judge_commit_43F546F0", 0x143F546F0, "Judge::Commit (protected): JR+0x38 card level"),
    # The Trap action's own tick (S5 of docs/re/pes-re-trap-inputs.md; TrapCutover2): plays the committed clip.
    ("trap_action_tick_797CD0", 0x140797CD0, "Trap action tick (plays the clip the kind-0 chain committed)"),
    # The kick kind chooser at Kick action start (KickKind): rcx = descriptor out, rdx = AnimePlayer, r8 = request.
    ("kick_chooser_8364D0", 0x1408364D0, "kick kind/side/band chooser at Kick action start"),
    # The actor's action switch (TrapTick2): the trap end handler 0x14079E5C0 calls it; it runs PES's per-action objects.
    ("action_switch_6EEBB0", 0x1406EEBB0, "actor action switch (P+0xAC8 action id; per-action objects)"),
    ("trap_end_other_79D4B0", 0x14079D4B0, "trap end handler's other branch"),
    # Goal cutscene kind-3 recompute (CutAnchor6): per-tick DemoInfo recompute and the A0+0xBC writer.
    ("recompute_471A80", 0x140471A80, "goal DemoInfo recompute (A0+0x30 for kind-3 rows)"),
    ("bcwriter_4B88B0", 0x1404B88B0, "A0+0xBC writer"),
    # Cutscene timing (CutsceneTiming): step end, step machine and the DemoInfo update (start delay, scene end).
    ("fixdemo_step_end_141F572F0", 0x141F572F0, "FixDemo step end (player state finished)"),
    ("fixdemo_step_machine_141F59380", 0x141F59380, "FixDemo step machine (frame after each step start)"),
    ("demoinfo_update_47C830", 0x14047C830, "DemoInfo update (go flag, first manager update, scene end)"),
    # Trap end handler (diagnostic): does the live game reach it at all? (no action_switch/trap_end_other capture yet)
    ("trap_end_handler_79E5C0", 0x14079E5C0, "Trap action end handler"),
    # SetMotion's anime interface slot +0x80 (TrapTick6): reached when FUN_1406AFBC0 returns 0 for the motion.
    ("anime_iface_80_6E72A0", 0x1406E72A0, "anime interface vtable slot +0x80 (SetMotion branch)"),
    # The FixDemo procedural camera engine (CutsceneTiming7): drives the cameras of records with no anime path.
    ("proc_camera_141F7BFE0", 0x141F7BFE0, "FixDemo procedural camera engine"),
    # SetMotion-adjacent displacement (TrapTick7): the trap tick phase fn calls it when action+0x26 == 0.
    ("anime_displace_6E85B0", 0x1406E85B0, "anime position displacement (trap tick phase)"),
    # Untranslated callee in the trap tick (TrapTick8): unbounded replays of trap_action_tick abort here.
    ("trap_tick_callee_84F140", 0x14084F140, "trap tick callee"),
    # --- PortMapMotion block (motion-side map; docs/re/pes-re-portmap-motion.md). Append-only.
    ("mot_kick_req_7324B0", 0x1407324B0, "PortMapMotion: kick request builder (heading/spot/bucket)"),
    ("mot_kick_geom_829E80", 0x140829E80, "PortMapMotion: kick geometry candidacy tests"),
    ("mot_kick_shot_8371A0", 0x1408371A0, "PortMapMotion: shot request builder (req 10)"),
    ("mot_kick_ground_837670", 0x140837670, "PortMapMotion: ground-pass request builder (req 9)"),
    ("mot_kick_high_8360F0", 0x1408360F0, "PortMapMotion: high-ball request builder (req 8)"),
    ("mot_kick_first_837FC0", 0x140837FC0, "PortMapMotion: first-time byte ap+0x2756"),
    ("mot_contact_head_876020", 0x140876020, "PortMapMotion: contact-chain head into the 876360 tick"),
    ("mot_judge_upd_7A14E0", 0x1407A14E0, "PortMapMotion: Judge::Update (referee movement)"),
    ("mot_linesman_upd_7A2920", 0x1407A2920, "PortMapMotion: LinesmanAction::Update"),
    ("mot_linesman_mov_7A1F30", 0x1407A1F30, "PortMapMotion: LinesmanMove::Update"),
    ("mot_ability_6E6660", 0x1406E6660, "PortMapMotion: ability byte (locomotion entries + kick stat)"),
    ("mot_entry_aim_BA7D0", 0x1406BA7D0, "PortMapMotion: entry relax/aim flag"),
    ("mot_cliplen_6B10D0", 0x1406B10D0, "PortMapMotion: clip length ticks (start-time formula)"),
    ("mot_ticklen_6B11E0", 0x1406B11E0, "PortMapMotion: tick length (start-time pair)"),
    ("mot_blendrec_AF620", 0x1406AF620, "PortMapMotion: blend/fixed record reader"),
    ("mot_cancelrec_AF8E0", 0x1406AF8E0, "PortMapMotion: cancel-frame record reader"),
    ("mot_rotend_6B1D00", 0x1406B1D00, "PortMapMotion: RotData end reader"),
    ("mot_rotoff_6B1F00", 0x1406B1F00, "PortMapMotion: RotData signed-offset reader"),
    ("mot_setkick_place_732D40", 0x140732D40, "PortMapMotion: SetKick placer (restart placement)"),
    ("mot_corner_body_732E30", 0x140732E30, "PortMapMotion: CornerKick::Update phases"),
    ("mot_tackle_reach_78A640", 0x14078A640, "PortMapMotion: tackle reach test"),
    ("mot_clip_solver_6ECBB0", 0x1406ECBB0, "PortMapMotion: clip-solver physics prediction"),
    ("mot_tracked_842BE0", 0x140842BE0, "PortMapMotion: per-part contact distance"),
    ("mot_tracked_max_843C50", 0x140843C50, "PortMapMotion: max over tracked parts (feet gate shares fn)"),
    # --- PortMapAI block (decision-side map; docs/re/pes-re-portmap-ai.md). Append-only.
    ("ai_press_writer_536FC0", 0x140536FC0, "PortMapAI: press writer (replaces PesLooseWriterTick stand-in)"),
    ("ai_press_eval_633940", 0x140633940, "PortMapAI: press target eval (replaces PesPressTick pass-2)"),
    ("ai_press_coord_634470", 0x140634470, "PortMapAI: press coordinator (replaces PesSecondPresserTick)"),
    ("ai_team_tick_4F6C30", 0x1404F6C30, "PortMapAI: team tick root (replaces Team::Process PES path)"),
    ("ai_concept_sel_579840", 0x140579840, "PortMapAI: concept selector (replaces RunConcepts)"),
    ("ai_lane_line_681FF0", 0x140681FF0, "PortMapAI: lane line post-step (replaces UpdateLaneLine)"),
    ("ai_forward_pull_682770", 0x140682770, "PortMapAI: forward pull (replaces Block A stand-in)"),
    ("ai_chance_606060", 0x140606060, "PortMapAI: ChanceSpaceRun (replaces ProcessRunManager chance)"),
    ("ai_secondline_60D210", 0x14060D210, "PortMapAI: SecondLine runner (replaces ProcessRunManager)"),
    ("ai_postplay_61F940", 0x14061F940, "PortMapAI: PostPlay executor (replaces ProcessRunManager)"),
    ("ai_cross_unit_2129590", 0x142129590, "PortMapAI: Cross unit (replaces CrossUnit stand-in)"),
    ("ai_receive_9F8220", 0x1409F8220, "PortMapAI: receive entry 3 (replaces _ReactionCommands receive)"),
    # --- AIConcepts block (concept slot functions no capture has entered; docs/ports/pes-port-AIConcepts.md). Append-only.
    ("aic_forecheck_start_5747F0", 0x1405747F0, "AIConcepts: ForeCheckPress start"),
    ("aic_forecheck_end_574710", 0x140574710, "AIConcepts: ForeCheckPress end"),
    ("aic_transchase_start_575510", 0x140575510, "AIConcepts: TransitionChase start"),
    ("aic_transchase_exec_575DA0", 0x140575DA0, "AIConcepts: TransitionChase execute"),
    ("aic_retreatblock_start_575EA0", 0x140575EA0, "AIConcepts: RetreatBlockCreate start"),
    ("aic_sidecounter_end_5404A0", 0x1405404A0, "AIConcepts: SideCounter end"),
    ("aic_sidecounter_exec_5415B0", 0x1405415B0, "AIConcepts: SideCounter execute"),
    ("aic_transchase_exec_575B60", 0x140575B60, "AIConcepts: TransitionChase execute (owner of the 0x140575DA0 thunk)"),
    # --- PressAI block (how PES starts a loose-ball press; docs/ports/pes-port-PressAI.md Left 1, 3). Append-only.
    ("press_order_A42940", 0x140A42940, "PressAI: order-word writer (r8d == 0x34 starts a press)"),
    ("press_reset_536690", 0x140536690, "PressAI: dispatcher slot reset"),
    ("press_helper_62AF10", 0x14062AF10, "PressAI: order-word helper"),
    # --- KickCutover block (the chooser's geometry words come from this builder; docs/ports/pes-port-KickCutover.md). Append-only.
    ("kick_header_833CD0", 0x140833CD0, "KickCutover: kick request header builder (fills the order-record geometry)"),
    # --- PressAI3 block (PES starts a press via MATCH_UP writer -> coordinator -> eval; its proof is synthetic only). Append-only.
    ("press_matchup_536210", 0x140536210, "PressAI3: MATCH_UP writer (real calls for the synthetic-only proof)"),
    # --- KickBranch block (real calls behind synthetic-only proofs; docs/ports/pes-port-KickBranch.md section 7). Append-only.
    ("kick_root_141AFDF10", 0x141AFDF10, "KickBranch: root-motion reader (turning clips reach 0x1423BAEB0)"),
    ("kick_widepass_140A510F0", 0x140A510F0, "KickBranch: request builder's wide-pass lane classifier"),
    ("kick_shotgoal_140A641D0", 0x140A641D0, "KickBranch: chooser shot path inside the goal-area window"),
    # --- RestingBall2 block (trap end transition and the action-4 virtual it reaches; pes-port-RestingBall2.md). Append-only.
    ("trap_endtrans_6B4580", 0x1406B4580, "RestingBall2: trap end transition's action dispatch (call from 0x14079EE70)"),
    ("trap_act4_7BA840", 0x1407BA840, "RestingBall2: action-4 virtual reached at the trap end (untranslated)"),
    # --- KickTick block (the Kick action's per-tick driver; pes-port-KickTick.md). Append-only.
    ("kick_try_140724CC0", 0x140724CC0, "KickTick: Kick action start (try)"),
    ("kick_gate_1407266B0", 0x1407266B0, "KickTick: Kick can-enter gate (rdx = P)"),
    ("kick_commit_1406F9B70", 0x1406F9B70, "KickTick: command commit (pending cmd, order record at +0x5EC)"),
    ("kick_tick_140719F20", 0x140719F20, "KickTick: Kick action tick switch"),
    ("kick_ai_exec_1409316E0", 0x1409316E0, "KickTick: AI executor"),
    # --- RestingBall3 block (the action-4 start virtual and its callees on the PES-wait trap path). Append-only.
    ("act4_start_7A38E0", 0x1407A38E0, "RestingBall3: action-4 start virtual (vt 0x1425E2440+0x58)"),
    ("act4_callee_7A4D30", 0x1407A4D30, "RestingBall3: action-4 start callee"),
    ("act4_callee_7B68B0", 0x1407B68B0, "RestingBall3: action-4 start callee"),
    ("act4_callee_7B6D80", 0x1407B6D80, "RestingBall3: action-4 start callee"),
    ("traj_caller_8495B0", 0x1408495B0, "RestingBall3: caller of the .pdata-less trajectory leaf 0x140852C00"),
    # --- TeamTick block (PES's team-tick pass-target rewrite; pes-port-TeamTick.md). Append-only.
    ("team_pass_4FB2A0", 0x1404FB2A0, "TeamTick: team tick pass-target rewrite (rcx = brain Y)"),
    ("team_pass_4FB380", 0x1404FB380, "TeamTick: pass-target decision tree"),
    ("team_pass_F0E50", 0x1404F0E50, "TeamTick: holder tree the decision ends in"),
    ("team_pass_4FB780", 0x1404FB780, "TeamTick: MI+0x116C==2 stoppage arm"),
    # --- TeamTick2 block (keeper GKDribble carrier unit the pass-target cutover reaches). Append-only.
    ("tt_142132E20", 0x142132E20, "TeamTick2: keeper GKDribble carrier unit kind 0x13"),
    # --- KickInputs block (the Kick window's missing inputs). Append-only.
    ("ball_nearest_45B090", 0x14045B090, "KickInputs: writer of the MA+0x2470C ball list (0x140A79640 sort inside)"),
    ("kick_cell_71E570", 0x14071E570, "KickInputs: Animetable cell lookup; from 0x140724F21 = a successful Kick start"),
    # --- KickJoints block (the player's anime joint block the Kick gate reads). Append-only.
    ("kick_joints_1406F6CE0", 0x1406F6CE0, "KickJoints: player per-tick update virtual (fills P+0xB1C, P+0xB38..0xBA0)"),
    ("anime_bones_141EB49B0", 0x141EB49B0, "KickJoints: anime joint writer (rcx = P)"),
    # --- TeamTick3 block (frames-since-event and the event-ring reader). Append-only.
    ("tt_since_140A51320", 0x140A51320, "TeamTick3: frames since event"),
    ("tt_ringval_140A3E840", 0x140A3E840, "TeamTick3: event-ring record reader"),
    # --- RestingBall5 block (contact-search leaves the failing tests' matches reach). Append-only.
    ("rb5_140862CE0", 0x140862CE0, "RestingBall5: contact-search leaf (throw-in + restart tests)"),
    ("rb5_140862AE0", 0x140862AE0, "RestingBall5: contact-search leaf (goal-scene test)"),
    ("rb5_140861170", 0x140861170, "RestingBall5: contact-search callee, static-reachable"),
    # --- KickJoints2 block (the Kick start's success tail). Append-only.
    ("kick_dd80_14071DD80", 0x14071DD80, "KickJoints2: success-only call after the Kick start's cell (2 callers)"),
    # --- TeamTick4 block (the arrival-list writer behind the pass target's default candidate). Append-only.
    ("ai_arrival_14045E0D0", 0x14045E0D0, "TeamTick4: writer of MA+0x24840/0x24908 arrival lists"),
    ("ti_unit_14211A340", 0x14211A340, "TeamTick4: carrier-unit vtable function the pass-target cutover reaches"),
    # --- AnimeLayer block (PES's fox AnimControl engine phases and the SetMotion layer pusher). Append-only.
    ("anime_phase3_1423A1AC0", 0x1423A1AC0, "AnimeLayer: phase 3 time advance + clip blend (writes L1+0x50)"),
    ("anime_stage_1423A2260", 0x1423A2260, "AnimeLayer: stage plugins (rdx 1 = IK/modify, 2 = bone-space convert)"),
    ("anime_phase4_1423A1900", 0x1423A1900, "AnimeLayer: phase 4 stage-0 sync plugins"),
    ("anime_pusher_141AF61C0", 0x141AF61C0, "AnimeLayer: SetMotion layer-record pusher (rcx = BodyAnimControl)"),
    # --- MoveStepCell block (the MOVESTEP request path and the Animetable cell lookup). Append-only.
    ("stop_entry_1406D5D60", 0x1406D5D60, "MoveStepCell: MOVESTEP STOP entry"),
    ("turnstop_sel_1406BA0D0", 0x1406BA0D0, "MoveStepCell: TurnStop selector (anime iface vt+0xE0)"),
    ("abb30_any_1406ABB30", 0x1406ABB30, "MoveStepCell: Animetable cell lookup (~80 callers; empty-cell answers)"),
    # --- TeamTick5 block (the carrier manager's unit 5). Append-only.
    ("ti_mgr_14211E2D0", 0x14211E2D0, "TeamTick5: carrier manager running the 9 sub-units"),
    ("ti_enter5_14211AB10", 0x14211AB10, "TeamTick5: carrier unit 5 can-enter"),
    ("ti_mgrtick_14211D710", 0x14211D710, "TeamTick5: carrier manager tick"),
    ("ti_enter4_14211B7B0", 0x14211B7B0, "TeamTick5: carrier unit 4 can-enter (runs just before unit 5)"),
    ("ti_enter3_14211AF20", 0x14211AF20, "TeamTick5: carrier unit 3 can-enter"),
]


def load_pdata(img):
    exc_rva, exc_size = img.exc
    funcs = []
    for i in range(exc_size // 12):
        b, e, _ = struct.unpack_from("<III", img.data, exc_rva + 12 * i)
        if b:
            funcs.append((b, e))
    funcs.sort()
    return funcs


def function_of(funcs, starts, rva):
    i = bisect.bisect_right(starts, rva) - 1
    if i >= 0 and funcs[i][0] <= rva < funcs[i][1]:
        return funcs[i]
    return None


def callers_of(img, blob, funcs, starts, fn_rva):
    """Direct E8 calls to fn_rva whose instruction is confirmed by a linear decode of the caller."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    out = {}
    for m in re.finditer(rb"\xe8", blob):
        i = m.start()
        if i + 5 > len(blob):
            continue
        rel = struct.unpack_from("<i", blob, i + 1)[0]
        rva = TEXT_RVA + i
        if rva + 5 + rel != fn_rva:
            continue
        fn = function_of(funcs, starts, rva)
        if fn is None:
            out.setdefault("unmapped", []).append(hex(img.base + rva))
            continue
        b, e = fn
        code = blob[b - TEXT_RVA:e - TEXT_RVA]
        confirmed = any(ins.address == img.base + rva and ins.mnemonic == "call"
                        for ins in md.disasm(code, img.base + b))
        if confirmed:
            out.setdefault(hex(img.base + b), []).append(hex(img.base + rva))
    return out


def branch_candidates(blob):
    """Raw-byte branch candidates in the code section: {target RVA: [source RVA, ...]} for rel32 call/jmp/jcc
    and rel8 jmp/jcc. Conservative: a data byte can make a candidate; an instruction cannot be missed."""
    cand = {}

    def add(src, tgt):
        cand.setdefault(tgt, []).append(src)

    for m in re.finditer(rb"[\xe8\xe9]", blob):
        i = m.start()
        if i + 5 <= len(blob):
            add(TEXT_RVA + i, TEXT_RVA + i + 5 + struct.unpack_from("<i", blob, i + 1)[0])
    for m in re.finditer(rb"\x0f[\x80-\x8f]", blob):
        i = m.start()
        if i + 6 <= len(blob):
            add(TEXT_RVA + i, TEXT_RVA + i + 6 + struct.unpack_from("<i", blob, i + 2)[0])
    for m in re.finditer(rb"[\xeb\x70-\x7f]", blob):
        i = m.start()
        if i + 2 <= len(blob):
            add(TEXT_RVA + i, TEXT_RVA + i + 2 + struct.unpack_from("<b", blob, i + 1)[0])
    return cand


def real_branch_into(img, blob, funcs, starts, src_rva, tgt_rva):
    """True when a linear decode of the function holding src_rva has a direct jump/call at src_rva to tgt_rva."""
    fn = function_of(funcs, starts, src_rva)
    if fn is None:
        return False
    b, e = fn
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    for ins in md.disasm(blob[b - TEXT_RVA:e - TEXT_RVA], img.base + b):
        if ins.address == img.base + src_rva:
            if not (ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_CALL)):
                return False
            imms = [o.imm for o in ins.operands if o.type == capstone.x86.X86_OP_IMM]
            return bool(imms) and imms[0] == img.base + tgt_rva
    return False


def main(argv):
    img_path, out_dir = argv[1], argv[2]
    os.makedirs(out_dir, exist_ok=True)
    img = rh.Image(img_path, flat=True)
    funcs = load_pdata(img)
    starts = [b for b, _ in funcs]
    blob = bytes(img.data[TEXT_RVA:TEXT_END_RVA])
    print("pdata functions: %d; code bytes scanned: %d" % (len(funcs), len(blob)))

    hooks, rows, cfg = [], [], ["# PES hook plan (scratch; expected bytes are PES code: never commit)"]
    seen = set()

    def add(name, va, note, entry_rva):
        if entry_rva in seen:
            return
        seen.add(entry_rva)
        entry = img.base + entry_rva
        code = img.read(entry, 64)
        length, why = rh.displaced_window(code, entry)
        row = {"name": name, "target": hex(va), "entry": hex(entry), "note": note,
               "is_entry": entry == va, "window": length, "refused": why}
        if length:
            row["insns"] = [i.mnemonic for i in capstone.Cs(capstone.CS_ARCH_X86,
                                                              capstone.CS_MODE_64).disasm(code[:length], entry)]
            hooks.append((name, entry, length, code[:length]))
        rows.append(row)

    for name, va, note in TARGETS:
        rva = va - img.base
        fn = function_of(funcs, starts, rva)
        if fn is None:
            rows.append({"name": name, "target": hex(va), "refused": "not inside any .pdata function"})
            continue
        add(name, va, note, fn[0])

    # Callers of the FixDemo evaluator (direct E8 calls, confirmed by decoding the caller).
    callers = callers_of(img, blob, funcs, starts, 0x141F4D2F0 - img.base)
    rows.append({"name": "fixdemo_eval_callers", "callers": callers})
    for caller_fn in sorted(k for k in callers if k.startswith("0x")):
        add("fixdemo_caller_" + caller_fn[2:], int(caller_fn, 16), "direct caller of the FixDemo evaluator",
            int(caller_fn, 16) - img.base)
    rows.append({"name": "camera_update_callers", "callers": callers_of(
        img, blob, funcs, starts, 0x14087F280 - img.base)})

    # Branches into each passing hook window: raw-byte candidates, each confirmed (or dismissed) by a linear
    # decode of the function that holds its source byte. A real branch into a window makes the hook unsafe.
    cand = branch_candidates(blob)
    for name, entry, length, _ in hooks:
        lo, hi = entry - img.base + 1, entry - img.base + length
        raw, real = [], []
        for tgt in sorted(t for t in cand if lo <= t < hi):
            for src in cand[tgt]:
                raw.append("%s->%s" % (hex(img.base + src), hex(img.base + tgt)))
                if real_branch_into(img, blob, funcs, starts, src, tgt):
                    real.append("%s->%s" % (hex(img.base + src), hex(img.base + tgt)))
        for row in rows:
            if row.get("entry") == hex(entry):
                row["raw_candidates_n"] = len(raw)
                row["raw_candidates"] = raw[:8]
                row["real_branches_into_window"] = real
    with open(os.path.join(out_dir, "plan.json"), "w") as f:
        json.dump(rows, f, indent=1, default=str)
    for name, entry, length, code in hooks:
        cfg.append("hook %s %#x %d 0 0 %s" % (name, entry, length, code.hex()))
    with open(os.path.join(out_dir, "pes_hooks.cfg"), "w") as f:
        f.write("\n".join(cfg) + "\n")
    for r in rows:
        if "entry" in r or "refused" in r:
            status = "PASS" if r.get("window") else "REFUSE"
            print("%-6s %-26s target=%s entry=%s %s %s" % (
                status, r["name"], r.get("target", "-"), r.get("entry", "-"),
                ("window=%d" % r["window"]) if r.get("window") else "",
                r.get("refused") or ""))
        elif "callers" in r:
            print("CALLERS %s: %s" % (r["name"], json.dumps(r["callers"])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
