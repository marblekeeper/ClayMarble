-- network.lua
-- MARBLE ENGINE: Network Intent Layer (Lua Client)
--
-- READ-ONLY representation layer. Submits intent packets to C authority.
-- NEVER owns or mutates game state directly.
--
-- OpCode Map (matches marble_net.h exactly):
--   0x00-0x0F : Movement    0x10-0x2F : Combat
--   0x30-0x4F : Inventory   0x50-0x6F : Environment
--   0xF0-0xFF : System

local M = {}

-- =========================================================================
-- OPCODE CONSTANTS (must match marble_net.h)
-- =========================================================================

M.OP = {
    MOVE_NORTH=0x00, MOVE_SOUTH=0x01, MOVE_EAST=0x02, MOVE_WEST=0x03,
    MOVE_NE=0x04, MOVE_NW=0x05, MOVE_SE=0x06, MOVE_SW=0x07,
    ASCEND=0x08, DESCEND=0x09, TELEPORT=0x0A,
    MELEE_ATTACK=0x10, RANGED_ATTACK=0x11, DEFEND=0x12,
    USE_SKILL=0x13, USE_MEDKIT=0x14,
    PICK_UP=0x30, DROP=0x31, EQUIP=0x32, CONSUME=0x33, USE_ITEM=0x34,
    INTERACT_DOOR=0x50, SEARCH=0x51, DISARM_TRAP=0x52, ACTIVATE=0x53,
    HEARTBEAT=0xF0, LOGIN=0xF1, LOGOUT=0xF2,
    ARENA_CHALLENGE=0xF3, ARENA_ACCEPT=0xF4, ARENA_DECLINE=0xF5,
    SYNC_REQUEST=0xFE, NOP=0xFF,
}

M.OP_NAMES = {}
for name, code in pairs(M.OP) do M.OP_NAMES[code] = name end

M.PROTOCOL_VERSION = 1
M.CMD_SIZE         = 16
M.MAX_CMD_QUEUE    = 128
M.TICK_INTERVAL_MS = 600

-- =========================================================================
-- OPCODE CLASSIFICATION
-- =========================================================================

function M.isMovement(op)    return op >= 0x00 and op <= 0x0F end
function M.isCombat(op)      return op >= 0x10 and op <= 0x2F end
function M.isInventory(op)   return op >= 0x30 and op <= 0x4F end
function M.isEnvironment(op) return op >= 0x50 and op <= 0x6F end
function M.isSystem(op)      return op >= 0xF0 end

-- =========================================================================
-- DIRECTION MAP (WASD / Arrow keys -> opcodes)
-- =========================================================================

M.DIRECTION_MAP = {
    up="MOVE_NORTH", down="MOVE_SOUTH", left="MOVE_WEST", right="MOVE_EAST",
    w="MOVE_NORTH", s="MOVE_SOUTH", a="MOVE_WEST", d="MOVE_EAST",
}

-- =========================================================================
-- COMMAND BUILDER
-- =========================================================================

local sequence_counter = 0

local function next_sequence()
    local seq = sequence_counter
    sequence_counter = (sequence_counter + 1) % 65536
    return seq
end

function M.createCommand(entity_id, opcode, param1, target_x, target_y, target_id)
    return {
        entity_id = entity_id or 0,
        opcode    = opcode or M.OP.NOP,
        param1    = param1 or 0,
        target_x  = target_x or 0,
        target_y  = target_y or 0,
        target_id = target_id or 0,
        sequence  = next_sequence(),
    }
end

-- Convenience builders
function M.moveNorth(eid)  return M.createCommand(eid, M.OP.MOVE_NORTH) end
function M.moveSouth(eid)  return M.createCommand(eid, M.OP.MOVE_SOUTH) end
function M.moveEast(eid)   return M.createCommand(eid, M.OP.MOVE_EAST)  end
function M.moveWest(eid)   return M.createCommand(eid, M.OP.MOVE_WEST)  end
function M.moveNE(eid)     return M.createCommand(eid, M.OP.MOVE_NE)    end
function M.moveNW(eid)     return M.createCommand(eid, M.OP.MOVE_NW)    end
function M.moveSE(eid)     return M.createCommand(eid, M.OP.MOVE_SE)    end
function M.moveSW(eid)     return M.createCommand(eid, M.OP.MOVE_SW)    end

