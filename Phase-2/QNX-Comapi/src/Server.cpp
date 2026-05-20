#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <sstream>
#include <CommonAPI/CommonAPI.hpp>
#include "v1/commonapi/HelloStubDefault.hpp"

using namespace v1::commonapi;

static std::atomic<bool> running(true);

static void signalHandler(int) {
    running = false;
}

class HelloStubImpl : public HelloStubDefault {
public:
    void sayHi(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _name, sayHiReply_t replay) override {
        std::stringstream messageStream;
        std::cout << "Received: " << _name << std::endl;
        messageStream << "Hello QNX, Please give me your license ^_^\n" << _name << " ";
        
        replay(messageStream.str());
    }
};

int main() {
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Creating the CommonAPI runtime and registering the Hello service stub implementation
    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    std::shared_ptr<HelloStubImpl> myService = std::make_shared<HelloStubImpl>();

    runtime->registerService("local", "commonapi.Hello", myService, "helloService");

    std::cout << "Hello Service registered on Host!" << std::endl;
    std::cout << "Waiting for clients..." << std::endl;

    while(running){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Server shutting down." << std::endl;
    runtime->unregisterService("local", HelloStubImpl::StubInterface::getInterface(), "commonapi.Hello");
    return 0;
}