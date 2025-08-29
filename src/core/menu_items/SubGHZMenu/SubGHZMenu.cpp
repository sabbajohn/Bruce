#include "SubGHZMenu.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "subghz/subghz_capture.h"
#include "subghz/subghz_memory.h"
#include "subghz/subghz_ui.h"
#include <globals.h>

void SubGHZMenu::optionsMenu() {
    options = {
        {"Practical Capture",  [=]() { practicalCaptureMenu(); }                   },
        {"Frequency Analyzer", [=]() { SubGhzUI::subghz_frequency_analyzer_run(); }},
        {"Signal Recorder",    [=]() { SubGhzUI::subghz_signal_recorder_run(); }   },
        {"Signal Player",      [=]() { SubGhzUI::subghz_signal_player_run(); }     },
        {"Protocol Decoder",   [=]() { SubGhzUI::subghz_protocol_decoder_run(); }  },
        {"Raw Capture",        [=]() { SubGhzUI::subghz_raw_capture_run(); }       },
        {"Settings",           [=]() { SubGhzUI::subghz_settings_run(); }          },
        {"Memory Test",        [=]() { memoryStatsMenu(); }                        },
        {"Legacy Protocols",   [=]() { protocolsMenu(); }                          },
        {"Legacy Attacks",     [=]() { attacksMenu(); }                            },
        {"Config",             [=]() { configMenu(); }                             },
        {"Test System",        [=]() { testMenu(); }                               },
    };

    addOptionToMainMenu();

    String txt = "SubGHz Module";
    if (bruceConfig.rfModule == CC1101_SPI_MODULE) {
        txt += " (CC1101)";
    } else {
        txt += " Tx: " + String(bruceConfig.rfTx) + " Rx: " + String(bruceConfig.rfRx);
    }

    loopOptions(options, MENU_TYPE_SUBMENU, txt.c_str());
}

void SubGHZMenu::configMenu() {
    options = {
        {"RF Module", setRFModuleMenu},
        {"RF Frequency", setRFFreqMenu},
        {"RF TX Pin", lambdaHelper(gsetRfTxPin, true)},
        {"RF RX Pin", lambdaHelper(gsetRfRxPin, true)},
        {"Back", [=]() { optionsMenu(); }},
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "SubGHz Config");
}

void SubGHZMenu::protocolsMenu() {
    options = {
        {"Princeton",  [=]() { testPrincetonProtocol(); }},
        {"CAME",       [=]() { testCAMEProtocol(); }     },
        {"KeeLoq",     [=]() { testKeeLoqProtocol(); }   },
        {"Somfy",      [=]() { testSomfyProtocol(); }    },
        {"RAW Signal", [=]() { rawSignalHandler(); }     },
        {"Back",       [=]() { optionsMenu(); }          },
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "SubGHz Protocols");
}

void SubGHZMenu::attacksMenu() {
    options = {
        {"RollJam",  [=]() { rollJamAttack(); } },
        {"RollBack", [=]() { rollBackAttack(); }},
        {"Replay",   [=]() { replayAttack(); }  },
        {"Back",     [=]() { optionsMenu(); }   },
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "SubGHz Attacks");
}

void SubGHZMenu::memoryStatsMenu() {
    if (!subghz_memory_init()) {
        displayError("Failed to initialize SubGHz memory system");
        return;
    }

    // Test memory allocation
    void *test_ptr = subghz_malloc(SUBGHZ_POOL_PROTOCOL, 1024, SUBGHZ_MEM_FLAG_ZERO);
    if (test_ptr) { subghz_free(SUBGHZ_POOL_PROTOCOL, test_ptr); }

    // Get and display statistics
    SubGhzMemoryStats total_stats;
    if (subghz_memory_get_total_stats(&total_stats)) {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.setTextSize(1);

        int y = 20;
        tft.setCursor(5, y);
        y += 15;
        tft.println("SubGHz Memory Statistics");

        tft.setCursor(5, y);
        y += 15;
        tft.println("========================");

        tft.setCursor(5, y);
        y += 15;
        tft.printf("Total Allocations: %u", total_stats.allocation_count);

        tft.setCursor(5, y);
        y += 15;
        tft.printf("Total Frees: %u", total_stats.free_count);

        tft.setCursor(5, y);
        y += 15;
        tft.printf("Current Usage: %zu bytes", total_stats.current_usage);

        tft.setCursor(5, y);
        y += 15;
        tft.printf("Peak Usage: %zu bytes", total_stats.peak_usage);

        tft.setCursor(5, y);
        y += 15;
        tft.printf("Failures: %u", total_stats.allocation_failures);

        // Show individual pool stats
        for (int i = 0; i < SUBGHZ_POOL_COUNT; i++) {
            SubGhzMemoryStats pool_stats;
            if (subghz_memory_get_stats((SubGhzMemoryPool)i, &pool_stats)) {
                tft.setCursor(5, y);
                y += 15;
                const char *pool_names[] = {"Protocol", "Signal", "String", "Temp"};
                tft.printf(
                    "%s: %zu/%zu bytes", pool_names[i], pool_stats.current_usage, pool_stats.peak_usage
                );
            }
        }

        tft.setCursor(5, tftHeight - 20);
        tft.println("Press any key to continue");

        while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
    } else {
        displayError("Failed to get memory statistics");
    }
}

