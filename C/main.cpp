#include "sdr.h"
#include "gui.h"
#include "protocol_analyzer.h"
#include <iostream>
#include <signal.h>

// globals for signal handler cleanup
SimpleSDR* sdr_instance = nullptr;
SDRGui* gui_instance = nullptr;
ProtocolAnalyzer* analyzer_instance = nullptr;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", stopping..." << std::endl;
    if (sdr_instance) {
        sdr_instance->stop();
    }
    if (analyzer_instance) {
        analyzer_instance->stop_frequency_scan();
    }
}

int main(int argc, char* argv[]) {
    SimpleSDR sdr;
    SDRGui gui;
    ProtocolAnalyzer analyzer;
    
    sdr_instance = &sdr;
    gui_instance = &gui;
    analyzer_instance = &analyzer;
    
    // catch ctrl+c and sigterm
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // gui needs to start first
    if (!gui.initialize()) {
        std::cerr << "Failed to initialize GUI!" << std::endl;
        return 1;
    }
    
    // setup the sdr hardware
    if (!sdr.initialize()) {
        std::cerr << "Failed to initialize SDR!" << std::endl;
        return 1;
    }
    
    // start protocol detection
    if (!analyzer.initialize()) {
        std::cerr << "Failed to initialize Protocol Analyzer!" << std::endl;
        return 1;
    }
    
    // wire everything together
    gui.set_sdr_reference(&sdr);
    gui.set_protocol_analyzer_reference(&analyzer);
    sdr.set_protocol_analyzer(&analyzer);
    analyzer.set_sdr_reference(&sdr);
    
    // tune to frequency from command line if given
    if (argc > 1) {
        uint32_t freq = std::stoul(argv[1]);
        sdr.set_frequency(freq);
    }
    
    std::cout << "Starting GUI mode with Protocol Analysis..." << std::endl;
    std::cout << "Controls: ↑↓ (±100kHz) ←→ (±1MHz) +/- (gain) Q/ESC (quit)" << std::endl;
    std::cout << "Protocol Scanner: S (start/stop scan) P (pause) M (manual control)" << std::endl;
    
    // user controls when to start scanning
    
    // main loop
    while (gui.is_running()) {
        // process keyboard/mouse input
        gui.handle_events();
        
        // apply frequency changes if user is manually tuning
        if (gui.should_update_frequency() && gui.is_user_manual_control()) {
            sdr.set_frequency(gui.get_target_frequency());
            gui.clear_frequency_change();
        }
        
        if (gui.should_update_gain()) {
            sdr.set_gain(gui.get_target_gain());
            gui.clear_gain_change();
        }
        
        // grab samples from the radio
        sdr.read_samples_async();
        
        // advance protocol scanning if it's running
        if (gui.is_protocol_scanning_enabled() && !gui.is_protocol_scanning_paused() && analyzer.is_scanning()) {
            static int scan_counter = 0;
            if (++scan_counter % 10 == 0) { // scan update every ~0.16 seconds
                analyzer.update_scan();
            }
        }
        
        // draw the display
        gui.update();
        gui.render();
        
        // keep cpu usage reasonable
        SDL_Delay(16); // roughly 60fps
    }
    
    std::cout << "Shutting down..." << std::endl;
    sdr.stop();
    
    return 0;
}
