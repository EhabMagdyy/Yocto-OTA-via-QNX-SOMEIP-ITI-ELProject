#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <CommonAPI/CommonAPI.hpp>
#include "v1/commonapi/HelloProxy.hpp"

using namespace v1::commonapi;

static std::atomic<bool> running(true);

static void signalHandler(int) {
    running = false;
}

int main() {
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Creating the CommonAPI runtime and building the Hello service proxy
    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    std::shared_ptr<HelloProxy<>> myProxy = runtime->buildProxy<HelloProxy>("local", "commonapi.Hello", "HelloClient");

    if(!myProxy){
        std::cerr << "Failed to create proxy!" << std::endl;
        return 1;
    }

    std::cout << "Waiting for service..." << std::endl;

    // Wait up to 50 seconds for the service to become available
    int timeout = 50; 
    while(!myProxy->isAvailable() && timeout > 0){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout--;
    }

    if(!myProxy->isAvailable()){
        std::cerr << "Service not available after timeout!" << std::endl;
        return 1;
    }

    std::cout << "Service available! Starting periodic requests..." << std::endl;

    int counter = 0;
    while(running){
        CommonAPI::CallStatus callStatus;
        std::string returnMessage;
        std::string hiMsg = "Hi Ehab from Yocto!" + std::to_string(counter);

        myProxy->sayHi(hiMsg, callStatus, returnMessage);
        std::cout << "Sending: " << hiMsg << std::endl;

        if(callStatus == CommonAPI::CallStatus::SUCCESS){
            std::cout << "[" << counter << "] Received Replay: '" << returnMessage << "'" << std::endl;
        } 
        else{
            std::cout << "[" << counter << "] Call failed: " << (int)callStatus << std::endl;
        }

        counter++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "Client shutting down." << std::endl;

    return 0;
}
