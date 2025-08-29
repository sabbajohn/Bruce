#ifndef __SUBGHZ_UI_H__
#define __SUBGHZ_UI_H__

#include "MenuItemInterface.h"
#include "subghz/subghz_core.h"
#include "subghz/subghz_receiver.h"
#include "subghz/subghz_transmitter.h"
#include <string>
#include <vector>

/**
 * @brief SubGHz UI integration for Bruce framework
 *
 * Provides menu interface and UI screens for SubGHz operations
 */

class SubGhzUI : public MenuItemInterface {
public:
    SubGhzUI();
    virtual ~SubGhzUI();

    // MenuItemInterface implementation
    void optionsMenu() override;
    void drawIcon(float scale = 1) override;
    void drawIconImg() override;
    bool getTheme() override;

    // SubGHz specific methods
    void showMainMenu();
    void showFrequencyAnalyzer();
    void showSignalRecorder();
    void showSignalPlayer();
    void showProtocolDecoder();
    void showRawCapture();
    void showSettings();

    // Static menu functions for integration
    static void subghz_frequency_analyzer_run();
    static void subghz_signal_recorder_run();
    static void subghz_signal_player_run();
    static void subghz_protocol_decoder_run();
    static void subghz_raw_capture_run();
    static void subghz_settings_run();

private:
    String _name;

    // UI state
    enum UIState {
        STATE_MAIN_MENU,
        STATE_FREQUENCY_ANALYZER,
        STATE_SIGNAL_RECORDER,
        STATE_SIGNAL_PLAYER,
        STATE_PROTOCOL_DECODER,
        STATE_RAW_CAPTURE,
        STATE_SETTINGS
    };

    UIState current_state;
    int menu_index;
    bool is_running;

    // SubGHz components
    SubGhzCore *subghz_core;
    SubGhzReceiver *receiver;
    SubGhzTransmitter *transmitter;

    // UI data
    std::vector<String> main_menu_items;
    std::vector<String> settings_menu_items;
    std::vector<String> saved_signals;

    // Current settings
    float current_frequency;
    SubGhzPreset current_preset;
    int8_t current_power;

    // Analysis data
    struct FrequencyData {
        float frequency;
        int32_t rssi;
        uint32_t activity_count;
        String protocol_name;
    };
    std::vector<FrequencyData> frequency_scan_results;

    // Signal recording
    bool is_recording;
    uint32_t recording_start_time;
    uint32_t recording_duration;

    // Protocol detection
    String last_detected_protocol;
    String last_detected_data;
    uint32_t detection_count;

    // Helper methods
    void initializeMenus();
    void initializeSubGhz();
    void cleanupSubGhz();

    // Drawing helpers
    void drawMainMenu();
    void drawFrequencyAnalyzer();
    void drawSignalRecorder();
    void drawSignalPlayer();
    void drawProtocolDecoder();
    void drawRawCapture();
    void drawSettings();
    void drawStatusBar();
    void drawProgressBar(int x, int y, int width, int height, int progress);

    // Input handling
    void handleMainMenuInput();
    void handleFrequencyAnalyzerInput();
    void handleSignalRecorderInput();
    void handleSignalPlayerInput();
    void handleProtocolDecoderInput();
    void handleRawCaptureInput();
    void handleSettingsInput();

    // SubGHz operations
    void startFrequencyAnalysis();
    void stopFrequencyAnalysis();
    void startSignalRecording();
    void stopSignalRecording();
    void playSelectedSignal();
    void startProtocolDecoding();
    void stopProtocolDecoding();
    void startRawCapture();
    void stopRawCapture();

    // File operations
    void loadSavedSignals();
    void saveSignalToFile(const String &filename);
    void loadSignalFromFile(const String &filename);
    void deleteSavedSignal(int index);

    // Settings management
    void loadSettings();
    void saveSettings();
    void resetSettings();

    // Callbacks
    static void frequencyAnalysisCallback(void *context, SubGhzProtocolConfig *protocol_config);
    static void signalRecordingCallback(void *context, bool level, uint32_t duration);
    static void protocolDecodingCallback(void *context, SubGhzProtocolConfig *protocol_config);
    static void transmissionCallback(void *context, SubGhzTransmitterEvent event);

    // Utilities
    String formatFrequency(float frequency);
    String formatRSSI(int32_t rssi);
    String formatDuration(uint32_t duration_ms);
    String getPresetName(SubGhzPreset preset);
    String getProtocolDescription(const String &protocol_name);

    // Constants
    static const int SCREEN_WIDTH = 240;
    static const int SCREEN_HEIGHT = 135;
    static const int MENU_ITEM_HEIGHT = 20;
    static const int STATUS_BAR_HEIGHT = 15;
    static const int MARGIN = 5;

    // Frequency analysis
    static constexpr float FREQUENCY_SCAN_START = 300.0f;
    static constexpr float FREQUENCY_SCAN_END = 928.0f;
    static constexpr float FREQUENCY_SCAN_STEP = 0.25f;
    static const uint32_t FREQUENCY_SCAN_DWELL_MS = 100;

    // Recording limits
    static const uint32_t MAX_RECORDING_DURATION_MS = 30000; // 30 seconds
    static const uint32_t MAX_SIGNAL_SAMPLES = 10000;
};

#endif // __SUBGHZ_UI_H__