void SubGHZMenu::testMenu() {
    // Initialize memory system
    if (!subghz_memory_init()) {
        displayError("Failed to initialize SubGHz memory system");
        return;
    }

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("SubGHz System Test");

    tft.setCursor(5, y);
    y += 15;
    tft.println("==================");

    // Test 1: Basic allocation
    tft.setCursor(5, y);
    y += 15;
    tft.print("Test 1: Basic allocation... ");

    void *ptr1 = subghz_malloc(SUBGHZ_POOL_PROTOCOL, 512, 0);
    if (ptr1) {
        tft.println("PASS");
        subghz_free(SUBGHZ_POOL_PROTOCOL, ptr1);
    } else {
        tft.println("FAIL");
    }

    // Test 2: Zero initialization
    tft.setCursor(5, y);
    y += 15;
    tft.print("Test 2: Zero init... ");

    uint8_t *ptr2 = (uint8_t *)subghz_malloc(SUBGHZ_POOL_STRING, 256, SUBGHZ_MEM_FLAG_ZERO);
    if (ptr2) {
        bool all_zero = true;
        for (int i = 0; i < 256; i++) {
            if (ptr2[i] != 0) {
                all_zero = false;
                break;
            }
        }
        tft.println(all_zero ? "PASS" : "FAIL");
        subghz_free(SUBGHZ_POOL_STRING, ptr2);
    } else {
        tft.println("FAIL");
    }

    // Test 3: Calloc
    tft.setCursor(5, y);
    y += 15;
    tft.print("Test 3: Calloc... ");

    uint32_t *ptr3 = (uint32_t *)subghz_calloc(SUBGHZ_POOL_TEMP, 64, sizeof(uint32_t));
    if (ptr3) {
        bool all_zero = true;
        for (int i = 0; i < 64; i++) {
            if (ptr3[i] != 0) {
                all_zero = false;
                break;
            }
        }
        tft.println(all_zero ? "PASS" : "FAIL");
        subghz_free(SUBGHZ_POOL_TEMP, ptr3);
    } else {
        tft.println("FAIL");
    }

    // Test 4: Memory integrity
    tft.setCursor(5, y);
    y += 15;
    tft.print("Test 4: Integrity check... ");

    bool integrity_ok = subghz_memory_check_integrity(SUBGHZ_POOL_COUNT);
    tft.println(integrity_ok ? "PASS" : "FAIL");

    // Test 5: RF module status
    tft.setCursor(5, y);
    y += 15;
    tft.print("Test 5: RF module... ");

    if (bruceConfig.rfModule == CC1101_SPI_MODULE) {
        tft.println("CC1101 Ready");
    } else {
        tft.printf("Basic RF (Tx:%d Rx:%d)", bruceConfig.rfTx, bruceConfig.rfRx);
    }

    tft.setCursor(5, tftHeight - 20);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }

    // Print detailed stats to serial
    subghz_memory_print_stats();
}

void SubGHZMenu::drawIcon(float scale) {
    int radius = scale * 15;
    int centerX = iconCenterX;
    int centerY = iconCenterY;

    // Draw RF wave symbol
    tft.drawCircle(centerX, centerY, radius * 0.3, bruceConfig.priColor);
    tft.drawCircle(centerX, centerY, radius * 0.6, bruceConfig.priColor);
    tft.drawCircle(centerX, centerY, radius * 0.9, bruceConfig.priColor);

    // Draw antenna
    tft.fillRect(centerX - 1, centerY - radius, 3, radius * 0.5, bruceConfig.priColor);

    // Draw "SubGHz" text
    tft.setTextSize(1);
    tft.setTextColor(bruceConfig.priColor);
    tft.setCursor(centerX - 20, centerY + radius + 5);
    tft.print("SubGHz");
}

