#include "gui.h"
#include "sdr.h"
#include "protocol_analyzer.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

SDRGui::SDRGui() : window(nullptr), renderer(nullptr), font(nullptr), 
                   running(false), target_frequency(100000000), target_gain(0),
                   frequency_changed(false), gain_changed(false), 
                   protocol_scanning_enabled(false), protocol_scanning_paused(false),
                   user_manual_control(false), sdr_ref(nullptr), protocol_analyzer_ref(nullptr),
                   fft_in(nullptr), fft_out(nullptr), waterfall_texture(nullptr), waterfall_pixels(nullptr) {
}

SDRGui::~SDRGui() {
    cleanup();
}

bool SDRGui::initialize() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    if (TTF_Init() < 0) {
        std::cerr << "TTF initialization failed: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return false;
    }
    
    window = SDL_CreateWindow("RTL-SDR Spectrum Analyzer",
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        cleanup();
        return false;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        cleanup();
        return false;
    }
    
    font = TTF_OpenFont("/usr/share/fonts/gnu-free/FreeSans.otf", 16);
    if (!font) {
        std::cerr << "Font loading failed: " << TTF_GetError() << std::endl;
        cleanup();
        return false;
    }
    
    // setup fft
    fft_in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * FFT_SIZE);
    fft_out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * FFT_SIZE);
    fft_plan = fftwf_plan_dft_1d(FFT_SIZE, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);
    
    if (!fft_in || !fft_out) {
        std::cerr << "FFT memory allocation failed!" << std::endl;
        cleanup();
        return false;
    }
    
    // setup waterfall
    init_waterfall();
    
    running = true;
    return true;
}

void SDRGui::cleanup() {
    // cleanup waterfall
    cleanup_waterfall();
    
    // cleanup fft
    if (fft_plan) {
        fftwf_destroy_plan(fft_plan);
    }
    if (fft_in) {
        fftwf_free(fft_in);
        fft_in = nullptr;
    }
    if (fft_out) {
        fftwf_free(fft_out);
        fft_out = nullptr;
    }
    
    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }
    
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    
    TTF_Quit();
    SDL_Quit();
}

void SDRGui::handle_events() {
    SDL_Event event;
    
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
                
            case SDL_KEYDOWN:
                handle_keyboard(event.key.keysym.sym);
                break;
        }
    }
}

void SDRGui::handle_keyboard(SDL_Keycode key) {
    const uint32_t freq_step = 100000; // 100 kHz
    const int gain_step = 10; // 1 dB
    
    switch (key) {
        case SDLK_UP:
            target_frequency += freq_step;
            frequency_changed = true;
            user_manual_control = true;  // user is taking manual control
            break;
            
        case SDLK_DOWN:
            if (target_frequency > freq_step) {
                target_frequency -= freq_step;
                frequency_changed = true;
                user_manual_control = true;  // user is taking manual control
            }
            break;
            
        case SDLK_RIGHT:
            target_frequency += freq_step * 10;
            frequency_changed = true;
            user_manual_control = true;  // user is taking manual control
            break;
            
        case SDLK_LEFT:
            if (target_frequency > freq_step * 10) {
                target_frequency -= freq_step * 10;
                frequency_changed = true;
                user_manual_control = true;  // user is taking manual control
            }
            break;
            
        case SDLK_PLUS:
        case SDLK_EQUALS:
            target_gain += gain_step;
            if (target_gain > 500) target_gain = 500; // max gain limit
            gain_changed = true;
            break;
            
        case SDLK_MINUS:
            target_gain -= gain_step;
            if (target_gain < 0) target_gain = 0; // min gain limit
            gain_changed = true;
            break;
            
        case SDLK_s:
            // toggle protocol scanning
            if (!protocol_scanning_enabled) {
                protocol_scanning_enabled = true;
                protocol_scanning_paused = false;
                user_manual_control = false;
                if (protocol_analyzer_ref) {
                    protocol_analyzer_ref->start_frequency_scan();
                }
                std::cout << "Protocol scanning started" << std::endl;
            } else {
                protocol_scanning_enabled = false;
                if (protocol_analyzer_ref) {
                    protocol_analyzer_ref->stop_frequency_scan();
                }
                std::cout << "Protocol scanning stopped" << std::endl;
            }
            break;
            
        case SDLK_p:
            // toggle pause protocol scanning
            if (protocol_scanning_enabled) {
                protocol_scanning_paused = !protocol_scanning_paused;
                std::cout << "Protocol scanning " << (protocol_scanning_paused ? "paused" : "resumed") << std::endl;
            }
            break;
            
        case SDLK_m:
            // return to manual control
            user_manual_control = true;
            protocol_scanning_paused = true;
            std::cout << "Manual frequency control enabled" << std::endl;
            break;
            
        case SDLK_ESCAPE:
        case SDLK_q:
            running = false;
            break;
    }
}

