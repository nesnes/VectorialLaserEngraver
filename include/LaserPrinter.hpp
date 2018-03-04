#ifndef LaserPrinter_hpp
#define LaserPrinter_hpp

#include <string>
#include <algorithm>
#include <thread>
#include <chrono>

#include "SerialPort.hpp"

#ifdef WITH_OPENCV
    #include "opencv2/opencv.hpp"
#endif

#define LASER_PRINTER_RESOLUTION_WIDTH 1024
#define LASER_PRINTER_RESOLUTION_HEIGHT 1024
#define LASER_PRINTER_MOVE_BUFFER_LENGHT 256


/**
* Print packet: {(A)0x00, (B)0x00, (C)0x00, (D)0x00}
*   A: 8 low signicative bits for the X position
*   B:
*    - 4 first bits: high signicative bits for the X position
*    - 4 last  bits: high signicative bits for the Y position
*   C: 8 low signicative bits for the Y position
*   D: laser burn duration at the given position
*   Print packets should be sent in batches of 256 packets.
*   If less than 256 packets need to be sent, fill with packets filled with 0x00.
*
*  x: between 0 and 1023
*  y: between 0 and 1023
*  duration: between 0 and 255
*/
struct LaserPrinterMove {
    unsigned int x;
    unsigned int y;
    uint8_t duration;

    LaserPrinterMove() {
        x = 0;
        y = 0;
        duration = 0;
    }

    LaserPrinterMove(unsigned int x, unsigned y, uint8_t duration) {
        this->x = x;
        this->y = y;
        this->duration = duration;
    }

    void fromCommand(uint8_t* command) {
        x = command[0] + 16 * (command[1] & 0xF0);
        y = command[2] + 255 * (command[1] & 0x0F);
        duration = command[3];
    }

    void toCommand(uint8_t* command) {
        command[0] = x & 0x0FF;
        command[1] = (x & 0xF00) / 16 + (y & 0xF00) / 255;
        command[2] = y & 0x0FF;
        command[3] = duration;
    }
};

struct LaserPrinterSegment {
    LaserPrinterSegment() {}
    LaserPrinterSegment(unsigned int _startX
        , unsigned int _startY
        , unsigned int _endX
        , unsigned int _endY
        , uint8_t _duration)
    {
        startX = _startX;
        startY = _startY;
        endX = _endX;
        endY = _endY;
        duration = _duration;
    }
    unsigned int startX = 0;
    unsigned int startY = 0;
    unsigned int endX = 0;
    unsigned int endY = 0;
    uint8_t duration;

    void reverse() {
        unsigned int _startX = startX;
        unsigned int _startY = startY;
        startX = endX;
        startY = endY;
        endX = _startX;
        endY = _startY;
    }

    std::vector<LaserPrinterMove> getInterpolation() {
        std::vector<LaserPrinterMove> out;
        float distanceX = (float)endX - (float)startX;
        float distanceY = (float)endY - (float)startY;
        float distance = std::sqrtf(std::powf(distanceX, 2.f) + std::powf(distanceY, 2.f));
        out.push_back(LaserPrinterMove(startX, startY, duration));
        if (distance > 1) {
            for (float i = 1; i < distance; i+=1) {
                float step = i / distance;
                out.push_back(LaserPrinterMove(lerp(startX, endX, step), lerp(startY, endY, step), duration));
            }
        }
        out.push_back(LaserPrinterMove(endX, endY, duration));
        return out;
    }
private:
    float lerp(float a, float b, float f)  {
        return a + f * (b - a);
    }
};

class LaserPrinter {
public:
    LaserPrinter(std::string serialPort, bool simulating=false)
        : m_serialPort("")
        , m_serial(NULL)
        , m_connected(false)
        , m_printOriginX(0)
        , m_printOriginY(0)
        , m_printing(false)
        , m_simulating(simulating)
    {
        if (serialPort == "auto") {
            autoConnect();
        }
        else if (serialPort != "") {
            connect(serialPort);
        }
        setSimulation(m_simulating);
    }

