#include <WiFiS3.h>
#include <ArduinoGraphics.h>
#include "Arduino_LED_Matrix.h"
#include "arduino_secrets.h"

// WiFi credentials
const char* ssid = SECRET_SSID; 
const char* password = SECRET_PASS; 

WiFiServer server(80);
ArduinoLEDMatrix matrix;

bool ledState[8][12] = {false};  // 8 rows, 12 columns

void setup() {
  Serial.begin(115200);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // Wait until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  delay(10000); // Allow time for WiFi to stabilize
  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.begin();
  matrix.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    String request = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        request += c;
        if (c == '\n') break;
      }
    }

    // Handle toggle request
    if (request.startsWith("GET /set?")) {
      int row = getParam(request, "row");
      int col = getParam(request, "col");
      int state = getParam(request, "state");

      if (row >= 0 && row < 8 && col >= 0 && col < 12) {
        ledState[row][col] = (state == 1);
        drawMatrix();
      }

      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain");
      client.println();
      client.println("OK");
      client.stop();
      return;
    }

    // Handle state query
    if (request.startsWith("GET /state")) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain");
      client.println();
      for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 12; c++) {
          client.print(ledState[r][c] ? '1' : '0');
        }
      }
      client.println();
      client.stop();
      return;
    }

    // Handle clear request
    if (request.startsWith("GET /clear")) {
      for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 12; c++) {
          ledState[r][c] = false;
        }
      }
      drawMatrix();

      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain");
      client.println();
      client.println("CLEARED");
      client.stop();
      return;
    }

    // Serve HTML
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    sendHTML(client);
    client.stop();
  }
}

void drawMatrix() {
  uint8_t bitmap[8][12] = {0};
  for (int col = 0; col < 12; col++) {
    for (int row = 0; row < 8; row++) {
      if (ledState[row][col]) {
        bitmap[row][col] = 1;
      }
    }
  }
  matrix.renderBitmap(bitmap, 8, 12);
}

int getParam(const String& req, const String& key) {
  int start = req.indexOf(key + "=");
  if (start == -1) return -1;
  start += key.length() + 1;
  int end = req.indexOf('&', start);
  if (end == -1) end = req.indexOf(' ', start);
  return req.substring(start, end).toInt();
}

void sendHTML(WiFiClient& client) {
  client.println(R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <title>LED Matrix</title>
    <style>
      table { border-collapse: collapse; margin: 20px auto; }
      td { width: 30px; height: 30px; text-align: center; border: 1px solid #ccc; }
      input[type="checkbox"] { width: 20px; height: 20px; }
      button { display: block; margin: 20px auto; padding: 10px 20px; font-size: 16px; }
    </style>
  </head>
  <body>
    <h2 style="text-align:center;">12x8 LED Matrix Controller</h2>
    <table id="matrix"></table>
    <button onclick="clearMatrix()">Clear Matrix</button>

    <script>
  const rows = 8, cols = 12;
  const table = document.getElementById("matrix");
  const checkboxes = [];

  // Build the checkbox grid first
  function createGrid() {
    for (let r = 0; r < rows; r++) {
      const tr = document.createElement("tr");
      checkboxes[r] = [];
      for (let c = 0; c < cols; c++) {
        const td = document.createElement("td");
        const cb = document.createElement("input");
        cb.type = "checkbox";
        cb.id = `r${r}c${c}`;
        cb.onclick = () => {
          const state = cb.checked ? 1 : 0;
          fetch(`/set?row=${r}&col=${c}&state=${state}`);
        };
        checkboxes[r][c] = cb;
        td.appendChild(cb);
        tr.appendChild(td);
      }
      table.appendChild(tr);
    }
  }

  // After grid is created, fetch the LED matrix state
  function fetchState() {
    fetch("/state")
      .then(res => res.text())
      .then(data => {
        data = data.replace(/\s/g, '');
        if (data.length === 96) {
          let i = 0;
          for (let r = 0; r < rows; r++) {
            for (let c = 0; c < cols; c++) {
              checkboxes[r][c].checked = data[i++] === '1';
            }
          }
        } else {
          console.error("Invalid LED matrix state:", data);
        }
      })
      .catch(err => console.error("Error fetching state:", err));
  }

  function clearMatrix() {
    fetch("/clear").then(() => {
      for (let r = 0; r < rows; r++) {
        for (let c = 0; c < cols; c++) {
          checkboxes[r][c].checked = false;
        }
      }
    });
  }

  // Main entry point
  // window.onload = function() {
  //   createGrid();
  //   setTimeout(fetchState, 200);  // wait just a bit to ensure grid is built
  // };
  createGrid();
  setInterval(fetchState, 1000);
</script>

  </body>
</html>
)rawliteral");
}
