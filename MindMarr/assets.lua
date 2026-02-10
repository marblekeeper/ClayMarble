-- assets.lua
-- MINDMARR: Optional sprite asset loading and rendering
-- Falls back to procedural rendering if assets unavailable

local K = require("constants")

local M = {}

-- Cache for loaded textures (stores {texture, width, height})
local textureCache = {}

-- Attempt to load a sprite texture
-- Returns {texture, width, height} or nil if loading fails
function M.loadSprite(key)
    if textureCache[key] ~= nil then
        return textureCache[key] -- Return cached (could be false for failed loads)
    end
    
    local path = K.assets.sprites[key]
    if not path then
        textureCache[key] = false
        return nil
    end
    
    -- Attempt to load texture via bridge
    if not bridge or not bridge.loadTexture then
        textureCache[key] = false
        return nil
    end
    
    -- loadTexture returns (textureId, width, height)
    local success, texture, w, h = pcall(bridge.loadTexture, path)
    if success and texture then
        textureCache[key] = {texture = texture, width = w or K.TS, height = h or K.TS}
        return textureCache[key]
    else
        textureCache[key] = false
        return nil
    end
end

-- Draw a sprite if available, return true if drawn
-- x, y: screen coordinates (top-left of tile)
-- w, h: width and height to draw (optional - uses sprite's natural size if nil)
-- key: sprite key from K.assets.sprites
function M.tryDrawSprite(key, x, y, w, h)
    local spriteData = M.loadSprite(key)
    if not spriteData or spriteData == false then
        return false
    end
    
    -- Use natural sprite dimensions if not specified
    local drawW = w or spriteData.width
    local drawH = h or spriteData.height
    
    -- Center the sprite in the tile if it's smaller than tile size
    local offsetX = 0
    local offsetY = 0
    if not w and drawW < K.TS then
        offsetX = (K.TS - drawW) / 2
    end
    if not h and drawH < K.TS then
        offsetY = (K.TS - drawH) / 2
    end
    
    if bridge and bridge.drawTexture then
        bridge.drawTexture(spriteData.texture, x + offsetX, y + offsetY, drawW, drawH)
        return true
    end
    
    return false
end

-- Clear texture cache (call on game reset if needed)
function M.clearCache()
    textureCache = {}
end

return M