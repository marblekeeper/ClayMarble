/*
 * marble_net.h -- MarbleEngine Network Protocol
 *
 * NASA Power of 10 Compliant:
 *   Rule 2:  Fixed loop bounds (MAX_CMD_QUEUE, MAX_SNAPSHOT_ENTITIES)
 *   Rule 3:  All memory statically allocated at compile time
 *   Rule 6:  All data declared at smallest scope
 *   Rule 9:  Minimal pointer use; struct arrays, not pointer graphs
 *   Rule 10: All code paths compilable with all warnings enabled
 *
 * ARCHITECTURE:
 *   Lua (client) --[InteractionCommand]--> C (authority)
 *   C (authority) --[Snapshot]--> Lua (read-only view)
 *
 *   Lua NEVER owns game state. It submits intent packets.
 *   C validates, applies to ECS, and emits a snapshot each tick.
 *
 * PACKET FORMAT:
 *   All packets are fixed-size (16 bytes for commands).
 *   This allows the command queue to be a flat array of structs.
 *
 * BUILD:
 *   Header-only. Include once with MARBLE_NET_IMPLEMENTATION defined.
 */

#ifndef MARBLE_NET_H
#define MARBLE_NET_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * COMPILE-TIME LIMITS (NASA Rule 3: no dynamic allocation)
 * ========================================================================= */

#define NET_MAX_CMD_QUEUE        128   /* Max pending commands per tick     */
#define NET_MAX_SNAPSHOT_ENTS    256   /* Max entities in a snapshot        */
#define NET_CMD_SIZE              16   /* Fixed packet size in bytes        */
#define NET_PROTOCOL_VERSION       1   /* Bump on breaking changes          */
#define NET_TICK_INTERVAL_MS     600   /* 0.6 seconds per tick              */

/* =========================================================================
 * OPCODE MAP (uint8_t: 256 unique interaction types)
 *
 * 0x00-0x0F : Movement
 * 0x10-0x2F : Combat
 * 0x30-0x4F : Inventory
 * 0x50-0x6F : Environment
 * 0xF0-0xFF : System / Meta
 * ========================================================================= */

typedef enum {
    /* Movement (0x00 - 0x0F) */
    OP_MOVE_NORTH       = 0x00,
    OP_MOVE_SOUTH       = 0x01,
    OP_MOVE_EAST        = 0x02,
    OP_MOVE_WEST        = 0x03,
    OP_MOVE_NE          = 0x04,
    OP_MOVE_NW          = 0x05,
    OP_MOVE_SE          = 0x06,
    OP_MOVE_SW          = 0x07,
    OP_ASCEND           = 0x08,
    OP_DESCEND          = 0x09,
    OP_TELEPORT         = 0x0A,

    /* Combat (0x10 - 0x2F) */
    OP_MELEE_ATTACK     = 0x10,
    OP_RANGED_ATTACK    = 0x11,
    OP_DEFEND           = 0x12,
    OP_USE_SKILL        = 0x13,
    OP_USE_MEDKIT       = 0x14,

    /* Inventory (0x30 - 0x4F) */
    OP_PICK_UP          = 0x30,
    OP_DROP             = 0x31,
    OP_EQUIP            = 0x32,
    OP_CONSUME          = 0x33,
    OP_USE_ITEM         = 0x34,

    /* Environment (0x50 - 0x6F) */
    OP_INTERACT_DOOR    = 0x50,
    OP_SEARCH           = 0x51,
    OP_DISARM_TRAP      = 0x52,
    OP_ACTIVATE         = 0x53,  /* Generic: lever, terminal, etc. */

    /* System (0xF0 - 0xFF) */
    OP_HEARTBEAT        = 0xF0,
    OP_LOGIN            = 0xF1,
    OP_LOGOUT           = 0xF2,
    OP_ARENA_CHALLENGE  = 0xF3,
    OP_ARENA_ACCEPT     = 0xF4,
    OP_ARENA_DECLINE    = 0xF5,
    OP_SYNC_REQUEST     = 0xFE,
    OP_NOP              = 0xFF,
} OpCode;

/* Human-readable names for debugging (indexed by opcode) */
static const char* OPCODE_NAMES[256];

/* =========================================================================
 * INTERACTION COMMAND (Client -> Server)
 *
 * Fixed 16 bytes. No pointers. No heap.
 * This is what Lua submits as an "intent."
 * ========================================================================= */

