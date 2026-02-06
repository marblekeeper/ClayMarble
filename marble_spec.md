# MarbleScript Language Reference v0.1

**File Extension:** `.marble`
**Status:** First-pass specification, derived from working C runtime (Phase 0.2)
**Target:** MarbleEngine discrete event simulation runtime (C99)

---

## 1. Overview

MarbleScript is a **non-Turing-complete, declarative schema language** for defining
simulation worlds. It does not support logic branching, loops, or general computation.
It defines *what exists* and *what can happen* -- never *how* it happens.

The C runtime is a **generic processor** that reads the translation tables MarbleScript
generates and evaluates them deterministically each tick. All behavioral logic lives in
the C layer; MarbleScript is the instruction set that tells the processor what to evaluate.

### 1.1 Pipeline

```
  .marble source
       |
       v
  MarbleScript Compiler (Phase 1+)
       |
       v
  manifest.json (intermediate representation)
       |
       v
  C Codegen: structs, enums, lookup tables, dispatch switch
       |
       v
  C99 Runtime (marble_core.h + marble_interact.h)
```

### 1.2 Design Principles

- **Declarative over imperative.** MarbleScript describes relationships and rules.
  The C runtime evaluates them. No mutable state is expressed in MarbleScript.
- **Composition over inheritance.** Entities are bags of components. Behavior emerges
  from which components are present, not from type hierarchies.
- **Translation tables, not code.** Every MarbleScript block compiles to a C lookup
  table (const array of structs). The generic processor walks these tables at runtime.
- **Bounded and predictable.** No unbounded recursion, no dynamic allocation, no
  strings at runtime. Everything has a known upper bound at compile time.

---

## 2. File Structure

A `.marble` file contains one or more **declaration blocks**. Order does not matter;
the compiler resolves references after parsing all blocks.

```marble
-- This is a comment (double-dash to end of line)

world "Oak Forest Demo" {
    max_entities 1024
    tick_interval_ms 600
    seed 42
}

-- Declarations follow in any order
material Wood { ... }
component Health { ... }
capability Chop { ... }
```

### 2.1 Comments

```marble
-- Single line comment (double-dash)
```

No block comments. Keep it simple.

### 2.2 Naming Conventions

- Block names: `PascalCase` (e.g., `Health`, `OakTree`, `Woodcutting`)
- Field names: `snake_case` (e.g., `max_hp`, `tick_interval_ms`)
- Enum-like references: `PascalCase.PascalCase` (e.g., `Material.Iron`, `Anatomy.Arms`)

---

## 3. Type System

MarbleScript has a minimal, fixed type system. No user-defined types beyond the
block declarations. No generics. No type inference.

| Type       | C Equivalent    | Description                          |
|------------|-----------------|--------------------------------------|
| `i32`      | `int32_t`       | Signed 32-bit integer                |
| `u32`      | `uint32_t`      | Unsigned 32-bit integer              |
| `f32`      | `float`         | 32-bit floating point                |
| `bool`     | `uint32_t` (0/1)| Boolean flag                         |
| `EntityID` | `uint32_t`      | Reference to another entity          |
| `enum`     | C enum          | Named integer constant               |

### 3.1 Fixed-Size Arrays

Fields can be fixed-size arrays. No dynamic arrays.

```marble
component Inventory {
    EntityID[8] slots    -- up to 8 item references
    u32         count    -- how many slots are occupied
}
```

Compiles to:

```c
typedef struct {
    EntityID slots[8];
    uint32_t count;
} CInventory;
```

---

## 4. Declaration Blocks

### 4.1 `world` -- Global Configuration

Exactly one per project. Defines engine limits and simulation parameters.

```marble
world "My Simulation" {
    max_entities     1024
    tick_interval_ms 600
    seed             42
    max_layers       4
    max_body_parts   6
    max_skills       8
}
```

**Compiles to:** `#define` constants in the generated header.

```c
#define MC_MAX_ENTITIES     1024
#define MC_TICK_INTERVAL_US 600000
#define WORLD_SEED          42u
#define MAX_LAYERS          4
#define MAX_BODY_PARTS      6
#define MAX_SKILLS          8
```

---

### 4.2 `material` -- Physical Constants

Defines a material with static properties used in condition checks.

