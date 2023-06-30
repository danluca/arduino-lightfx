//
// Created by dluca on 6/28/2023.
//

#include "mic.h"
#include "log.h"
#include <FastLED.h>

// default number of output channels
static const char channels = 1;
// default PCM output frequency
static const int frequency = 16000;
// Buffer to read samples into, each sample is 16-bits
short sampleBuffer[512];
// Number of audio samples read
volatile int samplesRead;

/**
  * Callback function to process the data from the PDM microphone.
  * NOTE: This callback is executed as part of an ISR.
  * Therefore using `Serial` to print messages inside this function isn't supported.
  */
void onPDMdata() {
    // Query the number of available bytes
    int bytesAvailable = PDM.available();
    // Read into the sample buffer
    PDM.read(sampleBuffer, bytesAvailable);
    // 16-bit, 2 bytes per sample
    samplesRead = bytesAvailable / 2;
}

void mic_setup() {
    // Configure the data receive callback
    PDM.onReceive(onPDMdata);

    // Optionally set the gain
    // Defaults to 20 on the BLE Sense and -10 on the Portenta Vision Shields
    // PDM.setGain(30);

    // Initialize PDM with:
    // - one channel (mono mode)
    // - a 16 kHz sample rate for the Arduino Nano 33 BLE Sense
    // - a 32 kHz or 64 kHz sample rate for the Arduino Portenta Vision Shields
    if (!PDM.begin(channels, frequency)) {
        Log.errorln("Failed to start PDM! (microphone sampling)");
        while (true);
    }
}

void mic_run() {
    // Wait for samples to be read
    if (samplesRead) {
        // Print samples to the serial monitor or plotter
        for (int i = 0; i < samplesRead; i++) {
            if (sampleBuffer[i] > 10000 || sampleBuffer[i] <= -10000) {
                random16_add_entropy(abs(sampleBuffer[i]));
            }
        }
        Log.infoln("Processed %d audio samples", samplesRead);
        // Clear the read count
        samplesRead = 0;
    }
    yield();
}
