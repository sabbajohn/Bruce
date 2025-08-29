#ifndef __SUBGHZ_MENU_H__
#define __SUBGHZ_MENU_H__

#include "MenuItemInterface.h"

class SubGHZMenu : public MenuItemInterface {
public:
    SubGHZMenu() : MenuItemInterface("SubGHZ") {}

    void optionsMenu(void) override;
    void drawIcon(float scale) override;
    void drawIconImg() override;
    bool getTheme() override;

private:
    void configMenu(void);
    void protocolsMenu(void);
    void attacksMenu(void);
    void memoryStatsMenu(void);
    void testMenu(void);

    // Practical capture functions
    void practicalCaptureMenu(void);
    void practicalFrequencyScan(void);
    void practicalSignalCapture(void);
    void practicalProtocolAnalysis(void);
    void practicalLiveMonitor(void);

    // Protocol implementations
    void testPrincetonProtocol(void);
    void testCAMEProtocol(void);
    void testKeeLoqProtocol(void);
    void testSomfyProtocol(void);
    void rawSignalHandler(void);

    // Attack implementations
    void rollJamAttack(void);
    void rollBackAttack(void);
    void replayAttack(void);
};

#endif // __SUBGHZ_MENU_H__