```marble
material Wood {
    hardness 30
}

material Iron {
    hardness 80
}

material Flesh {
    hardness 10
}

material Bark {
    hardness 25
}

material Bone {
    hardness 40
}
```

**Compiles to:** A `MaterialID` enum and a `MATERIAL_HARDNESS[]` lookup table.

```c
typedef enum { MAT_NONE=0, MAT_WOOD=1, MAT_IRON=2, ... } MaterialID;
static const int32_t MATERIAL_HARDNESS[] = { 0, 30, 80, 10, 25, 40 };
```

**Future fields** (not yet implemented): `density`, `flammability`, `conductivity`.

---

### 4.3 `component` -- Data Attached to Entities

Defines a struct that can be attached to any entity via a sparse set.

```marble
component Health {
    i32 hp
    i32 max_hp
}

component Position {
    f32 x
    f32 y
}

component Skills {
    i32[8] level    -- indexed by SkillID
}
```

**Compiles to:** A C struct and a static `SparseSet` instance.

```c
typedef struct { int32_t hp; int32_t max_hp; } CHealth;
static SparseSet g_pool_health;
// Init: mc_sparse_set_init(&g_pool_health, sizeof(CHealth));
```

---

### 4.4 `layer` -- Physical Structure Templates

Defines a reusable layer stack template for objects with physical structure.
Layers are ordered outside-in (index 0 = outermost).

```marble
layer OakTree {
    Bark  integrity 3
    Wood  integrity 5
}

layer HumanHand {
    Flesh integrity 2
    Bone  integrity 3
}

layer IronArmor {
    Iron integrity 10
}
```

**Compiles to:** An initialization function that populates a `CLayerStack`.

```c
static void layer_template_OakTree(CLayerStack* ls) {
    ls->layer_count = 2;
    ls->layers[0] = (Layer){ MAT_BARK, 3, 3 };
    ls->layers[1] = (Layer){ MAT_WOOD, 5, 5 };
}
```

---

### 4.5 `skill` -- Named Skill Identifiers

Declares a skill that can be referenced by capabilities and rolled against.

```marble
skill Woodcutting
skill Mining
skill Combat
```

**Compiles to:** A `SkillID` enum.

```c
typedef enum { SKILL_NONE=0, SKILL_WOODCUTTING=1, SKILL_MINING=2, SKILL_COMBAT=3, SKILL_COUNT } SkillID;
```

---

### 4.6 `anatomy` -- Body Structure Flags

Declares named anatomy flags that capabilities can require.

```marble
anatomy Arms
anatomy Legs
anatomy Hands
anatomy Mouth
```

**Compiles to:** An `AnatomyFlag` enum (bitfield values).

```c
typedef enum {
    ANAT_ARMS  = (1 << 0),
    ANAT_LEGS  = (1 << 1),
    ANAT_HANDS = (1 << 2),
    ANAT_MOUTH = (1 << 3)
} AnatomyFlag;
```

---

### 4.7 `bodypart` -- Named Body Part Slots

Declares named slots that can reference sub-entities.

```marble
bodypart RightHand
bodypart LeftHand
bodypart Torso
bodypart Head
```

**Compiles to:** A `BodyPartID` enum and the `CBodyParts` component struct.

---

### 4.8 `capability` -- What an Actor Can Do

Declares an actor-side permission. The processor checks this when an entity
attempts a verb.

```marble
capability Chop {
    require Anatomy.Arms
    require Anatomy.Hands
    require BodyPart.RightHand.integrity > 0   -- fine motor gate
    skill   Woodcutting
    min_skill 1
}

capability Mine {
    require Anatomy.Arms
    require Anatomy.Hands
    require BodyPart.RightHand.integrity > 0
    skill   Mining
    min_skill 1
}

capability Strike {
    require Anatomy.Arms
    skill   Combat
    min_skill 1
}
```

**Key design:** The `require BodyPart.X.integrity > 0` line is a **declarative
condition**, not an imperative check. The processor evaluates it every tick by
inspecting the body part entity's LayerStack. If the hand is destroyed, the
condition fails and the capability is effectively lost -- without any capability
flag ever being mutated.

**Compiles to:** A `CapabilityID` enum and a `CAPABILITY_DEFS[]` lookup table.

