#include "protocol_analyzer.h"
#include "sdr.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

ProtocolAnalyzer::ProtocolAnalyzer() : sdr_ref(nullptr), current_scan_frequency(433920000), 
                                     current_range_index(0), scanning_active(false) {
    // Initialize frequency scan ranges (Hz)
    scan_ranges = {
        {433050000, 434790000},  // 433 MHz ISM band (ITU Region 1)
        {868000000, 868600000},  // 868 MHz ISM band (Europe)
        {902000000, 928000000},  // 915 MHz ISM band (Americas)
        {863000000, 870000000},  // Extended 868 MHz band for LoRa
        {314000000, 315000000},  // 315 MHz (garage doors, car remotes)
        {390000000, 392000000}   // 390 MHz (some weather stations)
    };
}

ProtocolAnalyzer::~ProtocolAnalyzer() {
    stop_frequency_scan();
}

bool ProtocolAnalyzer::initialize() {
    std::cout << "Initializing Protocol Analyzer..." << std::endl;
    
    // Load protocol signatures
    load_protocol_signatures();
    
    std::cout << "Loaded " << protocol_signatures.size() << " protocol signatures" << std::endl;
    std::cout << "Configured " << scan_ranges.size() << " frequency ranges" << std::endl;
    
    return true;
}

void ProtocolAnalyzer::load_protocol_signatures() {
    protocol_signatures.clear();
    
    // 433 MHz ISM Band Protocols
    protocol_signatures.push_back({
        ProtocolType::ISM_433_OOK,
        "433MHz OOK",
        "On-Off Keying protocols at 433MHz - garage doors, weather stations, sensors",
        433050000, 434790000,
        25000,  // 25 kHz typical bandwidth
        "OOK",
        100, 10000,  // 100 bps to 10 kbps
        true,  // Burst mode
        {"Weather stations", "Garage door remotes", "Wireless doorbells", "Security sensors"},
        "Often unencrypted, vulnerable to replay attacks"
    });
    
    protocol_signatures.push_back({
        ProtocolType::ISM_433_FSK,
        "433MHz FSK",
        "Frequency Shift Keying protocols at 433MHz - more robust than OOK",
        433050000, 434790000,
        50000,  // 50 kHz typical bandwidth
        "FSK",
        1000, 50000,  // 1 kbps to 50 kbps
        true,
        {"Smart meters", "Industrial sensors", "Remote controls"},
        "Better resistance to interference, may have encryption"
    });
    
    protocol_signatures.push_back({
        ProtocolType::WEATHER_STATION,
        "Weather Station",
        "Wireless weather station protocols (Acurite, Oregon Scientific, etc.)",
        433800000, 434000000,
        10000,  // 10 kHz bandwidth
        "OOK",
        1000, 5000,  // 1-5 kbps
        true,
        {"Acurite sensors", "Oregon Scientific", "Ambient Weather", "La Crosse"},
        "Usually unencrypted sensor data, privacy concerns"
    });
    
    protocol_signatures.push_back({
        ProtocolType::GARAGE_DOOR,
        "Garage Door Remote",
        "Garage door opener remote controls",
        433920000, 433920000,  // Often exactly 433.92 MHz
        20000,  // 20 kHz bandwidth
        "OOK",
        500, 2000,  // 500 bps to 2 kbps
        true,
        {"Chamberlain", "LiftMaster", "Genie", "Craftsman"},
        "Critical security risk - often fixed codes, vulnerable to replay"
    });
    
    // 868 MHz European ISM Band
    protocol_signatures.push_back({
        ProtocolType::ISM_868_OOK,
        "868MHz OOK (EU)",
        "European ISM band On-Off Keying protocols",
        868000000, 868600000,
        25000,
        "OOK",
        100, 10000,
        true,
        {"European weather stations", "Home automation", "Security systems"},
        "European equivalent of 433MHz protocols"
    });
    
    protocol_signatures.push_back({
        ProtocolType::ZIGBEE_868,
        "Zigbee 868MHz",
        "Zigbee mesh networking protocol - European band",
        868000000, 868600000,
        600000,  // 600 kHz channel bandwidth
        "OQPSK",
        20000, 20000,  // 20 kbps fixed
        false,  // Continuous/mesh
        {"Smart home devices", "Industrial automation", "Smart lighting"},
        "AES-128 encryption available but not always enabled"
    });
    
    protocol_signatures.push_back({
        ProtocolType::LORA_868,
        "LoRa 868MHz",
        "Long Range IoT protocol - European band",
        863000000, 870000000,
        125000,  // 125 kHz typical
        "LoRa CSS",
        250, 5500,  // 250 bps to 5.5 kbps (SF12 to SF7)
        true,
        {"IoT sensors", "Smart city", "Agricultural monitoring", "Asset tracking"},
        "Application-layer encryption varies by implementation"
    });
    
    protocol_signatures.push_back({
        ProtocolType::WIRELESS_MBUS,
        "Wireless M-Bus",
        "Wireless meter reading protocol (European standard)",
        868950000, 869525000,
        50000,  // 50 kHz
        "FSK",
        32768, 100000,  // 32.768 kbps to 100 kbps
        true,
        {"Smart water meters", "Gas meters", "Heat meters", "Electricity meters"},
        "Contains sensitive consumption data, encryption varies"
    });
    
    // 915 MHz American ISM Band
    protocol_signatures.push_back({
        ProtocolType::ISM_915_OOK,
        "915MHz OOK (US)",
        "American ISM band On-Off Keying protocols",
        902000000, 928000000,
        25000,
        "OOK",
        100, 10000,
        true,
        {"US weather stations", "Sensors", "Remote controls"},
        "American equivalent of 433MHz protocols"
    });
    
    protocol_signatures.push_back({
        ProtocolType::ZIGBEE_915,
        "Zigbee 915MHz",
        "Zigbee mesh networking protocol - American band",
        902000000, 928000000,
        2000000,  // 2 MHz channel bandwidth
        "OQPSK",
        40000, 40000,  // 40 kbps fixed
        false,
        {"Smart home devices", "Industrial sensors", "Medical devices"},
        "AES-128 encryption capability, implementation varies"
    });
    
    protocol_signatures.push_back({
        ProtocolType::LORA_915,
        "LoRa 915MHz",
        "Long Range IoT protocol - American band",
        902000000, 928000000,
        125000,  // 125 kHz typical
        "LoRa CSS",
        980, 21900,  // Different data rates for US band
        true,
        {"IoT networks", "Smart agriculture", "Industrial monitoring"},
        "LoRaWAN security depends on proper key management"
    });
    
}

