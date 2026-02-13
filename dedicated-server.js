// dedicated-server.js - Server simulates the entire game
const WebSocket = require('ws');

const PORT = 8080;
const server = new WebSocket.Server({ port: PORT, host: '0.0.0.0' });

const W = 800, H = 600;
const TAU = Math.PI * 2;

// Game state
const game = {
    wave: 1,
    waveTimer: 0,
    enemies: [],
    pickups: [],
    players: new Map(), // playerId -> player data
    nextColorIdx: 1,
};

const playerColors = [
    [50, 255, 80], [255, 50, 255], [50, 200, 255], [255, 200, 50],
    [255, 100, 50], [150, 50, 255], [50, 255, 200], [255, 255, 50]
];

function rand(min, max) {
    if (max === undefined) { max = min; min = 0; }
    return min + Math.random() * (max - min);
}

function wrap(x, y) {
    if (x < -20) x += W + 40;
    if (x > W + 20) x -= W + 40;
    if (y < -20) y += H + 40;
    if (y > H + 20) y -= H + 40;
    return [x, y];
}

function dist(x1, y1, x2, y2) {
    const dx = x1 - x2, dy = y1 - y2;
    return Math.sqrt(dx*dx + dy*dy);
}

function createPlayer(id, colorIdx) {
    const angle = Math.random() * TAU;
    const d = 150 + Math.random() * 100;
    return {
        id, colorIdx,
        x: W/2 + Math.cos(angle) * d,
        y: H/2 + Math.sin(angle) * d,
        vx: 0, vy: 0,
        angle: -Math.PI/2,
        health: 3,
        maxHealth: 3,
        bullets: [],
        fireRate: 0.12,
        fireCooldown: 0,
        invincible: 3.0,
        alive: true,
        respawnTimer: 0,
        score: 0,
        // Input state
        input: { left: false, right: false, up: false, space: false }
    };
}

function spawnEnemy(type) {
    const side = Math.floor(Math.random() * 4);
    let x, y;
    if (side === 0) { x = rand(0, W); y = -20; }
    else if (side === 1) { x = rand(0, W); y = H + 20; }
    else if (side === 2) { x = -20; y = rand(0, H); }
    else { x = W + 20; y = rand(0, H); }

    let targetX = 400, targetY = 300;
    for (const p of game.players.values()) {
        if (p.alive) { targetX = p.x; targetY = p.y; break; }
    }

    const e = {
        x, y, vx: 0, vy: 0,
        angle: Math.random() * TAU,
        type: type || 'grunt',
        bullets: [],
        fireTimer: Math.random() * 2
    };

    const a = Math.atan2(targetY - y, targetX - x);
    
    if (e.type === 'grunt') {
        e.vx = Math.cos(a + (rand(-0.8, 0.8))) * (60 + game.wave * 8);
        e.vy = Math.sin(a + (rand(-0.8, 0.8))) * (60 + game.wave * 8);
        e.radius = 10; e.health = 1; e.score = 100;
    } else if (e.type === 'tank') {
        e.vx = Math.cos(a) * (35 + game.wave * 4);
        e.vy = Math.sin(a) * (35 + game.wave * 4);
        e.radius = 16; e.health = 3; e.score = 300;
    } else if (e.type === 'fast') {
        e.vx = Math.cos(a + (rand(-0.3, 0.3))) * (140 + game.wave * 10);
        e.vy = Math.sin(a + (rand(-0.3, 0.3))) * (140 + game.wave * 10);
        e.radius = 8; e.health = 1; e.score = 200;
    } else if (e.type === 'shooter') {
        e.vx = Math.cos(a + (rand(-1, 1))) * (50 + game.wave * 5);
        e.vy = Math.sin(a + (rand(-1, 1))) * (50 + game.wave * 5);
        e.radius = 12; e.health = 2; e.score = 250;
        e.fireRate = Math.max(0.8, 2.0 - game.wave * 0.1);
    }

    game.enemies.push(e);
}

