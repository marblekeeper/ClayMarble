-- network.lua
-- MARBLE ENGINE: Network Intent Layer (Lua Client)
--
-- This module is a READ-ONLY representation layer.
-- It can SUBMIT intent packets to the C authority via bridge functions.
-- It NEVER owns or mutates game state directly.
--
-- Architecture:
--   Lua submits InteractionCommands (16-byte fixed packets)
--   C validates, applies to ECS, returns Snapshots
--   Lua reads Snapshots for rendering (draw.lua / MindMarr.lua)
--
-- OpCode Map (matches marble_net.h exactly):
--   0x00-0x0F : Movement
--   0x10-0x2F : Combat
--   0x30-0x4F : Inventory
--   0x50-0x6F : Environment
--   0xF0-0xFF : System

local M = {}

-- =========================================================================
-- OPCODE CONSTANTS (must match marble_net.h)
-- =========================================================================

M.OP = {
    -- Movement (0x00 - 0x0F)
    MOVE_NORTH      = 0x00,
    MOVE_SOUTH      = 0x01,
    MOVE_EAST       = 0x02,
    MOVE_WEST       = 0x03,
    MOVE_NE         = 0x04,
    MOVE_NW         = 0x05,
    MOVE_SE         = 0x06,
    MOVE_SW         = 0x07,
    ASCEND          = 0x08,
    DESCEND         = 0x09,
    TELEPORT        = 0x0A,

    -- Combat (0x10 - 0x2F)
    MELEE_ATTACK    = 0x10,
    RANGED_ATTACK   = 0x11,
    DEFEND          = 0x12,
    USE_SKILL       = 0x13,
    USE_MEDKIT      = 0x14,

    -- Inventory (0x30 - 0x4F)
    PICK_UP         = 0x30,
    DROP            = 0x31,
    EQUIP           = 0x32,
    CONSUME         = 0x33,
    USE_ITEM        = 0x34,

    -- Environment (0x50 - 0x6F)
    INTERACT_DOOR   = 0x50,
    SEARCH          = 0x51,
    DISARM_TRAP     = 0x52,
    ACTIVATE        = 0x53,

    -- System (0xF0 - 0xFF)
    HEARTBEAT       = 0xF0,
    LOGIN           = 0xF1,
    LOGOUT          = 0xF2,
    ARENA_CHALLENGE = 0xF3,
    ARENA_ACCEPT    = 0xF4,
    ARENA_DECLINE   = 0xF5,
    SYNC_REQUEST    = 0xFE,
    NOP             = 0xFF,
}

-- Human-readable names
M.OP_NAMES = {}
for name, code in pairs(M.OP) do
    M.OP_NAMES[code] = name
end

-- =========================================================================
-- VALIDATION RESULTS (matches marble_net.h)
-- =========================================================================

M.VALIDATE = {
    OK              = 0,
    FAIL_UNKNOWN_OP = 1,
    FAIL_BAD_ENTITY = 2,
    FAIL_OUT_OF_TURN= 3,
    FAIL_BLOCKED    = 4,
    FAIL_NO_TARGET  = 5,
    FAIL_DEAD       = 6,
    FAIL_COOLDOWN   = 7,
}

-- =========================================================================
-- PROTOCOL CONSTANTS
-- =========================================================================

M.PROTOCOL_VERSION = 1
M.CMD_SIZE         = 16   -- Fixed packet size in bytes
M.MAX_CMD_QUEUE    = 128  -- Max pending commands per tick
M.TICK_INTERVAL_MS = 600  -- 0.6 seconds per tick

-- =========================================================================
-- COMMAND BUILDER
--
-- Creates intent packets. These are plain tables that get serialized
-- to 16 bytes before sending to C.
-- =========================================================================

-- Internal sequence counter
local sequence_counter = 0

local function next_sequence()
    local seq = sequence_counter
    sequence_counter = (sequence_counter + 1) % 65536
    return seq
end

--- Create a raw command packet (table form)
-- @param entity_id  uint32 - who is acting
-- @param opcode     uint8  - what they're doing
-- @param param1     uint8  - context-dependent parameter
-- @param target_x   uint16 - grid X (optional, default 0)
-- @param target_y   uint16 - grid Y (optional, default 0)
-- @param target_id  uint32 - target entity (optional, default 0)
-- @return table     command packet
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

-- =========================================================================
-- CONVENIENCE BUILDERS (match marble_net.h helpers)
-- =========================================================================

function M.moveNorth(entity_id)  return M.createCommand(entity_id, M.OP.MOVE_NORTH) end
function M.moveSouth(entity_id)  return M.createCommand(entity_id, M.OP.MOVE_SOUTH) end
function M.moveEast(entity_id)   return M.createCommand(entity_id, M.OP.MOVE_EAST)  end
function M.moveWest(entity_id)   return M.createCommand(entity_id, M.OP.MOVE_WEST)  end
function M.moveNE(entity_id)     return M.createCommand(entity_id, M.OP.MOVE_NE)    end
function M.moveNW(entity_id)     return M.createCommand(entity_id, M.OP.MOVE_NW)    end
function M.moveSE(entity_id)     return M.createCommand(entity_id, M.OP.MOVE_SE)    end
function M.moveSW(entity_id)     return M.createCommand(entity_id, M.OP.MOVE_SW)    end

function M.meleeAttack(entity_id, target_id)
    return M.createCommand(entity_id, M.OP.MELEE_ATTACK, 0, 0, 0, target_id)