void ProtocolAnalyzer::start_frequency_scan() {
    if (!sdr_ref) {
        std::cerr << "SDR reference not set!" << std::endl;
        return;
    }
    
    scanning_active = true;
    current_range_index = 0;
    current_scan_frequency = scan_ranges[0].first;
    
    std::cout << "Starting frequency scan..." << std::endl;
    std::cout << "Scan range 1: " << (scan_ranges[0].first / 1e6) << " - " 
              << (scan_ranges[0].second / 1e6) << " MHz (Fast Mode: 250kHz steps)" << std::endl;
    
    // Set initial frequency
    sdr_ref->set_frequency(current_scan_frequency);
}

void ProtocolAnalyzer::stop_frequency_scan() {
    scanning_active = false;
    std::cout << "Frequency scan stopped" << std::endl;
}

void ProtocolAnalyzer::update_scan() {
    if (!scanning_active || !sdr_ref) return;
    
    // step frequency by 250 khz
    const uint32_t step_size = 250000;
    current_scan_frequency += step_size;
    
    // check if we've reached the end of current range
    if (current_scan_frequency > scan_ranges[current_range_index].second) {
        current_range_index++;
        
        if (current_range_index >= scan_ranges.size()) {
            // Completed all ranges, restart from beginning
            current_range_index = 0;
            std::cout << "Completed scan cycle, restarting..." << std::endl;
        }
        
        current_scan_frequency = scan_ranges[current_range_index].first;
        std::cout << "Scanning range " << (current_range_index + 1) << ": " 
                  << (scan_ranges[current_range_index].first / 1e6) << " - " 
                  << (scan_ranges[current_range_index].second / 1e6) << " MHz" << std::endl;
    }
    
    // Set new frequency
    sdr_ref->set_frequency(current_scan_frequency);
}

bool ProtocolAnalyzer::detect_signals(const std::vector<std::complex<float>>& iq_data) {
    if (iq_data.empty()) return false;
    
    // Compute power spectrum
    power_spectrum = compute_power_spectrum(iq_data);
    
    // Estimate noise floor
    double noise_floor = estimate_noise_floor(power_spectrum);
    
    // Find signal peaks above threshold (lower threshold for faster detection)
    double threshold = noise_floor + 6.0; // 6 dB above noise floor (was 10 dB)
    auto peaks = find_signal_peaks(power_spectrum, threshold);
    
    bool signals_detected = false;
    
    // Analyze each detected peak
    for (const auto& peak : peaks) {
        double peak_frequency = peak.first;
        double peak_power = peak.second;
        
        // Analyze signal characteristics
        SignalCharacteristics signal = analyze_signal(iq_data, peak_frequency, peak_power);
        
        // Classify protocol
        ProtocolType protocol = classify_protocol(signal);
        
        if (protocol != ProtocolType::UNKNOWN) {
            std::cout << "Detected: " << get_protocol_name(protocol) 
                      << " at " << (signal.frequency / 1e6) << " MHz, "
                      << std::fixed << std::setprecision(1) << signal.power_db << " dB" << std::endl;
            
            // Update device database
            update_device_database(signal, protocol);
            signals_detected = true;
        }
    }
    
    return signals_detected;
}

