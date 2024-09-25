//
// Copyright (c) 2023,2024 by Dan Luca. All rights reserved
//

#include "mic.h"
#include "log.h"
#include "efx_setup.h"
#include "circular_buffer.h"

#define MIC_SAMPLE_SIZE 512
// one channel - mono mode for Nano RP2040 microphone, MP34DT06JTR
#define MIC_CHANNELS    1
// default PCM output frequency - 20kHz for Nano RP2040. Max is ~24kHz.
#define PCM_SAMPLE_FREQ 24000
// Buffer to read samples into, each sample is 16-bits
short sampleBuffer[MIC_SAMPLE_SIZE];

volatile size_t samplesRead;                    // Number of audio samples read
volatile uint16_t maxAudio[10] {};              // audio max levels histogram
volatile uint16_t audioBumpThreshold = 2000;    // the audio signal level beyond which entropy is added and an effect change is triggered

CircularBuffer<short> *audioData = new CircularBuffer<short>(1024);


void clearLevelHistory() {
    for (auto &l : maxAudio)
        l = 0;
}

/**
  * Callback function to process the data from the PDM microphone.
  * NOTE: This callback is executed as part of an ISR.
  * Therefore using `Serial` to print messages inside this function isn't supported.
  */
void onPDMdata() {
    // Query the number of available bytes
    size_t bytesAvailable = PDM.available();
    // Read into the sample buffer
    PDM.read(sampleBuffer, bytesAvailable);
    // 16-bit, 2 bytes per sample
    samplesRead = bytesAvailable / 2;
}

void mic_setup() {
    // Configure the data receive callback
    PDM.onReceive(onPDMdata);
    PDM.setBufferSize(MIC_SAMPLE_SIZE);
    // Optionally set the gain - Defaults to 20
    PDM.setGain(80);
    if (!PDM.begin(MIC_CHANNELS, PCM_SAMPLE_FREQ)) {
        //resetStatus(SYS_STATUS_MIC_MASK); //the default value of the flag is reset (0) and we can't leave the function if PDM doesn't initialize properly
        Log.errorln(F("Failed to start PDM library! (for microphone sampling)"));
        while (true) yield();
    }
    delay(1000);
    setSysStatus(SYS_STATUS_MIC);
    Log.infoln(F("PDM - microphone - setup ok"));
}

void mic_run() {
    // Wait for samples to be read
    if (samplesRead) {
        audioData->push_back(sampleBuffer, samplesRead);
        //Log.infoln(F("Audio data - added %d samples to circular buffer, size updated to %d items"), samplesRead, audioData->size());
        short maxSample = INT16_MIN;
        for (uint i = 0; i < samplesRead; i++) {
            if (sampleBuffer[i] > maxSample)
                maxSample = sampleBuffer[i];
        }
        if (maxSample > audioBumpThreshold) {
            fxBump = true;
            random16_add_entropy(abs(maxSample));
            Log.infoln(F("Audio sample: %d"), maxSample);

            //contribute to the audio histogram - the bins are 500 units wide and tailored around audioBumpThreshold.
            bool bFoundBin = false;
            for (uint8_t x = 0; x < AUDIO_HIST_BINS_COUNT; x++) {
                uint16_t binThr = audioBumpThreshold + (x+1)*500;
                if (maxSample <= binThr) {
                    maxAudio[x]++;
                    bFoundBin = true;
                    break;
                }
            }
            //if a bin not found, it means it's higher than max bin given the number of bins, place it in the last bin
            if (!bFoundBin)
                maxAudio[AUDIO_HIST_BINS_COUNT-1]++;
        }
        // Clear the read count
        samplesRead = 0;
    }
    yield();
}