function spawnWave() {
    const w = game.wave;
    const counts = {
        grunt: 3 + w * 2,
        tank: Math.floor(w / 2),
        fast: Math.floor(w / 3) + (w > 2 ? 1 : 0),
        shooter: Math.floor(w / 4) + (w > 3 ? 1 : 0)
    };
    
    for (let i = 0; i < counts.grunt; i++) spawnEnemy('grunt');
    for (let i = 0; i < counts.tank; i++) spawnEnemy('tank');
    for (let i = 0; i < counts.fast; i++) spawnEnemy('fast');
    for (let i = 0; i < counts.shooter; i++) spawnEnemy('shooter');
}

function fireBullet(entity, angle, speed) {
    entity.bullets.push({
        x: entity.x + Math.cos(angle) * (entity.radius || 12),
        y: entity.y + Math.sin(angle) * (entity.radius || 12),
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        life: 2.5
    });
}

function updateGame(dt) {
    const THRUST = 320, DRAG = 0.985, ROT_SPEED = 4.5;
    
    // Update players
    for (const p of game.players.values()) {
        if (p.alive) {
            if (p.input.left) p.angle -= ROT_SPEED * dt;
            if (p.input.right) p.angle += ROT_SPEED * dt;
            
            if (p.input.up) {
                p.vx += Math.cos(p.angle) * THRUST * dt;
                p.vy += Math.sin(p.angle) * THRUST * dt;
            }
            
            p.vx *= DRAG;
            p.vy *= DRAG;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            [p.x, p.y] = wrap(p.x, p.y);
            
            p.invincible = Math.max(0, p.invincible - dt);
            p.fireCooldown = Math.max(0, p.fireCooldown - dt);
            
            if (p.input.space && p.fireCooldown <= 0) {
                fireBullet(p, p.angle, 500);
                p.fireCooldown = p.fireRate;
            }
            
            // Update bullets
            for (let i = p.bullets.length - 1; i >= 0; i--) {
                const b = p.bullets[i];
                b.x += b.vx * dt;
                b.y += b.vy * dt;
                [b.x, b.y] = wrap(b.x, b.y);
                b.life -= dt;
                if (b.life <= 0) p.bullets.splice(i, 1);
            }
        } else {
            p.respawnTimer -= dt;
            if (p.respawnTimer <= 0) {
                const angle = Math.random() * TAU;
                const d = 150 + Math.random() * 100;
                p.x = W/2 + Math.cos(angle) * d;
                p.y = H/2 + Math.sin(angle) * d;
                p.vx = 0; p.vy = 0;
                p.alive = true;
                p.invincible = 3.0;
                p.health = p.maxHealth;
                p.bullets = [];
            }
        }
    }
    
    // Update enemies
    for (let ei = game.enemies.length - 1; ei >= 0; ei--) {
        const e = game.enemies[ei];
        e.x += e.vx * dt;
        e.y += e.vy * dt;
        [e.x, e.y] = wrap(e.x, e.y);
        
        // Shooter fires
        if (e.type === 'shooter' && e.fireRate) {
            e.fireTimer -= dt;
            if (e.fireTimer <= 0) {
                let nearest = null, nearestDist = 99999;
                for (const p of game.players.values()) {
                    if (p.alive) {
                        const d = dist(e.x, e.y, p.x, p.y);
                        if (d < nearestDist) { nearestDist = d; nearest = p; }
                    }
                }
                if (nearest) {
                    const a = Math.atan2(nearest.y - e.y, nearest.x - e.x);
                    fireBullet(e, a, 200 + game.wave * 10);
                    e.fireTimer = e.fireRate;
                }
            }
        }
        
        // Enemy bullets
        for (let bi = e.bullets.length - 1; bi >= 0; bi--) {
            const b = e.bullets[bi];
            b.x += b.vx * dt;
            b.y += b.vy * dt;
            [b.x, b.y] = wrap(b.x, b.y);
            b.life -= dt;
            
            // Hit players
            for (const p of game.players.values()) {
                if (p.alive && p.invincible <= 0 && b.life > 0) {
                    if (dist(b.x, b.y, p.x, p.y) < 12) {
                        b.life = 0;
                        p.health--;
                        if (p.health <= 0) {
                            p.alive = false;
                            p.respawnTimer = 2.0;
                        }
                    }
                }
            }
            
            if (b.life <= 0) e.bullets.splice(bi, 1);
        }
        
        // Player bullets hit enemy
        for (const p of game.players.values()) {
            for (let bi = p.bullets.length - 1; bi >= 0; bi--) {
                const b = p.bullets[bi];
                if (b.life > 0 && dist(b.x, b.y, e.x, e.y) < e.radius + 3) {
                    b.life = 0;
                    p.bullets.splice(bi, 1);
                    e.health--;
                    if (e.health <= 0) {
                        p.score += e.score;
                        game.enemies.splice(ei, 1);
                        break;
                    }
                }
            }
        }
        
        // Collide with players
        for (const p of game.players.values()) {
            if (p.alive && p.invincible <= 0) {
                if (dist(e.x, e.y, p.x, p.y) < e.radius + 12) {
                    p.health -= 2;
                    e.health -= 2;
                    if (p.health <= 0) {
                        p.alive = false;
                        p.respawnTimer = 2.0;
                    }
                    if (e.health <= 0) {
                        p.score += e.score;
                        game.enemies.splice(ei, 1);
                        break;
                    }
                }
            }
        }
    }
    
    // Wave spawning
    if (game.enemies.length === 0) {
        game.waveTimer += dt;
        if (game.waveTimer > 2.0) {
            game.wave++;
            game.waveTimer = 0;
            spawnWave();
        }
    }
}