```c
static const CapabilityDef CAPABILITY_DEFS[CAP_COUNT] = {
    { ANAT_ARMS|ANAT_HANDS, SKILL_WOODCUTTING, 1, BODYPART_RIGHT_HAND },
    ...
};
```

---

### 4.9 `affordance` -- What Can Be Done to an Object

Declares an object-side permission. The processor checks this when an entity
is targeted by a verb.

```marble
affordance Choppable {
    require_cap Chop
    condition   tool_harder_than_layer
    on_success  damage_layer
    difficulty  40
    crit_fail_threshold 5
    crit_fail_bodypart  RightHand
    crit_fail_damage    1
}

affordance Mineable {
    require_cap Mine
    condition   tool_harder_than_layer
    on_success  damage_layer
    difficulty  55
    crit_fail_threshold 5
    crit_fail_bodypart  RightHand
    crit_fail_damage    1
}
```

**Compiles to:** An `AffordanceID` enum and an `AFFORDANCE_DEFS[]` lookup table.

---

### 4.10 `condition` -- Reusable Predicate Atoms

Declares a named, reusable condition that affordances and other blocks can
reference. Conditions are **not logic** -- they are predefined predicate atoms
that the C processor evaluates via a switch statement.

```marble
condition tool_harder_than_layer {
    -- Checks: actor's equipped tool material hardness >
    --         target's outermost layer material hardness
    check Actor.Tool.Material.Hardness > Target.Layer[0].Material.Hardness
}

condition target_has_integrity {
    -- Checks: target's outermost layer integrity > 0
    check Target.Layer[0].integrity > 0
}

condition is_injured {
    -- Checks: actor's health is below max
    check Actor.Health.hp < Actor.Health.max_hp
}
```

**Compiles to:** A `ConditionID` enum and cases in the `evaluate_condition()` switch.

**Important constraint:** The `check` line is a **schema declaration**, not executable
code. The compiler maps it to a predefined C evaluator function. You cannot express
arbitrary logic -- only reference known component paths and comparison operators.

---

### 4.11 `effect` -- State Change Results

Declares a named effect that applies when an interaction succeeds (or crit-fails).

```marble
effect damage_layer {
    -- Reduces outermost layer integrity by 1.
    -- If integrity reaches 0, peels the layer.
    apply Target.Layer[0].integrity -= 1
}

effect heal_hand {
    -- Restores 1 integrity to outermost hand layer.
    apply Actor.BodyPart.RightHand.Layer[0].integrity += 1
}
```

**Compiles to:** An `EffectID` enum and cases in the `apply_effect()` switch.

Same constraint as conditions: `apply` lines are schema, not code.

---

### 4.12 `verb` -- The Action Bridge

Declares the connection between a capability and an affordance. This is
the "interaction request" that actors submit to the processor.

```marble
verb Chop {
    actor_cap   Chop
    target_aff  Choppable
}

verb Mine {
    actor_cap   Mine
    target_aff  Mineable
}

verb Strike {
    actor_cap   Strike
    target_aff  Hittable
}
```

**Compiles to:** A `VerbID` enum and a `VERB_DEFS[]` lookup table.

---

### 4.13 `system` -- Tick-Driven Processing

Declares a named system that runs at a specified tick frequency.

```marble
system TickLog {
    frequency 1        -- every tick
}

system InteractionProcessor {
    frequency 2        -- every 2 ticks
    requires Capabilities, Affordances, Anatomy, Skills, Tool, BodyParts, Layers
}

system WorldStatus {
    frequency 3        -- every 3 ticks
}
```

**Compiles to:** A `SystemID` enum, a `SYSTEM_FREQ[]` lookup table, and cases
in the `dispatch_system()` switch.

**`frequency N`** means the system runs on every tick where `tick_number % N == 0`.

**`requires`** declares which component pools the system reads/writes. This is
informational in Phase 0.2 but will enable dependency-based scheduling and
parallelization in future phases.

---

### 4.14 `entity` -- World Population (Future)

Declares a named entity template for world initialization. Not yet implemented
in the runtime but specced here for forward compatibility.

