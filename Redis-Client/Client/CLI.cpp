
#include "CLI.h"

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cerrno>

#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>

#include <readline/readline.h>
#include <readline/history.h>

static bool lineReady = false;
static std::string latestInput;

// Ctrl+D handling
static bool inputEOF = false;

// Readline callback
static void handleLine(char* line) {
    if (!line) {
        inputEOF = true;
        latestInput = "exit";
        lineReady = true;
        return;
    }

    latestInput = std::string(line);

    if (!latestInput.empty()) {
        add_history(latestInput.c_str());
    }

    free(line);
    lineReady = true;
}

// Trim whitespace
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r\f\v");

    if (start == std::string::npos) {
        return "";
    }

    size_t end = s.find_last_not_of(" \t\n\r\f\v");

    return s.substr(start, end - start + 1);
}

// Safely convert command to uppercase
static std::string toUpper(std::string s) {
    std::transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        }
    );

    return s;
}

// Check whether socket has been closed
static bool isSocketClosed(int sockfd) {
    char buffer[1];

    ssize_t bytes = recv(
        sockfd,
        buffer,
        sizeof(buffer),
        MSG_PEEK | MSG_DONTWAIT
    );

    if (bytes == 0) {
        return true;
    }

    if (bytes < 0 && errno != EAGAIN &&
        errno != EWOULDBLOCK && errno != EINTR) {
        return true;
    }

    return false;
}

void printHelp() {
    std::cout
        << "my_redis_cli 1.0.0\n"
        << "Usage:\n"
        << "      With arguments:            ./my_redis_cli -h <host> -p <port>\n"
        << "      Default Host (127.0.0.1):  ./my_redis_cli -p <port>\n"
        << "      Default Port (6379):       ./my_redis_cli -h <host>\n"
        << "      One-shot execution:        ./my_redis_cli <command> [arguments]\n"
        << "\n"
        << "Interactive Mode (REPL):\n"
        << "      ./my_redis_cli\n"
        << "      Type Redis commands directly.\n"
        << "\n"
        << "To get help:\n"
        << "      help  - Display this help message\n"
        << "      quit  - Exit\n"
        << "\n"
        << "Examples:\n"
        << "      ./my_redis_cli PING\n"
        << "      ./my_redis_cli SET mykey \"Hello World\"\n"
        << "      ./my_redis_cli GET mykey\n"
        << "\n"
        << "Preferences:\n"
        << "      :set hints   Enable online hints\n"
        << "      :set nohints Disable online hints\n"
        << std::endl;
}

CLI::CLI(const std::string& host, int port)
    : host(host), port(port), redisClient(host, port) {}


void CLI::run(const std::vector<std::string>& commandArgs) {

    if (!redisClient.connectToServer()) {
        return;
    }

    int sockfd = redisClient.getSocketFD();

    // One-shot command execution
    if (!commandArgs.empty()) {
        executeCommand(commandArgs);
        redisClient.disconnect();
        return;
    }

    std::cout << "Connected to Redis at "
              << host << ":" << port << "\n";

    struct pollfd fds[2];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    bool readlineActive = false;

    lineReady = false;
    inputEOF = false;
    latestInput.clear();

    // Install Readline ONCE
    rl_callback_handler_install(
        (host + ":" + std::to_string(port) + "> ").c_str(),
        handleLine
    );

    readlineActive = true;

    while (true) {

        int ret = poll(fds, 2, -1);

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("(Error) Poll failed");
            break;
        }

        // Redis socket error
        if (fds[1].revents & (POLLERR | POLLNVAL)) {
            std::cout << "\nRedis socket error. Exiting...\n";
            break;
        }

        // Redis socket closed
        if ((fds[1].revents & POLLHUP) ||
            ((fds[1].revents & POLLIN) && isSocketClosed(sockfd))) {

            std::cout << "\nRedis connection lost. Exiting...\n";
            break;
        }

        // Terminal input
        if (fds[0].revents & POLLIN) {
            rl_callback_read_char();
        }

        // Check whether Readline received a complete line
        if (!lineReady) {
            continue;
        }

        std::string line = trim(latestInput);

        lineReady = false;
        latestInput.clear();

        if (inputEOF || line == "quit" || line == "exit") {
            std::cout << "Goodbye.\n";
            break;
        }

        if (line.empty()) {
            continue;
        }

        if (line == "help") {
            printHelp();
            continue;
        }

        // Split command into arguments
        std::vector<std::string> args =
            CommandHandler::splitArgs(line);

        if (args.empty()) {
            continue;
        }

        std::string firstCmd = toUpper(args[0]);

        // Handle subscription separately
        if (firstCmd == "SUBSCRIBE") {

            if (readlineActive) {
                rl_callback_handler_remove();
                readlineActive = false;
            }

            handleSubscription(args);

            // Reset input state after subscription
            lineReady = false;
            inputEOF = false;
            latestInput.clear();

            // Restore normal CLI prompt
            rl_callback_handler_install(
                (host + ":" + std::to_string(port) + "> ").c_str(),
                handleLine
            );

            readlineActive = true;

            continue;
        }

        // Build RESP command
        std::string command =
            CommandHandler::buildRESPcommand(args);

        if (!redisClient.sendCommand(command)) {
            std::cerr << "(Error) Failed to send command.\n";
            break;
        }

        // Read and parse Redis response
        try {
            std::string response =
                ResponseParser::parseResponse(sockfd);

            std::cout << response << "\n";
            std::cout.flush();

        } catch (const std::exception& e) {

            std::cerr << "(Error) Failed to parse response: "
                      << e.what() << "\n";

            std::cerr << "Redis server might have disconnected.\n";
            break;

        } catch (...) {

            std::cerr << "(Error) Unknown error during response parsing.\n";
            break;
        }
    }

    // Cleanup Readline
    if (readlineActive) {
        rl_callback_handler_remove();
    }

    redisClient.disconnect();
}