end

function M.useItem(entity_id, slot)
    return M.createCommand(entity_id, M.OP.USE_ITEM, slot)
end

function M.useMedkit(entity_id)
    return M.createCommand(entity_id, M.OP.USE_MEDKIT)
end

function M.heartbeat(entity_id)
    return M.createCommand(entity_id, M.OP.HEARTBEAT)
end

-- =========================================================================
-- DIRECTION HELPERS
-- WASD / Arrow key mapping to opcodes
-- =========================================================================

M.DIRECTION_MAP = {
    up    = M.OP.MOVE_NORTH,
    down  = M.OP.MOVE_SOUTH,
    left  = M.OP.MOVE_WEST,
    right = M.OP.MOVE_EAST,
    w     = M.OP.MOVE_NORTH,
    s     = M.OP.MOVE_SOUTH,
    a     = M.OP.MOVE_WEST,
    d     = M.OP.MOVE_EAST,
}

--- Convert a key name to a movement command (or nil if not a direction)
function M.keyToMoveCommand(entity_id, keyName)
    local op = M.DIRECTION_MAP[keyName]
    if op then
        return M.createCommand(entity_id, op)
    end
    return nil
end

-- =========================================================================
-- OPCODE CLASSIFICATION (matches marble_net.h)
-- =========================================================================

function M.isMovement(opcode)    return opcode >= 0x00 and opcode <= 0x0F end
function M.isCombat(opcode)      return opcode >= 0x10 and opcode <= 0x2F end
function M.isInventory(opcode)   return opcode >= 0x30 and opcode <= 0x4F end
function M.isEnvironment(opcode) return opcode >= 0x50 and opcode <= 0x6F end
function M.isSystem(opcode)      return opcode >= 0xF0 end

-- =========================================================================
-- SERIALIZATION (Lua -> C wire format)
--
-- Packs a command table into a 16-byte string (little-endian).
-- This matches net_pack_command() in marble_net.h exactly.
-- =========================================================================

local function u32_to_bytes(v)
    return string.char(
        v % 256,
        math.floor(v / 256) % 256,
        math.floor(v / 65536) % 256,
        math.floor(v / 16777216) % 256
    )
end

local function u16_to_bytes(v)
    return string.char(v % 256, math.floor(v / 256) % 256)
end

local function bytes_to_u32(s, offset)
    local b0 = string.byte(s, offset + 1)
    local b1 = string.byte(s, offset + 2)
    local b2 = string.byte(s, offset + 3)
    local b3 = string.byte(s, offset + 4)
    return b0 + b1 * 256 + b2 * 65536 + b3 * 16777216
end

local function bytes_to_u16(s, offset)
    local b0 = string.byte(s, offset + 1)
    local b1 = string.byte(s, offset + 2)
    return b0 + b1 * 256
end

--- Serialize a command table to a 16-byte binary string
function M.packCommand(cmd)
    return u32_to_bytes(cmd.entity_id)
        .. string.char(cmd.opcode)
        .. string.char(cmd.param1)
        .. u16_to_bytes(cmd.target_x)
        .. u16_to_bytes(cmd.target_y)
        .. u32_to_bytes(cmd.target_id)
        .. u16_to_bytes(cmd.sequence)
end

--- Deserialize a 16-byte binary string to a command table
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
--
-- Sends an intent packet to the C backend.
-- The C side exposes bridge.submitCommand(bytes_16) or similar.
-- If no bridge is available, commands are queued locally for testing.
-- =========================================================================

local pending_queue = {}  -- Fallback queue when bridge is unavailable
local queue_stats = { submitted = 0, dropped = 0 }

--- Submit a command to the C authority
-- @param cmd  table - command packet from createCommand / builders
-- @return boolean   - true if submitted, false if dropped
function M.submitCommand(cmd)
    if not cmd then return false end

    local packed = M.packCommand(cmd)

    -- Try bridge first
    if bridge and bridge.submitCommand then
        local ok = bridge.submitCommand(packed)
        if ok then
            queue_stats.submitted = queue_stats.submitted + 1
            return true
        else
            queue_stats.dropped = queue_stats.dropped + 1
            return false
        end
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

--- Submit a movement command from a key press
-- @param entity_id  uint32 - who is moving
-- @param keyName    string - "w","a","s","d","up","down","left","right"
-- @return boolean
function M.submitMove(entity_id, keyName)
    local cmd = M.keyToMoveCommand(entity_id, keyName)
    if cmd then
        return M.submitCommand(cmd)
    end
    return false
end

-- =========================================================================
-- SNAPSHOT READING (C -> Lua, read-only)
--
-- The C backend pushes snapshots each tick.
-- Lua reads them for rendering. NEVER mutates.
-- =========================================================================

local last_snapshot = nil

--- Called by C bridge each tick with the current world snapshot
-- Stores it for Lua rendering to read
function M.receiveSnapshot(snap)
    last_snapshot = snap
end

--- Get the latest snapshot (read-only)
function M.getSnapshot()
    return last_snapshot
end

-- =========================================================================
-- STATS & DEBUG
-- =========================================================================

function M.getStats()
    return {
        submitted  = queue_stats.submitted,
        dropped    = queue_stats.dropped,
        queue_size = #pending_queue,
    }
end

function M.getLocalQueue()
    return pending_queue
end

function M.clearLocalQueue()
    pending_queue = {}
end

function M.resetSequence()
    sequence_counter = 0
end

--- Debug: Print a command in human-readable