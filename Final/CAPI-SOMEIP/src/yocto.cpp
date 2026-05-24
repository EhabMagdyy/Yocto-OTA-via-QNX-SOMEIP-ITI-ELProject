#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <CommonAPI/CommonAPI.hpp>
#include "v1/commonapi/OtaProxy.hpp"

using namespace v1::commonapi;

// ==================================================================
// RPi3 Yocto is the PROXY (client side).
//
// Responsibilities:
//   1. Connect to QNX OTA service via VSOMEIP service discovery
//   2. Subscribe to otaAvailable event
//   3. When event arrives: validate sha256 format + check not busy
//   4. Call confirmReceived("accepted") or ("rejected") on QNX
//   5. Subscribe to otaStatus events to track QNX progress
//   6. When SCP is done (otaStatus = "flashing") ota.sh is already
//      running — just monitor /var/log/ota-update.log
// ==================================================================

static std::atomic<bool> running(true);
static std::atomic<bool> busy(false);   // prevent concurrent OTA
static void signalHandler(int) { running = false; }

bool isValidSha256(const std::string& s){
    if (s.size() != 64) return false;
    for (char c : s)
        if (!isxdigit(c)) return false;
    return true;
}

int main(){
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto runtime = CommonAPI::Runtime::get();
    auto proxy   = runtime->buildProxy<OtaProxy>(
        "local", "commonapi.Ota", "yoctoService");

    std::cout << "[RPi3] Waiting for QNX OTA service..." << std::endl;

    // Block until QNX service is discovered via VSOMEIP SD
    while (!proxy->isAvailable() && running)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (!running) return 0;
    std::cout << "[RPi3] QNX service found" << std::endl;

    // ── Subscribe to otaAvailable ──────────────────────────────────
    // Fires when QNX has a validated image ready to send
    proxy->getOtaAvailableEvent().subscribe([&proxy](const std::string& sha256, const uint64_t& size){
        std::cout << "\n[RPi3] === OTA Available ===" << std::endl;
        std::cout << "[RPi3] SHA256: " << sha256 << std::endl;
        std::cout << "[RPi3] Size:   " << size << " bytes" << std::endl;

        // Reject if already doing an OTA
        if (busy.load()) {
            std::cout << "[RPi3] Already busy — rejecting" << std::endl;
            CommonAPI::CallStatus cs;
            proxy->confirmReceived("rejected", cs);
            return;
        }

        // Validate sha256 format
        if (!isValidSha256(sha256)) {
            std::cout << "[RPi3] Invalid SHA256 — rejecting" << std::endl;
            CommonAPI::CallStatus cs;
            proxy->confirmReceived("rejected", cs);
            return;
        }

        // Accept — tell QNX to start SCP
        busy.store(true);
        std::cout << "[RPi3] Accepting OTA" << std::endl;
        CommonAPI::CallStatus cs;
        proxy->confirmReceived("accepted", cs);

        if (cs != CommonAPI::CallStatus::SUCCESS) {
            std::cerr << "[RPi3] confirmReceived call failed" << std::endl;
            busy.store(false);
        }
        // After this, QNX starts SCP and then SSH-triggers ota.sh
        // ota.sh handles the rest: write → verify → switch → reboot
    });

    // ========= Subscribe to otaStatus
    // QNX fires these to tell RPi3 what's happening on its side
    proxy->getOtaStatusEvent().subscribe([](const std::string& status, const std::string& message){
        std::cout << "[RPi3] QNX status: [" << status << "] " << message << std::endl;

        if(status == "error") {
            std::cerr << "[RPi3] QNX reported error — resetting busy flag" << std::endl;
            busy.store(false);
        }

        if (status == "flashing") {
            // ota.sh is now running on RPi3
            // We can monitor our own log
            std::cout << "[RPi3] ota.sh started — monitor: "
                      << "tail -f /var/log/ota-update.log" << std::endl;
        }
    });

    while(running){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "[RPi3] Shutting down" << std::endl;

    return 0;
}