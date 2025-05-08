#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#define DATA_LENGTH 1000

// تابع برای تنظیمات پورت سریال
HANDLE init_serial(char *port_name) {
    HANDLE hSerial;
    hSerial = CreateFile(port_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            printf("Serial port does not exist.\n");
        }
        printf("Error while initializing the serial port.\n");
        return NULL;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        printf("Error getting state.\n");
        return NULL;
    }
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        printf("Error setting state.\n");
        return NULL;
    }
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        printf("Error setting timeouts.\n");
        return NULL;
    }
    return hSerial;
}

int main(int argc, char* argv[]) {
    HANDLE hSerial = init_serial("\\\\.\\COM3");
    if (hSerial == NULL) {
        return 1;
    }

    char buf[256];
    DWORD bytesRead;
    int data[DATA_LENGTH] = {0};
    int index = 0;

    // تنظیمات SDL برای ترسیم نمودار
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL Plot", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    while (1) {
        if (ReadFile(hSerial, buf, sizeof(buf) - 1, &bytesRead, NULL)) {
            buf[bytesRead] = 0;
            char* token = strtok(buf, ":");
            if (token != NULL) {
                token = strtok(NULL, ":");
                if (token != NULL) {
                    int value = atoi(token);
                    data[index] = value;
                    index = (index + 1) % DATA_LENGTH;

                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                    SDL_RenderClear(renderer);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

                    for (int i = 0; i < DATA_LENGTH - 1; i++) {
                        SDL_RenderDrawLine(renderer, i, 600 - (data[i] * 600 / 4096), i + 1, 600 - (data[i + 1] * 600 / 4096));
                    }

                    SDL_RenderPresent(renderer);
                }
            }
        }
    }

    CloseHandle(hSerial);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