void SDRGui::update() {
    // gui update logic here
}

void SDRGui::render() {
    // clear screen
    SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    SDL_RenderClear(renderer);
    
    // draw grid
    draw_grid();
    
    // render spectrum if sdr data is available
    if (sdr_ref && !sdr_ref->get_iq_buffer().empty()) {
        render_spectrum(sdr_ref->get_iq_buffer());
    }
    
    // render waterfall
    render_waterfall();
    
    // render control panel
    render_controls();
    
    // render protocol panel
    render_protocol_panel();
    
    SDL_RenderPresent(renderer);
}

void SDRGui::draw_grid() {
    SDL_SetRenderDrawColor(renderer, grid_color.r, grid_color.g, grid_color.b, grid_color.a);
    
    // vertical grid lines for spectrum
    for (int x = 0; x < WINDOW_WIDTH; x += 80) {
        SDL_RenderDrawLine(renderer, x, SPECTRUM_OFFSET, x, SPECTRUM_OFFSET + SPECTRUM_HEIGHT);
    }
    
    // horizontal grid lines for spectrum
    for (int y = SPECTRUM_OFFSET; y < SPECTRUM_OFFSET + SPECTRUM_HEIGHT; y += 40) {
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }
    
    // vertical grid lines for waterfall
    for (int x = 0; x < WINDOW_WIDTH; x += 80) {
        SDL_RenderDrawLine(renderer, x, WATERFALL_OFFSET, x, WATERFALL_OFFSET + WATERFALL_HEIGHT);
    }
    
    // horizontal grid lines for waterfall
    for (int y = WATERFALL_OFFSET; y < WATERFALL_OFFSET + WATERFALL_HEIGHT; y += 60) {
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }
}

