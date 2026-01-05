/************************************************************
 * SISTEMI EMBEDDED
 * PROGETTO: Controllo illuminazione e umidità di una stanza
 *
 * Il sistema è modellato tramite una macchina a stati finiti.
 * Sensori:
 *  - A0: luminosità interna
 *  - A1: luminosità esterna (giorno/notte)
 *  - A2: umidità
 *
 * Attuatori (PWM):
 *  - D9 : vetri oscuranti (opacità)
 *  - D10: illuminazione artificiale
 *  - D11: umidificatore
 *
 * Comando esterno:
 *  - D2 : selezione valori notturni alternativi
 ************************************************************/

/* ====================== DEFINIZIONE STATI ======================
 * Ogni stato rappresenta una fase funzionale del sistema
 */
enum State {
  INIT,              // Stato iniziale
  CHECK_DAY_NIGHT,   // Discriminazione giorno/notte
  DAY_LIGHT,         // Controllo luminosità diurna
  DAY_HUM,           // Controllo umidità diurna
  HUM_ON,            // Umidificatore attivo temporizzato
  NIGHT_LIGHT,       // Impostazione luce notturna
  NIGHT_HUM,         // Impostazione umidità notturna
  WAIT               // Attesa temporizzata tra due cicli
};

State state = INIT;   // Stato corrente della FSM

/* ====================== GESTIONE TEMPI ====================== */
// ---------- TEMPI DI TEST (DEBUG / SIMULAZIONE) ----------
const unsigned long T_CHECK = 10000;   // 10 secondi tra due controlli
const unsigned long T_HUM   = 5000;    // 5 secondi umidificatore ON

// ---------- TEMPI REALI (VERSIONE FINALE) ----------
// const unsigned long T_CHECK = 300000; // 5 minuti
// const unsigned long T_HUM   = 60000;  // 1 minuto

unsigned long t_state; // memorizza il tempo di ingresso nello stato

/* ====================== PIN HARDWARE ====================== */
// Sensori analogici
const int L_INT = A0;   // Fotoresistenza interna
const int L_EXT = A1;   // Fotoresistenza esterna
const int HUM   = A2;   // Umidità (potenziometro)

// Ingresso digitale
const int BTN   = 2;    // Pulsante comando notturno

// Attuatori PWM
const int WIN   = 9;    // Vetri oscuranti
const int LAMP  = 10;   // Lampada artificiale
const int HUMID = 11;   // Umidificatore

/* ====================== PARAMETRI DI CONTROLLO ====================== */
// Soglia per discriminare giorno/notte
const int DAY_TH = 400;

// Range luminosità diurna
const int L_MIN = 300;
const int L_MAX = 700;

// Umidità minima
const int H_MIN = 400;

// Valori notturni preimpostati
const int L_NIGHT     = 120;
const int H_NIGHT     = 120;

// Valori notturni alternativi (comando esterno)
const int L_NIGHT_ALT = 200;
const int H_NIGHT_ALT = 200;

/* ====================== SETUP ====================== */
void setup() {
  // Configurazione attuatori
  pinMode(WIN, OUTPUT);
  pinMode(LAMP, OUTPUT);
  pinMode(HUMID, OUTPUT);

  // Configurazione pulsante con pull-up interno
  pinMode(BTN, INPUT_PULLUP);

  // Inizializzazione comunicazione seriale
  Serial.begin(9600);
  Serial.println("=== SYSTEM START ===");

  // Inizializzazione temporizzatore FSM
  t_state = millis();
}

/* ====================== LOOP PRINCIPALE ======================
 * Implementazione della macchina a stati finiti
 */
