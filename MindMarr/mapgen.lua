-- mapgen.lua
-- MINDMARR: Map generation

local rand, floor = math.random, math.floor
local max, min = math.max, math.min

local state = require("state")
local K = require("constants")
local util = require("util")

local game = state.game
local player = state.player

local M = {}

local function carveRoom(rx, ry, rw, rh)
    for y = ry, ry + rh - 1 do
        for x = rx, rx + rw - 1 do
            util.setTile(x, y, 0)
        end
    end
    return {x = rx, y = ry, w = rw, h = rh,
            cx = floor(rx + rw/2), cy = floor(ry + rh/2)}
end

local function carveCorridor(x1, y1, x2, y2)
    local x, y = x1, y1
    while x ~= x2 do
        util.setTile(x, y, 0)
        x = x + (x2 > x and 1 or -1)
    end
    while y ~= y2 do
        util.setTile(x, y, 0)
        y = y + (y2 > y and 1 or -1)
    end
end

function M.generateMap()
    -- Reset map table in-place
    local map = state.map
    for k in pairs(map) do map[k] = nil end

    for y = 1, K.MH do
        map[y] = {}
        for x = 1, K.MW do
            map[y][x] = 1
        end
    end

    local rooms = {}
    local attempts = 0
    local numRooms = rand(6, 9) + floor(game.sector / 2)

    while #rooms < numRooms and attempts < 200 do
        attempts = attempts + 1
        local rw = rand(3, 7)
        local rh = rand(3, 5)
        local rx = rand(2, K.MW - rw - 1)
        local ry = rand(2, K.MH - rh - 1)

        local ok = true
        for _, r in ipairs(rooms) do
            if rx < r.x + r.w + 1 and rx + rw + 1 > r.x and
               ry < r.y + r.h + 1 and ry + rh + 1 > r.y then
                ok = false; break
            end
        end

        if ok then
            local room = carveRoom(rx, ry, rw, rh)
            if #rooms > 0 then
                local prev = rooms[#rooms]
                if rand() < 0.5 then
                    carveCorridor(prev.cx, prev.cy, room.cx, prev.cy)
                    carveCorridor(room.cx, prev.cy, room.cx, room.cy)
                else
                    carveCorridor(prev.cx, prev.cy, prev.cx, room.cy)
                    carveCorridor(prev.cx, room.cy, room.cx, room.cy)
                end
            end
            rooms[#rooms+1] = room
        end
    end

    for i = 1, floor(#rooms / 3) do
        local a = rooms[rand(1, #rooms)]
        local b = rooms[rand(1, #rooms)]
        if a ~= b then carveCorridor(a.cx, a.cy, b.cx, b.cy) end
    end

    return rooms
end

-- Infected templates
function M.infectedTemplate(sector)
    local templates = {
        {name="Scientist",    glyph="S", hp=7,  str=30, def=15, dmgMin=1, dmgMax=3, xp=5,  color={180,100,120}},
        {name="Technician",   glyph="T", hp=10, str=40, def=20, dmgMin=2, dmgMax=4, xp=8,  color={160,80,100}},
        {name="Security",     glyph="G", hp=14, str=50, def=25, dmgMin=2, dmgMax=5, xp=12, color={200,70,90}},
        {name="Commander",    glyph="C", hp=20, str=55, def=30, dmgMin=3, dmgMax=7, xp=20, color={220,50,70}},
        {name="MarsSpawn",    glyph="M", hp=16, str=60, def=45, dmgMin=3, dmgMax=6, xp=25, color={200,40,60}},
        {name="Hivemind",     glyph="H", hp=35, str=65, def=30, dmgMin=5, dmgMax=10,xp=40, color={180,30,80}},
        {name="MINDMARR",     glyph="@", hp=50, str=75, def=40, dmgMin=6, dmgMax=12,xp=60, color={255,20,50}},
    }

    local maxIdx = min(#templates, 2 + floor(sector / 2))
    local minIdx = max(1, maxIdx - 3)
    local t = templates[rand(minIdx, maxIdx)]

    local scale = 1.0 + (sector - 1) * 0.12
    return {
        name = t.name,
        glyph = t.glyph,
        hp = floor(t.hp * scale),
        maxHp = floor(t.hp * scale),
        str = min(90, floor(t.str + sector * 2)),
        def = min(80, floor(t.def + sector)),
        dmgMin = t.dmgMin + floor(sector / 3),
        dmgMax = t.dmgMax + floor(sector / 3),
        xp = floor(t.xp * scale),
        color = t.color,
        alive = true,
        sayTimer = 0,
        lastSaid = "",
    }
end

function M.populateFloor(rooms)
    -- Clear enemies/items in-place
    local enemies = state.enemies
    for k in pairs(enemies) do enemies[k] = nil end
    local items = state.items
    for k in pairs(items) do items[k] = nil end

    player.x = rooms[1].cx
    player.y = rooms[1].cy

    state.shuttle.x = rooms[#rooms].cx
    state.shuttle.y = rooms[#rooms].cy

    -- Place elevator in a random room (not start or end)
    if #rooms >= 3 then
        local elevRoom = rooms[rand(2, #rooms - 1)]
        state.elevator.x = elevRoom.cx
        state.elevator.y = elevRoom.cy
        state.elevator.revealed = false
    end

    -- Enemies
    local numEnemies = 3 + game.sector * 2 + rand(0, 2)
    for i = 1, numEnemies do
        local room = rooms[rand(2, #rooms)]
        local ex = rand(room.x, room.x + room.w - 1)
        local ey = rand(room.y, room.y + room.h - 1)
        if not (ex == player.x and ey == player.y) and util.tileAt(ex, ey) == 0 then
            local e = M.infectedTemplate(game.sector)
            e.x = ex
            e.y = ey
            enemies[#enemies+1] = e
        end
    end

    -- Items
    for i = 2, #rooms do
        local room = rooms[i]
        if rand() < 0.35 then
            items[#items+1] = {
                x = rand(room.x, room.x + room.w - 1),
                y = rand(room.y, room.y + room.h - 1),
                type = "supply",
                amount = rand(2, 6) + game.sector,
            }
        end
        if rand() < 0.2 then
            items[#items+1] = {
                x = rand(room.x, room.x + room.w - 1),
                y = rand(room.y, room.y + room.h - 1),
                type = "medkit",
            }
        end
        if rand() < 0.15 and player.cells < player.cellsNeeded then
            items[#items+1] = {
                x = rand(room.x, room.x + room.w - 1),
                y = rand(room.y, room.y + room.h - 1),
                type = "cell",
            }
        end
        if rand() < 0.2 then
            items[#items+1] = {
                x = rand(room.x, room.x + room.w - 1),
                y = rand(room.y, room.y + room.h - 1),
                type = "oxygen",
            }
        end
        -- Keycard spawn (rare, starts appearing sector 2+)
        if game.sector >= 2 and rand() < 0.12 then
            items[#items+1] = {
                x = rand(room.x, room.x + room.w - 1),
                y = rand(room.y, room.y + room.h - 1),
                type = "keycard",
            }
        end
    end
end

return M