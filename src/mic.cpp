//
// Created by dluca on 6/28/2023.
//

#include "mic.h"
#include "log.h"
#include "efx_setup.h"

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
    int bytesAvailable = PDMNano.available();
    // Read into the sample buffer
    PDMNano.read(sampleBuffer, bytesAvailable);
    // 16-bit, 2 bytes per sample
    samplesRead = bytesAvailable / 2;
}

void mic_setup() {
    // Configure the data receive callback
    PDMNano.onReceive(onPDMdata);
    // Optionally set the gain - Defaults to 20
    PDMNano.setGain(60);
    if (!PDMNano.begin(MIC_CHANNELS, PCM_SAMPLE_FREQ)) {
        Log.errorln("Failed to start PDM! (microphone sampling)");
        while (true);
    }
    delay(1000);
}

void mic_run() {
    // Wait for samples to be read
    if (samplesRead) {
        // Print samples to the serial monitor or plotter
        for (int i = 0; i < samplesRead; i++) {
//            if (sampleBuffer[i] > 10000 || sampleBuffer[i] <= -10000) {
            if (sampleBuffer[i] > 300) {
                random16_add_entropy(abs(sampleBuffer[i]));
                Log.infoln("Audio sample over 300");
            }
        }
        //Log.infoln("Processed %d audio samples", samplesRead);
        // Clear the read count
        samplesRead = 0;
    }
    yield();
}
