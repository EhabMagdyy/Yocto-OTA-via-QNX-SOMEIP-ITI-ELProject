#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <CommonAPI/CommonAPI.hpp>
#include "v1/commonapi/OtaProxy.hpp"

using namespace v1::commonapi;

static std::atomic<bool> running(true);
static void signalHandler(int) { running = false; }

std::shared_ptr<OtaProxy<>> myProxy;

void sendStatus(std::string status, std::string message) {
    if(!myProxy) return;
    CommonAPI::CallStatus cs;
    myProxy->updateStatus(status, message, cs);
}

void doTransfer(std::string sha256, uint64_t size) {
    const std::string RPI3_IP      = "192.168.50.50";
    const std::string REMOTE_STAGE = "/mnt/staging/ota_image.ext4";
    const std::string LOCAL_IMAGE  = "/tmp/ota_image.ext4";

    sendStatus("scp_started", "transferring image to RPi3");
    std::string scpCmd = "scp " + LOCAL_IMAGE + " root@" + RPI3_IP + ":" + REMOTE_STAGE;

    std::cout << "[QNX] " << scpCmd << std::endl;
    int ret = std::system(scpCmd.c_str());

    if(ret != 0){
        std::cerr << "[QNX] SCP failed: " << ret << std::endl;
        sendStatus("error", "SCP failed");
        return;
    }

    sendStatus("scp_done", "image transferred successfully");

    std::string otaCmd =
        "ssh root@" + RPI3_IP +
        " \"nohup /usr/bin/ota.sh " +
        sha256 + " " +
        std::to_string(size) + " " +
        REMOTE_STAGE +
        " > /var/log/ota-update.log 2>&1 &\"";

    std::cout << "[QNX] " << otaCmd << std::endl;
    ret = std::system(otaCmd.c_str());

    if(ret == 0) {
        sendStatus("flashing", "ota.sh started on RPi3");
    } else {
        sendStatus("error", "failed to trigger ota.sh");
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    if(argc < 3){
        std::cerr << "Usage: qnxClient <sha256> <size>" << std::endl;
        return 1;
    }

    std::string sha256 = argv[1];
    uint64_t size = std::stoull(argv[2]);

    auto runtime = CommonAPI::Runtime::get();
    myProxy = runtime->buildProxy<OtaProxy>("local", "commonapi.Ota", "qnxService");

    std::cout << "[QNX] Client started. Waiting for RPi3 OTA server..." << std::endl;

    while (!myProxy->isAvailable() && running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!running) return 0;
    std::cout << "[QNX] RPi3 Server found! Requesting OTA transfer..." << std::endl;

    CommonAPI::CallStatus cs;
    std::string status;
    myProxy->triggerOta(sha256, size, cs, status);

    if (cs != CommonAPI::CallStatus::SUCCESS) {
        std::cerr << "[QNX] Method call failed" << std::endl;
        return 1;
    }

    std::cout << "[QNX] RPi3 response: " << status << std::endl;

    if(status == "accepted") {
        std::thread([sha256, size]() { doTransfer(sha256, size); }).detach();
    } else {
        std::cout << "[QNX] RPi3 rejected — OTA aborted" << std::endl;
        sendStatus("error", "RPi3 rejected the update");
    }

    while(running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}