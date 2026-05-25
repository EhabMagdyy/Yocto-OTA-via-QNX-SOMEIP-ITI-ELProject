#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <CommonAPI/CommonAPI.hpp>
#include "v1/commonapi/OtaStubDefault.hpp"

using namespace v1::commonapi;

static std::atomic<bool> running(true);
static std::atomic<bool> busy(false);
static void signalHandler(int) { running = false; }

bool isValidSha256(const std::string& s){
    if (s.size() != 64) return false;
    for (char c : s) if (!isxdigit(c)) return false;
    return true;
}

class OtaStubImpl : public OtaStubDefault {
public:
    void triggerOta(const std::shared_ptr<CommonAPI::ClientId> _client, std::string sha256, uint64_t size, triggerOtaReply_t reply) override {
        std::cout << "\n[RPi3] === OTA Available ===" << std::endl;
        std::cout << "[RPi3] SHA256: " << sha256 << std::endl;
        std::cout << "[RPi3] Size:   " << size << " bytes" << std::endl;

        if (busy.load()) {
            std::cout << "[RPi3] Already busy — rejecting" << std::endl;
            reply("rejected");
            return;
        }

        if (!isValidSha256(sha256)) {
            std::cout << "[RPi3] Invalid SHA256 — rejecting" << std::endl;
            reply("rejected");
            return;
        }

        busy.store(true);
        std::cout << "[RPi3] Accepting OTA. Sending confirmation to QNX..." << std::endl;
        reply("accepted");
    }

    void updateStatus(const std::shared_ptr<CommonAPI::ClientId> _client, std::string status, std::string message, updateStatusReply_t reply) override {
        reply(); // unblock QNX immediately
        std::cout << "[RPi3] QNX status: [" << status << "] " << message << std::endl;

        if(status == "error") {
            std::cerr << "[RPi3] QNX reported error — resetting busy flag" << std::endl;
            busy.store(false);
        }
        else if (status == "flashing") {
            std::cout << "[RPi3] ota.sh started — monitor: tail -f /var/log/ota-update.log" << std::endl;
        }
        else if (status == "done") {
            std::cout << "[RPi3] OTA complete! Resetting state." << std::endl;
            busy.store(false);
        }
    }
};

int main() {
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto runtime = CommonAPI::Runtime::get();
    auto stub = std::make_shared<OtaStubImpl>();

    bool ok = runtime->registerService("local", "commonapi.Ota", stub, "yoctoService");
    if(!ok) {
        std::cerr << "[RPi3] Failed to register OTA service" << std::endl;
        return 1;
    }

    std::cout << "[RPi3] Server started. Waiting for QNX OTA client..." << std::endl;

    while(running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "[RPi3] Shutting down" << std::endl;
    return 0;
}