typedef struct {
    uint32_t entity_id;    /* Who is acting (4 bytes)                    */
    uint8_t  opcode;       /* What are they doing (1 byte, 1 of 256)    */
    uint8_t  param1;       /* Direction, inventory slot, skill ID, etc.  */
    uint16_t target_x;     /* Grid X coordinate (for positional ops)     */
    uint16_t target_y;     /* Grid Y coordinate                          */
    uint32_t target_id;    /* Target entity ID (for 1v1 / item target)   */
    uint16_t sequence;     /* Client sequence number (for ack/ordering)  */
} InteractionCommand;

/* Static assert: command must be exactly 16 bytes.
 * Packing may differ per platform, so we serialize manually. */
/* _Static_assert(sizeof(InteractionCommand) == 16, "Command size mismatch"); */

/* =========================================================================
 * COMMAND QUEUE (Static ring buffer)
 *
 * Commands are pushed by the input layer (Lua bridge or network recv).
 * Drained by the simulation tick. Excess commands are dropped (not OOM).
 * ========================================================================= */

typedef struct {
    InteractionCommand commands[NET_MAX_CMD_QUEUE];
    uint32_t head;          /* Next slot to read from     */
    uint32_t tail;          /* Next slot to write to      */
    uint32_t count;         /* Current number of commands  */
    uint32_t dropped;       /* Commands dropped (overflow) */
    uint32_t processed;     /* Lifetime commands processed */
    uint16_t next_sequence; /* Sequence counter            */
} CommandQueue;

/* =========================================================================
 * SNAPSHOT ENTITY (Server -> Client, read-only view)
 *
 * Minimal per-entity data that Lua needs for rendering.
 * The C authority packs this each tick; Lua reads it.
 * ========================================================================= */

typedef struct {
    uint32_t entity_id;
    uint16_t x;
    uint16_t y;
    uint8_t  glyph;        /* ASCII representation for fallback render */
    uint8_t  entity_type;  /* 0=player, 1=enemy, 2=item, 3=prop, etc. */
    int16_t  hp;
    int16_t  max_hp;
    uint8_t  flags;        /* Bitfield: alive, visible, etc.           */
    uint8_t  sprite_id;    /* Index into sprite table for Lua          */
} SnapshotEntity;

typedef struct {
    SnapshotEntity entities[NET_MAX_SNAPSHOT_ENTS];
    uint32_t       entity_count;
    uint32_t       tick_number;
    uint16_t       last_ack_sequence;  /* Last command the server processed */
    uint8_t        protocol_version;
    uint8_t        _pad;
} Snapshot;

/* =========================================================================
 * VALIDATION RESULT
 * ========================================================================= */

typedef enum {
    VALIDATE_OK               = 0,
    VALIDATE_FAIL_UNKNOWN_OP  = 1,
    VALIDATE_FAIL_BAD_ENTITY  = 2,
    VALIDATE_FAIL_OUT_OF_TURN = 3,
    VALIDATE_FAIL_BLOCKED     = 4,  /* e.g., wall in the way            */
    VALIDATE_FAIL_NO_TARGET   = 5,
    VALIDATE_FAIL_DEAD        = 6,
    VALIDATE_FAIL_COOLDOWN    = 7,
    VALIDATE_RESULT_COUNT
} ValidateResult;

static const char* VALIDATE_RESULT_NAMES[VALIDATE_RESULT_COUNT];

/* =========================================================================
 * SERIALIZATION
 *
 * Explicit byte packing for cross-platform determinism.
 * No reliance on struct layout or compiler padding.
 * ========================================================================= */

static inline void net_pack_command(const InteractionCommand* cmd, uint8_t out[NET_CMD_SIZE]) {
    out[0]  = (uint8_t)(cmd->entity_id & 0xFF);
    out[1]  = (uint8_t)((cmd->entity_id >> 8) & 0xFF);
    out[2]  = (uint8_t)((cmd->entity_id >> 16) & 0xFF);
    out[3]  = (uint8_t)((cmd->entity_id >> 24) & 0xFF);
    out[4]  = cmd->opcode;
    out[5]  = cmd->param1;
    out[6]  = (uint8_t)(cmd->target_x & 0xFF);
    out[7]  = (uint8_t)((cmd->target_x >> 8) & 0xFF);
    out[8]  = (uint8_t)(cmd->target_y & 0xFF);
    out[9]  = (uint8_t)((cmd->target_y >> 8) & 0xFF);
    out[10] = (uint8_t)(cmd->target_id & 0xFF);
    out[11] = (uint8_t)((cmd->target_id >> 8) & 0xFF);
    out[12] = (uint8_t)((cmd->target_id >> 16) & 0xFF);
    out[13] = (uint8_t)((cmd->target_id >> 24) & 0xFF);
    out[14] = (uint8_t)(cmd->sequence & 0xFF);
    out[15] = (uint8_t)((cmd->sequence >> 8) & 0xFF);
}

