#ifndef GUI_H
#define GUI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <fftw3.h>
#include <vector>
#include <complex>
#include <string>

class SimpleSDR; // forward declaration
class ProtocolAnalyzer; // forward declaration

class SDRGui {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    bool running;
    
    // window size
    static const int WINDOW_WIDTH = 1024;
    static const int WINDOW_HEIGHT = 768;
    
    // layout constants
    static const int SPECTRUM_HEIGHT = 200;
    static const int SPECTRUM_OFFSET = 60;
    static const int WATERFALL_HEIGHT = 280;
    static const int WATERFALL_OFFSET = 280;
    static const int CONTROL_PANEL_HEIGHT = 120;
    static const int MARGIN = 30;
    static const int TEXT_LINE_HEIGHT = 25;
    static const int SECTION_SPACING = 20;
    
    // colors
    SDL_Color bg_color = {20, 20, 30, 255};
    SDL_Color grid_color = {80, 80, 100, 255};
    SDL_Color spectrum_color = {100, 200, 255, 255};
    SDL_Color text_color = {255, 255, 255, 255};
    
    // control state
    uint32_t target_frequency;
    int target_gain;
    bool frequency_changed;
    bool gain_changed;
    
    // protocol scanning state
    bool protocol_scanning_enabled;
    bool protocol_scanning_paused;
    bool user_manual_control; // true when user is manually tuning
    
    SimpleSDR* sdr_ref;
    ProtocolAnalyzer* protocol_analyzer_ref;
    
    // fft stuff
    static const int FFT_SIZE = 1024;
    fftwf_complex *fft_in, *fft_out;
    fftwf_plan fft_plan;
    
    // waterfall display
    static const int WATERFALL_HISTORY = 300;
    std::vector<std::vector<float>> waterfall_data;
    SDL_Texture* waterfall_texture;
    uint32_t* waterfall_pixels;
    
public:
    SDRGui();
    ~SDRGui();
    
    bool initialize();
    void cleanup();
    bool is_running() const { return running; }
    
    void handle_events();
    void update();
    void render();
    
    void set_sdr_reference(SimpleSDR* sdr) { sdr_ref = sdr; }
    void set_protocol_analyzer_reference(ProtocolAnalyzer* analyzer) { protocol_analyzer_ref = analyzer; }
    
    // protocol scanning controls
    bool is_protocol_scanning_enabled() const { return protocol_scanning_enabled; }
    bool is_protocol_scanning_paused() const { return protocol_scanning_paused; }
    bool is_user_manual_control() const { return user_manual_control; }
    void set_user_manual_control(bool manual) { user_manual_control = manual; }
    
    // gui control methods
    bool should_update_frequency() const { return frequency_changed; }
    bool should_update_gain() const { return gain_changed; }
    uint32_t get_target_frequency() const { return target_frequency; }
    int get_target_gain() const { return target_gain; }
    void clear_frequency_change() { frequency_changed = false; }
    void clear_gain_change() { gain_changed = false; }
    
private:
    void render_spectrum(const std::vector<std::complex<float>>& iq_data);
    void render_waterfall();
    void render_controls();
    void render_protocol_panel();
    void render_text(const std::string& text, int x, int y);
    void render_text_colored(const std::string& text, int x, int y, SDL_Color color);
    void draw_grid();
    void handle_keyboard(SDL_Keycode key);
    
    // fft for spectrum display
    std::vector<float> compute_fft_magnitude(const std::vector<std::complex<float>>& iq_data);
    
    // waterfall functions
    void update_waterfall_data(const std::vector<float>& fft_magnitudes);
    void init_waterfall();
    void cleanup_waterfall();
    uint32_t magnitude_to_color(float magnitude_db);
};

#endif // GUI_H