void SDRGui::render_spectrum(const std::vector<std::complex<float>>& iq_data) {
    if (iq_data.empty()) return;
    
    std::vector<float> fft_mag = compute_fft_magnitude(iq_data);
    
    // update waterfall with new fft data
    update_waterfall_data(fft_mag);
    
    SDL_SetRenderDrawColor(renderer, spectrum_color.r, spectrum_color.g, spectrum_color.b, spectrum_color.a);
    
    int num_points = std::min((int)fft_mag.size(), WINDOW_WIDTH);
    
    for (int i = 1; i < num_points; i++) {
        int x1 = (i - 1) * WINDOW_WIDTH / num_points;
        int x2 = i * WINDOW_WIDTH / num_points;
        
        // normalize and scale magnitude for display
        float mag1 = std::max(0.0f, std::min(2.0f, (fft_mag[i-1] + 100.0f) / 100.0f));
        float mag2 = std::max(0.0f, std::min(2.0f, (fft_mag[i] + 100.0f) / 100.0f));
        
        int y1 = SPECTRUM_HEIGHT - (int)(mag1 * SPECTRUM_HEIGHT) + SPECTRUM_OFFSET;
        int y2 = SPECTRUM_HEIGHT - (int)(mag2 * SPECTRUM_HEIGHT) + SPECTRUM_OFFSET;
        
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
    
    // draw center frequency indicator line
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // red line
    int center_x = WINDOW_WIDTH / 2;
    SDL_RenderDrawLine(renderer, center_x, SPECTRUM_OFFSET, center_x, SPECTRUM_OFFSET + SPECTRUM_HEIGHT);
}

void SDRGui::render_controls() {
    int control_y = WATERFALL_OFFSET + WATERFALL_HEIGHT + SECTION_SPACING;
    
    // frequency display
    std::ostringstream freq_stream;
    freq_stream << "Frequency: " << std::fixed << std::setprecision(3) 
                << (target_frequency / 1000000.0) << " MHz";
    render_text(freq_stream.str(), MARGIN, control_y);
    
    // gain display
    std::ostringstream gain_stream;
    gain_stream << "Gain: " << (target_gain / 10.0) << " dB";
    render_text(gain_stream.str(), MARGIN, control_y + TEXT_LINE_HEIGHT);
    
    // mode indicator
    std::string mode_text = user_manual_control ? "Mode: MANUAL" : 
                           (protocol_scanning_enabled ? 
                            (protocol_scanning_paused ? "Mode: SCANNING (PAUSED)" : "Mode: SCANNING") : 
                            "Mode: IDLE");
    
    SDL_Color mode_color = user_manual_control ? SDL_Color{100, 255, 100, 255} :  // green for manual
                          (protocol_scanning_enabled && !protocol_scanning_paused) ? SDL_Color{255, 255, 100, 255} :  // yellow for scanning
                          SDL_Color{255, 100, 100, 255};  // red for paused/idle
    
    render_text_colored(mode_text, 450, control_y, mode_color);
    
    // instructions
    int instruction_y = control_y + TEXT_LINE_HEIGHT * 2 + SECTION_SPACING / 2;
    render_text("Controls: Arrows (freq) +/- (gain) S (scan) P (pause) M (manual) Q (quit)", MARGIN, instruction_y);
    render_text("Protocol Scanner: S=Start/Stop P=Pause M=Manual Control", MARGIN, instruction_y + TEXT_LINE_HEIGHT);
}

void SDRGui::render_text(const std::string& text, int x, int y) {
    if (!font) return;
    
    SDL_Surface* text_surface = TTF_RenderText_Solid(font, text.c_str(), text_color);
    if (!text_surface) return;
    
    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
    if (!text_texture) {
        SDL_FreeSurface(text_surface);
        return;
    }
    
    SDL_Rect dest_rect = {x, y, text_surface->w, text_surface->h};
    SDL_RenderCopy(renderer, text_texture, nullptr, &dest_rect);
    
    SDL_DestroyTexture(text_texture);
    SDL_FreeSurface(text_surface);
}

void SDRGui::render_text_colored(const std::string& text, int x, int y, SDL_Color color) {
    if (!font) return;
    
    SDL_Surface* text_surface = TTF_RenderText_Solid(font, text.c_str(), color);
    if (!text_surface) return;
    
    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
    if (!text_texture) {
        SDL_FreeSurface(text_surface);
        return;
    }
    
    SDL_Rect dest_rect = {x, y, text_surface->w, text_surface->h};
    SDL_RenderCopy(renderer, text_texture, nullptr, &dest_rect);
    
    SDL_DestroyTexture(text_texture);
    SDL_FreeSurface(text_surface);
}

void SDRGui::render_protocol_panel() {
    if (!protocol_analyzer_ref) return;
    
    int panel_y = WATERFALL_OFFSET + WATERFALL_HEIGHT + CONTROL_PANEL_HEIGHT + SECTION_SPACING;
    
    // panel background
    SDL_Rect panel_rect = {MARGIN/2, panel_y, WINDOW_WIDTH - MARGIN, 120};
    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
    SDL_RenderFillRect(renderer, &panel_rect);
    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(renderer, &panel_rect);
    
    // title
    render_text("PROTOCOL ANALYZER", MARGIN, panel_y + MARGIN/2);
    
    // current scan frequency
    if (protocol_scanning_enabled && !protocol_scanning_paused) {
        std::ostringstream scan_freq;
        scan_freq << "Scanning: " << std::fixed << std::setprecision(3) 
                  << (protocol_analyzer_ref->get_current_frequency() / 1000000.0) << " MHz";
        render_text_colored(scan_freq.str(), 250, panel_y + MARGIN/2, SDL_Color{255, 255, 100, 255});
    }
    
    // device count
    auto detected_devices = protocol_analyzer_ref->get_detected_devices();
    std::ostringstream device_count;
    device_count << "Devices Found: " << detected_devices.size();
    render_text(device_count.str(), MARGIN, panel_y + MARGIN/2 + TEXT_LINE_HEIGHT);
    
    // show last few detected protocols
    int x_offset = 250;
    int max_display = 4;
    int displayed = 0;
    
    for (const auto& device : detected_devices) {
        if (displayed >= max_display) break;
        
        std::ostringstream protocol_info;
        protocol_info << protocol_analyzer_ref->get_protocol_name(device.protocol) 
                      << " (" << std::fixed << std::setprecision(1) 
                      << (device.signal.frequency / 1000000.0) << "MHz)";
        
        SDL_Color device_color = device.is_authorized ? 
            SDL_Color{100, 255, 100, 255} :  // green for authorized
            SDL_Color{255, 100, 100, 255};   // red for unauthorized
            
        int device_y = panel_y + MARGIN/2 + TEXT_LINE_HEIGHT + (displayed % 2) * TEXT_LINE_HEIGHT;
        render_text_colored(protocol_info.str(), x_offset, device_y, device_color);
        displayed++;
        
        if (displayed == 2) {
            x_offset = MARGIN;  // Move to next column with proper margin
        }
    }
    
    // Security alerts with better positioning
    auto alerts = protocol_analyzer_ref->get_security_alerts();
    if (!alerts.empty()) {
        render_text_colored("SECURITY ALERT!", 550, panel_y + MARGIN/2, SDL_Color{255, 50, 50, 255});
        
        // Show first alert
        if (!alerts.empty()) {
            std::string alert_text = alerts[0];
            if (alert_text.length() > 50) {
                alert_text = alert_text.substr(0, 47) + "...";
            }
            render_text_colored(alert_text, 550, panel_y + MARGIN/2 + TEXT_LINE_HEIGHT, SDL_Color{255, 150, 150, 255});
        }
    }
}

std::vector<float> SDRGui::compute_fft_magnitude(const std::vector<std::complex<float>>& iq_data) {
    std::vector<float> magnitudes;
    
    if (iq_data.empty() || !fft_in || !fft_out) {
        return magnitudes;
    }
    
    // Take a chunk of data for FFT
    int samples_to_use = std::min((int)iq_data.size(), FFT_SIZE);
    
    // Copy I/Q data to FFT input buffer
    for (int i = 0; i < samples_to_use; i++) {
        fft_in[i][0] = iq_data[i].real();  // Real part
        fft_in[i][1] = iq_data[i].imag();  // Imaginary part
    }
    
    // Zero-pad if needed
    for (int i = samples_to_use; i < FFT_SIZE; i++) {
        fft_in[i][0] = 0.0f;
        fft_in[i][1] = 0.0f;
    }
    
    // Execute FFT
    fftwf_execute(fft_plan);
    
    // Calculate magnitude spectrum in dB
    magnitudes.reserve(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; i++) {
        float real = fft_out[i][0];
        float imag = fft_out[i][1];
        float magnitude = std::sqrt(real * real + imag * imag);
        float magnitude_db = 20.0f * std::log10(magnitude + 1e-10f);
        magnitudes.push_back(magnitude_db);
    }
    
    return magnitudes;
}

void SDRGui::init_waterfall() {
    // Initialize waterfall data buffer
    waterfall_data.clear();
    waterfall_data.reserve(WATERFALL_HISTORY);
    
    // Create waterfall texture
    waterfall_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, 
                                         SDL_TEXTUREACCESS_STREAMING, 
                                         WINDOW_WIDTH, WATERFALL_HEIGHT);
    
    if (!waterfall_texture) {
        std::cerr << "Failed to create waterfall texture: " << SDL_GetError() << std::endl;
        return;
    }
    
    // Allocate pixel buffer
    waterfall_pixels = new uint32_t[WINDOW_WIDTH * WATERFALL_HEIGHT];
    memset(waterfall_pixels, 0, WINDOW_WIDTH * WATERFALL_HEIGHT * sizeof(uint32_t));
}