```marble
entity Lumberjack {
    Health      { hp 100, max_hp 100 }
    Position    { x 5.0, y 3.0 }
    Anatomy     { Arms, Hands, Legs }
    Skills      { Woodcutting 60 }
    Capabilities { Chop }
    Tool        { material Iron }
    BodyParts   { RightHand -> @LumberjackHand }
}

entity LumberjackHand {
    Layers template HumanHand
}

entity OakTree {
    Position    { x 6.0, y 3.0 }
    Layers      template OakTree
    Affordances { Choppable }
}
```

**The `@` prefix** denotes an entity reference resolved at init time.

---

## 5. The Interaction Pipeline

This is the runtime evaluation order for every interaction request.
MarbleScript defines the *data*; this pipeline defines the *execution*.

```
Actor submits: (actor_eid, target_eid, verb_id)
         |
    1. Lookup VerbDef[verb_id]
         |  -> required_cap, required_aff
         |
    2. Actor has capability?
         |  Bitfield: actor.capabilities & (1 << required_cap)
         |  FAIL -> INTERACT_FAIL_NO_CAP
         |
    3a. Anatomy check
         |  actor.anatomy & cap_def.required_anatomy == required_anatomy
         |  FAIL -> INTERACT_FAIL_ANATOMY
         |
    3b. Body part integrity check (fine motor gate)
         |  actor.body_parts[cap_def.body_part].layers[0].integrity > 0
         |  FAIL -> INTERACT_FAIL_BODY_PART
         |
    3c. Skill level check
         |  actor.skills[cap_def.required_skill] >= cap_def.min_skill
         |  FAIL -> INTERACT_FAIL_SKILL_LOW
         |
    4. Target has affordance?
         |  Bitfield: target.affordances & (1 << required_aff)
         |  FAIL -> INTERACT_FAIL_NO_AFF
         |
    5. Evaluate condition
         |  Switch on aff_def.condition
         |  FAIL -> INTERACT_FAIL_CONDITION
         |
    6. Roll d100 (deterministic SplitMix32)
         |  seed = world_seed ^ tick ^ actor_id ^ target_id
         |
    6a. roll < crit_fail_threshold?
         |  YES -> apply crit damage to actor's body part
         |  RETURN INTERACT_CRIT_FAIL
         |
    6b. roll < (difficulty - skill_level)?
         |  YES -> RETURN INTERACT_FAIL_ROLL
         |
    7. Apply effect on target
         |  Switch on aff_def.on_success
         |
    RETURN INTERACT_SUCCESS
```

---

## 6. Constraint Checklist

| Constraint          | Enforcement                                                    |
|---------------------|----------------------------------------------------------------|
| No `malloc`/`free`  | All memory is static sparse sets sized by `world.max_entities` |
| No function pointers| All dispatch via `switch` on enum opcodes                      |
| No recursion        | All loops bounded by `max_entities` or `max_layers`            |
| No runtime strings  | All identifiers are integer enums/hashes                       |
| No polymorphism     | Entity behavior = which components are present                 |
| Deterministic       | SplitMix32 PRNG seeded per-interaction, no platform dependency |
| Bounded tick catch-up| Max 3 ticks per frame to prevent spiral-of-death              |

---

## 7. Complete Example: Lumberjack Scenario

```marble
-- MarbleScript: Oak Forest Demo
-- File: oak_forest.marble

world "Oak Forest Demo" {
    max_entities     1024
    tick_interval_ms 600
    seed             42
    max_layers       4
    max_body_parts   6
    max_skills       8
}

-- Materials
material Wood  { hardness 30 }
material Iron  { hardness 80 }
material Flesh { hardness 10 }
material Bark  { hardness 25 }
material Bone  { hardness 40 }

-- Layer templates
layer OakTree   { Bark integrity 3, Wood integrity 10 }
layer HumanHand { Flesh integrity 1, Bone integrity 1 }

-- Skills
skill Woodcutting

-- Anatomy
anatomy Arms
anatomy Hands
anatomy Legs

-- Body parts
bodypart RightHand

-- Capabilities
capability Chop {
    require Anatomy.Arms
    require Anatomy.Hands
    require BodyPart.RightHand.integrity > 0
    skill   Woodcutting
    min_skill 1
}

-- Conditions
condition tool_harder_than_layer {
    check Actor.Tool.Material.Hardness > Target.Layer[0].Material.Hardness
}

-- Effects
effect damage_layer {
    apply Target.Layer[0].integrity -= 1
}

-- Affordances
affordance Choppable {
    require_cap         Chop
    condition           tool_harder_than_layer
    on_success          damage_layer
    difficulty          40
    crit_fail_threshold 15
    crit_fail_bodypart  RightHand
    crit_fail_damage    2
}

-- Verbs
verb Chop {
    actor_cap  Chop
    target_aff Choppable
}

-- Systems
system TickLog {
    frequency 1
}

system InteractionProcessor {
    frequency 2
    requires Capabilities, Affordances, Anatomy, Skills, Tool, BodyParts, Layers
}

system WorldStatus {
    frequency 3
}

-- Entities
entity Lumberjack {
    Health       { hp 100, max_hp 100 }
    Position     { x 5.0, y 3.0 }
    Anatomy      { Arms, Hands, Legs }
    Skills       { Woodcutting 60 }
    Capabilities { Chop }
    Tool         { material Iron }
    BodyParts    { RightHand -> @LumberjackHand }
}

entity LumberjackHand {
    Layers template HumanHand
}

entity OakTree {
    Position     { x 6.0, y 3.0 }
    Layers       template OakTree
    Affordances  { Choppable }
}
```