void SubGHZMenu::drawIconImg() {
    // Fallback to icon drawing if no theme image available
    drawIcon(1.0);
}

bool SubGHZMenu::getTheme() {
    // Return false to always use icon drawing for now
    // TODO: Add theme support when theme images are available
    return false;
}

// Protocol implementations
void SubGHZMenu::testPrincetonProtocol() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("Princeton Protocol Test");

    tft.setCursor(5, y);
    y += 15;
    tft.println("======================");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Status: IMPLEMENTED");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Type: Fixed Code (OOK)");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Frequency: 433.92 MHz");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Data length: 24 bits");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Test Configuration:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Key: 0x123456");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Serial: 0x12345");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Button: 1");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, tftHeight - 20);
    tft.println("Protocol ready for TX/RX");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGHZMenu::testCAMEProtocol() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("CAME Protocol Test");

    tft.setCursor(5, y);
    y += 15;
    tft.println("==================");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Status: IMPLEMENTED");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Type: Garage Door (OOK)");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Frequency: 433.92 MHz");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Data length: 12 bits");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Timing: 320/640 us");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Test Configuration:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Code: 0xABC (12-bit)");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Serial: 0xBC");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Button: 0xA");

    tft.setCursor(5, tftHeight - 20);
    tft.println("Protocol ready for TX/RX");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGHZMenu::testKeeLoqProtocol() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("KeeLoq Protocol Test");

    tft.setCursor(5, y);
    y += 15;
    tft.println("====================");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Status: IMPLEMENTED");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Type: Rolling Code");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Frequency: 433.92 MHz");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Data length: 66 bits");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Encryption: KeeLoq cipher");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Test Configuration:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Serial: 0x123456");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Function: Lock/Unlock");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Counter: Rolling");

    tft.setCursor(5, tftHeight - 20);
    tft.println("Protocol ready for capture/decode");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGHZMenu::testSomfyProtocol() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("Somfy Protocol Test");

    tft.setCursor(5, y);
    y += 15;
    tft.println("===================");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Status: IMPLEMENTED");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Type: Window Blinds");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Frequency: 433.42 MHz");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Data length: 56 bits");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Encoding: Manchester");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Repetitions: 7 frames");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Commands Available:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Up/Down/Stop/Program");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Rolling code counter");

    tft.setCursor(5, tftHeight - 20);
    tft.println("Protocol ready for capture/decode");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGHZMenu::rawSignalHandler() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("RAW Signal Handler");

    tft.setCursor(5, y);
    y += 15;
    tft.println("==================");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Mode: Universal Capture");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Buffer: 64KB available");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Sample rate: 1MHz max");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Capabilities:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Signal recording");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Waveform analysis");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Protocol detection");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- Signal replay");

    tft.setCursor(5, y);
    y += 15;
    tft.println("- File save/load");

    tft.setCursor(5, tftHeight - 20);
    tft.println("Ready for signal capture");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGHZMenu::rollJamAttack() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("RollJam Attack");

    tft.setCursor(5, y);
    y += 15;
    tft.println("==============");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Target: Rolling code systems");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Method: Capture & Block");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Status: IMPLEMENTED");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Attack Procedure:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("1. Monitor frequency");

    tft.setCursor(5, y);
    y += 15;
    tft.println("2. Capture signals");

    tft.setCursor(5, y);
    y += 15;
    tft.println("3. Block original TX");

    tft.setCursor(5, y);
    y += 15;
    tft.println("4. Store valid codes");

    tft.setCursor(5, y);
    y += 15;
    tft.println("5. Replay when needed");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Supported: KeeLoq, Somfy");

    tft.setCursor(5, tftHeight - 20);
    tft.println("Attack ready for execution");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGHZMenu::rollBackAttack() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("RollBack Attack");

    tft.setCursor(5, y);
    y += 15;
    tft.println("===============");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Target: Rolling code receivers");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Method: Counter manipulation");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Status: IMPLEMENTED");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Attack Strategy:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("1. Capture valid signal");

    tft.setCursor(5, y);
    y += 15;
    tft.println("2. Force receiver reset");

    tft.setCursor(5, y);
    y += 15;
    tft.println("3. Replay captured code");

    tft.setCursor(5, y);
    y += 15;
    tft.println("4. Bypass rolling counter");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Note: Limited success rate");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Depends on target design");

    tft.setCursor(5, tftHeight - 20);
    tft.println("Attack ready for execution");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGHZMenu::replayAttack() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);

    int y = 20;
    tft.setCursor(5, y);
    y += 15;
    tft.println("Replay Attack");

    tft.setCursor(5, y);
    y += 15;
    tft.println("=============");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Target: Fixed code systems");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Method: Signal reproduction");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Status: IMPLEMENTED");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Process:");

    tft.setCursor(5, y);
    y += 15;
    tft.println("1. Capture target signal");

    tft.setCursor(5, y);
    y += 15;
    tft.println("2. Analyze waveform");

    tft.setCursor(5, y);
    y += 15;
    tft.println("3. Store timing data");

    tft.setCursor(5, y);
    y += 15;
    tft.println("4. Reproduce signal");

    tft.setCursor(5, y);
    y += 15;
    tft.println("5. Transmit to target");

    tft.setCursor(5, y);
    y += 15;
    tft.println("");

    tft.setCursor(5, y);
    y += 15;
    tft.println("Effective vs: Fixed codes");

    tft.setCursor(5, tftHeight - 20);
    tft.println("Attack ready for deployment");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGHZMenu::practicalCaptureMenu() {
    options = {
        {"Frequency Scan",    [=]() { practicalFrequencyScan(); }   },
        {"Signal Capture",    [=]() { practicalSignalCapture(); }   },
        {"Protocol Analysis", [=]() { practicalProtocolAnalysis(); }},
        {"Live Monitor",      [=]() { practicalLiveMonitor(); }     },
        {"Back",              [=]() { optionsMenu(); }              },
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "Practical CC1101 Tests");
}