static inline void net_unpack_command(const uint8_t in[NET_CMD_SIZE], InteractionCommand* cmd) {
    cmd->entity_id = (uint32_t)in[0]
                   | ((uint32_t)in[1] << 8)
                   | ((uint32_t)in[2] << 16)
                   | ((uint32_t)in[3] << 24);
    cmd->opcode    = in[4];
    cmd->param1    = in[5];
    cmd->target_x  = (uint16_t)in[6] | ((uint16_t)in[7] << 8);
    cmd->target_y  = (uint16_t)in[8] | ((uint16_t)in[9] << 8);
    cmd->target_id = (uint32_t)in[10]
                   | ((uint32_t)in[11] << 8)
                   | ((uint32_t)in[12] << 16)
                   | ((uint32_t)in[13] << 24);
    cmd->sequence  = (uint16_t)in[14] | ((uint16_t)in[15] << 8);
}

/* =========================================================================
 * QUEUE OPERATIONS
 * ========================================================================= */

static inline void net_queue_init(CommandQueue* q) {
    memset(q, 0, sizeof(CommandQueue));
}

/* Push a command. Returns 0 on success, -1 if queue is full (dropped). */
static inline int net_queue_push(CommandQueue* q, const InteractionCommand* cmd) {
    if (q->count >= NET_MAX_CMD_QUEUE) {
        q->dropped++;
        return -1;
    }
    q->commands[q->tail] = *cmd;
    q->commands[q->tail].sequence = q->next_sequence++;
    q->tail = (q->tail + 1) % NET_MAX_CMD_QUEUE;
    q->count++;
    return 0;
}

/* Pop the next command. Returns 0 on success, -1 if empty. */
static inline int net_queue_pop(CommandQueue* q, InteractionCommand* out) {
    if (q->count == 0) return -1;
    *out = q->commands[q->head];
    q->head = (q->head + 1) % NET_MAX_CMD_QUEUE;
    q->count--;
    q->processed++;
    return 0;
}

/* Peek at head without consuming. Returns NULL if empty. */
static inline const InteractionCommand* net_queue_peek(const CommandQueue* q) {
    if (q->count == 0) return NULL;
    return &q->commands[q->head];
}