---

## 8. Keyword Summary

| Keyword       | Category      | Purpose                                        |
|---------------|---------------|------------------------------------------------|
| `world`       | Configuration | Engine limits, tick rate, PRNG seed             |
| `material`    | Data          | Physical constants (hardness, etc.)             |
| `component`   | Data          | Struct definition attached via sparse set       |
| `layer`       | Data          | Physical structure template (outside-in)        |
| `skill`       | Data          | Named skill identifier for d100 rolls           |
| `anatomy`     | Data          | Body structure flag (bitfield)                  |
| `bodypart`    | Data          | Named body part slot (sub-entity reference)     |
| `capability`  | Interaction   | Actor-side permission + prerequisites           |
| `affordance`  | Interaction   | Object-side permission + condition/effect       |
| `condition`   | Interaction   | Reusable predicate atom                         |
| `effect`      | Interaction   | State change result                             |
| `verb`        | Interaction   | Bridges capability to affordance                |
| `system`      | Execution     | Tick-driven processing with frequency           |
| `entity`      | World         | Entity template for initialization              |

### Field Keywords (used inside blocks)

| Keyword           | Used In        | Purpose                                  |
|-------------------|----------------|------------------------------------------|
| `require`         | capability     | Anatomy/body part prerequisite           |
| `skill`           | capability     | Which skill is rolled                    |
| `min_skill`       | capability     | Minimum skill level to attempt           |
| `require_cap`     | affordance     | Which capability the actor needs         |
| `condition`       | affordance     | Pre-roll check                           |
| `on_success`      | affordance     | Effect to apply on success               |
| `difficulty`      | affordance     | d100 base threshold                      |
| `crit_fail_*`     | affordance     | Critical failure parameters              |
| `frequency`       | system         | Run every N ticks                        |
| `requires`        | system         | Component pool dependencies              |
| `check`           | condition      | Predicate expression (schema, not code)  |
| `apply`           | effect         | State mutation expression (schema)       |
| `integrity`       | layer          | Starting durability value                |
| `hardness`        | material       | Material hardness constant               |
| `template`        | entity/layers  | Reference to a layer template            |

---

## 9. What MarbleScript Does NOT Do

- **No logic branching.** No `if`, `else`, `while`, `for`. The C processor handles all logic.
- **No arithmetic expressions.** `check` and `apply` lines use predefined comparison
  and mutation patterns, not arbitrary math.
- **No function definitions.** Systems are declared but implemented in C.
- **No string manipulation.** All identifiers become integer enums at compile time.
- **No dynamic allocation.** All sizes are fixed at compile time from the `world` block.
- **No imports or modules.** One `.marble` file per project (multi-file is Phase 2+).

---

## 10. Versioning

This is **MarbleScript v0.1**. The spec will evolve as the runtime proves out
new features. Breaking changes are expected and will be documented.

| Version | Milestone                                          |
|---------|----------------------------------------------------|
| v0.1    | Core keywords, interaction pipeline, single file   |
| v0.2    | `condition` expression grammar, multi-file imports |
| v0.3    | Event queue declarations, priority scheduling      |
| v1.0    | Stable spec, backwards compatibility guarantee     |