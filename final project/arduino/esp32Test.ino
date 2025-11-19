// ESP32 + MQ gas module (AO via equal-resistor divider -> GPIO34)
//         + Analog mic (AO -> GPIO35)
// Streams one JSON line per ~1.2s:
// {"raqi_raw":..., "raqi_mV":..., "raqi_volt":..., "noise_rms":..., "noise_spikes":...}

#include <Arduino.h>

const int PIN_MQ   = 34;    // MQ AO -> divider midpoint -> GPIO34 (input-only ADC)
const int PIN_MIC  = 35;    // Mic AO -> GPIO35 (input-only ADC)
const float VREF   = 3.30;  // ESP32 ADC pin range is ~0..3.3V mapped to 0..4095

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);                 // 0..4095

  // MQ: use highest sensitivity (0 dB ~ 0..1.1V) since the divider lowers the voltage a lot
  analogSetPinAttenuation(PIN_MQ,  ADC_0db);

  // Mic: allow up to ~3.6–3.9V dynamic range (mic is powered at 3.3V)
  analogSetPinAttenuation(PIN_MIC, ADC_11db);

  Serial.println("{\"status\":\"boot\"}");
}

// Compute mic RMS over ~1s and then count spikes over ~200ms with hysteresis
static void mic_metrics(float &rms_out, unsigned &spikes_out) {
  long sum = 0, sumsq = 0;
  int cnt = 0;
  int minv = 4095, maxv = 0;

  // 1) Gather ~1 second of samples for RMS + stats
  unsigned long t0 = millis();
  while (millis() - t0 < 1000) {
    int x = analogRead(PIN_MIC);
    if (x < minv) minv = x;
    if (x > maxv) maxv = x;        // (ensure no clamping bug)
    sum   += x;
    sumsq += (long)x * x;
    cnt++;
    delayMicroseconds(60);         // ~8–10 kHz effective sampling
  }

  // RMS around the mean (not min/max midpoint)
  float mean = (cnt > 0) ? (sum / (float)cnt) : 0.0f;
  float var  = max(0.0f, (sumsq / (float)cnt) - mean * mean);
  float mic_rms = sqrt(var);

  // 2) Adaptive spike threshold with hysteresis
  //    Tune factor: 3.0 works well for claps/taps; lower to 2.5 if still zero.
  int thr_hi = max(12, (int)(3.0f * mic_rms));  // enter spike
  int thr_lo = max(8,  thr_hi / 2);             // exit spike (hysteresis)
  bool in_spike = false;
  unsigned spikes = 0;

  t0 = millis();
  while (millis() - t0 < 200) {
    int a = abs(analogRead(PIN_MIC) - (int)mean);
    if (!in_spike && a > thr_hi) { in_spike = true; spikes++; }
    else if (in_spike && a < thr_lo) { in_spike = false; }
    delay(1);
  }

  rms_out = mic_rms;
  spikes_out = spikes;
}

void loop() {
  // --- MQ average for stability ---
  const int N = 10;
  long acc = 0;
  for (int i = 0; i < N; i++) { acc += analogRead(PIN_MQ); delay(2); }
  float mq_raw = acc / (float)N;                 // 0..4095 at *pin* (after divider)
  uint32_t mv_pin = analogReadMilliVolts(PIN_MQ); // mV at pin (after divider)
  float v_pin  = (mq_raw / 4095.0f) * VREF;      // volts at pin
  float v_ao   = v_pin * 2.0f;                   // estimate original MQ AO volts (equal resistors)

  // --- Mic metrics ---
  float mic_rms = 0.0f;
  unsigned spikes = 0;
  mic_metrics(mic_rms, spikes);

  // --- JSON line ---
  Serial.print("{\"raqi_raw\":");      Serial.print((int)mq_raw);
  Serial.print(",\"raqi_mV\":");       Serial.print(mv_pin);
  Serial.print(",\"raqi_volt\":");     Serial.print(v_ao, 3);
  Serial.print(",\"noise_rms\":");     Serial.print(mic_rms, 1);
  Serial.print(",\"noise_spikes\":");  Serial.print(spikes);
  Serial.println("}");
}