    ~LaserPrinter() {
        if (m_serial != NULL) {
            if (m_connected) {
                m_serial->write("$42");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            delete m_serial;
            m_serial = NULL;
        }
    }

    void setSimulation(bool simulate) {
        m_simulating = simulate;
#ifdef WITH_OPENCV
        if(m_simulating)
            m_previewImage = cv::Mat::zeros(cv::Size(LASER_PRINTER_RESOLUTION_WIDTH, LASER_PRINTER_RESOLUTION_HEIGHT), CV_8UC1);
#endif
    }

    bool connect(std::string serialPort) {
        if (m_simulating) {
            m_serialPort = "simulating";
            m_connected = true;
            return m_connected;
        }
        m_serial = new SerialPort(serialPort);
        m_serial->write("$40");//Home position
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        std::string msg = m_serial->read();
        if (msg.find("connect") == std::string::npos) {
            delete m_serial;
            m_serial = NULL;
            m_connected = false;
        }
        else {
            m_serialPort = serialPort;
            m_connected = true;
        }
        return m_connected;
    }

    bool autoConnect() {
        if (m_simulating) {
            m_serialPort = "simulating";
            m_connected = true;
            return m_connected;
        }
        std::vector<std::string> serialList = SerialPort::getSerialPortsList();
        for (int i = 1; i < serialList.size(); i++) {
            if (connect(serialList.at(i)))
                return m_connected;
        }
    }

    bool isConnected() {
        return m_connected;
    }

    void setPrintOrigin(unsigned int x, unsigned int y) {
        m_printOriginX = (std::min)(x, static_cast<unsigned int>(LASER_PRINTER_RESOLUTION_WIDTH));
        m_printOriginY = (std::min)(y, static_cast<unsigned int>(LASER_PRINTER_RESOLUTION_HEIGHT));
    }

    void resetOrigin() {
        if (!m_connected || m_printing || m_simulating)
            return;
        m_serial->write("$42");
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        m_serial->read();
    }

    void startAreaPreview(int width, int height) {
        if (!m_connected || m_printing || m_simulating)
            return;
        m_serial->write("$20 P" + std::to_string(m_printOriginX) + " " + std::to_string(m_printOriginY) + " " + std::to_string(width) + " " + std::to_string(height));
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        m_serial->read();
    }

    void stopAreaPreview() {
        if (!m_connected || m_printing || m_simulating)
            return;
        m_serial->write("$25 P" + std::to_string(m_printOriginX) + " " + std::to_string(m_printOriginY));
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        m_serial->read();
    }

    int printImage(uint8_t* image, int width, int height, bool enableFan) {
        if (!m_connected || m_printing)
            return -1;
        if (width + m_printOriginX > LASER_PRINTER_RESOLUTION_WIDTH || height+ m_printOriginY > LASER_PRINTER_RESOLUTION_HEIGHT)
            return -2;
        m_printing = true;
        if (!m_simulating) {
            if (enableFan) {
                m_serial->write("$10 P1000");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            else {
                m_serial->write("$10 P0");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            //Send print order
            m_serial->write("$30 P" + std::to_string(m_printOriginX) + " " + std::to_string(m_printOriginY) + (enableFan ? " P2" : " P0"));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            m_serial->read();
        }

        //Send print packets
        LaserPrinterMove printPacket;
        uint8_t printBuffer[LASER_PRINTER_MOVE_BUFFER_LENGHT * 4];
        int bufferIndex = 0;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int xPos = x;
                int index = y*width + x;
                if (y % 2 == 0) {
                    index = ((y + 1)*width - 1) - x;
                    xPos = width - x -1;
                }
                if (image[index] != 0) {
                    printPacket.x = xPos;
                    printPacket.y = y;
                    printPacket.duration = image[index];
                    printPacket.toCommand(&printBuffer[bufferIndex]);
                    bufferIndex += 4;
                    if (bufferIndex >= LASER_PRINTER_MOVE_BUFFER_LENGHT * 4) {
                        sendPrintBuffer(printBuffer);
                        bufferIndex = 0;
                    }
                }
            }
        }
        printPacket.x = 0;
        printPacket.y = 0;
        printPacket.duration = 0;
        //buffer not full at print end
        while(bufferIndex > 0 && bufferIndex < LASER_PRINTER_MOVE_BUFFER_LENGHT * 4) {
            printPacket.toCommand(&printBuffer[bufferIndex]);
            bufferIndex += 4;
        }
        if (bufferIndex > 0) {
            sendPrintBuffer(printBuffer);
        }
        if (!m_simulating) {
            m_serial->write("$33");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            m_serial->read();
        }
        m_printing = false;
    }

    /*
    * \param power: laser power between 0 and 1
    */
    void setLaserPower(float power) {
        if (m_simulating)
            return;
        char power_str[10];
        sprintf(power_str, "%.3f", power);
        m_serial->write("$8 P"+ std::string(power_str));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    /*
    * \param power: laser power between 0 and 1
    */
    void setEngravingDepth(float depth) {
        if (m_simulating)
            return;
        char depth_str[10];
        sprintf(depth_str, "%.3f", depth);
        m_serial->write("$9 P" + std::string(depth_str));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    int printShape(std::vector<LaserPrinterSegment> &segments, int width, int height, bool enableFan) {
        if (!m_connected || m_printing)
            return -1;
        if (width + m_printOriginX > LASER_PRINTER_RESOLUTION_WIDTH || height + m_printOriginY > LASER_PRINTER_RESOLUTION_HEIGHT)
            return -2;
        m_printing = true;
        reorderSegments(segments);
        if (!m_simulating) {
            if (enableFan) {
                m_serial->write("$10 P1000");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            else {
                m_serial->write("$10 P0");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            //Send print order
            m_serial->write("$30 P" + std::to_string(m_printOriginX) + " " + std::to_string(m_printOriginY) + (enableFan ? " P2" : " P0"));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            m_serial->read();
        }

        //Send print packets
        uint8_t printBuffer[LASER_PRINTER_MOVE_BUFFER_LENGHT * 4];
        int bufferIndex = 0;
        for (int i = 0; i < segments.size(); i++) {
            if (segments.at(i).duration != 0) {
                std::vector<LaserPrinterMove> moves = segments.at(i).getInterpolation();
                for (int p = 0; p < moves.size(); p++) {
                    //avoid duplicates
                    if (p == moves.size() - 1 && i + 1 < segments.size()) {
                        if (moves.at(p).x == segments.at(i + 1).startX && moves.at(p).y == segments.at(i + 1).startY)
                            continue;
                    }
                    moves.at(p).toCommand(&printBuffer[bufferIndex]);
                    bufferIndex += 4;
                    if (bufferIndex >= LASER_PRINTER_MOVE_BUFFER_LENGHT * 4) {
                        sendPrintBuffer(printBuffer);
                        bufferIndex = 0;
                    }
                }
            }
        }

        LaserPrinterMove printPacket;
        //buffer not full at print end
        while (bufferIndex > 0 && bufferIndex < LASER_PRINTER_MOVE_BUFFER_LENGHT * 4) {
            printPacket.toCommand(&printBuffer[bufferIndex]);
            bufferIndex += 4;
        }
        if (bufferIndex > 0) {
            sendPrintBuffer(printBuffer);
        }
        if (!m_simulating) {
            m_serial->write("$33");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            while (m_serial->read().find("F22") == std::string::npos) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        m_printing = false;
    }

private:
    void reorderSegments(std::vector<LaserPrinterSegment> &segments) {
        int startIndex = 0;
        int endIndex = 0;
        bool startFound = false;
        for (int i = 0; i < segments.size(); i++) {
            if (segments.at(i).startY <= segments.at(i).endY) {
                if (!startFound) {
                    startIndex = i;
                    endIndex = i;
                    startFound = true;
                }
                else
                    endIndex = i;
            }
            else if (startFound) {
                startFound = false;
                for (int k = 0; k < endIndex - startIndex; k++) {
                    segments.at(startIndex + k).reverse();
                }
                std::reverse(segments.begin() + startIndex, segments.begin() + endIndex);
            }
        }
    }

    void sendPrintBuffer(uint8_t* buffer) {
        if (!m_simulating) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::string msg((char*)buffer, LASER_PRINTER_MOVE_BUFFER_LENGHT * 4);
            m_serial->write(msg);
            while (m_serial->read().find("B1") == std::string::npos) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        else {
#ifdef WITH_OPENCV
            displayPrintBuffer(buffer);
#endif
        }
    }

#ifdef WITH_OPENCV
    void displayPrintBuffer(uint8_t* buffer) {
        LaserPrinterMove move;
        for (int i = 0; i < LASER_PRINTER_MOVE_BUFFER_LENGHT * 4; i+=4) {
            move.fromCommand(buffer + i);
            int x = m_printOriginX + move.x;
            int y = m_printOriginY + move.y;
            if (x >= LASER_PRINTER_RESOLUTION_WIDTH) x = LASER_PRINTER_RESOLUTION_WIDTH-1;
            if (y >= LASER_PRINTER_RESOLUTION_HEIGHT) y = LASER_PRINTER_RESOLUTION_HEIGHT-1;
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            m_previewImage.at<uint8_t>(y, x) = move.duration;
        }
        cv::imshow("preview", m_previewImage);
        cv::waitKey(1);
    }
    cv::Mat m_previewImage;
#endif

    std::string m_serialPort;
    SerialPort* m_serial;
    bool m_connected;
    unsigned int m_printOriginX;
    unsigned int m_printOriginY;
    bool m_printing;
    bool m_simulating;

};

#endif // LaserPrinter_hpp