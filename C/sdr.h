#ifndef SDR_H
#define SDR_H

#include <vector>
#include <complex>
#include <cstdint>
#include <rtl-sdr.h>

// forward declaration
class ProtocolAnalyzer;

class SimpleSDR {
private:
    rtlsdr_dev_t* device;
    uint32_t sample_rate;
    uint32_t center_freq;
    int gain;
    bool running;
    
    std::vector<std::complex<float>> iq_buffer;
    ProtocolAnalyzer* protocol_analyzer;
    
public:
    SimpleSDR();
    ~SimpleSDR();
    
    bool initialize();
    void convert_samples(uint8_t* buffer, uint32_t len);
    void analyze_samples();
    void run();
    void stop();
    void set_frequency(uint32_t freq);
    
    // getters for gui
    uint32_t get_sample_rate() const { return sample_rate; }
    uint32_t get_center_freq() const { return center_freq; }
    int get_gain() const { return gain; }
    bool is_running() const { return running; }
    const std::vector<std::complex<float>>& get_iq_buffer() const { return iq_buffer; }
    
    // setters for gui
    void set_sample_rate(uint32_t rate);
    void set_gain(int new_gain);
    
    // non-blocking sample read for gui
    bool read_samples_async();
    
    // protocol analysis stuff
    void set_protocol_analyzer(ProtocolAnalyzer* analyzer) { protocol_analyzer = analyzer; }
    ProtocolAnalyzer* get_protocol_analyzer() const { return protocol_analyzer; }
};

#endif // SDR_H