// Execute a single command
void CLI::executeCommand(const std::vector<std::string>& args) {

    if (args.empty()) {
        return;
    }

    std::string command =
        CommandHandler::buildRESPcommand(args);

    if (!redisClient.sendCommand(command)) {
        std::cerr << "(Error) Failed to send command.\n";
        return;
    }

    try {

        std::string response =
            ResponseParser::parseResponse(redisClient.getSocketFD());

        std::cout << response << "\n";

    } catch (const std::exception& e) {

        std::cerr << "(Error) Failed to parse response: "
                  << e.what() << "\n";

        std::cerr << "Redis server might have disconnected.\n";

    } catch (...) {

        std::cerr << "(Error) Unknown error during response parsing.\n";
    }
}


// Handle Redis Pub/Sub subscription mode
void CLI::handleSubscription(const std::vector<std::string>& args) {

    if (args.size() < 2) {
        std::cerr << "(Error) Usage: SUBSCRIBE <channel> [channel ...]\n";
        return;
    }

    std::string command =
        CommandHandler::buildRESPcommand(args);

    if (!redisClient.sendCommand(command)) {
        std::cerr << "(Error) Failed to send SUBSCRIBE command.\n";
        return;
    }

    std::cout
        << "\n(Subscribed) Listening for messages.\n"
        << "Type 'exit' or 'quit' to leave subscription mode.\n\n";

    int sockfd = redisClient.getSocketFD();

    struct pollfd fds[2];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    lineReady = false;
    inputEOF = false;
    latestInput.clear();

    // Install Readline ONCE
    rl_callback_handler_install("", handleLine);

    bool inSubscription = true;
    bool waitingForUnsubscribe = false;

    while (inSubscription) {

        int ret = poll(fds, 2, -1);

        if (ret < 0) {

            if (errno == EINTR) {
                continue;
            }

            perror("(Error) Poll failed");
            break;
        }

        if (fds[1].revents & (POLLERR | POLLNVAL)) {
            std::cout << "\nRedis socket error. Exiting subscription mode.\n";
            break;
        }

        if ((fds[1].revents & POLLHUP) ||
            ((fds[1].revents & POLLIN) && isSocketClosed(sockfd))) {

            std::cout << "\nRedis connection lost.\n";
            break;
        }

        // Read messages from Redis
        if (fds[1].revents & POLLIN) {

            try {

                std::string message =
                    ResponseParser::parseResponse(sockfd);

                std::cout << "\n" << message << "\n";
                std::cout.flush();

                // Wait until the UNSUBSCRIBE acknowledgement arrives
                if (waitingForUnsubscribe) {

                    std::string lowerMessage = message;

                    std::transform(
                        lowerMessage.begin(),
                        lowerMessage.end(),
                        lowerMessage.begin(),
                        [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        }
                    );

                    if (lowerMessage.find("unsubscribe") !=
                        std::string::npos) {

                        inSubscription = false;
                    }
                }

            } catch (const std::exception& e) {

                std::cerr
                    << "(Error) Failed to parse Pub/Sub message: "
                    << e.what() << "\n";

                break;

            } catch (...) {

                std::cerr
                    << "(Error) Unknown error while reading Pub/Sub message.\n";

                break;
            }
        }

        // Read user input
        if (fds[0].revents & POLLIN) {
            rl_callback_read_char();
        }

        if (!lineReady) {
            continue;
        }

        std::string input = trim(latestInput);

        lineReady = false;
        latestInput.clear();

        if (inputEOF || input == "exit" || input == "quit") {

            if (!waitingForUnsubscribe) {

                std::cout << "\n(Unsubscribing...)\n";

                std::vector<std::string> unsubCmd = {
                    "UNSUBSCRIBE"
                };

                std::string unsubCommand =
                    CommandHandler::buildRESPcommand(unsubCmd);

                if (!redisClient.sendCommand(unsubCommand)) {
                    std::cerr
                        << "(Error) Failed to send UNSUBSCRIBE command.\n";

                    break;
                }

                waitingForUnsubscribe = true;
            }

            continue;
        }

        std::cout
            << "(Info) Type 'exit' or 'quit' to leave subscription mode.\n";
    }

    // Cleanup subscription Readline handler
    rl_callback_handler_remove();

    lineReady = false;
    inputEOF = false;
    latestInput.clear();

    std::cout << "\n(Exited subscription mode)\n";
}
