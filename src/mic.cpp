//
// Copyright (c) 2023,2024,2025,2026 by Dan Luca. All rights reserved
//

#include <PDM.h>
#include <cstring>
// #include <circular_buffer.h>
#include "mic.h"
#include "efx_setup.h"
#include "sysinfo.h"
#include "log.h"
#include "util.h"
#include "task_msg.h"

#define MIC_SAMPLE_SIZE 512
// one channel - mono mode for Nano RP2040 microphone, MP34DT06JTR
#define MIC_CHANNELS    1
// default PCM output frequency - 20kHz for Nano RP2040. Max is ~24kHz.
#define PCM_SAMPLE_FREQ 24000
// Buffer to read samples into, each sample is 16-bits
short sampleBuffer[MIC_SAMPLE_SIZE];

volatile size_t samplesRead;                    // Number of audio samples read
uint16_t maxAudio[AUDIO_HIST_BINS_COUNT] {}; // audio max levels histogram
std::atomic<uint16_t> audioBumpThreshold = 5000;    // the audio signal level beyond which entropy is added and an effect change is triggered
mutex_t audioStatsMutex{};

// CircularBuffer<short> *audioData = nullptr;
QueueHandle_t micQueue = nullptr;

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
    // audioData = new CircularBuffer<short>(1024);

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
    sysInfo->setSysStatus(SysStatus::Mic);
    log_info(F("PDM - microphone - setup ok"));
}

void mic_run() {
    static short localBuffer[MIC_SAMPLE_SIZE];

    // Atomically snapshot the samples read and copy the buffer to a local buffer to avoid ISR races
    size_t count = 0;
    noInterrupts();
    count = samplesRead;
    if (count) {
        if (count > MIC_SAMPLE_SIZE) count = MIC_SAMPLE_SIZE; // safety clamp
        memcpy(localBuffer, sampleBuffer, count * sizeof(short));
        samplesRead = 0; // acknowledge we've consumed this batch
    }
    interrupts();

    // Process the batch, if any
    if (count) {
        // audioData->push_back(localBuffer, count);
        //log_info(F("Audio data - added %d samples to circular buffer, size updated to %d items"), count, audioData->size());
        short maxSample = INT16_MIN;
        for (size_t i = 0; i < count; i++) {
            if (localBuffer[i] > maxSample)
                maxSample = localBuffer[i];
        }
        if (maxSample > audioBumpThreshold) {
            fxBump = true;
            random16_add_entropy(abs(maxSample));
            log_info(F("Audio sample: %hd"), maxSample);

            {
                //protect histogram updates
                //contribute to the audio histogram - the bins are 500 units wide and tailored around audioBumpThreshold.
                CoreMutex lock(&audioStatsMutex);
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
        }
    }

    AudioActionMessage msg{};
    if (pdTRUE == xQueueReceive(micQueue, &msg, 0)) {
        switch (msg.action) {
            case AUDIO_THRESHOLD_UPDATE: {
                // Protect threshold update
                CoreMutex lock(&audioStatsMutex);
                audioBumpThreshold = msg.data;
                clearLevelHistory();
                break;
            }
            default: log_error(F("Mic Action %hu not supported"), msg.action);
        }
    }

}
