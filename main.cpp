#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>

// --- Config ---
const int   DELAY_MIN_MS = 15;
const int   DELAY_MAX_MS = 40;
const int   SENS_INTERVAL_MIN = 5000;
const int   SENS_INTERVAL_MAX = 10000;
const int   BURST_INTERVAL_MIN = 3000;
const int   BURST_INTERVAL_MAX = 6000;
const int   BURST_COUNT_MIN = 2;
const int   BURST_COUNT_MAX = 3;
const int   BURST_CHANCE = 25;
bool  ENABLE_DELAY = false;
bool  ENABLE_SENSITIVITY = false;

// Listen keys -> osu! keys
const int  PHYS_KEY_1 = 'A';
const int  PHYS_KEY_2 = 'D';
const WORD VIRT_KEY_1 = 'Z';
const WORD VIRT_KEY_2 = 'X';

// --- Globals ---
std::atomic<bool> running(true);
std::atomic<bool> trigger1Active(false);
std::atomic<bool> trigger2Active(false);
std::mt19937 rng(std::random_device{}());

int randInt(int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(rng);
}

// --- Key send helpers ---
void sendKeyEvent(WORD vKey, bool down) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vKey;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

// --- Hold trigger: delays press, holds virtual key as long as physical is held ---
void executeHoldTrigger(int triggerId, int physicalKey) {
    WORD targetVKey = (triggerId == 1) ? VIRT_KEY_1 : VIRT_KEY_2;

    // Check if key is still physically held before we even delay
    // For singletaps this might already be false — send a tap anyway
    bool stillHeld = (GetAsyncKeyState(physicalKey) & 0x8000) != 0;

    int delay = randInt(DELAY_MIN_MS, DELAY_MAX_MS);

    // If it's a singletap (released before delay), cap the delay so it still registers
    if (!stillHeld) {
        delay = min(delay, 30); // fast tap: don't delay more than 30ms
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(delay));

    // Press down
    sendKeyEvent(targetVKey, true);
    std::cout << "[Trigger " << triggerId << "] Key DOWN after " << delay << "ms delay\n";

    // If already released (singletap), hold virtual key for a minimum guaranteed duration
    if (!(GetAsyncKeyState(physicalKey) & 0x8000)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30)); // guaranteed register window
    }
    else {
        // Still held — poll in 1ms chunks (was 5ms, too slow for fast taps)
        while (GetAsyncKeyState(physicalKey) & 0x8000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    sendKeyEvent(targetVKey, false);
    std::cout << "[Trigger " << triggerId << "] Key UP\n";

    if (triggerId == 1) trigger1Active = false;
    else                trigger2Active = false;
}

// --- Sensitivity randomizer ---
void applyRandomSens() {
    int zone = randInt(1, 10);
    int speed;
    if (zone <= 7) speed = randInt(6, 12);
    else                speed = randInt(13, 15);

    float mult = speed / 10.0f;
    SystemParametersInfo(SPI_SETMOUSESPEED, 0, (PVOID)(intptr_t)speed, SPIF_SENDCHANGE);
    std::cout << "[Sensitivity] Speed set to " << speed << " (x" << mult << ")\n";
}

void restoreMouseSensitivity() {
    SystemParametersInfo(SPI_SETMOUSESPEED, 0, (PVOID)(intptr_t)10, SPIF_SENDCHANGE);
    std::cout << "[Sensitivity] Restored to default\n";
}

void sensitivityThread() {
    while (running) {                          // exits on END key
        if (!ENABLE_SENSITIVITY) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;                          // idle loop, no changes
        }

        applyRandomSens();

        if (randInt(1, 100) <= BURST_CHANCE) {
            int count = randInt(BURST_COUNT_MIN, BURST_COUNT_MAX);
            std::cout << "[Burst] " << count << " rapid changes\n";
            for (int i = 0; i < count && running && ENABLE_SENSITIVITY; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    randInt(BURST_INTERVAL_MIN, BURST_INTERVAL_MAX)));
                applyRandomSens();
            }
        }

        int interval = randInt(SENS_INTERVAL_MIN, SENS_INTERVAL_MAX);
        if (randInt(1, 100) <= 20)
            interval /= randInt(2, 4);

        // Sleep in small chunks so it can react to `running` going false quickly
        int elapsed = 0;
        while (running && ENABLE_SENSITIVITY && elapsed < interval) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            elapsed += 50;
        }
    }
}


int main() {
    std::cout << "=== osu! Chaos Trainer ===\n";
    std::cout << "A -> Z  |  D -> X  (random delay, hold-compatible)\n";
    std::cout << "F1: toggle sensitivity  |  F2: toggle delay\n";
    std::cout << "END key to quit\n\n";

    std::thread sensThread(sensitivityThread);

    bool f1WasDown = false;
    bool f2WasDown = false;

    while (running) {
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            running = false;
            break;
        }

        // F1 toggle - debounced
        bool f1Down = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (f1Down && !f1WasDown) {
            ENABLE_SENSITIVITY = !ENABLE_SENSITIVITY;
            if (!ENABLE_SENSITIVITY) {
                restoreMouseSensitivity();
                std::cout << "Mouse sensitivity randomizer disabled.\n";
            }
            else {
                std::cout << "Mouse sensitivity randomizer enabled.\n";
            }
        }
        f1WasDown = f1Down;

        // F2 toggle - debounced
        bool f2Down = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
        if (f2Down && !f2WasDown) {
            ENABLE_DELAY = !ENABLE_DELAY;
            if (!ENABLE_DELAY) {
                std::cout << "Input delay randomizer disabled.\n";
            }
            else {
                std::cout << "Input delay randomizer enabled.\n";
            }
        }
        f2WasDown = f2Down;

        // Key triggers
        if (ENABLE_DELAY) {
            if ((GetAsyncKeyState(PHYS_KEY_1) & 0x8000) && !trigger1Active) {
                trigger1Active = true;
                std::thread(executeHoldTrigger, 1, PHYS_KEY_1).detach();
            }
            if ((GetAsyncKeyState(PHYS_KEY_2) & 0x8000) && !trigger2Active) {
                trigger2Active = true;
                std::thread(executeHoldTrigger, 2, PHYS_KEY_2).detach();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Always join — thread checks `running` internally to exit
    sensThread.join();
    restoreMouseSensitivity();
    std::cout << "Exited cleanly.\n";
    return 0;
}
