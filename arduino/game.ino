// ---------- Pins (Raspberry Pi Pico) ----------
const uint8_t LED_HB = 10;  // heartbeat LED on GP10
const uint8_t LED_ST = 11;  // status LED on GP11
/*
  GP9,  "B"},
  GP8,  "A"},
  GP7,  "LEFT"},
  GP6,  "DOWN"},
  GP5,  "RIGHT"},
  GP4,  "UP"
*/

// Buttons (active-low with internal pull-ups)
struct Btn {
  uint8_t pin;
  const char* name;
  bool raw=false, lastRaw=false, stable=false;
  unsigned long tEdge=0;
};

// Your mapping:
Btn btns[] = {
  {9,  "B"},
  {8,  "A"},
  {7,  "LEFT"},
  {6,  "DOWN"},
  {5,  "RIGHT"},
  {4,  "UP"},
};

const unsigned long DEBOUNCE_MS = 15;

// ---------- Setup ----------
void setup() {
  pinMode(LED_HB, OUTPUT);
  pinMode(LED_ST, OUTPUT);

  for (auto &b : btns) pinMode(b.pin, INPUT_PULLUP); // idle = HIGH, pressed = LOW

  // quick power-on self test
  digitalWrite(LED_HB, HIGH);
  digitalWrite(LED_ST, HIGH);
  delay(250);
  digitalWrite(LED_HB, LOW);
  digitalWrite(LED_ST, LOW);

  Serial.begin(115200);
  Serial.println("Pico button/LED test ready (GP4..GP9).");
}

// heartbeat state
unsigned long tHB = 0; bool hbOn = false;

void loop() {
  unsigned long now = millis();

  // --- Heartbeat on GP10 (~2 Hz, short pulse) ---
  if (!hbOn && now - tHB >= 500) { hbOn = true;  tHB = now; digitalWrite(LED_HB, HIGH); }
  if ( hbOn && now - tHB >= 60 ) { hbOn = false;             digitalWrite(LED_HB, LOW);  }

  // --- Read & debounce buttons ---
  bool anyPressed = false;

  for (auto &b : btns) {
    bool r = (digitalRead(b.pin) == LOW); // active-low = pressed
    if (r != b.lastRaw) { b.tEdge = now; b.lastRaw = r; }
    if ((now - b.tEdge) >= DEBOUNCE_MS && b.stable != r) {
      b.stable = r;
      Serial.print(b.stable ? "PRESS: " : "RELEASE: "); Serial.println(b.name);
    }
    anyPressed |= b.stable;
  }

  // --- Status LED on GP11: ON while any button is held ---
  digitalWrite(LED_ST, anyPressed ? HIGH : LOW);
}
