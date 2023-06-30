//
// Created by dluca on 6/28/2023.
//

#include "mic.h"
#include "log.h"
#include "efx_setup.h"

// Note there is a collision between FastLED and PDM, allegedly around the PIO pins used. Until further clarification, the mic functionality will not be engaged


#define MIC_SAMPLE_SIZE 512
// one channel - mono mode for Nano RP2040 microphone, MP34DT06JTR
#define MIC_CHANNELS    1
// default PCM output frequency - 20kHz for Nano RP2040
#define PCM_SAMPLE_FREQ 20000
// Buffer to read samples into, each sample is 16-bits
short sampleBuffer[MIC_SAMPLE_SIZE];
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
    // Optionally set the gain - Defaults to 20
    PDM.setGain(40);
    if (!PDM.begin(MIC_CHANNELS, PCM_SAMPLE_FREQ)) {
        Log.errorln("Failed to start PDM! (microphone sampling)");
        while (true);
    }
}

void mic_run() {
    // Wait for samples to be read
    if (samplesRead) {
        // Print samples to the serial monitor or plotter
        for (int i = 0; i < samplesRead; i++) {
//            if (sampleBuffer[i] > 10000 || sampleBuffer[i] <= -10000) {
            if (sampleBuffer[i] > 300) {
                random16_add_entropy(abs(sampleBuffer[i]));
            }
        }
        Log.infoln("Processed %d audio samples", samplesRead);
        // Clear the read count
        samplesRead = 0;
    }
    yield();
}
