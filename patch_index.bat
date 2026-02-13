@echo off
REM patch_index.bat - Injects WebSocket bridge into generated index.html

set "HTML_FILE=web\index.html"
set "TEMP_FILE=web\index_temp.html"

echo Patching %HTML_FILE% with WebSocket bridge...

REM Create the injection code
(
echo     ^<script^>
echo         // WebSocket for multiplayer - auto-connect on load
echo         let gameWS = null;
echo         let wsConnected = false;
echo         let wsMessages = [];
echo         let wsURL = "ws://10.0.0.147:8080";
echo.
echo         function connectWebSocket^(^) {
echo             console.log^("Auto-connecting to:", wsURL^);
echo.            
echo             try {
echo                 gameWS = new WebSocket^(wsURL^);
echo.                
echo                 gameWS.onopen = function^(^) {
echo                     console.log^("✅ WebSocket connected!"^);
echo                     wsConnected = true;
echo                 };
echo.                
echo                 gameWS.onmessage = function^(event^) {
echo                     // Queue messages for Lua to poll
echo                     wsMessages.push^(event.data^);
echo                 };
echo.                
echo                 gameWS.onerror = function^(error^) {
echo                     console.error^("❌ WebSocket error:", error^);
echo                     wsConnected = false;
echo                 };
echo.                
echo                 gameWS.onclose = function^(^) {
echo                     console.log^("WebSocket disconnected"^);
echo                     wsConnected = false;
echo.                    
echo                     // Auto-reconnect after 2 seconds
echo                     setTimeout^(connectWebSocket, 2000^);
echo                 };
echo.                
echo             } catch^(e^) {
echo                 console.error^("Failed to create WebSocket:", e^);
echo                 setTimeout^(connectWebSocket, 2000^);
echo             }
echo         }
echo.
echo         // Expose these to window so C can call them
echo         window.wsIsConnected = function^(^) {
echo             return wsConnected ? 1 : 0;
echo         };
echo.
echo         window.wsGetMessage = function^(^) {
echo             if ^(wsMessages.length ^> 0^) {
echo                 return wsMessages.shift^(^);
echo             }
echo             return "";
echo         };
echo.
echo         window.wsSendMessage = function^(msg^) {
echo             if ^(gameWS ^&^& gameWS.readyState === WebSocket.OPEN^) {
echo                 gameWS.send^(msg^);
echo                 return 1;
echo             }
echo             return 0;
echo         };
echo.
echo         // Auto-connect when page loads
echo         window.addEventListener^('load', function^(^) {
echo             console.log^("Page loaded, starting WebSocket connection..."^);
echo             connectWebSocket^(^);
echo         }^);
echo.        
echo         console.log^("WebSocket bridge ready"^);
echo     ^</script^>
) > websocket_inject.tmp

REM Find the </body> tag and inject before it
powershell -Command "(Get-Content '%HTML_FILE%') -replace '</body>', (Get-Content 'websocket_inject.tmp' -Raw) + '</body>' | Set-Content '%TEMP_FILE%'"

REM Replace original with patched version
move /Y "%TEMP_FILE%" "%HTML_FILE%" >nul

REM Cleanup
del websocket_inject.tmp >nul

echo Patched successfully!