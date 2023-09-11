//
// Created by carlostojal on 20-07-2023.
//

#ifndef CARNARY_SERVER_CARNARYSERVER_H
#define CARNARY_SERVER_CARNARYSERVER_H

#include <iostream>
#include <vector>
#include <memory>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <csignal>
#include "Negotiation.h"

#define NEGOTIATION_PORT 6666

namespace carnary::server {
    /*! \brief This class contains the functionality of the CARnary server. */
    class CARnaryServer {

        private:
            /*! \brief Vector of tracked negotiations. */
            std::vector<std::unique_ptr<carnary::server::Negotiation>> negotiations; 


            /*! \brief Creates the negotiation socket. */
            void setupNegotiationSocket();

            /*! \brief Sets up the SIGTERM signal handler. */
            void setupSignalHandlers();

        protected:
            CARnaryServer();
    
            /*! \brief Singleton instance. */
            static carnary::server::CARnaryServer* instance;


        public:

            /*! \brief Get singleton instance. */
            static carnary::server::CARnaryServer* getInstance();

            /*! \brief Routine to run on system failure (emergency). */
            void emergencyRoutine();
            
            /*! \brief Initialize the daemon. Open the negotiation socket. */
            void init();

            /*! \brief Destroy the server. Close the socket and perform cleanups. */
            void destroy();

            /*! \brief Add a negotiation to begin. */
            void addNegotiation(std::unique_ptr<carnary::server::Negotiation> negotiation);

            /*! \brief SIGTERM signal handler. Enters emergency mode. */
            friend void signalHandler(int signum);

    };
}


#endif //CARNARY_SERVER_CARNARYSERVER_H