SignalCharacteristics ProtocolAnalyzer::analyze_signal(const std::vector<std::complex<float>>& iq_data,
                                                     double peak_frequency, double peak_power) {
    SignalCharacteristics signal;
    
    signal.frequency = peak_frequency;
    signal.power_db = peak_power;
    signal.detection_time = std::chrono::steady_clock::now();
    
    // Estimate bandwidth (simple method - find -3dB points)
    signal.bandwidth = 25000; // Default 25 kHz, should be calculated properly
    
    // Estimate SNR
    double noise_floor = estimate_noise_floor(power_spectrum);
    signal.snr_db = peak_power - noise_floor;
    
    // Simple modulation detection (would need more sophisticated analysis)
    if (signal.snr_db > 20) {
        signal.modulation = "Strong signal - likely FSK/PSK";
    } else if (signal.snr_db > 10) {
        signal.modulation = "Medium signal - likely OOK/ASK";
    } else {
        signal.modulation = "Weak signal - unknown modulation";
    }
    
    // Estimate symbol rate (placeholder - needs proper analysis)
    signal.symbol_rate = 1000; // 1 kbps default
    
    // Burst detection (simplified)
    signal.is_burst = true;
    signal.burst_duration = 0.1; // 100ms default
    
    return signal;
}

ProtocolType ProtocolAnalyzer::classify_protocol(const SignalCharacteristics& signal) {
    // Check each protocol signature
    for (const auto& signature : protocol_signatures) {
        if (is_frequency_in_range(signal.frequency, signature.frequency_min, signature.frequency_max)) {
            // Additional checks could be added here for modulation, bandwidth, etc.
            return signature.type;
        }
    }
    
    return ProtocolType::UNKNOWN;
}

void ProtocolAnalyzer::update_device_database(const SignalCharacteristics& signal, ProtocolType protocol) {
    // Generate device ID based on signal characteristics
    std::string device_id = generate_device_id(signal, protocol);
    
    // Check if device already exists
    DetectedDevice* existing_device = find_device_by_signal(signal);
    
    if (existing_device) {
        // Update existing device
        existing_device->last_seen = std::chrono::steady_clock::now();
        existing_device->packet_count++;
        existing_device->signal = signal; // Update signal characteristics
    } else {
        // Add new device
        DetectedDevice new_device;
        new_device.protocol = protocol;
        new_device.signal = signal;
        new_device.device_id = device_id;
        new_device.device_type = get_protocol_name(protocol);
        new_device.is_authorized = false; // Default to unauthorized
        new_device.first_seen = std::chrono::steady_clock::now();
        new_device.last_seen = new_device.first_seen;
        new_device.packet_count = 1;
        
        // Add security flags for suspicious protocols
        if (protocol == ProtocolType::GARAGE_DOOR) {
            new_device.security_flags.push_back("CRITICAL: Garage door remote - replay attack risk");
        }
        if (protocol == ProtocolType::WEATHER_STATION) {
            new_device.security_flags.push_back("INFO: Unencrypted sensor data");
        }
        
        detected_devices.push_back(new_device);
        
        std::cout << "New device detected: " << device_id << " (" << get_protocol_name(protocol) << ")" << std::endl;
    }
}

std::vector<float> ProtocolAnalyzer::compute_power_spectrum(const std::vector<std::complex<float>>& iq_data) {
    std::vector<float> power_spectrum;
    
    if (iq_data.size() < DETECTION_FFT_SIZE) {
        return power_spectrum;
    }
    
    // Simple power calculation - in a real implementation, you'd use FFT
    power_spectrum.reserve(DETECTION_FFT_SIZE);
    
    for (size_t i = 0; i < DETECTION_FFT_SIZE && i < iq_data.size(); i++) {
        float power = std::norm(iq_data[i]); // |I|^2 + |Q|^2
        float power_db = 10.0f * std::log10(power + 1e-10f);
        power_spectrum.push_back(power_db);
    }
    
    return power_spectrum;
}

