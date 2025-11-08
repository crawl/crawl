# DCSS Autocombat Integration Plan

**Source Analysis:** Dungeon Crawl Stone Soup (DCSS) autocombat implementation
**Target Project:** Your roguelike (C++ ECS with Command pattern)
**Date:** 2025-11-08

---

## Executive Summary

This document provides a complete analysis of DCSS's autocombat (Tab key) feature and a detailed implementation plan adapted to your project's architecture (C++ ECS with Command pattern, JSON configuration, existing turn system).

**What Autocombat Does:**
- Press Tab → automatically move toward and attack nearest enemy
- Stops when: enemy dead, you take damage, multiple enemies in range, HP too low, terrain/decisions needed

**Key Implementation Needs:**
1. ✅ **Already Have:** Turn system, command pattern, ranged/melee combat, spell targeting
2. ❌ **Need to Build:** Pathfinding (A*/Dijkstra), nearest enemy detection, interrupt system, AutofightCommand

---

## Table of Contents

1. [DCSS Implementation Analysis](#dcss-implementation-analysis)
2. [Architecture Mapping](#architecture-mapping)
3. [Implementation Plan](#implementation-plan)
4. [Detailed Component Design](#detailed-component-design)
5. [Integration Steps](#integration-steps)
6. [Testing Plan](#testing-plan)

---

## DCSS Implementation Analysis

### Core Files & Architecture

DCSS implements autocombat as a **Lua-scripted feature** with C++ bindings:

| Component | DCSS Files | Role |
|-----------|------------|------|
| **Main Logic** | `dat/clua/autofight.lua` (532 lines) | Target selection, pathfinding, decision logic |
| **C++ Handler** | `main.cc:1886` (`_handle_autofight`) | Command routing to Lua |
| **Lua Bindings** | `l-crawl.cc`, `l-moninf.cc` | Monster queries, command execution |
| **Command Enum** | `command-type.h` | `CMD_AUTOFIGHT`, `CMD_AUTOFIGHT_NOMOVE`, `CMD_AUTOFIRE` |
| **Key Binding** | `cmd-keys.h:362` | `{CONTROL('I'), CMD_AUTOFIGHT}` (Tab key) |
| **Interrupt System** | `state.cc:204` | Cancels on HP loss, monster attacks, etc. |

### Enemy Detection Algorithm

**Function:** `get_target(no_move)` in `autofight.lua:322`

**Process:**
```
1. Scan all cells in LOS (line-of-sight) radius
2. For each cell (x, y):
   - Query monster at position via C++ binding
   - Filter candidates:
     ✗ Butterflies, orbs of destruction (excluded by name)
     ✗ "Firewood" monsters (harmless, except ballistomycetes)
     ✗ Damage-immune (unless warding + can move)
     ✓ Hostile attitude (ATT_HOSTILE)
     ✓ Neutral + frenzied
3. Build MonsterInfo for each valid target:
   - distance (negative Chebyshev: -max(|dx|, |dy|))
   - attack_type (MELEE, REACHING, RANGED, MOVES, FAILS)
   - can_attack, safe, good_stab, constricting_you, poor_stab, injury, threat
4. Sort via lexicographic comparison on priority flags
5. Return best target
```

**Priority Order (configurable via `autofight_flag_order`):**
```lua
{"bullseye_target", "can_attack", "safe", "good_stab",
 "distance", "constricting_you", "poor_stab", "injury",
 "threat", "orc_priest_wizard"}
```

**Distance Metric:** Chebyshev distance = `max(|dx|, |dy|)` (stored as negative for sorting)

### Attack Type Selection

**Attack Types:**
```lua
AF_FAILS = -1     -- Cannot reach target (blocked path)
AF_MOVES = 0      -- Need to move toward target
AF_REACHING = 1   -- In polearm/reach weapon range
AF_MELEE = 2      -- Adjacent (melee range)
AF_FIRE = 3       -- Ranged attack available
```

**Selection Logic (`get_monster_info`, autofight.lua:224):**
```
If wielding ranged weapon (launcher):
  → AF_FIRE if can see target without transparency
  → Else AF_MOVES

Elif have reaching weapon (polearm, reach_range > 1):
  distance = max(|dx|, |dy|)
  If distance > reach_range: → AF_MOVES
  Elif distance < 2: → AF_MELEE (adjacent)
  Else: → AF_REACHING if can_reach(dx, dy), else AF_MOVES

Else (melee only):
  → AF_MELEE if distance < 2
  → AF_MOVES otherwise

If attack_type == AF_MOVES and have_quiver_action():
  → Upgrade to AF_FIRE (quiver items like throwing knives)
```

### Pathfinding Algorithm

**Function:** `choose_move_towards(ax, ay, bx, by, square_func)` in `autofight.lua:131`

**Algorithm:** Greedy best-first (NOT A*, NO Dijkstra)
```
Input:
  - Current position (ax, ay)
  - Target position (bx, by)
  - Validation function (can_move_now or can_move_maybe)

Output: (mx, my) delta for next step, or nil if blocked

Steps:
1. Calculate deltas: dx = bx - ax, dy = by - ay

2. Try moves in priority order based on dominant axis:

   If |dx| > |dy|: (horizontal dominant)
     Priority: diagonal, horizontal, horizontal±perpendicular, vertical

   Elif |dx| == |dy|: (perfect diagonal)
     Priority: diagonal, horizontal, vertical

   Else: (vertical dominant, symmetric to horizontal)
     Priority: diagonal, vertical, vertical±perpendicular, horizontal

3. For each candidate move (mx, my), validate:
   ✓ Not (0, 0) (no move)
   ✓ Not outside LOS radius
   ✓ Target still visible from new position (view.cell_see_cell)
   ✓ Passes square_func validation (safe terrain, no blocking monster)

4. Return first valid move, or nil
```

**Pathability Check:** `will_tab(ax, ay, bx, by)` (autofight.lua:205)
```
Recursive check if path exists:
1. If in reach range: return true
2. Find next move via choose_move_towards(..., can_move_maybe)
3. If no move: return false
4. Recurse from new position
```

**Movement Validation:**
- `can_move_now(dx, dy)`: Safe terrain AND (no monster OR can push past)
- `can_move_maybe(dx, dy)`: Not unseen, safe terrain, not firewood monster

**Limitations:**
- ❌ No optimal path (greedy, not A*)
- ❌ Can get stuck on corners
- ❌ No backtracking
- ❌ Doesn't avoid dangerous terrain en route

### Stop Conditions

**Pre-Execution Checks** (`autofight_check_preconditions`, autofight.lua:414):
```lua
1. HP too low: 100*hp <= AUTOFIGHT_STOP*mhp (default: 50%)
   → "You are too injured to fight recklessly!"

2. Player confused: you.confused()
   → "You are too confused!"

3. Player caught (webbed/trapped) and AUTOFIGHT_CAUGHT=false
   → "You are caught/webbed!"
```

**Runtime Stops:**
```lua
1. No valid target found (info == nil)
   → "No target in view!" or "No reachable target in view!"

2. Target unreachable (info.attack_type == AF_FAILS)
   → "No reachable target in view!"

3. Movement needed but allow_movement=false
   → "No target in range!" or wait

4. Movement needed but weapon check fails
   → Prompts user, may cancel
```

**Interrupt System** (`state.cc:204`, `interrupt_cmd_repeat`):
```cpp
Cancels autofight on:
- hp_loss: Player takes damage
- monster_attacks: Being attacked
- mimic: Discovering a mimic
- teleport: Being teleported
- hit_monster: Hitting a monster (configurable)
- see_monster: New monster spotted (configurable)
```

**Recklessness Warning** (`main.cc:1855`, `_check_recklessness`):
```
If autofight repeated within autofight_warning milliseconds:
  → "You should not fight recklessly!" (danger message)
```

### Turn System Integration

**Execution Flow:**
```
1. User presses Tab → CMD_AUTOFIGHT
   ↓
2. main.cc:2120 → _handle_autofight(CMD_AUTOFIGHT, prev_cmd)
   ↓
3. Check recklessness (autofight_warning time threshold)
   ↓
4. Map command to Lua function: "hit_closest"
   ↓
5. Call clua.callfn("hit_closest", 0, 0)
   ↓
6. Lua executes autofight.lua:465 (hit_closest)
   ↓
7. Lua calls attack(allow_movement=true, check_caught=true)
   ↓
8. Lua gets target via get_target(no_move=false)
   ↓
9. Lua executes action based on attack_type:
   - AF_FIRE: crawl.do_targeted_command("CMD_FIRE", x, y, stop)
   - AF_MELEE: crawl.do_commands({"CMD_MOVE_[direction]"})
   - AF_REACHING: crawl.do_targeted_command("CMD_PRIMARY_ATTACK", x, y, true)
   - AF_MOVES: move_towards(x, y) → crawl.do_commands({"CMD_MOVE_..."})
   ↓
10. C++ executes command (movement, attack, fire)
    ↓
11. C++ sets you.turn_is_over = true (if action took time)
    ↓
12. Return to game loop
    ↓
13. If turn_is_over: advance turn counter, run monster AI
```

**Key Flag:** `you.turn_is_over`
- Set to `false` at start of player turn
- Set to `true` by time-consuming actions (move, attack, wait)
- Set to `false` by zero-time actions (menu, look, etc.)
- Checked by game loop to determine if monsters act

---

## Architecture Mapping

### DCSS → Your Project

| DCSS Component | DCSS Implementation | Your Project Equivalent |
|----------------|---------------------|-------------------------|
| **Autofight Logic** | Lua script (autofight.lua) | C++ `AutofightCommand` class |
| **Command Routing** | `_handle_autofight()` → `clua.callfn()` | `EventHandler` → `std::unique_ptr<AutofightCommand>` |
| **Monster Queries** | Lua binding `monster.get_monster_at(x, y)` | `EntityManager::GetEntitiesInRange()` + filtering |
| **Player Queries** | Lua binding `you.hp()`, `you.confused()` | Direct access to `Player` entity components |
| **Turn Flag** | `you.turn_is_over` | `Command::ConsumesTurn()` return value |
| **Command Execution** | `crawl.do_commands()` / `do_targeted_command()` | Create `MoveCommand`, `AttackCommand`, etc. |
| **Targeting System** | Manual Lua loops over LOS | Reuse `ClosestEnemySelector` + extend |
| **Pathfinding** | Greedy Lua algorithm | **Need to implement** A* or Dijkstra |
| **Configuration** | Lua global variables + `chk_lua_option` | JSON (`game.json` or `autofight_config.json`) |
| **Interrupts** | `interrupt_cmd_repeat()` in state.cc | **Need to implement** interrupt tracking |

### Your Existing Systems We Can Leverage

✅ **Turn System:**
- `TurnManager::ProcessCommand(Command* cmd)` handles turn logic
- `Command::ConsumesTurn()` determines if enemies act
- Events queued via `Engine::HandleEvents()`

✅ **Command Pattern:**
- Base class: `Command`
- Existing: `MoveCommand`, `AttackCommand`, `UseItemCommand`, `CastSpellCommand`
- **Need to add:** `AutofightCommand`

✅ **Targeting System:**
- `ClosestEnemySelector` already finds nearest enemy for spells
- Spell targeting types: `self`, `closest_enemy`, `single`, `area`, `beam`, `first_in_beam`
- **Reuse:** `ClosestEnemySelector` logic for target detection

✅ **Combat Systems:**
- Melee combat (directional attacks) via `AttackerComponent`
- Ranged combat (beam, area, single targeting) via spell system
- **Reuse:** Existing attack logic, just automate target selection

✅ **ECS Components:**
- `BaseEntity`, `Player`, `Npc`
- `AttackerComponent`, `DestructibleComponent`, `AiComponent`, `SpellcasterComponent`
- **Query:** Filter entities by components to find valid targets

✅ **JSON Configuration:**
- `game.json`, spell data, item data, entity data, level data
- **Extend:** Add autofight configuration section

### What We Need to Build

❌ **Pathfinding System:**
- DCSS uses greedy algorithm (not optimal)
- **Recommendation:** Implement A* for better pathing
- **Files to create:** `pathfinding.h/cpp`
- **Integration:** Use existing `Engine::IsWalkable()` for terrain checks

❌ **Interrupt System:**
- Track player HP changes, new monsters spotted, ambiguous targets
- **Files to create:** `interrupt_manager.h/cpp` or add to `TurnManager`
- **Integration:** Hook into `DestructibleComponent::TakeDamage()`, enemy visibility checks

❌ **AutofightCommand:**
- Command that executes autofight logic
- Returns child commands (`MoveCommand`, `AttackCommand`) or executes them
- **Files to create:** `autofight_command.h/cpp`
- **Integration:** Add to `EventHandler` key binding (Tab key)

❌ **Nearest Enemy Detection (Enhanced):**
- Extend `ClosestEnemySelector` to support priority-based targeting
- Add filtering (exclude harmless, prioritize threatening)
- **Files to modify:** Spell targeting system or create `autofight_targeting.h/cpp`

---

## Implementation Plan

### Phase 1: Pathfinding System (Week 1)

**Goal:** Implement A* pathfinding for autofight movement

**Files to Create:**
- `src/pathfinding.h`
- `src/pathfinding.cpp`

**Interface Design:**
```cpp
// pathfinding.h
#ifndef PATHFINDING_H
#define PATHFINDING_H

#include <vector>
#include <optional>
#include "position.h"

class Engine; // Forward declaration

struct PathNode {
    Position pos;
    int g_cost;  // Distance from start
    int h_cost;  // Heuristic to goal
    int f_cost() const { return g_cost + h_cost; }
};

class Pathfinder {
public:
    // Returns next step toward target, or nullopt if unreachable
    static std::optional<Position> FindNextStep(
        const Engine& engine,
        Position start,
        Position target,
        int max_distance = 100
    );

    // Returns full path to target
    static std::vector<Position> FindPath(
        const Engine& engine,
        Position start,
        Position target,
        int max_distance = 100
    );

    // Check if path exists
    static bool IsReachable(
        const Engine& engine,
        Position start,
        Position target,
        int max_distance = 100
    );

private:
    static int Heuristic(Position a, Position b); // Chebyshev distance
    static std::vector<Position> GetNeighbors(Position pos);
    static bool IsWalkable(const Engine& engine, Position pos);
};

#endif // PATHFINDING_H
```

**Algorithm: A* Pathfinding**
```cpp
// pathfinding.cpp implementation
#include "pathfinding.h"
#include "engine.h"
#include <queue>
#include <unordered_set>
#include <unordered_map>

std::optional<Position> Pathfinder::FindNextStep(
    const Engine& engine,
    Position start,
    Position target,
    int max_distance
) {
    auto path = FindPath(engine, start, target, max_distance);
    if (path.empty() || path.size() < 2) {
        return std::nullopt;
    }
    return path[1]; // Return first step (path[0] is start)
}

std::vector<Position> Pathfinder::FindPath(
    const Engine& engine,
    Position start,
    Position target,
    int max_distance
) {
    // A* implementation
    std::priority_queue<PathNode, std::vector<PathNode>,
                        std::greater<PathNode>> open_set;
    std::unordered_set<Position> closed_set;
    std::unordered_map<Position, Position> came_from;
    std::unordered_map<Position, int> g_score;

    PathNode start_node{start, 0, Heuristic(start, target)};
    open_set.push(start_node);
    g_score[start] = 0;

    while (!open_set.empty()) {
        PathNode current = open_set.top();
        open_set.pop();

        if (current.pos == target) {
            // Reconstruct path
            std::vector<Position> path;
            Position pos = target;
            while (pos != start) {
                path.push_back(pos);
                pos = came_from[pos];
            }
            path.push_back(start);
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (closed_set.count(current.pos)) continue;
        closed_set.insert(current.pos);

        for (const Position& neighbor : GetNeighbors(current.pos)) {
            if (!IsWalkable(engine, neighbor)) continue;
            if (closed_set.count(neighbor)) continue;

            int tentative_g = g_score[current.pos] + 1;

            if (!g_score.count(neighbor) || tentative_g < g_score[neighbor]) {
                came_from[neighbor] = current.pos;
                g_score[neighbor] = tentative_g;

                PathNode neighbor_node{
                    neighbor,
                    tentative_g,
                    Heuristic(neighbor, target)
                };
                open_set.push(neighbor_node);
            }
        }

        // Prevent infinite loops
        if (closed_set.size() > max_distance) break;
    }

    return {}; // No path found
}

int Pathfinder::Heuristic(Position a, Position b) {
    // Chebyshev distance (like DCSS)
    int dx = std::abs(a.x - b.x);
    int dy = std::abs(a.y - b.y);
    return std::max(dx, dy);
}

std::vector<Position> Pathfinder::GetNeighbors(Position pos) {
    return {
        {pos.x - 1, pos.y - 1}, {pos.x, pos.y - 1}, {pos.x + 1, pos.y - 1},
        {pos.x - 1, pos.y},                          {pos.x + 1, pos.y},
        {pos.x - 1, pos.y + 1}, {pos.x, pos.y + 1}, {pos.x + 1, pos.y + 1}
    };
}

bool Pathfinder::IsWalkable(const Engine& engine, Position pos) {
    // Reuse existing logic
    if (!engine.IsWalkable(pos)) return false;

    // Check for blocking entities
    auto* entity = engine.GetBlockingEntityAtLocation(pos);
    if (entity && entity->GetEntityType() == EntityType::NPC) {
        // Can't path through NPCs
        return false;
    }

    return true;
}
```

**Testing:**
```cpp
// test_pathfinding.cpp
TEST(Pathfinding, FindsDirectPath) {
    // Test straight line, diagonal, L-shape
}

TEST(Pathfinding, AvoidsWalls) {
    // Test pathfinding around obstacles
}

TEST(Pathfinding, ReturnsNulloptWhenBlocked) {
    // Test fully blocked path
}
```

---

### Phase 2: Enhanced Target Detection (Week 2)

**Goal:** Extend `ClosestEnemySelector` with priority-based targeting

**Files to Create/Modify:**
- `src/autofight_targeting.h`
- `src/autofight_targeting.cpp`
- Optionally modify existing `spells/targeting.cpp`

**Design:**
```cpp
// autofight_targeting.h
#ifndef AUTOFIGHT_TARGETING_H
#define AUTOFIGHT_TARGETING_H

#include "position.h"
#include "base_entity.h"
#include <vector>
#include <optional>

class Engine;

enum class AttackType {
    MELEE,      // Adjacent, melee attack
    RANGED,     // In range, ranged attack (bow, spell)
    MOVES       // Need to move toward target
};

struct TargetPriority {
    bool can_attack;         // Can attack without moving
    bool safe;               // Monster is harmless
    int distance;            // Chebyshev distance (negative for sorting)
    int threat;              // Threat level
    int injury;              // How injured (0-3: healthy, lightly, heavily, almost dead)
    bool constricting_you;   // Monster is constricting player

    // Lexicographic comparison (earlier fields have higher priority)
    bool operator<(const TargetPriority& other) const;
};

struct AutofightTarget {
    BaseEntity* entity;
    Position pos;
    AttackType attack_type;
    TargetPriority priority;
};

class AutofightTargeting {
public:
    // Find best target for autofight
    static std::optional<AutofightTarget> GetBestTarget(
        const Engine& engine,
        const Player& player,
        bool allow_movement
    );

private:
    static std::vector<AutofightTarget> GetCandidates(
        const Engine& engine,
        const Player& player,
        bool allow_movement
    );

    static bool IsCandidate(const BaseEntity* entity);
    static TargetPriority CalculatePriority(
        const Engine& engine,
        const Player& player,
        const BaseEntity* entity
    );

    static AttackType DetermineAttackType(
        const Engine& engine,
        const Player& player,
        const BaseEntity* entity
    );

    static int CalculateThreat(const BaseEntity* entity);
    static int GetInjuryLevel(const BaseEntity* entity);
    static int ChebyshevDistance(Position a, Position b);
};

#endif // AUTOFIGHT_TARGETING_H
```

**Implementation:**
```cpp
// autofight_targeting.cpp
#include "autofight_targeting.h"
#include "engine.h"
#include "player.h"
#include "npc.h"
#include "destructible_component.h"
#include "attacker_component.h"
#include "ai_component.h"

std::optional<AutofightTarget> AutofightTargeting::GetBestTarget(
    const Engine& engine,
    const Player& player,
    bool allow_movement
) {
    auto candidates = GetCandidates(engine, player, allow_movement);

    if (candidates.empty()) {
        return std::nullopt;
    }

    // Sort by priority (lowest priority value = highest priority target)
    std::sort(candidates.begin(), candidates.end(),
              [](const AutofightTarget& a, const AutofightTarget& b) {
                  return a.priority < b.priority;
              });

    return candidates.front();
}

std::vector<AutofightTarget> AutofightTargeting::GetCandidates(
    const Engine& engine,
    const Player& player,
    bool allow_movement
) {
    std::vector<AutofightTarget> candidates;

    // Get all NPCs in LOS (reuse existing visibility system)
    auto entities = engine.GetEntityManager().GetEntitiesInRange(
        player.GetPosition(),
        player.GetLosRange() // Assuming you have LOS range
    );

    for (auto* entity : entities) {
        if (!IsCandidate(entity)) continue;

        AttackType attack_type = DetermineAttackType(engine, player, entity);

        // Filter based on allow_movement
        if (!allow_movement && attack_type == AttackType::MOVES) {
            continue;
        }

        AutofightTarget target{
            entity,
            entity->GetPosition(),
            attack_type,
            CalculatePriority(engine, player, entity)
        };

        candidates.push_back(target);
    }

    return candidates;
}

bool AutofightTargeting::IsCandidate(const BaseEntity* entity) {
    // Must be NPC
    if (entity->GetEntityType() != EntityType::NPC) {
        return false;
    }

    const Npc* npc = static_cast<const Npc*>(entity);

    // Must be hostile (has AI that targets player)
    auto* ai = npc->GetComponent<AiComponent>();
    if (!ai || !ai->IsHostile()) {
        return false;
    }

    // Must be destructible (can take damage)
    auto* destructible = npc->GetComponent<DestructibleComponent>();
    if (!destructible || destructible->IsDead()) {
        return false;
    }

    // Exclude harmless monsters (optional - load from config)
    // if (npc->GetName() == "butterfly") return false;

    return true;
}

TargetPriority AutofightTargeting::CalculatePriority(
    const Engine& engine,
    const Player& player,
    const BaseEntity* entity
) {
    TargetPriority priority;

    Position player_pos = player.GetPosition();
    Position target_pos = entity->GetPosition();

    AttackType attack_type = DetermineAttackType(engine, player, entity);

    priority.can_attack = (attack_type != AttackType::MOVES);
    priority.safe = false; // TODO: detect harmless monsters
    priority.distance = -ChebyshevDistance(player_pos, target_pos); // Negative for sorting
    priority.threat = CalculateThreat(entity);
    priority.injury = GetInjuryLevel(entity);
    priority.constricting_you = false; // TODO: check if constricted

    return priority;
}

AttackType AutofightTargeting::DetermineAttackType(
    const Engine& engine,
    const Player& player,
    const BaseEntity* entity
) {
    Position player_pos = player.GetPosition();
    Position target_pos = entity->GetPosition();
    int distance = ChebyshevDistance(player_pos, target_pos);

    // Adjacent = melee
    if (distance <= 1) {
        return AttackType::MELEE;
    }

    // Check if player has ranged attack capability
    auto* spellcaster = player.GetComponent<SpellcasterComponent>();
    if (spellcaster && spellcaster->HasOffensiveSpells()) {
        // Assuming spells have range (you may need to check actual range)
        return AttackType::RANGED;
    }

    // TODO: Check for equipped bow/ranged weapon
    // For now, assume need to move if not adjacent
    return AttackType::MOVES;
}

int AutofightTargeting::CalculateThreat(const BaseEntity* entity) {
    auto* attacker = entity->GetComponent<AttackerComponent>();
    if (!attacker) return 0;

    // Simple threat = attack power
    return attacker->GetAttackPower();
}

int AutofightTargeting::GetInjuryLevel(const BaseEntity* entity) {
    auto* destructible = entity->GetComponent<DestructibleComponent>();
    if (!destructible) return 0;

    int hp = destructible->GetHp();
    int max_hp = destructible->GetMaxHp();
    float ratio = static_cast<float>(hp) / max_hp;

    if (ratio > 0.75f) return 0;      // Healthy
    if (ratio > 0.5f) return 1;       // Lightly injured
    if (ratio > 0.25f) return 2;      // Heavily injured
    return 3;                         // Almost dead
}

int AutofightTargeting::ChebyshevDistance(Position a, Position b) {
    int dx = std::abs(a.x - b.x);
    int dy = std::abs(a.y - b.y);
    return std::max(dx, dy);
}

bool TargetPriority::operator<(const TargetPriority& other) const {
    // Lexicographic comparison (earlier = higher priority)
    // Higher can_attack is better (true > false)
    if (can_attack != other.can_attack) return can_attack > other.can_attack;

    // Lower safe is better (false < true, i.e., unsafe/hostile prioritized)
    if (safe != other.safe) return safe < other.safe;

    // Higher distance (less negative) is closer, better
    if (distance != other.distance) return distance > other.distance;

    // Higher threat is worse, but we want to target threatening enemies
    if (threat != other.threat) return threat > other.threat;

    // Higher injury level = more injured = prioritize (finish off)
    if (injury != other.injury) return injury > other.injury;

    // Constricting player = higher priority
    if (constricting_you != other.constricting_you) return constricting_you > other.constricting_you;

    return false; // Equal priority
}
```

**Configuration (game.json):**
```json
{
  "autofight": {
    "priority_order": ["can_attack", "safe", "distance", "threat", "injury"],
    "excluded_monsters": ["butterfly"],
    "prefer_injured": true
  }
}
```

---

### Phase 3: Interrupt System (Week 3)

**Goal:** Track interrupts (damage, new monsters) to stop autofight

**Files to Create:**
- `src/interrupt_manager.h`
- `src/interrupt_manager.cpp`

**Design:**
```cpp
// interrupt_manager.h
#ifndef INTERRUPT_MANAGER_H
#define INTERRUPT_MANAGER_H

#include <string>

enum class InterruptType {
    NONE,
    HP_LOSS,           // Player took damage
    NEW_MONSTER,       // New hostile spotted
    AMBIGUOUS_TARGET,  // Multiple enemies at same distance
    TERRAIN_PROMPT     // Encountered dangerous terrain
};

class InterruptManager {
public:
    // Register an interrupt
    static void RegisterInterrupt(InterruptType type, const std::string& reason = "");

    // Check if autofight should stop
    static bool ShouldStopAutofight();

    // Get interrupt reason for messaging
    static std::string GetInterruptReason();

    // Clear interrupts (call at start of player turn)
    static void ClearInterrupts();

private:
    static InterruptType current_interrupt_;
    static std::string interrupt_reason_;
};

#endif // INTERRUPT_MANAGER_H
```

**Implementation:**
```cpp
// interrupt_manager.cpp
#include "interrupt_manager.h"

InterruptType InterruptManager::current_interrupt_ = InterruptType::NONE;
std::string InterruptManager::interrupt_reason_ = "";

void InterruptManager::RegisterInterrupt(InterruptType type, const std::string& reason) {
    current_interrupt_ = type;
    interrupt_reason_ = reason;
}

bool InterruptManager::ShouldStopAutofight() {
    return current_interrupt_ != InterruptType::NONE;
}

std::string InterruptManager::GetInterruptReason() {
    return interrupt_reason_;
}

void InterruptManager::ClearInterrupts() {
    current_interrupt_ = InterruptType::NONE;
    interrupt_reason_ = "";
}
```

**Integration Points:**

1. **Damage Tracking** (in `DestructibleComponent::TakeDamage()`):
```cpp
void DestructibleComponent::TakeDamage(int damage) {
    int old_hp = hp_;
    hp_ -= damage;

    // If this is the player, register interrupt
    if (owner_->GetEntityType() == EntityType::PLAYER && damage > 0) {
        InterruptManager::RegisterInterrupt(
            InterruptType::HP_LOSS,
            "You took damage!"
        );
    }

    // ... rest of damage logic
}
```

2. **New Monster Detection** (in AI/visibility update):
```cpp
// In TurnManager::ProcessEnemyTurn() or visibility update
void CheckForNewMonsters(const Player& player) {
    // Track previously visible monsters
    static std::unordered_set<BaseEntity*> previously_visible;

    auto visible_monsters = GetVisibleMonsters(player);

    for (auto* monster : visible_monsters) {
        if (!previously_visible.count(monster)) {
            InterruptManager::RegisterInterrupt(
                InterruptType::NEW_MONSTER,
                "A new enemy appears!"
            );
        }
    }

    previously_visible = visible_monsters;
}
```

3. **Clear Interrupts** (in `TurnManager::ProcessCommand()`):
```cpp
void TurnManager::ProcessCommand(Command* cmd) {
    // Clear interrupts at start of player turn
    InterruptManager::ClearInterrupts();

    // ... rest of turn processing
}
```

---

### Phase 4: AutofightCommand (Week 4)

**Goal:** Create command that executes autofight logic

**Files to Create:**
- `src/commands/autofight_command.h`
- `src/commands/autofight_command.cpp`

**Design:**
```cpp
// autofight_command.h
#ifndef AUTOFIGHT_COMMAND_H
#define AUTOFIGHT_COMMAND_H

#include "command.h"
#include "autofight_targeting.h"
#include <memory>

class Engine;

class AutofightCommand : public Command {
public:
    explicit AutofightCommand(Engine& engine);

    void Execute() override;
    bool ConsumesTurn() override;

private:
    bool CheckPreconditions();
    std::unique_ptr<Command> CreateActionCommand(const AutofightTarget& target);

    Engine& engine_;
    bool turn_consumed_ = false;
};

#endif // AUTOFIGHT_COMMAND_H
```

**Implementation:**
```cpp
// autofight_command.cpp
#include "autofight_command.h"
#include "engine.h"
#include "player.h"
#include "move_command.h"
#include "attack_command.h"
#include "cast_spell_command.h"
#include "autofight_targeting.h"
#include "pathfinding.h"
#include "interrupt_manager.h"
#include "message_log.h"

AutofightCommand::AutofightCommand(Engine& engine)
    : engine_(engine) {}

void AutofightCommand::Execute() {
    Player* player = engine_.GetPlayer();

    // 1. Check interrupts
    if (InterruptManager::ShouldStopAutofight()) {
        engine_.GetMessageLog().AddMessage(
            InterruptManager::GetInterruptReason(),
            MessageType::WARNING
        );
        turn_consumed_ = false;
        return;
    }

    // 2. Check preconditions
    if (!CheckPreconditions()) {
        turn_consumed_ = false;
        return;
    }

    // 3. Find best target
    auto target = AutofightTargeting::GetBestTarget(
        engine_,
        *player,
        true // allow_movement
    );

    if (!target) {
        engine_.GetMessageLog().AddMessage(
            "No targets in view!",
            MessageType::INFO
        );
        turn_consumed_ = false;
        return;
    }

    // 4. Create and execute appropriate command
    auto action = CreateActionCommand(*target);
    if (action) {
        action->Execute();
        turn_consumed_ = action->ConsumesTurn();
    } else {
        turn_consumed_ = false;
    }
}

bool AutofightCommand::ConsumesTurn() {
    return turn_consumed_;
}

bool AutofightCommand::CheckPreconditions() {
    Player* player = engine_.GetPlayer();
    auto* destructible = player->GetComponent<DestructibleComponent>();

    if (!destructible) return false;

    // Check HP threshold (configurable, default 50%)
    int hp = destructible->GetHp();
    int max_hp = destructible->GetMaxHp();
    float hp_ratio = static_cast<float>(hp) / max_hp;

    const float HP_THRESHOLD = 0.5f; // TODO: Load from config

    if (hp_ratio <= HP_THRESHOLD) {
        engine_.GetMessageLog().AddMessage(
            "You are too injured to fight recklessly!",
            MessageType::WARNING
        );
        return false;
    }

    // Check confusion (if you have status effects)
    // if (player->HasStatus(StatusEffect::CONFUSED)) {
    //     engine_.GetMessageLog().AddMessage(
    //         "You are too confused!",
    //         MessageType::WARNING
    //     );
    //     return false;
    // }

    // Check trapped/immobile (if you have these mechanics)
    // if (player->IsTrapped()) {
    //     engine_.GetMessageLog().AddMessage(
    //         "You are trapped!",
    //         MessageType::WARNING
    //     );
    //     return false;
    // }

    return true;
}

std::unique_ptr<Command> AutofightCommand::CreateActionCommand(
    const AutofightTarget& target
) {
    Player* player = engine_.GetPlayer();
    Position player_pos = player->GetPosition();
    Position target_pos = target.pos;

    switch (target.attack_type) {
        case AttackType::MELEE: {
            // Adjacent - create attack command
            // Assuming you have a constructor like AttackCommand(Engine&, Position)
            return std::make_unique<AttackCommand>(engine_, target_pos);
        }

        case AttackType::RANGED: {
            // In range - cast offensive spell or use ranged weapon
            auto* spellcaster = player->GetComponent<SpellcasterComponent>();
            if (spellcaster && spellcaster->HasOffensiveSpells()) {
                // Get first offensive spell (or implement smarter selection)
                // Assuming you have CastSpellCommand(Engine&, spell_index, target)
                // return std::make_unique<CastSpellCommand>(engine_, 0, target_pos);

                // For now, simplified - you'll need to adapt to your spell system
                engine_.GetMessageLog().AddMessage(
                    "Ranged attack not yet implemented in autofight!",
                    MessageType::WARNING
                );
                return nullptr;
            }
            return nullptr;
        }

        case AttackType::MOVES: {
            // Need to move - use pathfinding
            auto next_step = Pathfinder::FindNextStep(
                engine_,
                player_pos,
                target_pos
            );

            if (!next_step) {
                engine_.GetMessageLog().AddMessage(
                    "Failed to find path to target!",
                    MessageType::WARNING
                );
                return nullptr;
            }

            // Create move command
            return std::make_unique<MoveCommand>(engine_, *next_step);
        }

        default:
            return nullptr;
    }
}
```

---

### Phase 5: Input Binding (Week 5)

**Goal:** Bind Tab key to AutofightCommand

**Files to Modify:**
- `src/event_handler.cpp` (or wherever key bindings are handled)

**Implementation:**
```cpp
// In EventHandler::HandleKeyPress() or similar
std::unique_ptr<Command> EventHandler::HandleKeyPress(SDL_Event& event) {
    SDL_Keycode key = event.key.keysym.sym;

    // ... existing key handling ...

    // Add autofight binding
    if (key == SDLK_TAB) {
        return std::make_unique<AutofightCommand>(*engine_);
    }

    // Optional: Shift+Tab for no-movement autofight
    // if (key == SDLK_TAB && (SDL_GetModState() & KMOD_SHIFT)) {
    //     return std::make_unique<AutofightCommand>(*engine_, false); // allow_movement=false
    // }

    // ... rest of key handling ...
}
```

**Configuration (optional, in game.json):**
```json
{
  "keybindings": {
    "autofight": "Tab",
    "autofight_nomove": "Shift+Tab"
  }
}
```

---

### Phase 6: Configuration & Polish (Week 6)

**Goal:** Add JSON configuration and polish UX

**Configuration File:** `data/autofight_config.json` (or add to `game.json`)
```json
{
  "autofight": {
    "enabled": true,

    "preconditions": {
      "hp_threshold_percent": 50,
      "check_confusion": true,
      "check_trapped": true
    },

    "targeting": {
      "priority_order": [
        "can_attack",
        "safe",
        "distance",
        "threat",
        "injury",
        "constricting_you"
      ],
      "excluded_monsters": [
        "butterfly"
      ],
      "prefer_injured": true,
      "max_distance": 20
    },

    "pathfinding": {
      "max_path_length": 100,
      "avoid_dangerous_terrain": true
    },

    "interrupts": {
      "stop_on_hp_loss": true,
      "stop_on_new_monster": false,
      "stop_on_ambiguous_target": true
    },

    "messages": {
      "no_target": "No targets in view!",
      "hp_too_low": "You are too injured to fight recklessly!",
      "confused": "You are too confused!",
      "trapped": "You are trapped!",
      "took_damage": "You took damage - stopping autofight!",
      "new_monster": "A new enemy appears - stopping autofight!"
    }
  }
}
```

**Loading Configuration:**
```cpp
// autofight_config.h
struct AutofightConfig {
    bool enabled = true;

    struct {
        float hp_threshold = 0.5f;
        bool check_confusion = true;
        bool check_trapped = true;
    } preconditions;

    struct {
        std::vector<std::string> priority_order;
        std::vector<std::string> excluded_monsters;
        bool prefer_injured = true;
        int max_distance = 20;
    } targeting;

    struct {
        int max_path_length = 100;
        bool avoid_dangerous_terrain = true;
    } pathfinding;

    struct {
        bool stop_on_hp_loss = true;
        bool stop_on_new_monster = false;
        bool stop_on_ambiguous_target = true;
    } interrupts;

    struct {
        std::string no_target = "No targets in view!";
        std::string hp_too_low = "You are too injured to fight recklessly!";
        // ... other messages
    } messages;

    static AutofightConfig LoadFromJson(const std::string& filepath);
};
```

**Polish Features:**
- Visual indicator when autofight is active (highlight target, show path)
- Sound effect on autofight activation
- Animation for auto-movement
- Configurable key bindings
- Tutorial/help text explaining autofight

---

## Integration Steps

### Week 1: Foundation
- [ ] Implement `Pathfinder` class with A* algorithm
- [ ] Add unit tests for pathfinding (straight, diagonal, obstacles)
- [ ] Integrate with existing `Engine::IsWalkable()`
- [ ] Test: Player can pathfind to arbitrary position

### Week 2: Targeting
- [ ] Implement `AutofightTargeting` class
- [ ] Port priority comparison logic from DCSS
- [ ] Test: Can identify best target from multiple enemies
- [ ] Verify: Closest enemy prioritized, then threat level

### Week 3: Interrupts
- [ ] Implement `InterruptManager` singleton
- [ ] Hook into `DestructibleComponent::TakeDamage()`
- [ ] Add interrupt clearing to turn manager
- [ ] Test: Damage stops autofight, cleared next turn

### Week 4: Command
- [ ] Implement `AutofightCommand` class
- [ ] Wire up to pathfinding and targeting
- [ ] Add precondition checks (HP threshold)
- [ ] Test: Full autofight flow (find target → move/attack)

### Week 5: Input
- [ ] Bind Tab key to `AutofightCommand`
- [ ] Add to key bindings config
- [ ] Test: Pressing Tab executes autofight
- [ ] Optional: Add Shift+Tab for no-movement mode

### Week 6: Polish
- [ ] Load configuration from JSON
- [ ] Add all user messages
- [ ] Visual indicators (highlight target)
- [ ] Playtest and tune parameters

---

## Testing Plan

### Unit Tests

**Pathfinding:**
```cpp
TEST(Pathfinding, FindsDirectPath) {
    // Setup: Open 5x5 grid, player at (0,0), target at (4,4)
    // Expected: Diagonal path, 4 steps
}

TEST(Pathfinding, AvoidsWalls) {
    // Setup: Wall blocking direct path
    // Expected: Path around wall
}

TEST(Pathfinding, ReturnsNulloptWhenBlocked) {
    // Setup: Target completely walled off
    // Expected: std::nullopt
}
```

**Targeting:**
```cpp
TEST(Targeting, SelectsClosestEnemy) {
    // Setup: Enemies at distance 5 and 10
    // Expected: Selects distance 5
}

TEST(Targeting, PrioritizesThreat) {
    // Setup: Two enemies, same distance, different threat
    // Expected: Selects higher threat
}

TEST(Targeting, ExcludesHarmless) {
    // Setup: Hostile and "butterfly" at same distance
    // Expected: Selects hostile, ignores butterfly
}
```

**Interrupts:**
```cpp
TEST(Interrupts, StopsOnDamage) {
    // Setup: Autofight active, player takes damage
    // Expected: ShouldStopAutofight() == true
}

TEST(Interrupts, ClearsOnNewTurn) {
    // Setup: Interrupt registered, new turn starts
    // Expected: ShouldStopAutofight() == false
}
```

**Preconditions:**
```cpp
TEST(Autofight, BlocksWhenLowHP) {
    // Setup: Player at 40% HP, threshold 50%
    // Expected: CheckPreconditions() == false
}

TEST(Autofight, AllowsWhenHealthy) {
    // Setup: Player at 80% HP
    // Expected: CheckPreconditions() == true
}
```

### Integration Tests

**End-to-End Autofight:**
```cpp
TEST(AutofightIntegration, AttacksSingleEnemy) {
    // Setup: Player at (0,0), enemy at (1,0)
    // Action: Execute AutofightCommand
    // Expected: AttackCommand executed, enemy takes damage
}

TEST(AutofightIntegration, MovesTowardDistantEnemy) {
    // Setup: Player at (0,0), enemy at (5,5)
    // Action: Execute AutofightCommand
    // Expected: MoveCommand executed, player at (1,1)
}

TEST(AutofightIntegration, StopsWhenDamaged) {
    // Setup: Autofight active, enemy damages player
    // Action: Execute AutofightCommand
    // Expected: Autofight stops, message displayed
}
```

### Playtest Scenarios

1. **Basic Combat:**
   - Single melee enemy, no obstacles
   - Expected: Auto-move and attack until dead

2. **Ranged Combat:**
   - Enemy at distance, player has spells/ranged weapon
   - Expected: Attack without moving

3. **Pathing Around Obstacles:**
   - Enemy behind wall, requires navigation
   - Expected: Pathfind around wall, then attack

4. **Multiple Enemies:**
   - 3 enemies at different distances
   - Expected: Target closest, then next closest

5. **Low HP Stop:**
   - Player at 45% HP (threshold 50%)
   - Expected: Autofight refuses to start

6. **Damage Interrupt:**
   - Autofight active, enemy hits player
   - Expected: Autofight stops immediately

7. **Ambiguous Targeting:**
   - 2 enemies at equal distance, same priority
   - Expected: Pick one (deterministic tie-break)

---

## Key Differences from DCSS

### Improvements
✅ **A* Pathfinding** (vs DCSS's greedy algorithm)
- Better path quality
- Won't get stuck on corners
- Guaranteed shortest path (if exists)

✅ **C++ Performance** (vs Lua scripting)
- Faster execution
- Better debugging/profiling
- Type safety

✅ **ECS Architecture** (vs DCSS's mixed approach)
- Cleaner component queries
- Better cache coherency
- Easier to extend (add new components)

### Simplifications
⚠️ **No Reaching/Polearm Support** (for MVP)
- DCSS has complex reach mechanics (2+ range melee)
- Your project: Add later if needed

⚠️ **Simpler Targeting Priority** (for MVP)
- DCSS has 10+ priority factors
- Your project: Start with distance, threat, injury

⚠️ **No Recklessness Timer** (for MVP)
- DCSS warns if autofight spammed too fast
- Your project: Add later if players abuse it

### Extensions to Consider
🔮 **Safe Path Checking:**
- DCSS doesn't avoid dangerous terrain en route
- You could: Check path for traps/hazardous tiles

🔮 **Multi-Turn Lookahead:**
- DCSS is single-step greedy
- You could: Simulate next N turns for tactical decisions

🔮 **Threat Prediction:**
- DCSS uses static threat values
- You could: ML model or heuristic for threat assessment

---

## Troubleshooting

### Common Issues

**Issue:** Autofight gets stuck in loop (keeps attacking same spot)
- **Cause:** Target not dying, attack not executing
- **Fix:** Check `AttackCommand` executes properly, enemy HP decreases

**Issue:** Pathfinding fails for reachable target
- **Cause:** `IsWalkable()` too restrictive, or A* max iterations
- **Fix:** Debug path, increase `max_distance`, check terrain

**Issue:** Targets wrong enemy
- **Cause:** Priority comparison logic inverted
- **Fix:** Verify `operator<` in `TargetPriority`, check test cases

**Issue:** Doesn't stop on damage
- **Cause:** Interrupt not registered, or cleared prematurely
- **Fix:** Add logging in `TakeDamage()`, verify interrupt flow

**Issue:** Performance drops with many enemies
- **Cause:** A* running on every autofight call
- **Fix:** Cache paths, limit search radius, optimize neighbor iteration

---

## Performance Considerations

**Pathfinding:**
- A* can be expensive with large search spaces
- **Optimization:** Limit `max_distance` to LOS range (~20 tiles)
- **Optimization:** Cache last path, revalidate instead of full A*
- **Optimization:** Use jump point search (JPS) for grid-based maps

**Target Selection:**
- Scanning all entities each turn can be slow
- **Optimization:** Use spatial partitioning (quadtree, grid)
- **Optimization:** Only scan visible entities (already in LOS)
- **Optimization:** Early exit when best target found (if priority order strict)

**Interrupt Checking:**
- Checking interrupts on every command
- **Optimization:** Use bitflags instead of enum
- **Optimization:** Inline `ShouldStopAutofight()`

---

## Future Enhancements

### Phase 2 Features (Post-MVP)

1. **Autofight Modes:**
   - Tab: Move + Attack (current)
   - Shift+Tab: Attack in range only (no movement)
   - Ctrl+Tab: Ranged only (fire/spell)

2. **Targeting Filters:**
   - Configurable exclusion list (ignore certain monster types)
   - Priority overrides (always target healers first)
   - Distance limits (don't chase beyond X tiles)

3. **Advanced Interrupts:**
   - Stop on new monster (configurable)
   - Stop on ally in danger
   - Stop on item dropped

4. **Visual Feedback:**
   - Highlight target in color
   - Show path preview (dashed line)
   - Animation for auto-movement (smooth slide vs instant)

5. **Configuration UI:**
   - In-game menu to adjust thresholds
   - Save presets (aggressive, defensive, ranged)
   - Per-character autofight settings

6. **Analytics:**
   - Track autofight usage
   - Measure player deaths during autofight (safety check)
   - Optimize priority weights based on player behavior

---

## Code Integration Checklist

Before submitting/committing:

- [ ] All unit tests pass (`make test`)
- [ ] No memory leaks (run valgrind)
- [ ] Code follows project style guide
- [ ] Added comments to complex algorithms (A*, priority comparison)
- [ ] Configuration loads from JSON correctly
- [ ] Integrated with existing systems (TurnManager, EntityManager, EventHandler)
- [ ] User messages display properly in MessageLog
- [ ] Key binding documented (README or in-game help)
- [ ] Playtest completed (all scenarios pass)
- [ ] Performance profiled (no frame drops)

---

## Questions to Answer Before Implementation

1. **Ranged Combat Integration:**
   - How do spells interact with autofight?
   - Should autofight prefer spells or melee?
   - Do you have equipped weapon detection (bow, staff)?

2. **Status Effects:**
   - Do you have confusion, paralysis, trapped states?
   - Should autofight check these in preconditions?

3. **Visibility System:**
   - How do you track LOS (line of sight)?
   - Do you have FOV (field of view) calculation?
   - Can you get all visible entities easily?

4. **Spatial Partitioning:**
   - Do you have a spatial grid for entities?
   - How efficient is `GetEntitiesInRange()`?

5. **Animation/Timing:**
   - Are moves instant or animated?
   - Should autofight wait for animations?
   - How do you handle rapid input during autofight?

6. **Save/Load:**
   - Should autofight state persist across saves?
   - Or is it transient (resets on load)?

---

## Additional Resources

**DCSS Source Files to Reference:**
- `crawl-ref/source/dat/clua/autofight.lua` - Main logic
- `crawl-ref/source/main.cc` - Command handling
- `crawl-ref/source/l-crawl.cc` - Lua bindings (do_commands, do_targeted_command)
- `crawl-ref/source/l-moninf.cc` - Monster info binding (get_monster_at)
- `crawl-ref/source/state.cc` - Interrupt system

**Algorithms:**
- A* Pathfinding: https://www.redblobgames.com/pathfinding/a-star/
- Chebyshev Distance: https://en.wikipedia.org/wiki/Chebyshev_distance

**Your Project Files (assumed):**
- `src/command.h` - Command base class
- `src/commands/move_command.h` - MoveCommand
- `src/commands/attack_command.h` - AttackCommand
- `src/turn_manager.h` - TurnManager
- `src/entity_manager.h` - EntityManager
- `src/engine.h` - Engine
- `src/event_handler.h` - EventHandler (input)

---

## Summary

**What You're Building:**
1. **Pathfinding System** (A* algorithm)
2. **Target Selection** (priority-based, like DCSS)
3. **Interrupt Manager** (stop on damage, new enemies)
4. **AutofightCommand** (orchestrates above systems)
5. **Key Binding** (Tab key)
6. **Configuration** (JSON)

**What You're Reusing:**
- ✅ Turn system (TurnManager, Command::ConsumesTurn)
- ✅ Combat system (AttackCommand, MoveCommand)
- ✅ ECS (Entity queries, component access)
- ✅ JSON configuration
- ✅ Message log

**Estimated Timeline:**
- Week 1: Pathfinding
- Week 2: Targeting
- Week 3: Interrupts
- Week 4: AutofightCommand
- Week 5: Input binding
- Week 6: Polish & config

**Total:** 6 weeks for complete, polished implementation

---

## Contact/Questions

When implementing with Claude Code, reference:
- This document for architecture decisions
- DCSS source code at `/home/user/crawl/crawl-ref/source/` for algorithm details
- Your project's existing patterns (Command pattern, ECS components, JSON config)

**Start with Phase 1 (Pathfinding)** - it's the foundation for everything else.

Good luck! 🎮