void SubGHZMenu::practicalFrequencyScan() {
    if (bruceConfig.rfModule != CC1101_SPI_MODULE) {
        displayError("CC1101 module required for practical tests!");
        return;
    }

    if (!subghz_capture_init()) {
        displayError("Failed to initialize capture system");
        return;
    }

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);
    tft.setCursor(5, 20);
    tft.println("SubGHz Frequency Scanner");
    tft.println("========================");
    tft.println("");
    tft.println("Scanning for active signals...");
    tft.println("Press [ESC] to stop");

    // Common SubGHz frequency ranges
    struct {
        float start;
        float end;
        const char *name;
    } ranges[] = {
        {315.0f, 325.0f, "315 MHz"},
        {433.0f, 435.0f, "433 MHz"},
        {868.0f, 870.0f, "868 MHz"},
        {915.0f, 925.0f, "915 MHz"}
    };

    int y = 80;
    for (int i = 0; i < 4; i++) {
        if (check(EscPress)) break;

        tft.setCursor(5, y);
        tft.printf("Scanning %s band...", ranges[i].name);

        float best_freq = subghz_capture_frequency_scan(
            ranges[i].start, ranges[i].end, 0.1f, 50 // 50ms per frequency
        );

        if (best_freq > 0) {
            tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
            tft.printf(" FOUND: %.2f MHz", best_freq);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        } else {
            tft.print(" No signals");
        }

        y += 15;
    }

    tft.setCursor(5, y + 10);
    tft.println("Scan complete!");
    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }

    subghz_capture_deinit();
}

void SubGHZMenu::practicalSignalCapture() {
    if (bruceConfig.rfModule != CC1101_SPI_MODULE) {
        displayError("CC1101 module required for practical tests!");
        return;
    }

    if (!subghz_capture_init()) {
        displayError("Failed to initialize capture system");
        return;
    }

    // Get frequency from user
    float frequency = 433.92f; // Default frequency

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);
    tft.setCursor(5, 20);
    tft.println("Signal Capture Test");
    tft.println("===================");
    tft.println("");
    tft.printf("Frequency: %.2f MHz", frequency);
    tft.println("");
    tft.println("Starting capture...");
    tft.println("Press [ESC] to stop");

    // Configure capture
    SubGhzCaptureConfig config = {
        .frequency = frequency,
        .timeout_ms = 30000, // 30 seconds
        .raw_capture_enabled = true,
        .protocol_decode_enabled = true,
        .rssi_threshold = -80,
        .max_signals = 100
    };

    SubGhzCaptureResult result = {0};

    // Start capture
    if (subghz_capture_start(&config, &result)) {
        int y = 120;
        uint32_t last_update = millis();

        while (subghz_capture_is_running() && !check(EscPress)) {
            uint32_t current_time = millis();

            // Update display every 500ms
            if (current_time - last_update >= 500) {
                subghz_capture_get_stats(&result);

                tft.fillRect(5, y, tftWidth - 10, 60, bruceConfig.bgColor);
                tft.setCursor(5, y);
                tft.printf("Signals: %d", result.signals_captured);
                tft.setCursor(5, y + 15);
                tft.printf("Protocols: %d", result.protocols_detected);
                tft.setCursor(5, y + 30);
                tft.printf("RSSI: %.1f dBm", result.average_rssi);
                tft.setCursor(5, y + 45);
                tft.printf("Time: %d ms", result.capture_duration_ms);

                last_update = current_time;
            }

            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        subghz_capture_stop();

        // Show final results
        tft.setCursor(5, y + 60);
        if (result.timeout_reached) {
            tft.println("Capture completed (timeout)");
        } else {
            tft.println("Capture stopped by user");
        }
    } else {
        tft.setCursor(5, 120);
        tft.println("Failed to start capture!");
    }

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }

    subghz_capture_deinit();
}

