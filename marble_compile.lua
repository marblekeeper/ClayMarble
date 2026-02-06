#!/usr/bin/env lua
-- ============================================================
-- marble_compile.lua -- MarbleScript Compiler v0.1
--
-- Parses .marble files and emits a C header (marble_gen.h)
-- containing all enums, lookup tables, and struct definitions
-- that the runtime needs.
--
-- Usage:
--   lua marble_compile.lua oak_forest.marble
--   lua marble_compile.lua oak_forest.marble -o marble_gen.h
--
-- Compatible with Lua 5.3+ (also works under texlua/luatex)
--
-- WHAT IT GENERATES:
--   - MaterialID enum + MATERIAL_HARDNESS[] + MATERIAL_NAMES[]
--   - SkillID enum
--   - AnatomyFlag enum (bitfield)
--   - BodyPartID enum + BODYPART_NAMES[]
--   - ConditionID enum
--   - EffectID enum
--   - CapabilityID enum + CAPABILITY_DEFS[]
--   - AffordanceID enum + AFFORDANCE_DEFS[]
--   - VerbID enum + VERB_DEFS[]
--   - SystemID enum + SYSTEM_FREQ[]
--   - Layer template initializer functions
--   - World config #defines
--
-- WHAT IT DOES NOT GENERATE (yet):
--   - component struct definitions (Phase 2)
--   - entity initialization code (Phase 2)
-- ============================================================

local VERSION = "0.1"

-- ============================================================
-- LEXER: line-by-line, token-by-token
-- ============================================================

local function read_file(path)
    local f = io.open(path, "r")
    if not f then
        io.stderr:write("ERROR: cannot open file: " .. path .. "\n")
        os.exit(1)
    end
    local content = f:read("*a")
    f:close()
    return content
end

