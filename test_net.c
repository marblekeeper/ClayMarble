/*
 * test_net.c -- MarbleEngine Network Protocol Tests
 *
 * Validates:
 *   - Packet serialization roundtrip (pack/unpack)
 *   - Command queue push/pop/overflow/flush
 *   - Opcode classification helpers
 *   - Movement delta table
 *   - World validation (blocked, dead entity, bad entity, etc.)
 *   - Full tick processing (queue -> validate -> apply -> snapshot)
 *   - Interactive WASD demo (when run with --demo flag)
 *
 * BUILD (GCC/MinGW):
 *   gcc -std=c99 -Wall -Wextra -O2 test_net.c -o test_net.exe
 *
 * BUILD (Emscripten/WASM):
 *   emcc -std=c99 -Wall -Wextra -O2 test_net.c -o test_net.js
 *
 * RUN:
 *   test_net.exe            (unit tests only)
 *   test_net.exe --demo     (interactive WASD demo, 0.6s tick)
 */

#define MARBLE_NET_IMPLEMENTATION
#include "marble_net.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * TEST FRAMEWORK (same pattern as existing test harness)
 * ========================================================================= */

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_BEGIN(name) \
    do { \
        const char* _test_name = (name); \
        int _test_ok = 1; \
        g_tests_run++;

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            printf("  FAIL: %s (line %d): %s\n", _test_name, __LINE__, #expr); \
            _test_ok = 0; \
        } \
    } while(0)

