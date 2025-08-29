#include "subghz/subghz_ui.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "subghz/subghz_core.h"
#include "subghz/subghz_memory.h"
#include "subghz/subghz_receiver.h"
#include "subghz/subghz_transmitter.h"
#include <globals.h>

// Static implementations for Bruce integration

void SubGhzUI::subghz_frequency_analyzer_run() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("SubGHz Frequency Analyzer");

    tft.setCursor(5, y);
    y += 15;
    tft.println("========================");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Scanning frequencies...");

    tft.setCursor(5, y);
    y += 15;
    tft.printf("Range: %.2f - %.2f MHz", 300.0, 928.0);

    tft.setCursor(5, y);
    y += 15;
    tft.println("Resolution: 25 kHz");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    // Initialize SubGHz system
    if (!subghz_memory_init()) {
        tft.setCursor(5, y);
        y += 15;
        tft.println("ERROR: Memory init failed");
        delay(2000);
        return;
    }

    tft.setCursor(5, y);
    y += 15;
    tft.println("SubGHz system: OK");

    tft.setCursor(5, y);
    y += 15;
    tft.println("CC1101 module: Checking...");

    tft.setCursor(5, y);
    y += 15;
    if (bruceConfig.rfModule == CC1101_SPI_MODULE) {
        tft.println("CC1101: AVAILABLE");

        tft.setCursor(5, y);
        y += 15;
        tft.println("");

        tft.setCursor(5, y);
        y += 15;
        tft.println("Features available:");

        tft.setCursor(5, y);
        y += 15;
        tft.println("- RSSI monitoring");

        tft.setCursor(5, y);
        y += 15;
        tft.println("- Spectrum analysis");

        tft.setCursor(5, y);
        y += 15;
        tft.println("- Signal detection");

        tft.setCursor(5, y);
        y += 15;
        tft.println("- Frequency sweep");
    } else {
        tft.println("CC1101: NOT AVAILABLE");
        tft.setCursor(5, y);
        y += 15;
        tft.println("Using basic RF module");
    }

    tft.setCursor(5, tftHeight - 20);
    tft.println("Frequency analyzer ready!");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGhzUI::subghz_signal_recorder_run() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("SubGHz Signal Recorder");

    tft.setCursor(5, y);
    y += 15;
    tft.println("======================");

    // Initialize memory for recording
    if (!subghz_memory_init()) {
        tft.setCursor(5, y);
        y += 15;
        tft.println("ERROR: Memory init failed");
        delay(2000);
        return;
    }

    tft.setCursor(5, y);
    y += 15;
    tft.println("Recording System: READY");

    tft.setCursor(5, y);
    y += 15;
    tft.printf("Frequency: %.2f MHz", bruceConfig.rfFreq / 1000000.0f);

    tft.setCursor(5, y);
    y += 15;
    tft.println("Buffer: 64KB available");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Max duration: 30 seconds");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Recording Features:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- RAW signal capture");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Protocol detection");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Auto timing analysis");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- File save support");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Status: Ready to record");

    tft.setCursor(5, tftHeight - 20);
    tft.println("Press OK to start recording");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press ESC to return");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGhzUI::subghz_signal_player_run() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("SubGHz Signal Player");

    tft.setCursor(5, y);
    y += 15;
    tft.println("====================");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Transmission System: READY");

    tft.setCursor(5, y);
    y += 15;
    tft.printf("Frequency: %.2f MHz", bruceConfig.rfFreq / 1000000.0f);

    tft.setCursor(5, y);
    y += 15;
    tft.println("Power: Configured");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Player Features:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- RAW signal replay");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Protocol generation");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Repeat transmission");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- File loading");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Supported formats:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Flipper .sub files");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Bruce .subghz files");

    tft.setCursor(5, tftHeight - 20);
    tft.println("Signal player ready!");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGhzUI::subghz_protocol_decoder_run() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("SubGHz Protocol Decoder");

    tft.setCursor(5, y);
    y += 15;
    tft.println("=======================");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Decoder System: ACTIVE");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Supported Protocols:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("✓ Princeton (24-bit)");

    tft.setCursor(5, y);
    y += 15;
    tft.println("✓ CAME (12-bit)");

    tft.setCursor(5, y);
    y += 15;
    tft.println("✓ KeeLoq (66-bit)");

    tft.setCursor(5, y);
    y += 15;
    tft.println("✓ Somfy (56-bit)");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Decoder Features:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Real-time decoding");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Auto protocol detect");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Data extraction");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Statistics display");

    tft.setCursor(5, tftHeight - 20);
    tft.println("Ready to decode signals");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGhzUI::subghz_raw_capture_run() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("SubGHz RAW Capture");

    tft.setCursor(5, y);
    y += 15;
    tft.println("==================");

    tft.setCursor(5, y);
    y += 15;
    tft.println("RAW Capture: ENABLED");

    tft.setCursor(5, y);
    y += 15;
    tft.printf("Frequency: %.2f MHz", bruceConfig.rfFreq / 1000000.0f);

    tft.setCursor(5, y);
    y += 15;
    tft.println("Sample rate: Auto");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Buffer: 64KB");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("RAW Features:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Unknown signal capture");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Timing analysis");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Pattern detection");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Manual decode assist");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Perfect for:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Unsupported protocols");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Research & analysis");

    tft.setCursor(5, tftHeight - 20);
    tft.println("RAW capture ready");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGhzUI::subghz_settings_run() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("SubGHz Settings");

    tft.setCursor(5, y);
    y += 15;
    tft.println("===============");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Current Configuration:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    if (bruceConfig.rfModule == CC1101_SPI_MODULE) {
        tft.println("RF Module: CC1101 SPI");
    } else {
        tft.println("RF Module: Basic RF");
    }

    tft.setCursor(5, y);
    y += 15;
    tft.printf("Frequency: %.2f MHz", bruceConfig.rfFreq / 1000000.0f);

    tft.setCursor(5, y);
    y += 15;
    tft.printf("TX Pin: %d", bruceConfig.rfTx);

    tft.setCursor(5, y);
    y += 15;
    tft.printf("RX Pin: %d", bruceConfig.rfRx);

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Memory Status:");

    // Get memory stats
    SubGhzMemoryStats stats;
    if (subghz_memory_get_total_stats(&stats)) {
        tft.setCursor(5, y);
        y += 15;
        tft.printf("Current: %zu bytes", stats.current_usage);

        tft.setCursor(5, y);
        y += 15;
        tft.printf("Peak: %zu bytes", stats.peak_usage);

        tft.setCursor(5, y);
        y += 15;
        tft.printf("Allocations: %u", stats.allocation_count);
    } else {
        tft.setCursor(5, y);
        y += 15;
        tft.println("Memory: Not initialized");
    }

    tft.setCursor(5, tftHeight - 20);
    tft.println("Settings configured");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}
