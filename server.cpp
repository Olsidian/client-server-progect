#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <cstring>
#include <fstream>

#define PORT 8080

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Создание сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Привязка сокета к порту
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Ожидание подключения клиента
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server is listening on port " << PORT << std::endl;

    // Прием соединения
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // Открытие файла для сохранения скриншота
    std::ofstream outFile("received_screenshot.png", std::ios::binary);
    if (!outFile) {
        std::cerr << "Error creating file!" << std::endl;
        close(new_socket);
        close(server_fd);
        return 1;
    }

    // Чтение данных от клиента и запись в файл
    int valread;
    while ((valread = read(new_socket, buffer, sizeof(buffer))) > 0) {
        outFile.write(buffer, valread);
        std::cout << "Received " << valread << " bytes" << std::endl;
    }

    outFile.close();
    std::cout << "Screenshot received and saved as 'received_screenshot.png'" << std::endl;

    // Закрытие сокета
    close(new_socket);
    close(server_fd);

    return 0;
}


