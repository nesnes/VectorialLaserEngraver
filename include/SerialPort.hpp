#ifndef SERIALPORT_H
#define SERIALPORT_H

#define MAX_DATA_LENGTH 65535

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

class SerialPort
{
public:
    inline SerialPort(std::string portName) {
        std::string port_str = "\\\\.\\";
        port_str += portName;
        this->connected = false;

        this->handler = CreateFileA(static_cast<LPCSTR>(port_str.c_str()),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (this->handler == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                printf("ERROR: Handle was not attached. Reason: %s not available\n", portName);
            }
            else
            {
                printf("ERROR!!!");
            }
        }
        else {
            DCB dcbSerialParameters = { 0 };

            if (!GetCommState(this->handler, &dcbSerialParameters)) {
                printf("failed to get current serial parameters");
            }
            else {
                dcbSerialParameters.BaudRate = CBR_115200;
                dcbSerialParameters.ByteSize = 8;
                dcbSerialParameters.StopBits = ONESTOPBIT;
                dcbSerialParameters.Parity = NOPARITY;
                dcbSerialParameters.fDtrControl = DTR_CONTROL_ENABLE;

                if (!SetCommState(handler, &dcbSerialParameters))
                {
                    printf("ALERT: could not set Serial port parameters\n");
                }
                else {
                    this->connected = true;
                    PurgeComm(this->handler, PURGE_RXCLEAR | PURGE_TXCLEAR);
                }
            }
        }
    }

    inline ~SerialPort() {
        if (this->connected) {
            this->connected = false;
            CloseHandle(this->handler);
        }
    }

    inline std::string read() {
        DWORD bytesRead;
        unsigned int toRead = 0;
        ClearCommError(this->handler, &this->errors, &this->status);
        if (this->status.cbInQue > 0) {
            if (this->status.cbInQue > MAX_DATA_LENGTH)
                toRead = MAX_DATA_LENGTH;
            else
                toRead = this->status.cbInQue;
        }
        if (toRead>0 && ReadFile(this->handler, this->inputBuffer, toRead, &bytesRead, NULL))
            return std::string(this->inputBuffer, bytesRead);
        return "";
    }

    inline bool write(std::string message) {
        DWORD bytesSend;
        if (!WriteFile(this->handler, (void*)message.c_str(), message.length(), &bytesSend, 0)) {
            ClearCommError(this->handler, &this->errors, &this->status);
            return false;
        }
        else return true;
    }

    inline bool isConnected() {
        return this->connected;
    }

    inline static std::vector<std::string> getSerialPortsList() {
        std::vector<std::string> serialList;
        TCHAR lpTargetPath[5000];
        DWORD test;
        for (int i = 0; i<255; i++) {
            std::string str = "COM" + std::to_string(i);
            test = QueryDosDevice(str.c_str(), (LPSTR)lpTargetPath, 5000);
            if (test != 0)
                serialList.push_back(str);

            if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                lpTargetPath[10000]; // in case the buffer got filled, increase size of the buffer.
                continue;
            }
        }
        return serialList;
    }

private:
    HANDLE handler;
    bool connected;
    COMSTAT status;
    DWORD errors;
    char inputBuffer[MAX_DATA_LENGTH];
};

#endif // SERIALPORT_H