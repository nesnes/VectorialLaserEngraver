#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "LaserPrinter.hpp"
#include "SVGParser.hpp"

void printSVGFile(LaserPrinter &printer, std::string filePath);
void printSquareInCircle(LaserPrinter &printer);
void printImage(LaserPrinter &printer);

int main(int argc, char **argv) {

    bool simulation = false; //Will print in an OpenCV windows instead of using the printer
    LaserPrinter printer("COM5", simulation); //If the simulation is used, no connection will be established
    if(!printer.isConnected()) {
        std::cout << "Laser printer not found" << std::endl;
        exit(1);
    }

    std::string svgFilePath = "../github.svg";
    if (argc > 1) svgFilePath = argv[1];

    //Select a printing sample:
    printSVGFile(printer, svgFilePath);
    //printSquareInCircle(printer);
    //printImage(printer);

    std::cout << "Type a character to close: " << std::endl;
    char wait;
    std::cin >> wait;
    return 0;
}

void printSVGFile(LaserPrinter &printer, std::string filePath) {
    std::cout << "Reading SVG file" << std::endl;
    int width, height;
    std::vector<LaserPrinterSegment> svgSegments = SVGParser::getSegments(filePath, width, height);

    std::cout << "setPrintOrigin at 0,0" << std::endl;
    printer.setPrintOrigin(0, 0);

    std::cout << "startAreaPreview for 5 seconds" << std::endl;
    printer.startAreaPreview(width, height);
    Sleep(5000);

    std::cout << "stopAreaPreview" << std::endl;
    printer.stopAreaPreview();

    std::cout << "setLaserPower to 1.0 (100%)" << std::endl;
    printer.setLaserPower(1.f);

    std::cout << "setEngravingDepth to 0.6 (60%)" << std::endl;
    printer.setEngravingDepth(0.6f);

    std::cout << "start printShape" << std::endl;
    if (printer.printShape(svgSegments, width, height, true) == -2) {
        std::cout << "The shape is out of the printing area. The maximium SVG file should be 50mmx50mm." << std::endl;
    }
}

void getCircleCoordinates(int angle, int radius, int &x, int &y) {
    x = radius + cos(((float)angle*3.14156) / 180.0) * radius;
    y = radius + sin(((float)angle*3.14156) / 180.0) * radius;
}

void printSquareInCircle(LaserPrinter &printer) {
    std::vector<LaserPrinterSegment> shapes;
    int radius = 100;

    //Create SQUARE
    int squarePoint1 = radius * 0.3;
    int squarePoint2 = radius * 1.7;
    shapes.push_back(LaserPrinterSegment(squarePoint1, squarePoint1, squarePoint2, squarePoint1, 127));
    shapes.push_back(LaserPrinterSegment(squarePoint2, squarePoint1, squarePoint2, squarePoint2, 127));
    shapes.push_back(LaserPrinterSegment(squarePoint2, squarePoint2, squarePoint1, squarePoint2, 127));
    shapes.push_back(LaserPrinterSegment(squarePoint1, squarePoint2, squarePoint1, squarePoint1, 127));

    //Create CIRCLE
    int step = 360 / 64;
    for (int a = 0; a < 360; a += step) {
        int x1, y1, x2, y2;
        getCircleCoordinates(a, radius, x1, y1);
        getCircleCoordinates(a + step, radius, x2, y2);
        LaserPrinterSegment segment(x1, y1, x2, y2, 255);
        shapes.push_back(segment);
    }

    std::cout << "setPrintOrigin at 0,0" << std::endl;
    printer.setPrintOrigin(0, 0);

    std::cout << "startAreaPreview for 5 seconds" << std::endl;
    printer.startAreaPreview(radius * 2, radius * 2);
    Sleep(5000);

    std::cout << "stopAreaPreview" << std::endl;
    printer.stopAreaPreview();

    std::cout << "setLaserPower to 1.0 (100%)" << std::endl;
    printer.setLaserPower(1.f);

    std::cout << "setEngravingDepth to 0.6 (60%)" << std::endl;
    printer.setEngravingDepth(0.6f);

    std::cout << "start printShape" << std::endl;
    if (printer.printShape(shapes, radius * 2, radius * 2, true) == -2) {
        std::cout << "The shape is out of the printing area. The maximium SVG file should be 50mmx50mm." << std::endl;
    }
}

void printImage(LaserPrinter &printer) {
    int width = 512;
    int height = 512;

    //Create dummy test image with vertical lines
    uint8_t* img = (uint8_t*)malloc(width * height * sizeof(uint8_t));
    memset(img, 0, width * height);
    for (int x = 0; x < width; x+=50) {
        for (int y = 0; y < height; y ++) {
            img[512 * y + x] = 255;
        }
    }

    std::cout << "setPrintOrigin at 0,0" << std::endl;
    printer.setPrintOrigin(0, 0);

    std::cout << "startAreaPreview for 5 seconds" << std::endl;
    printer.startAreaPreview(width, height);
    Sleep(5000);

    std::cout << "stopAreaPreview" << std::endl;
    printer.stopAreaPreview();

    std::cout << "setLaserPower to 1.0 (100%)" << std::endl;
    printer.setLaserPower(1.f);

    std::cout << "setEngravingDepth to 0.6 (60%)" << std::endl;
    printer.setEngravingDepth(0.6f);

    std::cout << "start printImage" << std::endl;
    if (printer.printImage(img, width, height, true) == -2) {
        std::cout << "The image is out of the printing area. The maximium size should be 1024*1024." << std::endl;
    }
}