function M.meleeAttack(eid, tid)
    return M.createCommand(eid, M.OP.MELEE_ATTACK, 0, 0, 0, tid)
end

function M.useItem(eid, slot)
    return M.createCommand(eid, M.OP.USE_ITEM, slot)
end

function M.useMedkit(eid) return M.createCommand(eid, M.OP.USE_MEDKIT) end
function M.heartbeat(eid) return M.createCommand(eid, M.OP.HEARTBEAT) end

function M.keyToMoveCommand(entity_id, keyName)
    local name = M.DIRECTION_MAP[keyName]
    if name then return M.createCommand(entity_id, M.OP[name]) end
    return nil
end

-- =========================================================================
-- SERIALIZATION (Little-endian, matches net_pack_command in C)
-- =========================================================================

local function u32_to_bytes(v)
    return string.char(v%256, math.floor(v/256)%256, math.floor(v/65536)%256, math.floor(v/16777216)%256)
end

local function u16_to_bytes(v)
    return string.char(v%256, math.floor(v/256)%256)
end

local function bytes_to_u32(s, off)
    return string.byte(s,off+1) + string.byte(s,off+2)*256 + string.byte(s,off+3)*65536 + string.byte(s,off+4)*16777216
end

local function bytes_to_u16(s, off)
    return string.byte(s,off+1) + string.byte(s,off+2)*256
end

function M.packCommand(cmd)
    return u32_to_bytes(cmd.entity_id)
        .. string.char(cmd.opcode)
        .. string.char(cmd.param1)
        .. u16_to_bytes(cmd.target_x)
        .. u16_to_bytes(cmd.target_y)
        .. u32_to_bytes(cmd.target_id)
        .. u16_to_bytes(cmd.sequence)
end

function M.unpackCommand(data)
    if #data ~= 16 then return nil, "invalid packet size: " .. #data end
    return {
        entity_id = bytes_to_u32(data, 0),
        opcode    = string.byte(data, 5),
        param1    = string.byte(data, 6),
        target_x  = bytes_to_u16(data, 6),
        target_y  = bytes_to_u16(data, 8),
        target_id = bytes_to_u32(data, 10),
        sequence  = bytes_to_u16(data, 14),
    }
end

-- =========================================================================
-- BRIDGE SUBMISSION
-- =========================================================================

local pending_queue = {}
local queue_stats = { submitted = 0, dropped = 0 }

