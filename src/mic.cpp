//
// Created by dluca on 6/28/2023.
//

#include "mic.h"
#include "log.h"
#include "efx_setup.h"

#define MIC_SAMPLE_SIZE 512
// default number of output channels
static const char channels = 1;
// default PCM output frequency - 20kHz for Nano RP2040
static const int frequency = 20000;
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
    int bytesAvailable = capu(PDM.available(), MIC_SAMPLE_SIZE);
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
    PDM.setGain(40);

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
