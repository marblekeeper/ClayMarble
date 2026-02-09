-- constants.lua
-- MINDMARR: Shared constants, palette, config

local TS = 24
local MW, MH = 30, 22

local C = {
    void      = {4, 2, 6},
    wall      = {90, 40, 25},
    wallHi    = {120, 55, 35},
    floor     = {35, 18, 14},
    floorLit  = {55, 28, 22},
    player    = {60, 200, 255},
    infected  = {180, 40, 60},
    infGlow   = {220, 60, 80},
    supply    = {100, 220, 140},
    cell      = {180, 60, 200},
    shuttle   = {255, 220, 80},
    fog       = {6, 3, 8},
    blood     = {140, 30, 40},
    xp        = {200, 120, 255},
    crit      = {255, 200, 60},
    miss      = {100, 80, 80},
    hit       = {255, 80, 60},
    hud_bg    = {12, 6, 10},
    hud_border= {80, 35, 50},
    mars      = {200, 60, 40},
    whisper   = {160, 50, 70},
    sanity    = {120, 180, 255},
    oxygen    = {80, 200, 220},
    keycard   = {255, 200, 50},
    elevator  = {100, 220, 255},
}

local MINDMARR_SAYS = {
    "mindmarr...", "MINDMARR!", "mind...marr...", "mindmarr", "MiNdMaRr",
    "m i n d m a r r", "MINDMARR MINDMARR", "...mindmarr...",
    "mindmarr?", "MINDMARR.", "mind...m a r r...", "mindmarrMINDMARR",
}

local marsWhispers = {
    "The ground pulses beneath you...",
    "You hear your name spoken from below...",
    "The walls are breathing...",
    "Something remembers you were born...",
    "Mars knows your mother's name...",
    "The red dust rearranges into a face...",
    "You feel the planet thinking...",
    "Your shadow moved on its own...",
    "The air tastes like someone else's memory...",
    "mind...marr... NO. Focus.",
    "A voice in the static: 'Join us.'",
    "Your reflection blinked before you did...",
}

local levelChoices = {
    {name = "+5 Max HP & heal"},
    {name = "+8 STR (hit chance)"},
    {name = "+8 DEF (dodge)"},
    {name = "+2 Max Damage"},
    {name = "+1 Suit Armor"},
    {name = "+15 Sanity restored"},
    {name = "+3 Crit Range"},
}

return {
    TS = TS,
    MW = MW,
    MH = MH,
    C = C,
    MINDMARR_SAYS = MINDMARR_SAYS,
    marsWhispers = marsWhispers,
    levelChoices = levelChoices,
}