-- Strip comments (-- to end of line) and return lines
local function strip_comments(source)
    local lines = {}
    for line in source:gmatch("[^\r\n]+") do
        -- Remove comment portion
        local stripped = line:gsub("%-%-.*$", "")
        lines[#lines + 1] = stripped
    end
    return lines
end

-- Tokenize a single line into words/symbols
local function tokenize_line(line)
    local tokens = {}
    -- Match: quoted strings, words, numbers, symbols
    for tok in line:gmatch('[%w_%.@]+[%w_%.@]*|"[^"]*"|[{}>,<=%+%-]') do
        tokens[#tokens + 1] = tok
    end
    -- Fallback: simpler pattern if above misses
    if #tokens == 0 then
        for tok in line:gmatch('%S+') do
            -- Strip trailing commas
            tok = tok:gsub(",+$", "")
            if tok ~= "" and tok ~= "," then
                tokens[#tokens + 1] = tok
            end
        end
    end
    return tokens
end

-- ============================================================
-- PARSER: block-level
-- Reads tokens and builds an AST (table of typed blocks)
-- ============================================================

local function parse(source)
    local lines = strip_comments(source)
    local ast = {
        world = nil,
        materials = {},
        skills = {},
        anatomy = {},
        bodyparts = {},
        conditions = {},
        effects = {},
        capabilities = {},
        affordances = {},
        verbs = {},
        systems = {},
        layers = {},
    }

    local i = 1
    while i <= #lines do
        local tokens = tokenize_line(lines[i])

        if #tokens >= 1 then
            local keyword = tokens[1]

            if keyword == "world" then
                -- world "Name" { ... }
                local name = (tokens[2] or ""):gsub('"', '')
                local block = { name = name }
                i = i + 1
                while i <= #lines do
                    local t = tokenize_line(lines[i])
                    if #t >= 1 and t[1] == "}" then break end
                    if #t >= 2 then
                        block[t[1]] = tonumber(t[2]) or t[2]
                    end
                    i = i + 1
                end
                ast.world = block

            elseif keyword == "material" then
                -- material Name { hardness N }
                -- Can be single-line or multi-line
                local name = tokens[2]
                local block = { name = name }
                -- Check if entire block is on one line
                local full_line = lines[i]
                local inline_fields = full_line:match("{(.-)}")
                if inline_fields then
                    -- Single-line block: parse fields from inline content
                    for k, v in inline_fields:gmatch("(%w+)%s+(%S+)") do
                        block[k] = tonumber(v) or v
                    end
                elseif tokens[3] == "{" then
                    -- Multi-line block
                    i = i + 1
                    while i <= #lines do
                        local t = tokenize_line(lines[i])
                        if #t >= 1 and t[1] == "}" then break end
                        if #t >= 2 then
                            block[t[1]] = tonumber(t[2]) or t[2]
                        end
                        i = i + 1
                    end
                end
                ast.materials[#ast.materials + 1] = block

            elseif keyword == "skill" then
                ast.skills[#ast.skills + 1] = { name = tokens[2] }

            elseif keyword == "anatomy" then
                ast.anatomy[#ast.anatomy + 1] = { name = tokens[2] }

            elseif keyword == "bodypart" then
                ast.bodyparts[#ast.bodyparts + 1] = { name = tokens[2] }

            elseif keyword == "condition" then
                local name = tokens[2]
                local block = { name = name, check = "" }
                i = i + 1
                while i <= #lines do
                    local t = tokenize_line(lines[i])
                    if #t >= 1 and t[1] == "}" then break end
                    if #t >= 2 and t[1] == "check" then
                        -- Capture the entire rest of the line as the check expression
                        local check_line = lines[i]:match("check%s+(.+)")
                        if check_line then
                            block.check = check_line:gsub("%s+$", "")
                        end
                    end
                    i = i + 1
                end
                ast.conditions[#ast.conditions + 1] = block

            elseif keyword == "effect" then
                local name = tokens[2]
                local block = { name = name, apply = "" }
                i = i + 1
                while i <= #lines do
                    local t = tokenize_line(lines[i])
                    if #t >= 1 and t[1] == "}" then break end
                    if #t >= 2 and t[1] == "apply" then
                        local apply_line = lines[i]:match("apply%s+(.+)")
                        if apply_line then
                            block.apply = apply_line:gsub("%s+$", "")
                        end
                    end
                    i = i + 1
                end
                ast.effects[#ast.effects + 1] = block

            elseif keyword == "capability" then
                local name = tokens[2]
                local block = { name = name, require_anatomy = {}, skill = nil,
                                min_skill = 0, body_part = nil }
                i = i + 1
                while i <= #lines do
                    local t = tokenize_line(lines[i])
                    if #t >= 1 and t[1] == "}" then break end
                    if #t >= 2 then
                        if t[1] == "require" then
                            -- require Anatomy.X or require BodyPart.X.integrity > 0
                            local ref = t[2]
                            if ref:match("^Anatomy%.") then
                                local anat_name = ref:match("^Anatomy%.(.+)")
                                block.require_anatomy[#block.require_anatomy + 1] = anat_name
                            elseif ref:match("^BodyPart%.") then
                                local bp_name = ref:match("^BodyPart%.(.-)%.")
                                block.body_part = bp_name
                            end
                        elseif t[1] == "skill" then
                            block.skill = t[2]
                        elseif t[1] == "min_skill" then
                            block.min_skill = tonumber(t[2]) or 0
                        end
                    end
                    i = i + 1
                end
                ast.capabilities[#ast.capabilities + 1] = block

            elseif keyword == "affordance" then
                local name = tokens[2]
                local block = { name = name, require_cap = nil, condition = nil,
                                on_success = nil, difficulty = 0,
                                crit_fail_threshold = 0, crit_fail_bodypart = nil,
                                crit_fail_damage = 0 }
                i = i + 1
                while i <= #lines do
                    local t = tokenize_line(lines[i])
                    if #t >= 1 and t[1] == "}" then break end
                    if #t >= 2 then
                        if t[1] == "require_cap" then
                            block.require_cap = t[2]
                        elseif t[1] == "condition" then
                            block.condition = t[2]
                        elseif t[1] == "on_success" then
                            block.on_success = t[2]
                        elseif t[1] == "difficulty" then
                            block.difficulty = tonumber(t[2]) or 0
                        elseif t[1] == "crit_fail_threshold" then
                            block.crit_fail_threshold = tonumber(t[2]) or 0
                        elseif t[1] == "crit_fail_bodypart" then
                            block.crit_fail_bodypart = t[2]
                        elseif t[1] == "crit_fail_damage" then
                            block.crit_fail_damage = tonumber(t[2]) or 0
                        end
                    end
                    i = i + 1
                end
                ast.affordances[#ast.affordances + 1] = block

            elseif keyword == "verb" then
                local name = tokens[2]
                local block = { name = name, actor_cap = nil, target_aff = nil }
                i = i + 1
                while i <= #lines do
                    local t = tokenize_line(lines[i])
                    if #t >= 1 and t[1] == "}" then break end
                    if #t >= 2 then
                        if t[1] == "actor_cap" then
                            block.actor_cap = t[2]
                        elseif t[1] == "target_aff" then
                            block.target_aff = t[2]
                        end
                    end
                    i = i + 1
                end
                ast.verbs[#ast.verbs + 1] = block

            elseif keyword == "system" then
                local name = tokens[2]
                local block = { name = name, frequency = 1, requires = {} }
                i = i + 1
                while i <= #lines do
                    local t = tokenize_line(lines[i])
                    if #t >= 1 and t[1] == "}" then break end
                    if #t >= 2 then
                        if t[1] == "frequency" then
                            block.frequency = tonumber(t[2]) or 1
                        elseif t[1] == "requires" then
                            for j = 2, #t do
                                block.requires[#block.requires + 1] = t[j]
                            end
                        end
                    end
                    i = i + 1
                end
                ast.systems[#ast.systems + 1] = block

            elseif keyword == "layer" then
                local name = tokens[2]
                local block = { name = name, entries = {} }
                local full_line = lines[i]
                local inline = full_line:match("{(.-)}")
                if inline then
                    -- Single-line: "Bark integrity 3, Wood integrity 10"
                    for mat, integ in inline:gmatch("(%w+)%s+integrity%s+(%d+)") do
                        block.entries[#block.entries + 1] = {
                            material = mat,
                            integrity = tonumber(integ)
                        }
                    end
                else
                    i = i + 1
                    while i <= #lines do
                        local t = tokenize_line(lines[i])
                        if #t >= 1 and t[1] == "}" then break end
                        if #t >= 3 and t[2] == "integrity" then
                            block.entries[#block.entries + 1] = {
                                material = t[1],
                                integrity = tonumber(t[3]) or 1
                            }
                        end
                        i = i + 1
                    end
                end
                ast.layers[#ast.layers + 1] = block
            end
            -- entity blocks: skip for now (Phase 2)
        end

        i = i + 1
    end

    return ast
end

-- ============================================================
-- NAME CONVERSION UTILITIES
-- ============================================================

-- PascalCase -> UPPER_SNAKE_CASE
local function to_upper_snake(name)
    -- Insert underscore before uppercase letters (except first)
    local result = name:gsub("(%u)", function(c) return "_" .. c end)
    result = result:gsub("^_", "") -- remove leading underscore
    return result:upper()
end

-- Lookup helper: find index of name in ordered list
local function find_index(list, name)
    for idx, item in ipairs(list) do
        if item.name == name then return idx end
    end
    return nil
end

-- ============================================================
-- CODE GENERATOR
-- ============================================================

local function generate(ast)
    local out = {}
    local function emit(...)
        for _, s in ipairs({...}) do
            out[#out + 1] = s
        end
    end
    local function emitf(fmt, ...)
        out[#out + 1] = string.format(fmt, ...)
    end

    -- ---- HEADER ----
    emit("/*")
    emit(" * marble_gen.h -- AUTO-GENERATED by marble_compile.lua v" .. VERSION)
    emit(" *")
    emit(" * Source: " .. (ast._source_file or "unknown"))
    emit(" * DO NOT EDIT -- regenerate from .marble source")
    emit(" */")
    emit("")
    emit("#ifndef MARBLE_GEN_H")
    emit("#define MARBLE_GEN_H")
    emit("")
    emit('#include "marble_core.h"')
    emit("")

    -- ---- WORLD CONFIG ----
    if ast.world then
        emit("/* ---- World Configuration ---- */")
        local w = ast.world
        if w.max_entities then emitf("#define MC_GEN_MAX_ENTITIES     %d", w.max_entities) end
        if w.tick_interval_ms then emitf("#define MC_GEN_TICK_INTERVAL_US %d", w.tick_interval_ms * 1000) end
        if w.seed then emitf("#define MC_GEN_WORLD_SEED       %du", w.seed) end
        if w.max_layers then emitf("#define MC_GEN_MAX_LAYERS       %d", w.max_layers) end
        if w.max_body_parts then emitf("#define MC_GEN_MAX_BODY_PARTS   %d", w.max_body_parts) end
        if w.max_skills then emitf("#define MC_GEN_MAX_SKILLS       %d", w.max_skills) end
        emit("")
    end

    -- ---- MATERIALS ----
    if #ast.materials > 0 then
        emit("/* ---- Materials ---- */")
        emit("typedef enum {")
        emit("    MAT_NONE = 0,")
        for idx, mat in ipairs(ast.materials) do
            emitf("    MAT_%s = %d,", to_upper_snake(mat.name), idx)
        end
        emitf("    MAT_COUNT = %d", #ast.materials + 1)
        emit("} MaterialID;")
        emit("")

        emit("static const char* MATERIAL_NAMES[MAT_COUNT] = {")
        emit('    "None",')
        for _, mat in ipairs(ast.materials) do
            emitf('    "%s",', mat.name)
        end
        emit("};")
        emit("")

        emit("static const int32_t MATERIAL_HARDNESS[MAT_COUNT] = {")
        emit("    /*NONE*/ 0,")
        for _, mat in ipairs(ast.materials) do
            emitf("    /*%s*/ %d,", to_upper_snake(mat.name), mat.hardness or 0)
        end
        emit("};")
        emit("")
    end

    -- ---- LAYER / LAYERSTACK STRUCTS ----
    emit("/* ---- Layer System ---- */")
    if ast.world and ast.world.max_layers then
        emitf("#define MAX_LAYERS %d", ast.world.max_layers)
    else
        emit("#define MAX_LAYERS 4")
    end
    emit("")
    emit("typedef struct {")
    emit("    MaterialID material;")
    emit("    int32_t    integrity;")
    emit("    int32_t    max_integrity;")
    emit("} Layer;")
    emit("")
    emit("typedef struct {")
    emit("    Layer    layers[MAX_LAYERS];")
    emit("    uint32_t layer_count;")
    emit("} CLayerStack;")
    emit("")

    -- ---- LAYER TEMPLATES ----
    if #ast.layers > 0 then
        emit("/* ---- Layer Templates ---- */")
        for _, tmpl in ipairs(ast.layers) do
            emitf("static void layer_template_%s(CLayerStack* ls) {", tmpl.name)
            emitf("    ls->layer_count = %d;", #tmpl.entries)
            for idx, entry in ipairs(tmpl.entries) do
                local mat_enum = "MAT_" .. to_upper_snake(entry.material)
                emitf("    ls->layers[%d].material      = %s;", idx - 1, mat_enum)
                emitf("    ls->layers[%d].integrity     = %d;", idx - 1, entry.integrity)
                emitf("    ls->layers[%d].max_integrity = %d;", idx - 1, entry.integrity)
            end
            emit("}")
            emit("")
        end
    end

    -- ---- SKILLS ----
    if #ast.skills > 0 then
        emit("/* ---- Skills ---- */")
        emit("typedef enum {")
        emit("    SKILL_NONE = 0,")
        for idx, sk in ipairs(ast.skills) do
            emitf("    SKILL_%s = %d,", to_upper_snake(sk.name), idx)
        end
        emitf("    SKILL_COUNT = %d", #ast.skills + 1)
        emit("} SkillID;")
        emit("")
        if ast.world and ast.world.max_skills then
            emitf("#define MAX_SKILLS %d", ast.world.max_skills)
        else
            emit("#define MAX_SKILLS 8")
        end
        emit("")
        emit("typedef struct {")
        emit("    int32_t level[MAX_SKILLS];")
        emit("} CSkills;")
        emit("")
    end

    -- ---- ANATOMY ----
    if #ast.anatomy > 0 then
        emit("/* ---- Anatomy Flags ---- */")
        emit("typedef enum {")
        for idx, an in ipairs(ast.anatomy) do
            emitf("    ANAT_%s = (1 << %d),", to_upper_snake(an.name), idx - 1)
        end
        emit("} AnatomyFlag;")
        emit("")
        emit("typedef struct {")
        emit("    uint32_t flags;")
        emit("} CAnatomy;")
        emit("")
    end

    -- ---- BODY PARTS ----
    if #ast.bodyparts > 0 then
        emit("/* ---- Body Parts ---- */")
        emit("typedef enum {")
        emit("    BODYPART_NONE = 0,")
        for idx, bp in ipairs(ast.bodyparts) do
            emitf("    BODYPART_%s = %d,", to_upper_snake(bp.name), idx)
        end
        emitf("    BODYPART_COUNT = %d", #ast.bodyparts + 1)
        emit("} BodyPartID;")
        emit("")

        emit("static const char* BODYPART_NAMES[BODYPART_COUNT] = {")
        emit('    "None",')
        for _, bp in ipairs(ast.bodyparts) do
            -- Insert spaces before capitals for display name
            local display = bp.name:gsub("(%u)", " %1"):gsub("^ ", "")
            emitf('    "%s",', display)
        end
        emit("};")
        emit("")

        if ast.world and ast.world.max_body_parts then
            emitf("#define MAX_BODY_PARTS %d", ast.world.max_body_parts)
        else
            emit("#define MAX_BODY_PARTS 6")
        end
        emit("")
        emit("typedef struct {")
        emit("    EntityID part_entity[MAX_BODY_PARTS];")
        emit("} CBodyParts;")
        emit("")
    end

    -- ---- CONDITIONS ----
    emit("/* ---- Conditions ---- */")
    emit("typedef enum {")
    emit("    COND_NONE = 0,")
    for idx, cond in ipairs(ast.conditions) do
        emitf("    COND_%s = %d,", to_upper_snake(cond.name), idx)
    end
    emitf("    COND_COUNT = %d", #ast.conditions + 1)
    emit("} ConditionID;")
    emit("")

    -- ---- EFFECTS ----
    emit("/* ---- Effects ---- */")
    emit("typedef enum {")
    emit("    EFFECT_NONE = 0,")
    for idx, eff in ipairs(ast.effects) do
        emitf("    EFFECT_%s = %d,", to_upper_snake(eff.name), idx)
    end
    emitf("    EFFECT_COUNT = %d", #ast.effects + 1)
    emit("} EffectID;")
    emit("")

    -- ---- CAPABILITIES ----
    if #ast.capabilities > 0 then
        emit("/* ---- Capabilities ---- */")
        emit("typedef enum {")
        emit("    CAP_NONE = 0,")
        for idx, cap in ipairs(ast.capabilities) do
            emitf("    CAP_%s = %d,", to_upper_snake(cap.name), idx)
        end
        emitf("    CAP_COUNT = %d", #ast.capabilities + 1)
        emit("} CapabilityID;")
        emit("")

        emit("typedef struct {")
        emit("    uint32_t   required_anatomy;")
        emit("    SkillID    required_skill;")
        emit("    int32_t    min_skill_level;")
        emit("    BodyPartID body_part_required;")
        emit("} CapabilityDef;")
        emit("")

        emit("static const CapabilityDef CAPABILITY_DEFS[CAP_COUNT] = {")
        emit("    /*CAP_NONE*/ { 0, SKILL_NONE, 0, BODYPART_NONE },")
        for _, cap in ipairs(ast.capabilities) do
            -- Build anatomy flags
            local anat_parts = {}
            for _, a in ipairs(cap.require_anatomy) do
                anat_parts[#anat_parts + 1] = "ANAT_" .. to_upper_snake(a)
            end
            local anat_str = #anat_parts > 0 and table.concat(anat_parts, " | ") or "0"

            -- Skill
            local skill_str = cap.skill and ("SKILL_" .. to_upper_snake(cap.skill)) or "SKILL_NONE"

            -- Body part
            local bp_str = cap.body_part and ("BODYPART_" .. to_upper_snake(cap.body_part)) or "BODYPART_NONE"

            emitf("    /*CAP_%s*/ { %s, %s, %d, %s },",
                to_upper_snake(cap.name), anat_str, skill_str, cap.min_skill, bp_str)
        end
        emit("};")
        emit("")

        emit("typedef struct {")
        emit("    uint32_t flags;")
        emit("} CCapabilities;")
        emit("")
    end

    -- ---- AFFORDANCES ----
    if #ast.affordances > 0 then
        emit("/* ---- Affordances ---- */")
        emit("typedef enum {")
        emit("    AFF_NONE = 0,")
        for idx, aff in ipairs(ast.affordances) do
            emitf("    AFF_%s = %d,", to_upper_snake(aff.name), idx)
        end
        emitf("    AFF_COUNT = %d", #ast.affordances + 1)
        emit("} AffordanceID;")
        emit("")

        emit("typedef struct {")
        emit("    CapabilityID required_cap;")
        emit("    ConditionID  condition;")
        emit("    EffectID     on_success;")
        emit("    int32_t      difficulty;")
        emit("    int32_t      crit_fail_threshold;")
        emit("    BodyPartID   crit_fail_bodypart;")
        emit("    int32_t      crit_fail_damage;")
        emit("} AffordanceDef;")
        emit("")

        emit("static const AffordanceDef AFFORDANCE_DEFS[AFF_COUNT] = {")
        emit("    /*AFF_NONE*/ { CAP_NONE, COND_NONE, EFFECT_NONE, 0, 0, BODYPART_NONE, 0 },")
        for _, aff in ipairs(ast.affordances) do
            local cap_str = aff.require_cap and ("CAP_" .. to_upper_snake(aff.require_cap)) or "CAP_NONE"
            local cond_str = aff.condition and ("COND_" .. to_upper_snake(aff.condition)) or "COND_NONE"
            local eff_str = aff.on_success and ("EFFECT_" .. to_upper_snake(aff.on_success)) or "EFFECT_NONE"
            local bp_str = aff.crit_fail_bodypart and ("BODYPART_" .. to_upper_snake(aff.crit_fail_bodypart)) or "BODYPART_NONE"

            emitf("    /*AFF_%s*/ { %s, %s, %s, %d, %d, %s, %d },",
                to_upper_snake(aff.name), cap_str, cond_str, eff_str,
                aff.difficulty, aff.crit_fail_threshold, bp_str, aff.crit_fail_damage)
        end
        emit("};")
        emit("")

        emit("typedef struct {")
        emit("    uint32_t flags;")
        emit("} CAffordances;")
        emit("")
    end

    -- ---- TOOL COMPONENT ----
    emit("/* ---- Tool Component ---- */")
    emit("typedef struct {")
    emit("    MaterialID material;")
    emit("} CTool;")
    emit("")

    -- ---- VERBS ----
    if #ast.verbs > 0 then
        emit("/* ---- Verbs ---- */")
        emit("typedef enum {")
        emit("    VERB_NONE = 0,")
        for idx, verb in ipairs(ast.verbs) do
            emitf("    VERB_%s = %d,", to_upper_snake(verb.name), idx)
        end
        emitf("    VERB_COUNT = %d", #ast.verbs + 1)
        emit("} VerbID;")
        emit("")

        emit("typedef struct {")
        emit("    CapabilityID actor_cap;")
        emit("    AffordanceID target_aff;")
        emit("} VerbDef;")
        emit("")

        emit("static const VerbDef VERB_DEFS[VERB_COUNT] = {")
        emit("    /*VERB_NONE*/ { CAP_NONE, AFF_NONE },")
        for _, verb in ipairs(ast.verbs) do
            local cap_str = verb.actor_cap and ("CAP_" .. to_upper_snake(verb.actor_cap)) or "CAP_NONE"
            local aff_str = verb.target_aff and ("AFF_" .. to_upper_snake(verb.target_aff)) or "AFF_NONE"
            emitf("    /*VERB_%s*/ { %s, %s },", to_upper_snake(verb.name), cap_str, aff_str)
        end
        emit("};")
        emit("")
    end

    -- ---- SYSTEMS ----
    if #ast.systems > 0 then
        emit("/* ---- Systems ---- */")
        emit("typedef enum {")
        for idx, sys in ipairs(ast.systems) do
            emitf("    SYS_%s = %d,", to_upper_snake(sys.name), idx - 1)
        end
        emitf("    SYS_COUNT = %d", #ast.systems)
        emit("} SystemID;")
        emit("")

        emit("static const uint32_t SYSTEM_FREQ[SYS_COUNT] = {")
        for _, sys in ipairs(ast.systems) do
            emitf("    /*SYS_%s*/ %d,", to_upper_snake(sys.name), sys.frequency)
        end
        emit("};")
        emit("")
    end

    -- ---- INTERACTION REQUEST ----
    emit("/* ---- Interaction Request ---- */")
    emit("#define MAX_INTERACTION_REQUESTS 64")
    emit("")
    emit("typedef struct {")
    emit("    EntityID actor;")
    emit("    EntityID target;")
    emit("    VerbID   verb;")
    emit("} InteractionRequest;")
    emit("")

    -- ---- FOOTER ----
    emit("#endif /* MARBLE_GEN_H */")

    return table.concat(out, "\n") .. "\n"
end

-- ============================================================
-- MAIN
-- ============================================================

local function main()
    local input_file = arg[1]
    local output_file = "marble_gen.h"

    if not input_file then
        io.stderr:write("Usage: lua marble_compile.lua <input.marble> [-o output.h]\n")
        os.exit(1)
    end

    -- Parse -o flag
    if arg[2] == "-o" and arg[3] then
        output_file = arg[3]
    end

    io.write("marble_compile v" .. VERSION .. "\n")
    io.write("  Input:  " .. input_file .. "\n")
    io.write("  Output: " .. output_file .. "\n")

    local source = read_file(input_file)
    local ast = parse(source)
    ast._source_file = input_file

    -- Stats
    io.write("  Parsed:\n")
    io.write("    materials:    " .. #ast.materials .. "\n")
    io.write("    skills:       " .. #ast.skills .. "\n")
    io.write("    anatomy:      " .. #ast.anatomy .. "\n")
    io.write("    bodyparts:    " .. #ast.bodyparts .. "\n")
    io.write("    conditions:   " .. #ast.conditions .. "\n")
    io.write("    effects:      " .. #ast.effects .. "\n")
    io.write("    capabilities: " .. #ast.capabilities .. "\n")
    io.write("    affordances:  " .. #ast.affordances .. "\n")
    io.write("    verbs:        " .. #ast.verbs .. "\n")
    io.write("    systems:      " .. #ast.systems .. "\n")
    io.write("    layers:       " .. #ast.layers .. "\n")

    local code = generate(ast)

    local f = io.open(output_file, "w")
    if not f then
        io.stderr:write("ERROR: cannot write to " .. output_file .. "\n")
        os.exit(1)
    end
    f:write(code)
    f:close()

    io.write("  Generated: " .. output_file .. " (" .. #code .. " bytes)\n")
    io.write("  Done.\n")
end

main()