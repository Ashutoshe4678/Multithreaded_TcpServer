
import socket
import threading
import time

HOST = "127.0.0.1"
PORT = 8080

CLIENT_LEVELS = [1, 3, 5, 8, 10]
MESSAGES_PER_CLIENT = 20


def recv_line(client):
    data = b""

    while not data.endswith(b"\n"):
        chunk = client.recv(1024)

        if not chunk:
            raise ConnectionError("Server disconnected")

        data += chunk

    return data.rstrip(b"\r\n")


def simulate_client(client_id, results, lock, barrier):
    successful = 0
    failed = 0

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client:
            client.settimeout(10)
            client.connect((HOST, PORT))

            barrier.wait()

            for i in range(MESSAGES_PER_CLIENT):
                message = f"Client {client_id}: Message {i}\n"
                client.sendall(message.encode())

                response = recv_line(client)

                if response == b"Server is busy. Try again later.":
                    failed += 1
                    break

                if response != b"Message received by server!":
                    failed += 1
                    break

                successful += 1

    except (OSError, ValueError):
        failed += 1

    with lock:
        results.append((successful, failed))


def run_test(num_clients):
    results = []
    lock = threading.Lock()
    barrier = threading.Barrier(num_clients + 1)
    threads = []

    start = time.perf_counter()

    for i in range(num_clients):
        thread = threading.Thread(
            target=simulate_client,
            args=(i + 1, results, lock, barrier)
        )
        threads.append(thread)
        thread.start()

    # Release all clients at approximately the same time
    barrier.wait()

    for thread in threads:
        thread.join()

    duration = time.perf_counter() - start

    successful = sum(r[0] for r in results)
    failed = sum(r[1] for r in results)

    throughput = successful / duration if duration > 0 else 0

    return successful, failed, duration, throughput


print("\n========== CONCURRENCY BENCHMARK ==========")

for clients in CLIENT_LEVELS:
    successful, failed, duration, throughput = run_test(clients)

    print(f"\nClients: {clients}")
    print(f"Successful responses: {successful}")
    print(f"Failed messages/clients: {failed}")
    print(f"Duration: {duration:.4f} seconds")
    print(f"Throughput: {throughput:.2f} messages/sec")

print("\n========== BENCHMARK COMPLETE ==========")