/* Drain all commands (reset to empty). */
static inline void net_queue_flush(CommandQueue* q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

/* =========================================================================
 * CONVENIENCE: Create movement commands
 * ========================================================================= */

static inline InteractionCommand net_cmd_move(uint32_t entity_id, OpCode direction) {
    InteractionCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.entity_id = entity_id;
    cmd.opcode    = (uint8_t)direction;
    return cmd;
}

static inline InteractionCommand net_cmd_melee(uint32_t entity_id, uint32_t target_id) {
    InteractionCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.entity_id = entity_id;
    cmd.opcode    = OP_MELEE_ATTACK;
    cmd.target_id = target_id;
    return cmd;
}

static inline InteractionCommand net_cmd_use_item(uint32_t entity_id, uint8_t slot) {
    InteractionCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.entity_id = entity_id;
    cmd.opcode    = OP_USE_ITEM;
    cmd.param1    = slot;
    return cmd;
}

static inline InteractionCommand net_cmd_heartbeat(uint32_t entity_id) {
    InteractionCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.entity_id = entity_id;
    cmd.opcode    = OP_HEARTBEAT;
    return cmd;
}

/* =========================================================================
 * OPCODE CLASSIFICATION
 * ========================================================================= */

static inline int net_opcode_is_movement(uint8_t op) {
    return op <= 0x0F;
}

static inline int net_opcode_is_combat(uint8_t op) {
    return op >= 0x10 && op <= 0x2F;
}

static inline int net_opcode_is_inventory(uint8_t op) {
    return op >= 0x30 && op <= 0x4F;
}

static inline int net_opcode_is_environment(uint8_t op) {
    return op >= 0x50 && op <= 0x6F;
}

static inline int net_opcode_is_system(uint8_t op) {
    return op >= 0xF0;
}

/* Direction deltas for movement opcodes */
static inline void net_move_delta(uint8_t opcode, int* dx, int* dy) {
    *dx = 0; *dy = 0;
    switch (opcode) {
        case OP_MOVE_NORTH: *dy = -1; break;
        case OP_MOVE_SOUTH: *dy =  1; break;
        case OP_MOVE_EAST:  *dx =  1; break;
        case OP_MOVE_WEST:  *dx = -1; break;
        case OP_MOVE_NE:    *dx =  1; *dy = -1; break;
        case OP_MOVE_NW:    *dx = -1; *dy = -1; break;
        case OP_MOVE_SE:    *dx =  1; *dy =  1; break;
        case OP_MOVE_SW:    *dx = -1; *dy =  1; break;
        default: break;
    }
}

/* =========================================================================
 * SNAPSHOT HELPERS
 * ========================================================================= */

static inline void net_snapshot_init(Snapshot* s) {
    memset(s, 0, sizeof(Snapshot));
    s->protocol_version = NET_PROTOCOL_VERSION;
}

static inline int net_snapshot_add_entity(Snapshot* s, const SnapshotEntity* ent) {
    if (s->entity_count >= NET_MAX_SNAPSHOT_ENTS) return -1;
    s->entities[s->entity_count++] = *ent;
    return 0;
}

/* =========================================================================
 * SIMPLE TILE MAP FOR VALIDATION (static, fixed bounds)
 * ========================================================================= */

#define NET_MAP_W  30
#define NET_MAP_H  22

typedef struct {
    uint8_t tiles[NET_MAP_H][NET_MAP_W]; /* 0 = floor, 1 = wall */
} TileMap;

static inline void net_map_init(TileMap* m) {
    memset(m, 1, sizeof(TileMap)); /* All walls */
}

static inline int net_map_walkable(const TileMap* m, int x, int y) {
    if (x < 0 || x >= NET_MAP_W || y < 0 || y >= NET_MAP_H) return 0;
    return m->tiles[y][x] == 0;
}

/* =========================================================================
 * TICK-LEVEL COMMAND PROCESSOR
 *
 * Drains the queue, validates each command, applies valid ones.
 * Returns number of commands applied this tick.
 * ========================================================================= */

typedef struct {
    uint32_t entity_id;
    uint16_t x, y;
    int16_t  hp, max_hp;
    uint8_t  alive;
    uint8_t  glyph;
} NetEntity;

#define NET_MAX_ENTITIES 64

typedef struct {
    NetEntity  entities[NET_MAX_ENTITIES];
    uint32_t   entity_count;
    TileMap    map;
    uint32_t   tick;
    uint32_t   cmds_applied;
    uint32_t   cmds_rejected;
} NetWorld;

static inline void net_world_init(NetWorld* w) {
    memset(w, 0, sizeof(NetWorld));
}

static inline NetEntity* net_world_find_entity(NetWorld* w, uint32_t id) {
    uint32_t i;
    for (i = 0; i < w->entity_count; i++) {
        if (w->entities[i].entity_id == id) return &w->entities[i];
    }
    return NULL;
}

static inline int net_world_add_entity(NetWorld* w, uint32_t id, uint16_t x, uint16_t y,
                                        int16_t hp, int16_t max_hp, uint8_t glyph) {
    if (w->entity_count >= NET_MAX_ENTITIES) return -1;
    NetEntity* e = &w->entities[w->entity_count++];
    e->entity_id = id;
    e->x = x; e->y = y;
    e->hp = hp; e->max_hp = max_hp;
    e->alive = 1;
    e->glyph = glyph;
    return 0;
}

/* Validate + apply a single command against the world state.
 * Returns VALIDATE_OK if applied, or a failure code. */
static inline ValidateResult net_process_command(NetWorld* w, const InteractionCommand* cmd) {
    NetEntity* actor = net_world_find_entity(w, cmd->entity_id);
    if (!actor) return VALIDATE_FAIL_BAD_ENTITY;
    if (!actor->alive) return VALIDATE_FAIL_DEAD;

    if (net_opcode_is_movement(cmd->opcode)) {
        int dx, dy;
        int nx, ny;
        net_move_delta(cmd->opcode, &dx, &dy);
        nx = (int)actor->x + dx;
        ny = (int)actor->y + dy;

        if (!net_map_walkable(&w->map, nx, ny)) {
            return VALIDATE_FAIL_BLOCKED;
        }

        /* Check for entity collision (bump-to-attack becomes melee) */
        /* For now, just move */
        actor->x = (uint16_t)nx;
        actor->y = (uint16_t)ny;
        w->cmds_applied++;
        return VALIDATE_OK;
    }

    if (cmd->opcode == OP_MELEE_ATTACK) {
        NetEntity* target = net_world_find_entity(w, cmd->target_id);
        if (!target) return VALIDATE_FAIL_NO_TARGET;
        if (!target->alive) return VALIDATE_FAIL_NO_TARGET;
        /* Simple damage for demo */
        target->hp -= 5;
        if (target->hp <= 0) {
            target->hp = 0;
            target->alive = 0;
        }
        w->cmds_applied++;
        return VALIDATE_OK;
    }

    if (cmd->opcode == OP_HEARTBEAT || cmd->opcode == OP_NOP) {
        w->cmds_applied++;
        return VALIDATE_OK;
    }

    return VALIDATE_FAIL_UNKNOWN_OP;
}

/* Process all queued commands for this tick. Returns count applied. */
static inline uint32_t net_tick(NetWorld* w, CommandQueue* q) {
    uint32_t applied = 0;
    InteractionCommand cmd;
    uint32_t safety = 0;

    while (net_queue_pop(q, &cmd) == 0 && safety < NET_MAX_CMD_QUEUE) {
        ValidateResult vr = net_process_command(w, &cmd);
        if (vr == VALIDATE_OK) {
            applied++;
        } else {
            w->cmds_rejected++;
        }
        safety++;
    }
    w->tick++;
    return applied;
}

/* Build a snapshot from current world state */
static inline void net_build_snapshot(const NetWorld* w, Snapshot* s) {
    uint32_t i;
    net_snapshot_init(s);
    s->tick_number = w->tick;

    for (i = 0; i < w->entity_count && i < NET_MAX_SNAPSHOT_ENTS; i++) {
        const NetEntity* e = &w->entities[i];
        SnapshotEntity se;
        memset(&se, 0, sizeof(se));
        se.entity_id   = e->entity_id;
        se.x           = e->x;
        se.y           = e->y;
        se.hp          = e->hp;
        se.max_hp      = e->max_hp;
        se.glyph       = e->glyph;
        se.entity_type = (i == 0) ? 0 : 1; /* first entity = player */
        se.flags       = e->alive ? 0x01 : 0x00;
        net_snapshot_add_entity(s, &se);
    }
}

/* =========================================================================
 * STRING TABLES (implementation in guarded block)
 * ========================================================================= */

#ifdef MARBLE_NET_IMPLEMENTATION

static const char* OPCODE_NAMES[256] = {
    [OP_MOVE_NORTH]     = "MOVE_NORTH",
    [OP_MOVE_SOUTH]     = "MOVE_SOUTH",
    [OP_MOVE_EAST]      = "MOVE_EAST",
    [OP_MOVE_WEST]      = "MOVE_WEST",
    [OP_MOVE_NE]        = "MOVE_NE",
    [OP_MOVE_NW]        = "MOVE_NW",
    [OP_MOVE_SE]        = "MOVE_SE",
    [OP_MOVE_SW]        = "MOVE_SW",
    [OP_ASCEND]         = "ASCEND",
    [OP_DESCEND]        = "DESCEND",
    [OP_TELEPORT]       = "TELEPORT",
    [OP_MELEE_ATTACK]   = "MELEE_ATTACK",
    [OP_RANGED_ATTACK]  = "RANGED_ATTACK",
    [OP_DEFEND]         = "DEFEND",
    [OP_USE_SKILL]      = "USE_SKILL",
    [OP_USE_MEDKIT]     = "USE_MEDKIT",
    [OP_PICK_UP]        = "PICK_UP",
    [OP_DROP]           = "DROP",
    [OP_EQUIP]          = "EQUIP",
    [OP_CONSUME]        = "CONSUME",
    [OP_USE_ITEM]       = "USE_ITEM",
    [OP_INTERACT_DOOR]  = "INTERACT_DOOR",
    [OP_SEARCH]         = "SEARCH",
    [OP_DISARM_TRAP]    = "DISARM_TRAP",
    [OP_ACTIVATE]       = "ACTIVATE",
    [OP_HEARTBEAT]      = "HEARTBEAT",
    [OP_LOGIN]          = "LOGIN",
    [OP_LOGOUT]         = "LOGOUT",
    [OP_ARENA_CHALLENGE]= "ARENA_CHALLENGE",
    [OP_ARENA_ACCEPT]   = "ARENA_ACCEPT",
    [OP_ARENA_DECLINE]  = "ARENA_DECLINE",
    [OP_SYNC_REQUEST]   = "SYNC_REQUEST",
    [OP_NOP]            = "NOP",
};

static const char* VALIDATE_RESULT_NAMES[VALIDATE_RESULT_COUNT] = {
    "OK",
    "FAIL_UNKNOWN_OP",
    "FAIL_BAD_ENTITY",
    "FAIL_OUT_OF_TURN",
    "FAIL_BLOCKED",
    "FAIL_NO_TARGET",
    "FAIL_DEAD",
    "FAIL_COOLDOWN",
};

#endif /* MARBLE_NET_IMPLEMENTATION */

#endif /* MARBLE_NET_H */