void SDRGui::cleanup_waterfall() {
    if (waterfall_texture) {
        SDL_DestroyTexture(waterfall_texture);
        waterfall_texture = nullptr;
    }
    
    if (waterfall_pixels) {
        delete[] waterfall_pixels;
        waterfall_pixels = nullptr;
    }
    
    waterfall_data.clear();
}

void SDRGui::update_waterfall_data(const std::vector<float>& fft_magnitudes) {
    if (fft_magnitudes.empty()) return;
    
    // Add new line to waterfall data
    waterfall_data.push_back(fft_magnitudes);
    
    // Keep only the most recent history
    if (waterfall_data.size() > WATERFALL_HISTORY) {
        waterfall_data.erase(waterfall_data.begin());
    }
}

uint32_t SDRGui::magnitude_to_color(float magnitude_db) {
    // Normalize magnitude to 0-1 range (assuming -100 to 0 dB range)
    float normalized = std::max(0.0f, std::min(1.0f, (magnitude_db + 100.0f) / 100.0f));
    
    // Black to white gradient
    uint8_t intensity = (uint8_t)(normalized * 255);
    uint8_t r = intensity;
    uint8_t g = intensity;
    uint8_t b = intensity;
    uint8_t a = 255;
    
    return (r << 24) | (g << 16) | (b << 8) | a;
}

