
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <set>
#include <csignal>
#include <cerrno>

using namespace std;

// Configuration
const int PORT = 8080;
const int THREAD_COUNT = 3;
const int MAX_QUEUE_SIZE = 5;

// Task queue: stores client socket and client ID
queue<pair<int, int>> taskQueue;

// Synchronization variables
mutex queueMutex;
mutex coutMutex;
condition_variable condition;

// Active client sockets
set<int> activeSockets;

// Shutdown variables
bool stopping = false;
volatile sig_atomic_t stopRequested = 0;

// Signal handler for Ctrl+C
void handleSignal(int signal) {
    stopRequested = 1;
}


const size_t MAX_MESSAGE_SIZE = 4096;

// Send all bytes, even if send() sends only part of the data
bool sendAll(int socketFd, const string& message) {
    size_t totalSent = 0;

    while (totalSent < message.size()) {
        ssize_t bytesSent = send(
            socketFd,
            message.data() + totalSent,
            message.size() - totalSent,
            MSG_NOSIGNAL
        );

        if (bytesSent <= 0) {
            return false;
        }

        totalSent += bytesSent;
    }

    return true;
}

// Receive one complete newline-terminated message
bool recvLine(int socketFd, string& pending, string& line) {
    char buffer[1024];

    while (true) {
        size_t newlinePos = pending.find('\n');

        if (newlinePos != string::npos) {
            line = pending.substr(0, newlinePos);
            pending.erase(0, newlinePos + 1);

            // Remove optional carriage return
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            return true;
        }

        if (pending.size() > MAX_MESSAGE_SIZE) {
            return false;
        }

        ssize_t bytesReceived = recv(
            socketFd,
            buffer,
            sizeof(buffer),
            0
        );

        if (bytesReceived <= 0) {
            return false;
        }

        pending.append(buffer, bytesReceived);

        if (pending.size() > MAX_MESSAGE_SIZE &&
            pending.find('\n') == string::npos) {
            return false;
        }
    }
}

// Handle communication with one client

void handleClient(int clientSocket, int clientId) {
    string pending;
    string message;

    while (recvLine(clientSocket, pending, message)) {

        {
            lock_guard<mutex> lock(coutMutex);

            cout << "[Client " << clientId
                 << "] Says: " << message << endl;
        }

        string response = "Message received by server!\n";

        if (!sendAll(clientSocket, response)) {
            break;
        }
    }

    {
        lock_guard<mutex> lock(coutMutex);

        cout << "[Client " << clientId
             << "] Disconnected" << endl;
    }

    close(clientSocket);
}


// Worker thread function
void workerFunction(int workerId) {
    while (true) {
        pair<int, int> task;

        {
            unique_lock<mutex> lock(queueMutex);

            condition.wait(lock, [] {
                return !taskQueue.empty() || stopping;
            });

            // Exit if shutdown requested and no tasks remain
            if (stopping && taskQueue.empty()) {
                return;
            }

            // Get the next client task
            task = taskQueue.front();
            taskQueue.pop();

            activeSockets.insert(task.first);
        }

        {
            lock_guard<mutex> lock(coutMutex);

            cout << "[Worker " << workerId
                 << "] Handling Client " << task.second
                 << endl;
        }

        // Handle client communication
        handleClient(task.first, task.second);

        // Remove socket from active socket tracking
        {
            lock_guard<mutex> lock(queueMutex);
            activeSockets.erase(task.first);
        }
    }
}

int main() {

    // Register Ctrl+C signal handler
    struct sigaction sa{};

    sa.sa_handler = handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, nullptr);

    // Create server socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Allow address reuse
    int opt = 1;

    if (setsockopt(
            serverSocket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) < 0) {

        perror("setsockopt failed");
        close(serverSocket);
        return 1;
    }

    // Configure server address
    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Bind socket
    if (bind(
            serverSocket,
            (struct sockaddr*)&serverAddress,
            sizeof(serverAddress)
        ) < 0) {

        perror("Bind failed");
        close(serverSocket);
        return 1;
    }

    // Start listening
    if (listen(serverSocket, 10) < 0) {
        perror("Listen failed");
        close(serverSocket);
        return 1;
    }

    cout << "Server is listening on port "
         << PORT << "..." << endl;

    // Create worker threads
    vector<thread> workers;

    for (int i = 0; i < THREAD_COUNT; i++) {
        workers.emplace_back(workerFunction, i + 1);
    }

    int clientId = 1;

    // Accept incoming clients
    while (!stopRequested) {

        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);

        int clientSocket = accept(
            serverSocket,
            (struct sockaddr*)&clientAddress,
            &clientLength
        );

        if (clientSocket < 0) {

            if (errno == EINTR && stopRequested) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            perror("Accept failed");
            continue;
        }

        int currentClientId = clientId++;

        bool queueFull = false;

        {
            lock_guard<mutex> lock(queueMutex);

            if (taskQueue.size() >= MAX_QUEUE_SIZE) {
                queueFull = true;
            }
            else {
                taskQueue.push({
                    clientSocket,
                    currentClientId
                });
            }
        }

        if (queueFull) {

string response =
    "Server is busy. Try again later.\n";

sendAll(clientSocket, response);

            {
                lock_guard<mutex> lock(coutMutex);

                cout << "[Server] Rejected Client "
                     << currentClientId
                     << " - Queue full" << endl;
            }

            close(clientSocket);
        }
        else {
            condition.notify_one();
        }
    }

    // =================================
    // GRACEFUL SERVER SHUTDOWN
    // =================================

    cout << "\n[Server] Shutting down..." << endl;

    {
        lock_guard<mutex> lock(queueMutex);

        stopping = true;

        // Close clients waiting in the queue
        while (!taskQueue.empty()) {

            int socketFd = taskQueue.front().first;

            taskQueue.pop();

            close(socketFd);
        }

        // Disconnect currently active clients
        for (int socketFd : activeSockets) {
            shutdown(socketFd, SHUT_RDWR);
        }
    }

    // Wake all waiting worker threads
    condition.notify_all();

    // Wait for every worker to finish
    for (auto& worker : workers) {

        if (worker.joinable()) {
            worker.join();
        }
    }

    // Close listening socket
    close(serverSocket);

    cout << "[Server] Shutdown complete." << endl;

    return 0;
}
