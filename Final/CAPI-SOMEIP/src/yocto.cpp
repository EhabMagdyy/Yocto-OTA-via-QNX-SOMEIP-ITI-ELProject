#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <fstream>
#include <CommonAPI/CommonAPI.hpp>
#include "v1/commonapi/OtaStubDefault.hpp"

using namespace v1::commonapi;

static std::atomic<bool> running(true);
static std::atomic<bool> busy(false);
static void signalHandler(int){ running = false; }

const std::string LOG_PATH = "/var/log/ota-update.log";

bool isValidSha256(const std::string& s){
    if(s.size() != 64)
        return false;

    for(char c : s){
        if(!isxdigit(c)) 
            return false;
    }

    return true;
}

class OtaStubImpl : public OtaStubDefault {
public:
    void triggerOta(const std::shared_ptr<CommonAPI::ClientId> _client, std::string sha256, uint64_t size, triggerOtaReply_t reply) override {
        std::cout << "\n[RPi3] === OTA Available ===" << std::endl;
        std::cout << "[RPi3] SHA256: " << sha256 << std::endl;
        std::cout << "[RPi3] Size:   " << size << " bytes" << std::endl;

        if(busy.load()){
            std::cout << "[RPi3] Already busy — rejecting" << std::endl;
            reply("rejected");

            return;
        }

        if(!isValidSha256(sha256)){
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

        if(status == "error"){
            std::cerr << "[RPi3] QNX reported error — resetting busy flag" << std::endl;
            busy.store(false);
        }
        else if(status == "flashing"){
            std::cout << "[RPi3] ota.sh started — monitor: tail -f /var/log/ota-update.log" << std::endl;
        }
        else if(status == "done"){
            std::cout << "[RPi3] OTA complete! Resetting state." << std::endl;
            busy.store(false);
        }
    }
};

// Thread function dedicated to monitoring the script log file (and push the status to QNX)
void logMonitorThread(std::shared_ptr<OtaStubImpl> stub) {
    std::ifstream logFile;
    
    while(running){
        // Wait until the script creates/opens the file
        if(!logFile.is_open()){
            logFile.open(LOG_PATH);
            if(!logFile.is_open()){
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
            std::cout << "[RPi3 Monitor] Log file detected. Scanning..." << std::endl;
        }

        std::string line;
        while(std::getline(logFile, line)){
            // Check if the script logged the error string
            if(line.find("ERROR:") != std::string::npos){
                std::cout << "[RPi3 Monitor] Script error matched: " << line << std::endl;
                stub->fireOtaExecutionStatusEvent("failed", line);
                busy.store(false);
            }
            // Check if the script logged your final success string
            else if(line.find("=== OTA Complete") != std::string::npos){
                std::cout << "[RPi3 Monitor] Success milestone reached: " << line << std::endl;
                stub->fireOtaExecutionStatusEvent("success", "OTA Update Validated and Complete! Rebooting now.");
                // Give someip processing headroom to flush out the network buffer before thread exit
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                busy.store(false);
            }
        }

        // Clear EOF flags if log finishes writing temporarily but script is still processing
        logFile.clear();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

int main(){
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto runtime = CommonAPI::Runtime::get();
    auto stub = std::make_shared<OtaStubImpl>();

    bool ok = runtime->registerService("local", "commonapi.Ota", stub, "yoctoService");
    if(!ok){
        std::cerr << "[RPi3] Failed to register OTA service" << std::endl;
        return 1;
    }

    std::cout << "[RPi3] Server started. Waiting for QNX OTA client..." << std::endl;

    // Start background thread to monitor the script log file for status updates and push them to QNX via events
    std::thread monitor(logMonitorThread, stub);
    monitor.detach();   // you're on your own now, little buddy ><

    while(running){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "[RPi3] Shutting down" << std::endl;
    return 0;
}