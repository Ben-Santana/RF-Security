#include "sdr.h"
#include "protocol_analyzer.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <unistd.h>

SimpleSDR::SimpleSDR() : device(nullptr), sample_rate(2048000), 
                        center_freq(100000000), gain(0), running(false), protocol_analyzer(nullptr) {
    iq_buffer.reserve(131072);
}

SimpleSDR::~SimpleSDR() {
    if (device) {
        rtlsdr_close(device);
    }
}

bool SimpleSDR::initialize() {
    int device_count = rtlsdr_get_device_count();
    if (device_count == 0) {
        std::cerr << "No RTL-SDR devices found!" << std::endl;
        return false;
    }
    
    std::cout << "Found " << device_count << " device(s):" << std::endl;
    for (int i = 0; i < device_count; i++) {
        std::cout << "  " << i << ": " << rtlsdr_get_device_name(i) << std::endl;
    }
    
    int result = rtlsdr_open(&device, 0);
    if (result < 0) {
        std::cerr << "Failed to open RTL-SDR device!" << std::endl;
        return false;
    }
    
    rtlsdr_set_sample_rate(device, sample_rate);
    rtlsdr_set_center_freq(device, center_freq);
    rtlsdr_set_tuner_gain_mode(device, 1);
    rtlsdr_set_tuner_gain(device, gain);
    rtlsdr_reset_buffer(device);
    
    std::cout << "RTL-SDR initialized:" << std::endl;
    std::cout << "  Sample rate: " << rtlsdr_get_sample_rate(device) << " Hz" << std::endl;
    std::cout << "  Center frequency: " << rtlsdr_get_center_freq(device) << " Hz" << std::endl;
    std::cout << "  Tuner gain: " << rtlsdr_get_tuner_gain(device) << " dB" << std::endl;
    
    return true;
}

void SimpleSDR::convert_samples(uint8_t* buffer, uint32_t len) {
    iq_buffer.clear();
    for (uint32_t i = 0; i < len; i += 2) {
        float I = (buffer[i] - 127.5f) / 127.5f;
        float Q = (buffer[i + 1] - 127.5f) / 127.5f;
        iq_buffer.emplace_back(I, Q);
    }
}

void SimpleSDR::analyze_samples() {
    if (iq_buffer.empty()) return;
    
    float max_magnitude = 0.0f;
    float avg_magnitude = 0.0f;
    float avg_power = 0.0f;
    
    for (const auto& sample : iq_buffer) {
        float magnitude = std::abs(sample);
        float power = magnitude * magnitude;
        
        max_magnitude = std::max(max_magnitude, magnitude);
        avg_magnitude += magnitude;
        avg_power += power;
    }
    
    avg_magnitude /= iq_buffer.size();
    avg_power /= iq_buffer.size();
    
    float avg_power_db = 10.0f * std::log10(avg_power + 1e-10f);
    
    std::cout << "Samples: " << iq_buffer.size() 
              << ", Max: " << max_magnitude 
              << ", Avg: " << avg_magnitude
              << ", Power: " << avg_power_db << " dB" << std::endl;
}

void SimpleSDR::run() {
    if (!device) {
        std::cerr << "Device not initialized!" << std::endl;
        return;
    }
    
    running = true;
    uint8_t buffer[131072];
    int n_read;
    
    std::cout << "Starting capture... Press Ctrl+C to stop" << std::endl;
    
    while (running) {
        int result = rtlsdr_read_sync(device, buffer, sizeof(buffer), &n_read);
        if (result < 0) {
            std::cerr << "WARNING: sync read failed." << std::endl;
            break;
        }
        
        convert_samples(buffer, n_read);
        analyze_samples();
        
        usleep(100000);
    }
}

void SimpleSDR::stop() {
    running = false;
}

void SimpleSDR::set_frequency(uint32_t freq) {
    center_freq = freq;
    if (device) {
        rtlsdr_set_center_freq(device, center_freq);
        std::cout << "Frequency set to: " << freq << " Hz" << std::endl;
    }
}

void SimpleSDR::set_sample_rate(uint32_t rate) {
    sample_rate = rate;
    if (device) {
        rtlsdr_set_sample_rate(device, sample_rate);
    }
}

void SimpleSDR::set_gain(int new_gain) {
    gain = new_gain;
    if (device) {
        rtlsdr_set_tuner_gain(device, gain);
    }
}

bool SimpleSDR::read_samples_async() {
    if (!device) return false;
    
    uint8_t buffer[8192]; // smaller buffer for gui
    int n_read;
    
    int result = rtlsdr_read_sync(device, buffer, sizeof(buffer), &n_read);
    if (result < 0) {
        return false;
    }
    
    convert_samples(buffer, n_read);
    
    // run protocol analysis if analyzer is connected
    if (protocol_analyzer && !iq_buffer.empty()) {
        protocol_analyzer->detect_signals(iq_buffer);
    }
    
    return true;
}