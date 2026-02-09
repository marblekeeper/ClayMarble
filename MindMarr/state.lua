-- state.lua
-- MINDMARR: Shared mutable game state

local game = {
    state = "title",
    sector = 1,
    maxSectors = 7,
    turn = 0,
    messages = {},
    maxMessages = 6,
    shakeTimer = 0,
    shakeIntensity = 0,
    particles = {},
    camX = 0, camY = 0,
    inputCooldown = 0,
    keyWasDown = {},
    marsWhisperTimer = 0,
    pulseTimer = 0,
    won = false,
}

local player = {
    x = 0, y = 0,
    hp = 30, maxHp = 30,
    str = 55,
    def = 40,
    dmgMin = 2, dmgMax = 5,
    armor = 0,
    xp = 0,
    xpNext = 20,
    level = 1,
    sanity = 100,
    oxygen = 100,
    medkits = 1,
    cells = 0,
    cellsNeeded = 3,
    kills = 0,
    critBonus = 5,
    seen = {},
    keycards = 0,
}

local map = {}
local enemies = {}
local items = {}
local shuttle = {x = 0, y = 0}
local elevator = {x = 0, y = 0, revealed = false}

return {
    game = game,
    player = player,
    map = map,
    enemies = enemies,
    items = items,
    shuttle = shuttle,
    elevator = elevator,
}