function broadcastGameState() {
    const state = {
        type: 'state',
        wave: game.wave,
        enemies: game.enemies.map(e => ({
            x: e.x, y: e.y, type: e.type, 
            bullets: e.bullets, radius: e.radius
        })),
        players: {}
    };
    
    for (const [id, p] of game.players) {
        state.players[id] = {
            x: p.x, y: p.y, vx: p.vx, vy: p.vy,
            angle: p.angle, health: p.health, alive: p.alive,
            invincible: p.invincible, score: p.score,
            colorIdx: p.colorIdx, bullets: p.bullets
        };
    }
    
    broadcast(state);
}

function broadcast(msg) {
    const data = JSON.stringify(msg);
    for (const [socket] of clients) {
        if (socket.readyState === WebSocket.OPEN) {
            socket.send(data);
        }
    }
}

const clients = new Map(); // socket -> playerId

server.on('connection', (socket) => {
    const playerId = `p${Date.now().toString(36).slice(-4)}`;
    const colorIdx = game.nextColorIdx++;
    
    const player = createPlayer(playerId, colorIdx);
    game.players.set(playerId, player);
    clients.set(socket, playerId);
    
    console.log(`✅ ${playerId} joined (color ${colorIdx}) - ${game.players.size} total`);
    
    socket.send(JSON.stringify({
        type: 'welcome',
        playerId: playerId,
        colorIdx: colorIdx
    }));
    
    broadcast({
        type: 'playerJoined',
        playerId: playerId,
        colorIdx: colorIdx
    });
    
    socket.on('message', (data) => {
        try {
            const msg = JSON.parse(data);
            if (msg.type === 'input') {
                const p = game.players.get(playerId);
                if (p) {
                    p.input = msg.input;
                }
            }
        } catch (err) {}
    });
    
    socket.on('close', () => {
        console.log(`❌ ${playerId} left - ${game.players.size - 1} remaining`);
        game.players.delete(playerId);
        clients.delete(socket);
        broadcast({ type: 'playerLeft', playerId });
    });
});

// Game loop - 60 FPS
spawnWave();
setInterval(() => {
    updateGame(1/60);
    broadcastGameState();
}, 16);

console.log(`🎮 Dedicated Server running on port ${PORT}`);
console.log(`   Players connect to: ws://10.0.0.147:${PORT}\n`);