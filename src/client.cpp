
#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

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

bool recvLine(int socketFd, string& pending, string& line) {
    char buffer[1024];

    while (true) {
        size_t newlinePos = pending.find('\n');

        if (newlinePos != string::npos) {
            line = pending.substr(0, newlinePos);
            pending.erase(0, newlinePos + 1);

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            return true;
        }

        if (pending.size() > 4096) {
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

        if (pending.size() > 4096 &&
            pending.find('\n') == string::npos) {
            return false;
        }
    }
}

int main() {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);

    inet_pton(
        AF_INET,
        "127.0.0.1",
        &serverAddress.sin_addr
    );

    if (connect(
            clientSocket,
            (struct sockaddr*)&serverAddress,
            sizeof(serverAddress)
        ) < 0) {

        perror("Connection failed");
        close(clientSocket);
        return 1;
    }

    cout << "Connected to server!" << endl;

    string message;
    string response;
    string pending;

    while (true) {
        cout << "Enter message: ";

        if (!getline(cin, message)) {
            break;
        }

        if (message == "exit") {
            cout << "Disconnecting from server..." << endl;
            break;
        }

        // Add newline as message delimiter
        message += '\n';

        if (!sendAll(clientSocket, message)) {
            cout << "Failed to send message." << endl;
            break;
        }

        if (!recvLine(clientSocket, pending, response)) {
            cout << "Server disconnected or response incomplete."
                 << endl;
            break;
        }

        cout << "Server: " << response << endl;

        if (response == "Server is busy. Try again later.") {
            cout << "Connection rejected by server." << endl;
            break;
        }
    }

    close(clientSocket);

    cout << "Client connection closed." << endl;

    return 0;
}
