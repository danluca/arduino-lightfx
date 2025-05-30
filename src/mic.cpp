//
// Copyright (c) 2023,2024,2025 by Dan Luca. All rights reserved
//

#include <PDM.h>
#include <FreeRTOS.h>
#include <task.h>
#include "mic.h"
#include "efx_setup.h"
#include "sysinfo.h"
#include "log.h"
#include "util.h"

#define MIC_SAMPLE_SIZE 512
// one channel - mono mode for Nano RP2040 microphone, MP34DT06JTR
#define MIC_CHANNELS    1
// default PCM output frequency - 20kHz for Nano RP2040. Max is ~24kHz.
#define PCM_SAMPLE_FREQ 24000
// Buffer to read samples into, each sample is 16-bits
short sampleBuffer[MIC_SAMPLE_SIZE];

volatile size_t samplesRead;                    // Number of audio samples read
volatile uint16_t maxAudio[10] {};              // audio max levels histogram
volatile uint16_t audioBumpThreshold = 5000;    // the audio signal level beyond which entropy is added and an effect change is triggered

CircularBuffer<short> *audioData = new CircularBuffer<short>(1024);


void clearLevelHistory() {
    for (auto &l : maxAudio)
        l = 0;
}

/**
  * Callback function to process the data from the PDM microphone.
  * NOTE: This callback is executed as part of an ISR.
  * Therefore, using `Serial` to print messages inside this function isn't supported.
  */
void onPDMdata() {
    // Query the number of available bytes
    const size_t bytesAvailable = PDM.available();
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
    PDM.setGain(5);
    if (!PDM.begin(MIC_CHANNELS, PCM_SAMPLE_FREQ)) {
        //resetStatus(SYS_STATUS_MIC_MASK); //the default value of the flag is reset (0) and we can't leave the function if PDM doesn't initialize properly
        log_error(F("Failed to start PDM library! (for microphone sampling)"));
        vTaskSuspend(nullptr);
        // while (true) taskYIELD();
    }
    taskDelay(1000);
    sysInfo->setSysStatus(SYS_STATUS_MIC);
    log_info(F("PDM - microphone - setup ok"));
}

void mic_run() {
    // Wait for samples to be read
    if (samplesRead) {
        audioData->push_back(sampleBuffer, samplesRead);
        //log_info(F("Audio data - added %d samples to circular buffer, size updated to %d items"), samplesRead, audioData->size());
        short maxSample = INT16_MIN;
        for (uint i = 0; i < samplesRead; i++) {
            if (sampleBuffer[i] > maxSample)
                maxSample = sampleBuffer[i];
        }
        if (maxSample > audioBumpThreshold) {
            fxBump = true;
            random16_add_entropy(abs(maxSample));
            log_info(F("Audio sample: %hd"), maxSample);

            //contribute to the audio histogram - the bins are 500 units wide and tailored around audioBumpThreshold.
            bool bFoundBin = false;
            for (uint8_t x = 0; x < AUDIO_HIST_BINS_COUNT; x++) {
                if (const uint16_t binThr = audioBumpThreshold + (x+1)*500; maxSample <= binThr) {
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
}
