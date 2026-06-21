#include <WiFi.h>
#include <WebServer.h>

// ==========================================
// 1. PIN DEFINITIONS & SETTINGS
// ==========================================
#define RELAY_PIN 13 // Main Ignition / Safety Cutoff
#define ONBOARD_LED 2

// Motor A (Left)
#define ENA 26
#define IN1 25
#define IN2 33

// Motor B (Right)
#define IN3 32
#define IN4 18
#define ENB 5

// PWM Properties for ESP32 (UPDATED FOR V3.0)
const int freq = 1000;
const int resolution = 8; // 0 to 255

// TUNE THESE: Milliseconds required for exact turns
int time90 = 450;  // Adjust this based on floor testing
int time180 = 900; // Adjust this based on floor testing

bool ignitionState = false;

// ==========================================
// 2. NETWORK SETTINGS
// ==========================================
const char* ssid = "robotic bull car";
const char* password = "robotic1234";

WebServer server(80);

// ==========================================
// 3. CYBERPUNK WEB GUI (HTML/CSS/JS)
// ==========================================
const char* htmlPage PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Robotic Bull Car - OS</title>
  <style>
    body { background-color: #0d0d0d; color: #00ffcc; font-family: 'Courier New', Courier, monospace; margin: 0; padding: 20px; user-select: none; display: flex; flex-direction: column; height: 90vh; }
    h1 { text-align: center; color: #ff0055; text-transform: uppercase; letter-spacing: 2px; text-shadow: 0 0 10px #ff0055; margin-bottom: 5px; }
    .status-bar { display: flex; justify-content: space-between; border-bottom: 2px solid #00ffcc; padding-bottom: 10px; margin-bottom: 20px; font-weight: bold; }
    .main-layout { display: flex; justify-content: space-between; align-items: center; flex-grow: 1; }
    .side-panel { display: flex; flex-direction: column; gap: 20px; width: 25%; }
    .center-console { display: flex; flex-direction: column; align-items: center; justify-content: center; width: 45%; gap: 20px; }
    .d-pad { display: grid; grid-template-columns: repeat(3, 80px); grid-template-rows: repeat(3, 80px); gap: 10px; justify-content: center; }
    .btn { background: #1a1a1a; border: 2px solid #00ffcc; color: #00ffcc; border-radius: 10px; font-size: 24px; font-weight: bold; box-shadow: 0 0 10px rgba(0,255,204,0.2); transition: 0.1s; display: flex; align-items: center; justify-content: center; cursor: pointer; }
    .btn:active { background: #00ffcc; color: #000; box-shadow: 0 0 20px #00ffcc; transform: scale(0.95); }
    .empty { visibility: hidden; }
    .action-btn { background: #1a1a1a; border: 2px solid #ffaa00; color: #ffaa00; padding: 20px; font-size: 18px; font-weight: bold; text-align: center; border-radius: 8px; box-shadow: 0 0 10px rgba(255,170,0,0.2); cursor: pointer; }
    .action-btn:active { background: #ffaa00; color: #000; }
    .ignition-btn { background: #330000; border: 3px solid #ff0055; color: #ff0055; padding: 15px 40px; font-size: 22px; font-weight: bold; text-transform: uppercase; border-radius: 5px; cursor: pointer; box-shadow: 0 0 15px #ff0055; }
    .ignition-on { background: #003300; border-color: #00ff00; color: #00ff00; box-shadow: 0 0 15px #00ff00; }
    .gear-container { text-align: center; margin-top: 20px; }
    .gear-row { display: flex; gap: 10px; justify-content: center; margin-top: 10px; }
    .gear-btn { padding: 15px 25px; background: #111; border: 1px solid #555; color: #fff; font-size: 20px; border-radius: 5px; cursor: pointer; }
    .gear-active { background: #00ffcc; color: #000; font-weight: bold; box-shadow: 0 0 10px #00ffcc; }
  </style>
</head>
<body>
  <h1>Robotic Bull Car</h1>
  <div class="status-bar">
    <span id="connStatus">NETWORK: SECURE</span>
    <span id="igStatus">SYSTEM: OFFLINE</span>
  </div>
  <div class="main-layout">
    <div class="side-panel">
      <div class="action-btn" onclick="sendCmd('L90')">↰ 90° LEFT</div>
      <div class="action-btn" onclick="sendCmd('L180')">↶ 180° LEFT</div>
    </div>
    <div class="center-console">
      <button id="ignitionBtn" class="ignition-btn" onclick="toggleIgnition()">IGNITION: OFF</button>
      <div class="d-pad">
        <div class="empty"></div>
        <div class="btn" ontouchstart="sendCmd('F')" ontouchend="sendCmd('S')" onmousedown="sendCmd('F')" onmouseup="sendCmd('S')">▲</div>
        <div class="empty"></div>
        <div class="btn" ontouchstart="sendCmd('L')" ontouchend="sendCmd('S')" onmousedown="sendCmd('L')" onmouseup="sendCmd('S')">◄</div>
        <div class="btn" ontouchstart="sendCmd('B')" ontouchend="sendCmd('S')" onmousedown="sendCmd('B')" onmouseup="sendCmd('S')">▼</div>
        <div class="btn" ontouchstart="sendCmd('R')" ontouchend="sendCmd('S')" onmousedown="sendCmd('R')" onmouseup="sendCmd('S')">►</div>
      </div>
    </div>
    <div class="side-panel">
      <div class="action-btn" onclick="sendCmd('R90')">90° RIGHT ↱</div>
      <div class="action-btn" onclick="sendCmd('R180')">180° RIGHT ↷</div>
    </div>
  </div>
  <div class="gear-container">
    <div>TRANSMISSION / SPEED</div>
    <div class="gear-row">
      <div class="gear-btn" id="g1" onclick="setGear(1)">1</div>
      <div class="gear-btn" id="g2" onclick="setGear(2)">2</div>
      <div class="gear-btn gear-active" id="g3" onclick="setGear(3)">3</div>
      <div class="gear-btn" id="g4" onclick="setGear(4)">4</div>
      <div class="gear-btn" id="g5" onclick="setGear(5)">MAX</div>
    </div>
  </div>
  <script>
    let currentGear = 3;
    let isIgnitionOn = false;
    function setGear(gear) {
      currentGear = gear;
      document.querySelectorAll('.gear-btn').forEach(btn => btn.classList.remove('gear-active'));
      document.getElementById('g' + gear).classList.add('gear-active');
      fetch('/gear?v=' + gear);
    }
    function toggleIgnition() {
      isIgnitionOn = !isIgnitionOn;
      let btn = document.getElementById('ignitionBtn');
      let status = document.getElementById('igStatus');
      if(isIgnitionOn) {
        btn.classList.add('ignition-on');
        btn.innerText = 'IGNITION: LIVE (12V)';
        status.innerText = 'SYSTEM: ARMED';
        status.style.color = '#00ff00';
      } else {
        btn.classList.remove('ignition-on');
        btn.innerText = 'IGNITION: OFF';
        status.innerText = 'SYSTEM: OFFLINE';
        status.style.color = '#ff0055';
      }
      fetch('/ignition?state=' + (isIgnitionOn ? '1' : '0'));
    }
    function sendCmd(cmd) {
      if(!isIgnitionOn) return; 
      fetch('/drive?cmd=' + cmd);
    }
  </script>
</body>
</html>
)rawliteral";

// ==========================================
// 4. MOTOR CONTROL FUNCTIONS (UPDATED FOR V3.0)
// ==========================================
int currentSpeed = 180; // Default Gear 3

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  ledcWrite(ENA, 0); ledcWrite(ENB, 0);
  digitalWrite(ONBOARD_LED, HIGH); // SYSTEM IDLE: Turn Blue LED ON
}

void drive(String dir) {
  digitalWrite(ONBOARD_LED, LOW); // SYSTEM ACTIVE: Turn Blue LED OFF
  
  ledcWrite(ENA, currentSpeed);
  ledcWrite(ENB, currentSpeed);

  if (dir == "F") {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  } else if (dir == "B") {
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  } else if (dir == "L") {
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); 
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); 
  } else if (dir == "R") {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); 
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
  } else if (dir == "L90") {
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    delay(time90); stopMotors();
  } else if (dir == "L180") {
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    delay(time180); stopMotors();
  } else if (dir == "R90") {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
    delay(time90); stopMotors();
  } else if (dir == "R180") {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
    delay(time180); stopMotors();
  } else if (dir == "S") {
    stopMotors();
  }
}

// ==========================================
// 5. SERVER ENDPOINTS
// ==========================================
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleIgnition() {
  String state = server.arg("state");
  if(state == "1") {
    digitalWrite(RELAY_PIN, LOW); // ACTIVE-LOW: LOW turns the 12V ON!
    ignitionState = true;
  } else {
    digitalWrite(RELAY_PIN, HIGH);  // ACTIVE-LOW: HIGH turns the 12V OFF!
    ignitionState = false;
    stopMotors();
  }
  server.send(200, "text/plain", "OK");
}

void handleDrive() {
  if(!ignitionState) {
    server.send(200, "text/plain", "Ignition OFF");
    return;
  }
  String cmd = server.arg("cmd");
  drive(cmd);
  server.send(200, "text/plain", "OK");
}

void handleGear() {
  String g = server.arg("v");
  int gear = g.toInt();
  if(gear == 1) currentSpeed = 110;
  else if(gear == 2) currentSpeed = 150;
  else if(gear == 3) currentSpeed = 180;
  else if(gear == 4) currentSpeed = 220;
  else if(gear == 5) currentSpeed = 255;
  server.send(200, "text/plain", "OK");
}

// ==========================================
// 6. MAIN SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);

  // Pin Setup
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // ACTIVE-LOW: HIGH means Ignition is OFF!

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, HIGH); // Boot up with LED ON (Idle State)

  // Configure PWM for Speed Control (UPDATED FOR V3.0)
  ledcAttach(ENA, freq, resolution);
  ledcAttach(ENB, freq, resolution);

  // Start Access Point
  Serial.println("\nStarting Hotspot...");
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Define Server Routes
  server.on("/", handleRoot);
  server.on("/ignition", handleIgnition);
  server.on("/drive", handleDrive);
  server.on("/gear", handleGear);

  server.begin();
  Serial.println("Cyberpunk Web Server Started!");
}

void loop() {
  server.handleClient();
}

// END OF FILE - MAKE SURE YOU COPIED DOWN TO THIS LINE!