function M.submitCommand(cmd)
    if not cmd then return false end
    local packed = M.packCommand(cmd)

    -- Try C bridge first
    if bridge and bridge.submitCommand then
        local ok = bridge.submitCommand(packed)
        if ok then queue_stats.submitted = queue_stats.submitted + 1; return true
        else queue_stats.dropped = queue_stats.dropped + 1; return false end
    end

    -- Fallback: local queue (for testing without C backend)
    if #pending_queue >= M.MAX_CMD_QUEUE then
        queue_stats.dropped = queue_stats.dropped + 1
        return false
    end
    pending_queue[#pending_queue + 1] = packed
    queue_stats.submitted = queue_stats.submitted + 1
    return true
end

function M.submitMove(entity_id, keyName)
    local cmd = M.keyToMoveCommand(entity_id, keyName)
    if cmd then return M.submitCommand(cmd) end
    return false
end

-- =========================================================================
-- SNAPSHOT (C -> Lua, read-only)
-- =========================================================================

local last_snapshot = nil
function M.receiveSnapshot(snap) last_snapshot = snap end
function M.getSnapshot() return last_snapshot end

-- =========================================================================
-- STATS & DEBUG
-- =========================================================================

function M.getStats()
    return { submitted = queue_stats.submitted, dropped = queue_stats.dropped, queue_size = #pending_queue }
end
function M.getLocalQueue() return pending_queue end
function M.clearLocalQueue() pending_queue = {} end
function M.resetSequence() sequence_counter = 0 end

function M.debugCommand(cmd)
    local name = M.OP_NAMES[cmd.opcode] or string.format("0x%02X", cmd.opcode)
    return string.format("CMD[seq=%d] eid=%d op=%s p1=%d tx=%d ty=%d tid=%d",
        cmd.sequence, cmd.entity_id, name, cmd.param1, cmd.target_x, cmd.target_y, cmd.target_id)
end

-- =========================================================================
-- SELF-TEST (verifies Lua pack/unpack matches C wire format)
-- =========================================================================

function M.selfTest()
    local passed, failed = 0, 0

    local function check(name, got, expect)
        if got == expect then passed = passed + 1
        else failed = failed + 1; print(string.format("  FAIL: %s: got %s, expected %s", name, tostring(got), tostring(expect))) end
    end

    print("[Lua Network Self-Test]")

    -- Roundtrip zero command
    local cmd0 = M.createCommand(0, M.OP.NOP); cmd0.sequence = 0
    local p0 = M.packCommand(cmd0)
    check("pack size", #p0, 16)
    local rt0 = M.unpackCommand(p0)
    check("rt0 entity_id", rt0.entity_id, 0)
    check("rt0 opcode", rt0.opcode, M.OP.NOP)

    -- Roundtrip populated
    local cmd1 = M.createCommand(42, M.OP.MELEE_ATTACK, 7, 100, 200, 99); cmd1.sequence = 1234
    local p1 = M.packCommand(cmd1)
    check("pack size full", #p1, 16)
    local rt1 = M.unpackCommand(p1)
    check("rt1 entity_id", rt1.entity_id, 42)
    check("rt1 opcode", rt1.opcode, M.OP.MELEE_ATTACK)
    check("rt1 param1", rt1.param1, 7)
    check("rt1 sequence", rt1.sequence, 1234)

    -- Direction mapping
    check("W->NORTH", M.DIRECTION_MAP["w"], "MOVE_NORTH")
    check("A->WEST",  M.DIRECTION_MAP["a"], "MOVE_WEST")
    check("S->SOUTH", M.DIRECTION_MAP["s"], "MOVE_SOUTH")
    check("D->EAST",  M.DIRECTION_MAP["d"], "MOVE_EAST")

    -- Classification
    check("NORTH is movement", M.isMovement(M.OP.MOVE_NORTH), true)
    check("MELEE is combat",   M.isCombat(M.OP.MELEE_ATTACK), true)
    check("PICK_UP is inv",    M.isInventory(M.OP.PICK_UP), true)
    check("DOOR is env",       M.isEnvironment(M.OP.INTERACT_DOOR), true)
    check("HEARTBEAT is sys",  M.isSystem(M.OP.HEARTBEAT), true)
    check("MELEE not movement", M.isMovement(M.OP.MELEE_ATTACK), false)

    -- Queue overflow
    M.clearLocalQueue(); M.resetSequence()
    queue_stats.submitted = 0; queue_stats.dropped = 0
    local all_ok = true
    for i = 1, M.MAX_CMD_QUEUE do
        if not M.submitCommand(M.heartbeat(0)) then all_ok = false end
    end
    check("128 submits ok", all_ok, true)
    check("129th dropped", M.submitCommand(M.heartbeat(0)), false)
    check("dropped count", M.getStats().dropped, 1)
    M.clearLocalQueue()

    print(string.format("  TOTAL: %d  PASSED: %d  FAILED: %d", passed+failed, passed, failed))
    if failed == 0 then print("  ALL LUA TESTS PASSED")
    else print("  *** LUA FAILURES DETECTED ***") end
    return failed == 0
end

return M