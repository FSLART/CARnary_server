//
// Created by carlostojal on 10-09-2023.
//

#include "Watcher.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <thread>
#include <carnary/Utils.h>

using namespace carnary::server;

Watcher::Watcher(struct negotiation_t* negotiation) : negotiation(negotiation) {
    this->sockfd = -1;

    // fill the client address with zeros
    memset(&(this->clientAddr), 0, sizeof(struct sockaddr_in));

    // get the daemon instance
    this->instance = carnary::server::CARnaryServer::getInstance();
}

void Watcher::enterDaemonEmergency() {
    // try to execute directly the emergency routine
    instance->emergencyRoutine();

    // send a signal to the daemon
    if(kill(negotiation->daemonPID, SIGTERM) < 0) {
        throw std::runtime_error("Error signaling the daemon to enter emergency mode!");
    }
}

void Watcher::init() {

    // wait for the daemon semaphore
    // when the semaphore is signaled, the watcher can read its consigned port from the pipe
    if(sem_wait(&(this->negotiation->daemonWatcherSemaphore)) < 0) {
        throw std::runtime_error("Error waiting for the semaphore!");
    }

    // read the updated negotiation, containing the port
    if(read(this->negotiation->daemonWatcherPipe[0], this->negotiation, sizeof(struct negotiation_t)) < 0) {
        std::uint8_t response = WATCHER_NACK;
        // reply with a NACK
        if(write(this->negotiation->daemonWatcherPipe[1], &response, sizeof(std::uint8_t)) < 0) {
            throw std::runtime_error("Error replying to the daemon!");
        }
        throw std::runtime_error("Error receiving data from the daemon through the pipe!");
    }

    // open the monitoring port (TCP socket)
    try {
        this->sockfd = carnary::lib::Utils::createClientSocket("0.0.0.0", this->negotiation->monitoringPort, carnary::lib::TCP_SOCKET);
    } catch(std::runtime_error& ex) {
        std::uint8_t response = WATCHER_NACK;
        // reply with NACK
        if(write(this->negotiation->daemonWatcherPipe[1], &response, sizeof(std::uint8_t)) < 0) {
            throw std::runtime_error("Error replying to the daemon!");
        }
        throw ex;
    }

    // reply with an ACK
    std::uint8_t response = WATCHER_ACK;
    if(write(this->negotiation->daemonWatcherPipe[1], &response, sizeof(std::uint8_t)) < 0) {
        throw std::runtime_error("Error replying to the daemon!");
    }

    auto rateCheckingRoutine = [&] () {
        // get the current time point
        std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();

        // calculate the period
        auto period = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->lastHeartbeat);

        // calculate the rate
        float rate = (float) 1 / ((float) period.count() / 1000);

        if((std::uint16_t) rate < this->negotiation->minHeartbeatRate) {
            // enter emergency
            this->enterDaemonEmergency();
        }

        // sleep some milliseconds to reduce resource consumption
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    };

    // wait the client on a new thread, to unblock this one
    auto clientAcceptRoutine = [&] () {
        // accept the client
        socklen_t clientLen = sizeof(struct sockaddr);

        int clientfd;
        if((clientfd = accept(this->sockfd, (struct sockaddr*) &(this->clientAddr), &clientLen)) < 0) {
            throw std::runtime_error("Error accepting the client!");
        }
        std::cout << "Client accepted " << negotiation->serviceName << std::endl;

        // start the thread which monitors heartbeat rate
        std::thread heartbeatRateMonitoringThread(rateCheckingRoutine);
        heartbeatRateMonitoringThread.detach();

        std::uint8_t heartbeat;

        // infinite cycle receiving heartbeat
        do {
            // receive the heartbeat

            if (recv(clientfd, &heartbeat, sizeof(std::uint8_t), MSG_TRUNC | MSG_DONTWAIT) < 0) {
                // if the problem is blocking, we're fine with that
                if(errno != EWOULDBLOCK && errno != EAGAIN)
                    throw std::runtime_error("Error receiving the heartbeat!");
            }

            // register heartbeat time point
            this->lastHeartbeat = std::chrono::high_resolution_clock::now();

        } while(heartbeat != PANIC);

        // if execution gets to this point, this is panic
        this->enterDaemonEmergency();
    };

    // start the thread and detach it
    std::thread clientAcceptThread(clientAcceptRoutine);
    clientAcceptThread.detach();

}
