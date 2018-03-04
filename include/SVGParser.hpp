#ifndef SVGParser_hpp
#define SVGParser_hpp

#include <stdio.h>
#include <string.h>
#include <math.h>
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h" // Thanks to memononen for his nanosvg library (http://github.com/memononen/nanosvg).


/**
* \brief Static class that converts paths in an SVG file into laser printer segments
*/
class SVGParser {
public:

    /**
    * \brief Open an SVG file and parse its paths to return a vector of segments to be laser printed.
    */
    static std::vector<LaserPrinterSegment> getSegments(std::string filePath, int &width, int &height) {
        std::vector<LaserPrinterSegment> svgSegments;
        //read SVG image
        struct NSVGimage* svgFile;
        svgFile = nsvgParseFromFile(filePath.c_str(), "px", 505);
        if (!svgFile) {
            std::cout << "SVG file not found" << std::endl;
            return svgSegments;
        }
        width = svgFile->width;
        height = svgFile->height;
        for (NSVGshape* shape = svgFile->shapes; shape != NULL; shape = shape->next) {
            uint8_t color = 255;
            if (shape->stroke.type == NSVG_PAINT_COLOR) {
                float r = ((uint8_t*)&shape->stroke.color)[0];
                float g = ((uint8_t*)&shape->stroke.color)[1];
                float b = ((uint8_t*)&shape->stroke.color)[2];
                float a = ((uint8_t*)&shape->stroke.color)[3];
                color = 255 - ((r + g + b) / (255.f * 3.f))*a;
            }
            for (NSVGpath* path = shape->paths; path != NULL; path = path->next) {
                std::vector<LaserPrinterSegment> pathSegments = cubicBezierToSegments(path);
                for (int s = 0; s < pathSegments.size(); s++) {
                    pathSegments.at(s).duration = color;
                    svgSegments.push_back(pathSegments.at(s));
                }
            }
        }
        nsvgDelete(svgFile);
        return svgSegments;
    }

private:

    SVGParser() {}

    static float getPt(float n1, float n2, float perc) {
        return n1 + ((n2 - n1) * perc);
    }

    /**
    * \brief compute the xy position of a point on a cubic bezier curve at a given [position] on the curve.
    * \param position: the position on the curve between 0(start) and 1(end).
    */
    static void getCubicBezierPoint(float &x, float &y, float position, float x1, float y1, float cpx1, float cpy1, float cpx2, float cpy2, float x2, float y2) {
        // level 1
        float xa = getPt(x1, cpx1, position);
        float ya = getPt(y1, cpy1, position);
        float xb = getPt(cpx1, cpx2, position);
        float yb = getPt(cpy1, cpy2, position);
        float xc = getPt(cpx2, x2, position);
        float yc = getPt(cpy2, y2, position);
        // level 2
        float xm = getPt(xa, xb, position);
        float ym = getPt(ya, yb, position);
        float xn = getPt(xb, xc, position);
        float yn = getPt(yb, yc, position);
        // level 3
        x = getPt(xm, xn, position);
        y = getPt(ym, yn, position);
    }

    /**
    * \brief Rasterize a path into a list of segments.
    */
    static std::vector<LaserPrinterSegment> cubicBezierToSegments(NSVGpath* path) {
        std::vector<LaserPrinterSegment> segmentList;
        for (int p = 0; p < path->npts - 1; p += 3) {
            float* pts = &path->pts[p * 2];
            float x1 = pts[0];
            float y1 = pts[1];
            float cpx1 = pts[2];
            float cpy1 = pts[3];
            float cpx2 = pts[4];
            float cpy2 = pts[5];
            float x2 = pts[6];
            float y2 = pts[7];
            float distanceX = x2 - x1;
            float distanceY = y2 - y1;
            float distance = std::sqrtf(std::powf(distanceX, 2.f) + std::powf(distanceY, 2.f));
            if (distance <= 3) {
                segmentList.push_back(LaserPrinterSegment(x1, y1, x2, y2, 255));
            }
            else {
                float lastX = x1;
                float lastY = y1;
                for (double i = 0; i <= 1; i += 0.01) {
                    float newX = 0, newY = 0;
                    getCubicBezierPoint(newX, newY, i, x1, y1, cpx1, cpy1, cpx2, cpy2, x2, y2);
                    if (fabs(newX - lastX) >= 1 || fabs(newY - lastY) >= 1) {
                        segmentList.push_back(LaserPrinterSegment(lastX, lastY, newX, newY, 255));
                        lastX = newX;
                        lastY = newY;
                    }
                }
                segmentList.push_back(LaserPrinterSegment(lastX, lastY, x2, y2, 255));
            }
        }
        return segmentList;
    }
};
#endif //SVGParser_hpp