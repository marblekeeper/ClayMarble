#!/usr/bin/env python3
"""patch_index.py - Injects WebSocket bridge into generated index.html"""

import sys
import os

HTML_FILE = "web/index.html"

WEBSOCKET_CODE = """    <script>
        // WebSocket for multiplayer - auto-connect on load
        let gameWS = null;
        let wsConnected = false;
        let wsMessages = [];
        let wsURL = "ws://10.0.0.147:8080";

        function connectWebSocket() {
            console.log("Auto-connecting to:", wsURL);
            
            try {
                gameWS = new WebSocket(wsURL);
                
                gameWS.onopen = function() {
                    console.log("✅ WebSocket connected!");
                    wsConnected = true;
                };
                
                gameWS.onmessage = function(event) {
                    // Queue messages for Lua to poll
                    wsMessages.push(event.data);
                };
                
                gameWS.onerror = function(error) {
                    console.error("❌ WebSocket error:", error);
                    wsConnected = false;
                };
                
                gameWS.onclose = function() {
                    console.log("WebSocket disconnected");
                    wsConnected = false;
                    
                    // Auto-reconnect after 2 seconds
                    setTimeout(connectWebSocket, 2000);
                };
                
            } catch(e) {
                console.error("Failed to create WebSocket:", e);
                setTimeout(connectWebSocket, 2000);
            }
        }

        // Expose these to window so C can call them
        window.wsIsConnected = function() {
            return wsConnected ? 1 : 0;
        };

        window.wsGetMessage = function() {
            if (wsMessages.length > 0) {
                return wsMessages.shift();
            }
            return "";
        };

        window.wsSendMessage = function(msg) {
            if (gameWS && gameWS.readyState === WebSocket.OPEN) {
                gameWS.send(msg);
                return 1;
            }
            return 0;
        };

        // Auto-connect when page loads
        window.addEventListener('load', function() {
            console.log("Page loaded, starting WebSocket connection...");
            connectWebSocket();
        });
        
        console.log("WebSocket bridge ready");
    </script>
"""

def patch_html():
    if not os.path.exists(HTML_FILE):
        print(f"Error: {HTML_FILE} not found!")
        return False
    
    print(f"Patching {HTML_FILE} with WebSocket bridge...")
    
    with open(HTML_FILE, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Inject before </body>
    if '</body>' in content:
        content = content.replace('</body>', WEBSOCKET_CODE + '\n</body>')
        
        with open(HTML_FILE, 'w', encoding='utf-8') as f:
            f.write(content)
        
        print("✅ Patched successfully!")
        return True
    else:
        print("Error: </body> tag not found in HTML!")
        return False

if __name__ == "__main__":
    success = patch_html()
    sys.exit(0 if success else 1)