void loop() {
  switch (state) {
    /* ---------- STATO INIT ---------- */
    case INIT:
      Serial.println("[STATE] INIT");
      state = CHECK_DAY_NIGHT;
      t_state = millis();
      break;

    /* ---------- STATO CHECK DAY / NIGHT ---------- */
    case CHECK_DAY_NIGHT:
      Serial.println("[STATE] CHECK_DAY_NIGHT");
      Serial.print("L_ext = ");
      Serial.println(analogRead(L_EXT));

      // Se la luminosità esterna supera la soglia -> giorno
      if (analogRead(L_EXT) > DAY_TH) {
        Serial.println("-> DAY MODE");
        state = DAY_LIGHT;
      }
      // Altrimenti -> notte
      else {
        Serial.println("-> NIGHT MODE");
        state = NIGHT_LIGHT;
      }

      t_state = millis();
      break;

    /* ---------- CONTROLLO LUMINOSITÀ DIURNA ---------- */
    case DAY_LIGHT:
      Serial.println("[STATE] DAY_LIGHT");
      dayLightControl();        // funzione dedicata
      state = DAY_HUM;          // si passa al controllo umidità
      t_state = millis();
      break;

    /* ---------- CONTROLLO UMIDITÀ DIURNA ---------- */
    case DAY_HUM:
      Serial.println("[STATE] DAY_HUM");
      Serial.print("Humidity = ");
      Serial.println(analogRead(HUM));

      // Se umidità sotto il minimo
      if (analogRead(HUM) < H_MIN) {
        Serial.println("-> HUMIDITY LOW: HUM_ON");
        analogWrite(HUMID, 255); // umidificatore ON massimo
        state = HUM_ON;
        t_state = millis();
      }
      // Altrimenti si attende il prossimo ciclo
      else {
        Serial.println("-> HUMIDITY OK");
        state = WAIT;
        t_state = millis();
      }
      break;

    /* ---------- UMIDIFICATORE ATTIVO TEMPORIZZATO ---------- */
    case HUM_ON:
      if (millis() - t_state >= T_HUM) {
        Serial.println("[STATE] HUM_ON -> OFF");
        analogWrite(HUMID, 0);   // spegnimento
        state = WAIT;
        t_state = millis();
      }
      break;

    /* ---------- IMPOSTAZIONE LUMINOSITÀ NOTTURNA ---------- */
    case NIGHT_LIGHT:
      Serial.println("[STATE] NIGHT_LIGHT");
      nightLightControl();
      state = NIGHT_HUM;
      t_state = millis();
      break;

    /* ---------- IMPOSTAZIONE UMIDITÀ NOTTURNA ---------- */
    case NIGHT_HUM:
      Serial.println("[STATE] NIGHT_HUM");
      nightHumControl();
      state = WAIT;
      t_state = millis();
      break;

    /* ---------- ATTESA TRA DUE CICLI ---------- */
    case WAIT:
      if (millis() - t_state >= T_CHECK) {
        Serial.println("[STATE] WAIT DONE -> RESTART");
        state = CHECK_DAY_NIGHT;
        t_state = millis();
      }
      break;
  }
}

/* ====================== FUNZIONI DI CONTROLLO ====================== */

/* Controllo luminosità diurna */
void dayLightControl() {
  int L = analogRead(L_INT);

  Serial.print("L_int = ");
  Serial.println(L);

  // Troppa luce -> oscuramento vetri
  if (L > L_MAX) {
    Serial.println("-> TOO BRIGHT: DARKEN WINDOWS");
    analogWrite(WIN, map(L, L_MAX, 1023, 80, 255));
    analogWrite(LAMP, 0);
  }
  // Troppa poca luce -> accensione lampada
  else if (L < L_MIN) {
    Serial.println("-> TOO DARK: TURN ON LAMP");
    analogWrite(LAMP, map(L, 0, L_MIN, 255, 80));
    analogWrite(WIN, 0);
  }
  // Luminosità corretta
  else {
    Serial.println("-> LIGHT OK");
    analogWrite(WIN, 0);
    analogWrite(LAMP, 0);
  }
}

/* Impostazione luce notturna */
void nightLightControl() {
  analogWrite(WIN, 0); // vetri sempre trasparenti

  if (digitalRead(BTN) == LOW) {
    Serial.println("-> NIGHT ALT LIGHT");
    analogWrite(LAMP, L_NIGHT_ALT);
  } else {
    Serial.println("-> NIGHT DEFAULT LIGHT");
    analogWrite(LAMP, L_NIGHT);
  }
}

/* Impostazione umidità notturna */
void nightHumControl() {
  if (digitalRead(BTN) == LOW) {
    Serial.println("-> NIGHT ALT HUMIDITY");
    analogWrite(HUMID, H_NIGHT_ALT);
  } else {
    Serial.println("-> NIGHT DEFAULT HUMIDITY");
    analogWrite(HUMID, H_NIGHT);
  }
}