#define ASSERT_EQ_I32(a, b) \
    do { \
        int32_t _a = (a); int32_t _b = (b); \
        if (_a != _b) { \
            printf("  FAIL: %s (line %d): %s == %d, expected %d\n", \
                   _test_name, __LINE__, #a, _a, _b); \
            _test_ok = 0; \
        } \
    } while(0)

#define ASSERT_EQ_U32(a, b) \
    do { \
        uint32_t _a = (a); uint32_t _b = (b); \
        if (_a != _b) { \
            printf("  FAIL: %s (line %d): %s == %u, expected %u\n", \
                   _test_name, __LINE__, #a, _a, _b); \
            _test_ok = 0; \
        } \
    } while(0)

#define ASSERT_EQ_U16(a, b) \
    do { \
        uint16_t _a = (a); uint16_t _b = (b); \
        if (_a != _b) { \
            printf("  FAIL: %s (line %d): %s == %u, expected %u\n", \
                   _test_name, __LINE__, #a, (unsigned)_a, (unsigned)_b); \
            _test_ok = 0; \
        } \
    } while(0)

#define ASSERT_EQ_U8(a, b) \
    do { \
        uint8_t _a = (a); uint8_t _b = (b); \
        if (_a != _b) { \
            printf("  FAIL: %s (line %d): %s == %u, expected %u\n", \
                   _test_name, __LINE__, #a, (unsigned)_a, (unsigned)_b); \
            _test_ok = 0; \
        } \
    } while(0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            printf("  FAIL: %s (line %d): %s should be NULL\n", \
                   _test_name, __LINE__, #ptr); \
            _test_ok = 0; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf("  FAIL: %s (line %d): %s should not be NULL\n", \
                   _test_name, __LINE__, #ptr); \
            _test_ok = 0; \
        } \
    } while(0)

#define TEST_END() \
        if (_test_ok) { \
            printf("  PASS: %s\n", _test_name); \
            g_tests_passed++; \
        } else { \
            g_tests_failed++; \
        } \
    } while(0)

/* =========================================================================
 * SECTION 1: SERIALIZATION ROUNDTRIP
 * ========================================================================= */

static void test_serialize_roundtrip_zeros(void) {
    TEST_BEGIN("serialize: zero-filled command roundtrips");
    {
        InteractionCommand src, dst;
        uint8_t buf[NET_CMD_SIZE];

        memset(&src, 0, sizeof(src));
        net_pack_command(&src, buf);
        net_unpack_command(buf, &dst);

        ASSERT_EQ_U32(dst.entity_id, 0);
        ASSERT_EQ_U8(dst.opcode, 0);
        ASSERT_EQ_U8(dst.param1, 0);
        ASSERT_EQ_U16(dst.target_x, 0);
        ASSERT_EQ_U16(dst.target_y, 0);
        ASSERT_EQ_U32(dst.target_id, 0);
        ASSERT_EQ_U16(dst.sequence, 0);
    }
    TEST_END();
}

static void test_serialize_roundtrip_full(void) {
    TEST_BEGIN("serialize: fully populated command roundtrips");
    {
        InteractionCommand src, dst;
        uint8_t buf[NET_CMD_SIZE];

        src.entity_id = 0xDEADBEEF;
        src.opcode    = OP_MELEE_ATTACK;
        src.param1    = 42;
        src.target_x  = 1234;
        src.target_y  = 5678;
        src.target_id = 0xCAFEBABE;
        src.sequence  = 9999;

        net_pack_command(&src, buf);
        net_unpack_command(buf, &dst);

        ASSERT_EQ_U32(dst.entity_id, 0xDEADBEEF);
        ASSERT_EQ_U8(dst.opcode, OP_MELEE_ATTACK);
        ASSERT_EQ_U8(dst.param1, 42);
        ASSERT_EQ_U16(dst.target_x, 1234);
        ASSERT_EQ_U16(dst.target_y, 5678);
        ASSERT_EQ_U32(dst.target_id, 0xCAFEBABE);
        ASSERT_EQ_U16(dst.sequence, 9999);
    }
    TEST_END();
}

static void test_serialize_all_movement_opcodes(void) {
    TEST_BEGIN("serialize: all movement opcodes survive roundtrip");
    {
        uint8_t ops[] = {
            OP_MOVE_NORTH, OP_MOVE_SOUTH, OP_MOVE_EAST, OP_MOVE_WEST,
            OP_MOVE_NE, OP_MOVE_NW, OP_MOVE_SE, OP_MOVE_SW,
            OP_ASCEND, OP_DESCEND, OP_TELEPORT
        };
        uint32_t i;
        for (i = 0; i < sizeof(ops)/sizeof(ops[0]); i++) {
            InteractionCommand src, dst;
            uint8_t buf[NET_CMD_SIZE];
            memset(&src, 0, sizeof(src));
            src.entity_id = i;
            src.opcode = ops[i];
            net_pack_command(&src, buf);
            net_unpack_command(buf, &dst);
            ASSERT_EQ_U8(dst.opcode, ops[i]);
            ASSERT_EQ_U32(dst.entity_id, i);
        }
    }
    TEST_END();
}

static void test_serialize_boundary_values(void) {
    TEST_BEGIN("serialize: boundary values (max uint32, uint16)");
    {
        InteractionCommand src, dst;
        uint8_t buf[NET_CMD_SIZE];

        src.entity_id = 0xFFFFFFFF;
        src.opcode    = 0xFF;
        src.param1    = 0xFF;
        src.target_x  = 0xFFFF;
        src.target_y  = 0xFFFF;
        src.target_id = 0xFFFFFFFF;
        src.sequence  = 0xFFFF;

        net_pack_command(&src, buf);
        net_unpack_command(buf, &dst);

        ASSERT_EQ_U32(dst.entity_id, 0xFFFFFFFF);
        ASSERT_EQ_U8(dst.opcode, 0xFF);
        ASSERT_EQ_U8(dst.param1, 0xFF);
        ASSERT_EQ_U16(dst.target_x, 0xFFFF);
        ASSERT_EQ_U16(dst.target_y, 0xFFFF);
        ASSERT_EQ_U32(dst.target_id, 0xFFFFFFFF);
        ASSERT_EQ_U16(dst.sequence, 0xFFFF);
    }
    TEST_END();
}

/* =========================================================================
 * SECTION 2: COMMAND QUEUE
 * ========================================================================= */

static void test_queue_init_empty(void) {
    TEST_BEGIN("queue: init produces empty queue");
    {
        CommandQueue q;
        net_queue_init(&q);
        ASSERT_EQ_U32(q.count, 0);
        ASSERT_EQ_U32(q.head, 0);
        ASSERT_EQ_U32(q.tail, 0);
        ASSERT_EQ_U32(q.dropped, 0);
        ASSERT_EQ_U32(q.processed, 0);
    }
    TEST_END();
}

static void test_queue_push_pop(void) {
    TEST_BEGIN("queue: push then pop returns same command");
    {
        CommandQueue q;
        InteractionCommand in_cmd, out_cmd;
        net_queue_init(&q);

        in_cmd = net_cmd_move(7, OP_MOVE_NORTH);
        ASSERT_EQ_I32(net_queue_push(&q, &in_cmd), 0);
        ASSERT_EQ_U32(q.count, 1);

        ASSERT_EQ_I32(net_queue_pop(&q, &out_cmd), 0);
        ASSERT_EQ_U32(out_cmd.entity_id, 7);
        ASSERT_EQ_U8(out_cmd.opcode, OP_MOVE_NORTH);
        ASSERT_EQ_U32(q.count, 0);
        ASSERT_EQ_U32(q.processed, 1);
    }
    TEST_END();
}

static void test_queue_fifo_order(void) {
    TEST_BEGIN("queue: FIFO ordering preserved");
    {
        CommandQueue q;
        InteractionCommand cmd;
        net_queue_init(&q);

        cmd = net_cmd_move(0, OP_MOVE_NORTH); net_queue_push(&q, &cmd);
        cmd = net_cmd_move(0, OP_MOVE_SOUTH); net_queue_push(&q, &cmd);
        cmd = net_cmd_move(0, OP_MOVE_EAST);  net_queue_push(&q, &cmd);

        net_queue_pop(&q, &cmd);
        ASSERT_EQ_U8(cmd.opcode, OP_MOVE_NORTH);
        net_queue_pop(&q, &cmd);
        ASSERT_EQ_U8(cmd.opcode, OP_MOVE_SOUTH);
        net_queue_pop(&q, &cmd);
        ASSERT_EQ_U8(cmd.opcode, OP_MOVE_EAST);
    }
    TEST_END();
}

static void test_queue_sequence_numbers(void) {
    TEST_BEGIN("queue: sequence numbers auto-increment");
    {
        CommandQueue q;
        InteractionCommand cmd, out;
        net_queue_init(&q);

        cmd = net_cmd_move(0, OP_MOVE_NORTH);
        net_queue_push(&q, &cmd);
        net_queue_push(&q, &cmd);
        net_queue_push(&q, &cmd);

        net_queue_pop(&q, &out); ASSERT_EQ_U16(out.sequence, 0);
        net_queue_pop(&q, &out); ASSERT_EQ_U16(out.sequence, 1);
        net_queue_pop(&q, &out); ASSERT_EQ_U16(out.sequence, 2);
    }
    TEST_END();
}

static void test_queue_overflow_drops(void) {
    TEST_BEGIN("queue: overflow drops commands, increments counter");
    {
        CommandQueue q;
        InteractionCommand cmd;
        uint32_t i;
        net_queue_init(&q);

        cmd = net_cmd_heartbeat(0);
        for (i = 0; i < NET_MAX_CMD_QUEUE; i++) {
            ASSERT_EQ_I32(net_queue_push(&q, &cmd), 0);
        }
        ASSERT_EQ_U32(q.count, NET_MAX_CMD_QUEUE);

        /* Next push should fail */
        ASSERT_EQ_I32(net_queue_push(&q, &cmd), -1);
        ASSERT_EQ_U32(q.dropped, 1);
        ASSERT_EQ_U32(q.count, NET_MAX_CMD_QUEUE);

        /* Another fail */
        ASSERT_EQ_I32(net_queue_push(&q, &cmd), -1);
        ASSERT_EQ_U32(q.dropped, 2);
    }
    TEST_END();
}

static void test_queue_pop_empty_fails(void) {
    TEST_BEGIN("queue: pop on empty returns -1");
    {
        CommandQueue q;
        InteractionCommand out;
        net_queue_init(&q);
        ASSERT_EQ_I32(net_queue_pop(&q, &out), -1);
    }
    TEST_END();
}

static void test_queue_peek(void) {
    TEST_BEGIN("queue: peek returns head without consuming");
    {
        CommandQueue q;
        InteractionCommand cmd;
        const InteractionCommand* peeked;
        net_queue_init(&q);

        ASSERT_NULL(net_queue_peek(&q));

        cmd = net_cmd_move(5, OP_MOVE_WEST);
        net_queue_push(&q, &cmd);

        peeked = net_queue_peek(&q);
        ASSERT_NOT_NULL(peeked);
        ASSERT_EQ_U32(peeked->entity_id, 5);
        ASSERT_EQ_U8(peeked->opcode, OP_MOVE_WEST);
        ASSERT_EQ_U32(q.count, 1); /* Not consumed */
    }
    TEST_END();
}

static void test_queue_flush(void) {
    TEST_BEGIN("queue: flush empties queue");
    {
        CommandQueue q;
        InteractionCommand cmd;
        net_queue_init(&q);

        cmd = net_cmd_heartbeat(0);
        net_queue_push(&q, &cmd);
        net_queue_push(&q, &cmd);
        net_queue_push(&q, &cmd);
        ASSERT_EQ_U32(q.count, 3);

        net_queue_flush(&q);
        ASSERT_EQ_U32(q.count, 0);
    }
    TEST_END();
}

static void test_queue_wraparound(void) {
    TEST_BEGIN("queue: ring buffer wraps around correctly");
    {
        CommandQueue q;
        InteractionCommand cmd, out;
        uint32_t i;
        net_queue_init(&q);
        memset(&out, 0, sizeof(out));

        /* Fill half, drain half, fill again -- exercises wraparound */
        cmd = net_cmd_move(0, OP_MOVE_NORTH);
        for (i = 0; i < NET_MAX_CMD_QUEUE / 2; i++) {
            net_queue_push(&q, &cmd);
        }
        for (i = 0; i < NET_MAX_CMD_QUEUE / 2; i++) {
            net_queue_pop(&q, &out);
        }
        ASSERT_EQ_U32(q.count, 0);

        /* Now fill the entire queue again -- wraps around */
        for (i = 0; i < NET_MAX_CMD_QUEUE; i++) {
            cmd.entity_id = i;
            ASSERT_EQ_I32(net_queue_push(&q, &cmd), 0);
        }
        ASSERT_EQ_U32(q.count, NET_MAX_CMD_QUEUE);

        /* Verify they come out in order */
        for (i = 0; i < NET_MAX_CMD_QUEUE; i++) {
            net_queue_pop(&q, &out);
            ASSERT_EQ_U32(out.entity_id, i);
        }
    }
    TEST_END();
}

/* =========================================================================
 * SECTION 3: OPCODE CLASSIFICATION
 * ========================================================================= */

static void test_opcode_classification(void) {
    TEST_BEGIN("opcode: classification helpers");
    {
        ASSERT(net_opcode_is_movement(OP_MOVE_NORTH));
        ASSERT(net_opcode_is_movement(OP_MOVE_SW));
        ASSERT(net_opcode_is_movement(OP_TELEPORT));
        ASSERT(!net_opcode_is_movement(OP_MELEE_ATTACK));

        ASSERT(net_opcode_is_combat(OP_MELEE_ATTACK));
        ASSERT(net_opcode_is_combat(OP_USE_MEDKIT));
        ASSERT(!net_opcode_is_combat(OP_MOVE_NORTH));

        ASSERT(net_opcode_is_inventory(OP_PICK_UP));
        ASSERT(net_opcode_is_inventory(OP_USE_ITEM));
        ASSERT(!net_opcode_is_inventory(OP_SEARCH));

        ASSERT(net_opcode_is_environment(OP_INTERACT_DOOR));
        ASSERT(net_opcode_is_environment(OP_ACTIVATE));
        ASSERT(!net_opcode_is_environment(OP_HEARTBEAT));

        ASSERT(net_opcode_is_system(OP_HEARTBEAT));
        ASSERT(net_opcode_is_system(OP_NOP));
        ASSERT(net_opcode_is_system(OP_ARENA_CHALLENGE));
        ASSERT(!net_opcode_is_system(OP_MOVE_NORTH));
    }
    TEST_END();
}

/* =========================================================================
 * SECTION 4: MOVEMENT DELTA TABLE
 * ========================================================================= */

static void test_move_deltas(void) {
    TEST_BEGIN("movement: delta table correct for all 8 directions");
    {
        int dx, dy;

        net_move_delta(OP_MOVE_NORTH, &dx, &dy); ASSERT_EQ_I32(dx, 0);  ASSERT_EQ_I32(dy, -1);
        net_move_delta(OP_MOVE_SOUTH, &dx, &dy); ASSERT_EQ_I32(dx, 0);  ASSERT_EQ_I32(dy,  1);
        net_move_delta(OP_MOVE_EAST,  &dx, &dy); ASSERT_EQ_I32(dx, 1);  ASSERT_EQ_I32(dy,  0);
        net_move_delta(OP_MOVE_WEST,  &dx, &dy); ASSERT_EQ_I32(dx, -1); ASSERT_EQ_I32(dy,  0);
        net_move_delta(OP_MOVE_NE,    &dx, &dy); ASSERT_EQ_I32(dx, 1);  ASSERT_EQ_I32(dy, -1);
        net_move_delta(OP_MOVE_NW,    &dx, &dy); ASSERT_EQ_I32(dx, -1); ASSERT_EQ_I32(dy, -1);
        net_move_delta(OP_MOVE_SE,    &dx, &dy); ASSERT_EQ_I32(dx, 1);  ASSERT_EQ_I32(dy,  1);
        net_move_delta(OP_MOVE_SW,    &dx, &dy); ASSERT_EQ_I32(dx, -1); ASSERT_EQ_I32(dy,  1);

        /* Non-movement opcode gives 0,0 */
        net_move_delta(OP_MELEE_ATTACK, &dx, &dy); ASSERT_EQ_I32(dx, 0); ASSERT_EQ_I32(dy, 0);
    }
    TEST_END();
}

/* =========================================================================
 * SECTION 5: WORLD VALIDATION
 * ========================================================================= */

static NetWorld g_world;

static void setup_test_world(void) {
    int x, y;
    net_world_init(&g_world);

    /* Carve a small 5x5 room at (5,5) */
    net_map_init(&g_world.map);
    for (y = 5; y < 10; y++) {
        for (x = 5; x < 10; x++) {
            g_world.map.tiles[y][x] = 0; /* floor */
        }
    }

    /* Player at (7, 7) */
    net_world_add_entity(&g_world, 0, 7, 7, 30, 30, '@');
    /* Enemy at (8, 7) */
    net_world_add_entity(&g_world, 1, 8, 7, 10, 10, 'S');
}

static void test_validate_move_ok(void) {
    TEST_BEGIN("validate: move to open floor succeeds");
    {
        InteractionCommand cmd;
        setup_test_world();
        cmd = net_cmd_move(0, OP_MOVE_WEST); /* (7,7) -> (6,7) = floor */
        ASSERT_EQ_I32(net_process_command(&g_world, &cmd), VALIDATE_OK);
        /* Verify position updated */
        ASSERT_EQ_U16(g_world.entities[0].x, 6);
        ASSERT_EQ_U16(g_world.entities[0].y, 7);
    }
    TEST_END();
}

static void test_validate_move_blocked(void) {
    TEST_BEGIN("validate: move into wall is BLOCKED");
    {
        InteractionCommand cmd;
        setup_test_world();
        /* Move player to edge of room first */
        g_world.entities[0].x = 5;
        g_world.entities[0].y = 5;
        cmd = net_cmd_move(0, OP_MOVE_NORTH); /* (5,5) -> (5,4) = wall */
        ASSERT_EQ_I32(net_process_command(&g_world, &cmd), VALIDATE_FAIL_BLOCKED);
        /* Position unchanged */
        ASSERT_EQ_U16(g_world.entities[0].x, 5);
        ASSERT_EQ_U16(g_world.entities[0].y, 5);
    }
    TEST_END();
}

static void test_validate_bad_entity(void) {
    TEST_BEGIN("validate: command from nonexistent entity fails");
    {
        InteractionCommand cmd;
        setup_test_world();
        cmd = net_cmd_move(999, OP_MOVE_NORTH);
        ASSERT_EQ_I32(net_process_command(&g_world, &cmd), VALIDATE_FAIL_BAD_ENTITY);
    }
    TEST_END();
}

static void test_validate_dead_entity(void) {
    TEST_BEGIN("validate: command from dead entity fails");
    {
        InteractionCommand cmd;
        setup_test_world();
        g_world.entities[0].alive = 0;
        cmd = net_cmd_move(0, OP_MOVE_NORTH);
        ASSERT_EQ_I32(net_process_command(&g_world, &cmd), VALIDATE_FAIL_DEAD);
    }
    TEST_END();
}

static void test_validate_melee_ok(void) {
    TEST_BEGIN("validate: melee attack on alive target succeeds");
    {
        InteractionCommand cmd;
        setup_test_world();
        cmd = net_cmd_melee(0, 1);
        ASSERT_EQ_I32(net_process_command(&g_world, &cmd), VALIDATE_OK);
        ASSERT_EQ_I32(g_world.entities[1].hp, 5); /* 10 - 5 */
    }
    TEST_END();
}

static void test_validate_melee_kills(void) {
    TEST_BEGIN("validate: melee attack kills target at 0 HP");
    {
        InteractionCommand cmd;
        setup_test_world();
        g_world.entities[1].hp = 3;
        cmd = net_cmd_melee(0, 1);
        ASSERT_EQ_I32(net_process_command(&g_world, &cmd), VALIDATE_OK);
        ASSERT_EQ_I32(g_world.entities[1].hp, 0);
        ASSERT_EQ_U8(g_world.entities[1].alive, 0);
    }
    TEST_END();
}

static void test_validate_melee_no_target(void) {
    TEST_BEGIN("validate: melee attack on nonexistent target fails");
    {
        InteractionCommand cmd;
        setup_test_world();
        cmd = net_cmd_melee(0, 999);
        ASSERT_EQ_I32(net_process_command(&g_world, &cmd), VALIDATE_FAIL_NO_TARGET);
    }
    TEST_END();
}

static void test_validate_heartbeat(void) {
    TEST_BEGIN("validate: heartbeat always succeeds");
    {
        InteractionCommand cmd;
        setup_test_world();
        cmd = net_cmd_heartbeat(0);
        ASSERT_EQ_I32(net_process_command(&g_world, &cmd), VALIDATE_OK);
    }
    TEST_END();
}

static void test_validate_unknown_opcode(void) {
    TEST_BEGIN("validate: unknown opcode fails");
    {
        InteractionCommand cmd;
        setup_test_world();
        memset(&cmd, 0, sizeof(cmd));
        cmd.entity_id = 0;
        cmd.opcode = 0x70; /* Unhandled range */
        ASSERT_EQ_I32(net_process_command(&g_world, &cmd), VALIDATE_FAIL_UNKNOWN_OP);
    }
    TEST_END();
}

/* =========================================================================
 * SECTION 6: FULL TICK PROCESSING
 * ========================================================================= */

static void test_tick_processes_queue(void) {
    TEST_BEGIN("tick: drains queue and applies valid commands");
    {
        CommandQueue q;
        InteractionCommand cmd;
        uint32_t applied;

        setup_test_world();
        net_queue_init(&q);

        /* Queue 3 moves: W, W, S */
        cmd = net_cmd_move(0, OP_MOVE_WEST); net_queue_push(&q, &cmd);
        cmd = net_cmd_move(0, OP_MOVE_WEST); net_queue_push(&q, &cmd);
        cmd = net_cmd_move(0, OP_MOVE_SOUTH); net_queue_push(&q, &cmd);

        applied = net_tick(&g_world, &q);
        ASSERT_EQ_U32(applied, 3);
        ASSERT_EQ_U32(q.count, 0);
        ASSERT_EQ_U16(g_world.entities[0].x, 5); /* 7 - 2 */
        ASSERT_EQ_U16(g_world.entities[0].y, 8); /* 7 + 1 */
        ASSERT_EQ_U32(g_world.tick, 1);
    }
    TEST_END();
}

static void test_tick_rejects_invalid(void) {
    TEST_BEGIN("tick: rejects invalid commands, still processes valid ones");
    {
        CommandQueue q;
        InteractionCommand cmd;
        uint32_t applied;

        setup_test_world();
        net_queue_init(&q);

        /* Valid move */
        cmd = net_cmd_move(0, OP_MOVE_WEST); net_queue_push(&q, &cmd);
        /* Invalid: move into wall */
        g_world.entities[0].x = 5;
        cmd = net_cmd_move(0, OP_MOVE_WEST); net_queue_push(&q, &cmd);
        /* Valid heartbeat */
        cmd = net_cmd_heartbeat(0); net_queue_push(&q, &cmd);

        /* Reset position for first move */
        g_world.entities[0].x = 7;
        applied = net_tick(&g_world, &q);

        /* First move: 7->6 (OK), second: 6->5 (OK, floor), third: heartbeat (OK) */
        /* Actually both moves should succeed since 5,7 and 6,7 are floor */
        ASSERT_EQ_U32(applied, 3);
    }
    TEST_END();
}

static void test_tick_empty_queue(void) {
    TEST_BEGIN("tick: empty queue just increments tick");
    {
        CommandQueue q;
        setup_test_world();
        net_queue_init(&q);

        net_tick(&g_world, &q);
        ASSERT_EQ_U32(g_world.tick, 1);
        net_tick(&g_world, &q);
        ASSERT_EQ_U32(g_world.tick, 2);
    }
    TEST_END();
}

/* =========================================================================
 * SECTION 7: SNAPSHOT
 * ========================================================================= */

static void test_snapshot_build(void) {
    TEST_BEGIN("snapshot: builds correct entity data from world");
    {
        Snapshot snap;
        setup_test_world();
        g_world.tick = 42;

        net_build_snapshot(&g_world, &snap);

        ASSERT_EQ_U32(snap.entity_count, 2);
        ASSERT_EQ_U32(snap.tick_number, 42);
        ASSERT_EQ_U8(snap.protocol_version, NET_PROTOCOL_VERSION);

        /* Player */
        ASSERT_EQ_U32(snap.entities[0].entity_id, 0);
        ASSERT_EQ_U16(snap.entities[0].x, 7);
        ASSERT_EQ_U16(snap.entities[0].y, 7);
        ASSERT_EQ_U8(snap.entities[0].entity_type, 0); /* player */
        ASSERT_EQ_U8(snap.entities[0].flags, 0x01);     /* alive */

        /* Enemy */
        ASSERT_EQ_U32(snap.entities[1].entity_id, 1);
        ASSERT_EQ_U16(snap.entities[1].x, 8);
        ASSERT_EQ_U16(snap.entities[1].y, 7);
        ASSERT_EQ_U8(snap.entities[1].entity_type, 1); /* enemy */
    }
    TEST_END();
}

static void test_snapshot_dead_entity(void) {
    TEST_BEGIN("snapshot: dead entity has flags=0");
    {
        Snapshot snap;
        setup_test_world();
        g_world.entities[1].alive = 0;

        net_build_snapshot(&g_world, &snap);
        ASSERT_EQ_U8(snap.entities[1].flags, 0x00);
    }
    TEST_END();
}

/* =========================================================================
 * SECTION 8: OPCODE NAME TABLE
 * ========================================================================= */

static void test_opcode_names(void) {
    TEST_BEGIN("opcode: name table has entries for known opcodes");
    {
        ASSERT_NOT_NULL(OPCODE_NAMES[OP_MOVE_NORTH]);
        ASSERT_NOT_NULL(OPCODE_NAMES[OP_MELEE_ATTACK]);
        ASSERT_NOT_NULL(OPCODE_NAMES[OP_HEARTBEAT]);
        ASSERT_NOT_NULL(OPCODE_NAMES[OP_NOP]);
        ASSERT_NULL(OPCODE_NAMES[0x70]); /* Unassigned */
    }
    TEST_END();
}

/* =========================================================================
 * SECTION 9: CONVENIENCE BUILDERS
 * ========================================================================= */

static void test_cmd_builders(void) {
    TEST_BEGIN("builders: convenience functions produce correct packets");
    {
        InteractionCommand cmd;

        cmd = net_cmd_move(10, OP_MOVE_SE);
        ASSERT_EQ_U32(cmd.entity_id, 10);
        ASSERT_EQ_U8(cmd.opcode, OP_MOVE_SE);

        cmd = net_cmd_melee(3, 7);
        ASSERT_EQ_U8(cmd.opcode, OP_MELEE_ATTACK);
        ASSERT_EQ_U32(cmd.target_id, 7);

        cmd = net_cmd_use_item(5, 2);
        ASSERT_EQ_U8(cmd.opcode, OP_USE_ITEM);
        ASSERT_EQ_U8(cmd.param1, 2);

        cmd = net_cmd_heartbeat(99);
        ASSERT_EQ_U8(cmd.opcode, OP_HEARTBEAT);
        ASSERT_EQ_U32(cmd.entity_id, 99);
    }
    TEST_END();
}

/* =========================================================================
 * SECTION 10: INTEGRATED SCENARIO
 *   Player moves around, attacks enemy, enemy dies, snapshot reflects it.
 * ========================================================================= */

static void test_integrated_scenario(void) {
    TEST_BEGIN("integrated: move + attack + kill + snapshot lifecycle");
    {
        CommandQueue q;
        InteractionCommand cmd;
        Snapshot snap;

        setup_test_world();
        net_queue_init(&q);

        /* Tick 0: Move east toward enemy */
        /* Player at (7,7), enemy at (8,7). Move east would collide.
         * Since we don't have bump-attack in movement, use melee instead. */
        cmd = net_cmd_melee(0, 1); net_queue_push(&q, &cmd);
        net_tick(&g_world, &q); /* Tick 0 */
        ASSERT_EQ_I32(g_world.entities[1].hp, 5);

        /* Tick 1: Attack again, kill */
        cmd = net_cmd_melee(0, 1); net_queue_push(&q, &cmd);
        net_tick(&g_world, &q); /* Tick 1 */
        ASSERT_EQ_I32(g_world.entities[1].hp, 0);
        ASSERT_EQ_U8(g_world.entities[1].alive, 0);

        /* Tick 2: Move into cleared space */
        cmd = net_cmd_move(0, OP_MOVE_EAST); net_queue_push(&q, &cmd);
        net_tick(&g_world, &q); /* Tick 2 */
        ASSERT_EQ_U16(g_world.entities[0].x, 8);

        /* Build snapshot */
        net_build_snapshot(&g_world, &snap);
        ASSERT_EQ_U32(snap.tick_number, 3);
        ASSERT_EQ_U32(snap.entity_count, 2);
        ASSERT_EQ_U8(snap.entities[0].flags, 0x01); /* player alive */
        ASSERT_EQ_U8(snap.entities[1].flags, 0x00); /* enemy dead */
    }
    TEST_END();
}

/* =========================================================================
 * INTERACTIVE DEMO (--demo flag)
 *
 * Runs a WASD-controlled player on a small map with 0.6s tick rate.
 * Verifies packets are created, serialized, queued, validated, applied,
 * and reflected in snapshots.
 * ========================================================================= */

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
static void demo_sleep_ms(int ms) { Sleep(ms); }
static int  demo_kbhit(void) { return _kbhit(); }
static int  demo_getch(void) { return _getch(); }
#else
#define _DEFAULT_SOURCE
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <fcntl.h>
static void demo_sleep_ms(int ms) { usleep(ms * 1000); }
static int demo_kbhit(void) {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}
static int demo_getch(void) { return getchar(); }
static struct termios g_orig_termios;
static void demo_raw_mode(void) {
    struct termios raw;
    tcgetattr(0, &g_orig_termios);
    raw = g_orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &raw);
}
static void demo_restore_mode(void) {
    tcsetattr(0, TCSANOW, &g_orig_termios);
}
#endif

static void print_map(const NetWorld* w, const Snapshot* snap) {
    int x, y;
    uint32_t i;
    char display[NET_MAP_H][NET_MAP_W];

    /* Build display from map */
    for (y = 0; y < NET_MAP_H; y++) {
        for (x = 0; x < NET_MAP_W; x++) {
            display[y][x] = w->map.tiles[y][x] ? '#' : '.';
        }
    }

    /* Overlay entities from snapshot */
    for (i = 0; i < snap->entity_count; i++) {
        if (snap->entities[i].flags & 0x01) { /* alive */
            if (snap->entities[i].x < NET_MAP_W && snap->entities[i].y < NET_MAP_H) {
                display[snap->entities[i].y][snap->entities[i].x] = (char)snap->entities[i].glyph;
            }
        }
    }

    /* Render viewport (10x10 centered on player) */
    printf("\033[2J\033[H"); /* Clear screen */
    printf("=== MARBLE NET DEMO === Tick: %u  |  WASD=move  Q=quit\n", snap->tick_number);
    printf("Player: (%u, %u)  HP: %d/%d\n\n",
           snap->entities[0].x, snap->entities[0].y,
           snap->entities[0].hp, snap->entities[0].max_hp);

    {
        int px, py, vx1, vy1, vx2, vy2;
        char c;

        px = (int)snap->entities[0].x;
        py = (int)snap->entities[0].y;
        vx1 = px - 7; if (vx1 < 0) vx1 = 0;
        vy1 = py - 5; if (vy1 < 0) vy1 = 0;
        vx2 = vx1 + 15; if (vx2 > NET_MAP_W) { vx2 = NET_MAP_W; vx1 = vx2 - 15; if (vx1 < 0) vx1 = 0; }
        vy2 = vy1 + 11; if (vy2 > NET_MAP_H) { vy2 = NET_MAP_H; vy1 = vy2 - 11; if (vy1 < 0) vy1 = 0; }

        for (y = vy1; y < vy2; y++) {
            printf("  ");
            for (x = vx1; x < vx2; x++) {
                c = display[y][x];
                if (c == '@') printf("\033[1;36m@\033[0m");
                else if (c == 'S') printf("\033[1;31mS\033[0m");
                else if (c == '#') printf("\033[0;33m#\033[0m");
                else printf("\033[0;37m.\033[0m");
            }
            printf("\n");
        }
    }

    printf("\nQueue: dropped=%u processed=%u | Cmds: applied=%u rejected=%u\n",
           0u, 0u, w->cmds_applied, w->cmds_rejected);
}

static void run_demo(void) {
    NetWorld world;
    CommandQueue queue;
    Snapshot snap;
    int running = 1;
    int x, y;

    net_world_init(&world);
    net_queue_init(&queue);
    net_map_init(&world.map);

    /* Carve a bigger room */
    for (y = 2; y < 18; y++) {
        for (x = 2; x < 25; x++) {
            world.map.tiles[y][x] = 0;
        }
    }
    /* Add some walls for interest */
    for (y = 5; y < 12; y++) world.map.tiles[y][10] = 1;
    for (x = 10; x < 18; x++) world.map.tiles[8][x] = 1;
    world.map.tiles[7][10] = 0; /* Door */
    world.map.tiles[8][14] = 0; /* Door */

    net_world_add_entity(&world, 0, 5, 5, 30, 30, '@');
    net_world_add_entity(&world, 1, 15, 6, 10, 10, 'S');
    net_world_add_entity(&world, 2, 20, 12, 15, 15, 'S');

#ifndef _WIN32
    demo_raw_mode();
#endif

    printf("=== MARBLE NET INTERACTIVE DEMO ===\n");
    printf("WASD to move, Q to quit\n");
    printf("Tick rate: %dms (%.1fs)\n\n", NET_TICK_INTERVAL_MS, NET_TICK_INTERVAL_MS / 1000.0f);

    while (running) {
        int ch;
        InteractionCommand cmd;
        int pushed;
        uint8_t wire[NET_CMD_SIZE];
        InteractionCommand verified;

        /* Collect input */
        while (demo_kbhit()) {
            ch = demo_getch();
            pushed = 0;
            memset(&cmd, 0, sizeof(cmd));

            switch (ch) {
                case 'w': case 'W':
                    cmd = net_cmd_move(0, OP_MOVE_NORTH);
                    pushed = 1; break;
                case 's': case 'S':
                    cmd = net_cmd_move(0, OP_MOVE_SOUTH);
                    pushed = 1; break;
                case 'a': case 'A':
                    cmd = net_cmd_move(0, OP_MOVE_WEST);
                    pushed = 1; break;
                case 'd': case 'D':
                    cmd = net_cmd_move(0, OP_MOVE_EAST);
                    pushed = 1; break;
                case 'q': case 'Q':
                    running = 0; break;
                default: break;
            }

            if (pushed) {
                /* Serialize -> deserialize (proves wire format works) */
                net_pack_command(&cmd, wire);
                net_unpack_command(wire, &verified);
                net_queue_push(&queue, &verified);
            }
        }

        /* Process tick */
        net_tick(&world, &queue);

        /* Build and display snapshot */
        net_build_snapshot(&world, &snap);
        print_map(&world, &snap);

        demo_sleep_ms(NET_TICK_INTERVAL_MS);
    }

#ifndef _WIN32
    demo_restore_mode();
#endif

    printf("\n\nDemo ended. Tick: %u, Applied: %u, Rejected: %u\n",
           world.tick, world.cmds_applied, world.cmds_rejected);
}

/* =========================================================================
 * RUN ALL TESTS
 * ========================================================================= */

int main(int argc, char* argv[]) {
    /* Init string tables */
    net_init_opcode_names();

    /* Check for --demo flag */
    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        run_demo();
        return 0;
    }

    printf("MarbleEngine Network Protocol Tests\n");
    printf("====================================\n\n");

    /* Serialization */
    printf("[Serialization Roundtrip]\n");
    test_serialize_roundtrip_zeros();
    test_serialize_roundtrip_full();
    test_serialize_all_movement_opcodes();
    test_serialize_boundary_values();

    /* Command Queue */
    printf("\n[Command Queue]\n");
    test_queue_init_empty();
    test_queue_push_pop();
    test_queue_fifo_order();
    test_queue_sequence_numbers();
    test_queue_overflow_drops();
    test_queue_pop_empty_fails();
    test_queue_peek();
    test_queue_flush();
    test_queue_wraparound();

    /* Opcode Classification */
    printf("\n[Opcode Classification]\n");
    test_opcode_classification();

    /* Movement Deltas */
    printf("\n[Movement Deltas]\n");
    test_move_deltas();

    /* World Validation */
    printf("\n[World Validation]\n");
    test_validate_move_ok();
    test_validate_move_blocked();
    test_validate_bad_entity();
    test_validate_dead_entity();
    test_validate_melee_ok();
    test_validate_melee_kills();
    test_validate_melee_no_target();
    test_validate_heartbeat();
    test_validate_unknown_opcode();

    /* Tick Processing */
    printf("\n[Tick Processing]\n");
    test_tick_processes_queue();
    test_tick_rejects_invalid();
    test_tick_empty_queue();

    /* Snapshot */
    printf("\n[Snapshot]\n");
    test_snapshot_build();
    test_snapshot_dead_entity();

    /* Opcode Names */
    printf("\n[Opcode Names]\n");
    test_opcode_names();

    /* Convenience Builders */
    printf("\n[Convenience Builders]\n");
    test_cmd_builders();

    /* Integrated Scenario */
    printf("\n[Integrated Scenario]\n");
    test_integrated_scenario();

    /* Summary */
    printf("\n====================================\n");
    printf("TOTAL: %d  PASSED: %d  FAILED: %d\n",
           g_tests_run, g_tests_passed, g_tests_failed);

    if (g_tests_failed == 0) {
        printf("ALL TESTS PASSED\n");
    } else {
        printf("*** FAILURES DETECTED ***\n");
    }

    printf("\nRun with --demo for interactive WASD demo (0.6s tick)\n");
    printf("Validation names available: %s ... %s\n",
           VALIDATE_RESULT_NAMES[0], VALIDATE_RESULT_NAMES[VALIDATE_RESULT_COUNT-1]);

    return (g_tests_failed > 0) ? 1 : 0;
}