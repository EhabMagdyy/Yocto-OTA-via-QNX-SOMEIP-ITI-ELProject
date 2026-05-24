#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <CommonAPI/CommonAPI.hpp>
#include "v1/commonapi/OtaStubDefault.hpp"

using namespace v1::commonapi;

// ==================================================================
// QNX is the STUB (server side).
//
// Responsibilities:
//   1. Register the OTA service on the network
//   2. When triggered (by host script), fire otaAvailable event
//      carrying only sha256 + size — UUID check already done
//   3. Wait for RPi3 to call confirmReceived
//   4. If accepted → SCP image to RPi3 staging → trigger ota.sh
//   5. Fire otaStatus events to keep RPi3 informed of progress
// ==================================================================

static std::atomic<bool> running(true);
static void signalHandler(int) { running = false; }

class OtaStubImpl : public OtaStubDefault {

    std::string m_sha256;
    uint64_t    m_size = 0;

    // === RPi3 calls this method after receiving otaAvailable event 
    // This is the ONLY method RPi3 can call on QNX.
    // status = "accepted" → start SCP
    // status = "rejected" → log and stop
    void confirmReceived(const std::shared_ptr<CommonAPI::ClientId>, std::string status, confirmReceivedReply_t reply) override{
        // Reply immediately so RPi3 doesn't block
        reply();

        std::cout << "[QNX] RPi3 response: " << status << std::endl;

        if(status == "accepted") {
            // Run SCP and ota.sh in a separate thread
            // so we don't block the CAPI event loop
            std::thread([this]() { doTransfer(); }).detach();
        }
        else {
            std::cout << "[QNX] RPi3 rejected — OTA aborted" << std::endl;
            fireOtaStatusEvent("error", "RPi3 rejected the update");
        }
    }

    // SCP image to RPi3 then trigger ota.sh
    void doTransfer() {
        const std::string RPI3_IP      = "192.168.50.50";
        const std::string REMOTE_STAGE = "/mnt/staging/ota_image.ext4";
        const std::string LOCAL_IMAGE  = "/tmp/ota_image.ext4";

        // Step 1 - SCP
        fireOtaStatusEvent("scp_started", "transferring image to RPi3");
        std::string scpCmd = "scp " + LOCAL_IMAGE + " root@" + RPI3_IP + ":" + REMOTE_STAGE;

        std::cout << "[QNX] " << scpCmd << std::endl;
        int ret = std::system(scpCmd.c_str());

        if(ret != 0){
            std::cerr << "[QNX] SCP failed: " << ret << std::endl;
            fireOtaStatusEvent("error", "SCP failed");
            return;
        }

        fireOtaStatusEvent("scp_done", "image transferred successfully");

        // Step 2 - trigger ota.sh on RPi3 (detached via nohup)
        std::string otaCmd =
            "ssh root@" + RPI3_IP +
            " \"nohup /usr/bin/ota.sh " +
            m_sha256 + " " +
            std::to_string(m_size) + " " +
            REMOTE_STAGE +
            " > /var/log/ota-update.log 2>&1 &\"";

        std::cout << "[QNX] " << otaCmd << std::endl;
        ret = std::system(otaCmd.c_str());

        if(ret == 0) {
            fireOtaStatusEvent("flashing", "ota.sh started on RPi3");
        }
        else{
            fireOtaStatusEvent("error", "failed to trigger ota.sh");
        }
    }

public:
    // Called from main() when host script triggers QNX
    void notifyOtaAvailable(const std::string& sha256, uint64_t size) {
        m_sha256 = sha256;
        m_size   = size;

        std::cout << "[QNX] Firing otaAvailable event" << std::endl;
        std::cout << "[QNX] SHA256: " << sha256 << std::endl;
        std::cout << "[QNX] Size:   " << size << " bytes" << std::endl;

        fireOtaAvailableEvent(sha256, size);
    }
};

// Main
int main(int argc, char* argv[]){
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // sha256 and size passed as arguments from qnx-ota-validate.sh
    if(argc < 3){
        std::cerr << "Usage: qnxService <sha256> <size>" << std::endl;
        return 1;
    }

    std::string sha256 = argv[1];
    uint64_t size = std::stoull(argv[2]);

    auto runtime = CommonAPI::Runtime::get();
    auto stub = std::make_shared<OtaStubImpl>();

    bool ok = runtime->registerService("local", "commonapi.Ota", stub, "qnxService");

    if(!ok) {
        std::cerr << "[QNX] Failed to register OTA service" << std::endl;
        return 1;
    }

    std::cout << "[QNX] OTA service registered" << std::endl;

    // Give VSOMEIP time to announce the service via SD
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Fire the event — RPi3 will respond via confirmReceived
    stub->notifyOtaAvailable(sha256, size);

    while(running){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    runtime->unregisterService("local", OtaStubImpl::StubInterface::getInterface(), "commonapi.Ota");

    return 0;
}