void SDRGui::render_waterfall() {
    if (!waterfall_texture || !waterfall_pixels || waterfall_data.empty()) return;
    
    // Shift existing pixels down
    for (int y = WATERFALL_HEIGHT - 1; y > 0; y--) {
        memcpy(&waterfall_pixels[y * WINDOW_WIDTH], 
               &waterfall_pixels[(y - 1) * WINDOW_WIDTH], 
               WINDOW_WIDTH * sizeof(uint32_t));
    }
    
    // Add new line at top
    const std::vector<float>& latest_fft = waterfall_data.back();
    int top_row = 0;
    
    for (int x = 0; x < WINDOW_WIDTH; x++) {
        int fft_index = x * latest_fft.size() / WINDOW_WIDTH;
        if (fft_index >= 0 && fft_index < (int)latest_fft.size()) {
            waterfall_pixels[top_row + x] = magnitude_to_color(latest_fft[fft_index]);
        } else {
            waterfall_pixels[top_row + x] = 0xFF000000; // Black
        }
    }
    
    // Update texture
    void* pixels;
    int pitch;
    if (SDL_LockTexture(waterfall_texture, nullptr, &pixels, &pitch) == 0) {
        memcpy(pixels, waterfall_pixels, WINDOW_WIDTH * WATERFALL_HEIGHT * sizeof(uint32_t));
        SDL_UnlockTexture(waterfall_texture);
    }
    
    // Render waterfall
    SDL_Rect waterfall_rect = {0, WATERFALL_OFFSET, WINDOW_WIDTH, WATERFALL_HEIGHT};
    SDL_RenderCopy(renderer, waterfall_texture, nullptr, &waterfall_rect);
}