double ProtocolAnalyzer::estimate_noise_floor(const std::vector<float>& power_spectrum) {
    if (power_spectrum.empty()) return NOISE_FLOOR_DB;
    
    // Simple noise floor estimation - use median or percentile
    std::vector<float> sorted_power = power_spectrum;
    std::sort(sorted_power.begin(), sorted_power.end());
    
    // Use 25th percentile as noise floor estimate
    size_t index = sorted_power.size() / 4;
    return sorted_power[index];
}

std::vector<std::pair<double, double>> ProtocolAnalyzer::find_signal_peaks(
    const std::vector<float>& power_spectrum, double threshold_db) {
    
    std::vector<std::pair<double, double>> peaks;
    
    for (size_t i = 1; i < power_spectrum.size() - 1; i++) {
        if (power_spectrum[i] > threshold_db &&
            power_spectrum[i] > power_spectrum[i-1] &&
            power_spectrum[i] > power_spectrum[i+1]) {
            
            double frequency = frequency_from_fft_bin(i, DETECTION_FFT_SIZE, 2048000); // Assume 2.048 MHz sample rate
            peaks.push_back({frequency, power_spectrum[i]});
        }
    }
    
    return peaks;
}

std::string ProtocolAnalyzer::get_protocol_name(ProtocolType type) const {
    for (const auto& sig : protocol_signatures) {
        if (sig.type == type) {
            return sig.name;
        }
    }
    return "Unknown Protocol";
}

std::string ProtocolAnalyzer::get_protocol_description(ProtocolType type) const {
    for (const auto& sig : protocol_signatures) {
        if (sig.type == type) {
            return sig.description;
        }
    }
    return "Unknown protocol type";
}

std::vector<DetectedDevice> ProtocolAnalyzer::get_unauthorized_devices() const {
    std::vector<DetectedDevice> unauthorized;
    for (const auto& device : detected_devices) {
        if (!device.is_authorized) {
            unauthorized.push_back(device);
        }
    }
    return unauthorized;
}

std::vector<std::string> ProtocolAnalyzer::get_security_alerts() const {
    std::vector<std::string> alerts;
    
    for (const auto& device : detected_devices) {
        if (!device.is_authorized) {
            std::ostringstream alert;
            alert << "UNAUTHORIZED DEVICE: " << device.device_id 
                  << " (" << get_protocol_name(device.protocol) << ") "
                  << "at " << std::fixed << std::setprecision(3) 
                  << (device.signal.frequency / 1e6) << " MHz";
            alerts.push_back(alert.str());
        }
        
        // Add protocol-specific security flags
        for (const auto& flag : device.security_flags) {
            alerts.push_back(device.device_id + ": " + flag);
        }
    }
    
    return alerts;
}

// Helper function implementations
std::string ProtocolAnalyzer::generate_device_id(const SignalCharacteristics& signal, ProtocolType protocol) {
    std::ostringstream id;
    id << get_protocol_name(protocol) << "_" 
       << std::fixed << std::setprecision(3) << (signal.frequency / 1e6) << "MHz_" 
       << std::hex << std::hash<double>{}(signal.frequency);
    return id.str();
}

bool ProtocolAnalyzer::is_frequency_in_range(double frequency, double min_freq, double max_freq) {
    return frequency >= min_freq && frequency <= max_freq;
}

double ProtocolAnalyzer::frequency_from_fft_bin(int bin, int fft_size, double sample_rate) {
    return (bin * sample_rate) / fft_size;
}

DetectedDevice* ProtocolAnalyzer::find_device_by_signal(const SignalCharacteristics& signal) {
    const double freq_tolerance = 50000; // 50 kHz tolerance
    
    for (auto& device : detected_devices) {
        if (std::abs(device.signal.frequency - signal.frequency) < freq_tolerance) {
            return &device;
        }
    }
    return nullptr;
}

void ProtocolAnalyzer::mark_device_authorized(const std::string& device_id) {
    for (auto& device : detected_devices) {
        if (device.device_id == device_id) {
            device.is_authorized = true;
            std::cout << "Device " << device_id << " marked as authorized" << std::endl;
            return;
        }
    }
}

void ProtocolAnalyzer::cleanup_old_devices() {
    auto now = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::minutes(10); // Remove devices not seen for 10 minutes
    
    detected_devices.erase(
        std::remove_if(detected_devices.begin(), detected_devices.end(),
            [now, timeout](const DetectedDevice& device) {
                return (now - device.last_seen) > timeout;
            }
        ),
        detected_devices.end()
    );
}
