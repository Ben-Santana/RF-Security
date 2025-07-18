#ifndef PROTOCOL_ANALYZER_H
#define PROTOCOL_ANALYZER_H

#include <vector>
#include <complex>
#include <string>
#include <unordered_map>
#include <chrono>

// forward declaration
class SimpleSDR;

enum class ProtocolType {
    UNKNOWN = 0,
    ISM_433_OOK,           // 433mhz on-off keying (garage doors, weather stations)
    ISM_433_FSK,           // 433mhz frequency shift keying
    ISM_915_OOK,           // 915mhz on-off keying (us ism band)
    ISM_868_OOK,           // 868mhz on-off keying (eu ism band)
    ZIGBEE_915,            // zigbee 915mhz (us)
    ZIGBEE_868,            // zigbee 868mhz (eu)
    LORA_433,              // lora 433mhz
    LORA_868,              // lora 868mhz (eu)
    LORA_915,              // lora 915mhz (us)
    WIRELESS_MBUS,         // wireless m-bus (meter reading)
    TPMS,                  // tire pressure monitoring system
    WEATHER_STATION,       // weather station protocols
    GARAGE_DOOR,           // garage door remotes
    SECURITY_SENSOR        // home security sensors
};

struct SignalCharacteristics {
    double frequency;           // center frequency in hz
    double bandwidth;           // signal bandwidth in hz
    double power_db;           // signal power in db
    double snr_db;             // signal-to-noise ratio in db
    std::string modulation;    // modulation type (ook, fsk, psk, lora)
    double symbol_rate;        // symbol rate in symbols/second
    bool is_burst;             // is this a burst transmission?
    double burst_duration;     // duration of burst in seconds
    std::chrono::steady_clock::time_point detection_time;
};

struct ProtocolSignature {
    ProtocolType type;
    std::string name;
    std::string description;
    double frequency_min;      // minimum frequency in hz
    double frequency_max;      // maximum frequency in hz
    double bandwidth_typical;  // typical bandwidth in hz
    std::string modulation;    // expected modulation
    double symbol_rate_min;    // minimum symbol rate
    double symbol_rate_max;    // maximum symbol rate
    bool is_burst_mode;        // typically burst or continuous
    std::vector<std::string> common_devices; // common device types
    std::string security_notes; // security implications
};

struct DetectedDevice {
    ProtocolType protocol;
    SignalCharacteristics signal;
    std::string device_id;     // unique identifier if available
    std::string manufacturer;  // detected manufacturer
    std::string device_type;   // type of device
    bool is_authorized;        // whether device is known/authorized
    std::chrono::steady_clock::time_point first_seen;
    std::chrono::steady_clock::time_point last_seen;
    int packet_count;          // number of packets detected
    std::vector<std::string> security_flags; // security concerns
};

class ProtocolAnalyzer {
private:
    SimpleSDR* sdr_ref;
    
    // detection parameters
    static const int DETECTION_FFT_SIZE = 2048;
    static constexpr double SIGNAL_THRESHOLD_DB = -60.0;  // minimum signal level
    static constexpr double NOISE_FLOOR_DB = -90.0;       // typical noise floor
    
    // frequency scan ranges
    std::vector<std::pair<uint32_t, uint32_t>> scan_ranges;
    
    // protocol signatures database
    std::vector<ProtocolSignature> protocol_signatures;
    
    // detected devices
    std::vector<DetectedDevice> detected_devices;
    
    // analysis state
    uint32_t current_scan_frequency;
    size_t current_range_index;
    bool scanning_active;
    
    // signal processing buffers
    std::vector<float> power_spectrum;
    std::vector<float> noise_floor_estimate;
    
public:
    ProtocolAnalyzer();
    ~ProtocolAnalyzer();
    
    // initialization
    bool initialize();
    void set_sdr_reference(SimpleSDR* sdr) { sdr_ref = sdr; }
    
    // protocol signature database
    void load_protocol_signatures();
    void add_custom_signature(const ProtocolSignature& signature);
    
    // frequency scanning
    void start_frequency_scan();
    void stop_frequency_scan();
    bool is_scanning() const { return scanning_active; }
    void update_scan(); // call this periodically to advance scan
    
    // signal detection and analysis
    bool detect_signals(const std::vector<std::complex<float>>& iq_data);
    SignalCharacteristics analyze_signal(const std::vector<std::complex<float>>& iq_data, 
                                       double peak_frequency, double peak_power);
    ProtocolType classify_protocol(const SignalCharacteristics& signal);
    
    // device management
    void update_device_database(const SignalCharacteristics& signal, ProtocolType protocol);
    std::vector<DetectedDevice> get_detected_devices() const { return detected_devices; }
    void mark_device_authorized(const std::string& device_id);
    void remove_device(const std::string& device_id);
    
    // security analysis
    std::vector<DetectedDevice> get_unauthorized_devices() const;
    std::vector<std::string> get_security_alerts() const;
    bool is_suspicious_activity(const DetectedDevice& device) const;
    
    // information retrieval
    std::vector<ProtocolSignature> get_protocol_signatures() const { return protocol_signatures; }
    std::string get_protocol_name(ProtocolType type) const;
    std::string get_protocol_description(ProtocolType type) const;
    uint32_t get_current_frequency() const { return current_scan_frequency; }
    
private:
    // signal processing helpers
    std::vector<float> compute_power_spectrum(const std::vector<std::complex<float>>& iq_data);
    double estimate_noise_floor(const std::vector<float>& power_spectrum);
    std::vector<std::pair<double, double>> find_signal_peaks(const std::vector<float>& power_spectrum, 
                                                          double threshold_db);
    
    // protocol classification helpers
    bool matches_ook_characteristics(const SignalCharacteristics& signal);
    bool matches_fsk_characteristics(const SignalCharacteristics& signal);
    bool matches_lora_characteristics(const SignalCharacteristics& signal);
    bool matches_zigbee_characteristics(const SignalCharacteristics& signal);
    
    // utility functions
    std::string generate_device_id(const SignalCharacteristics& signal, ProtocolType protocol);
    bool is_frequency_in_range(double frequency, double min_freq, double max_freq);
    double frequency_from_fft_bin(int bin, int fft_size, double sample_rate);
    
    // database helpers
    DetectedDevice* find_device_by_signal(const SignalCharacteristics& signal);
    void cleanup_old_devices(); // remove devices not seen recently
};

#endif // PROTOCOL_ANALYZER_H