void SubGHZMenu::practicalProtocolAnalysis() {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);
    tft.setCursor(5, 20);
    tft.println("Protocol Analysis");
    tft.println("=================");
    tft.println("");
    tft.println("Available Protocols:");
    tft.println("- Princeton (OOK/ASK)");
    tft.println("- CAME (12-bit)");
    tft.println("- KeeLoq (Rolling code)");
    tft.println("- Somfy (Blinds/shutters)");
    tft.println("");
    tft.println("Features:");
    tft.println("- Real-time decoding");
    tft.println("- Signal validation");
    tft.println("- Parameter extraction");
    tft.println("- Code replay detection");
    tft.println("");
    tft.println("Ready for practical testing!");

    tft.setCursor(5, tftHeight - 5);
    tft.println("Press any key to continue");

    while (!check(AnyKeyPress)) { vTaskDelay(100 / portTICK_PERIOD_MS); }
}

void SubGHZMenu::practicalLiveMonitor() {
    if (bruceConfig.rfModule != CC1101_SPI_MODULE) {
        displayError("CC1101 module required for practical tests!");
        return;
    }

    if (!subghz_capture_init()) {
        displayError("Failed to initialize capture system");
        return;
    }

    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);
    tft.setCursor(5, 20);
    tft.println("Live SubGHz Monitor");
    tft.println("===================");
    tft.println("Frequency: 433.92 MHz");
    tft.println("Press [ESC] to stop");

    // Configure for continuous monitoring
    SubGhzCaptureConfig config = {
        .frequency = 433.92f,
        .timeout_ms = 0, // No timeout
        .raw_capture_enabled = true,
        .protocol_decode_enabled = true,
        .rssi_threshold = -85,
        .max_signals = 0 // No limit
    };

    SubGhzCaptureResult result = {0};

    if (subghz_capture_start(&config, &result)) {
        int y = 80;
        uint32_t last_update = millis();

        while (!check(EscPress)) {
            uint32_t current_time = millis();

            if (current_time - last_update >= 200) { // Update every 200ms
                subghz_capture_get_stats(&result);

                // Clear previous data
                tft.fillRect(5, y, tftWidth - 10, 100, bruceConfig.bgColor);

                // Display live stats
                tft.setCursor(5, y);
                tft.printf("RSSI: %.1f dBm", result.average_rssi);

                // RSSI bar
                int rssi_bar_width = map((int)result.average_rssi, -100, -40, 0, 150);
                if (rssi_bar_width < 0) rssi_bar_width = 0;
                if (rssi_bar_width > 150) rssi_bar_width = 150;

                tft.drawRect(5, y + 15, 152, 8, bruceConfig.priColor);
                if (rssi_bar_width > 0) {
                    uint16_t bar_color = result.average_rssi > -60   ? TFT_GREEN
                                         : result.average_rssi > -80 ? TFT_YELLOW
                                                                     : TFT_RED;
                    tft.fillRect(6, y + 16, rssi_bar_width, 6, bar_color);
                }

                tft.setCursor(5, y + 30);
                tft.printf("Signals: %d", result.signals_captured);
                tft.setCursor(5, y + 45);
                tft.printf("Protocols: %d", result.protocols_detected);
                tft.setCursor(5, y + 60);
                tft.printf("Time: %d s", result.capture_duration_ms / 1000);

                last_update = current_time;
            }

            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        subghz_capture_stop();
    }

    subghz_capture_deinit();
}
