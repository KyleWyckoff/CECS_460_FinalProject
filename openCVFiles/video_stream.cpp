#include <opencv2/opencv.hpp>
using namespace cv;

int main() {
    VideoCapture cap(0);  // /dev/video0 (USB camera)
    if (!cap.isOpened()) {
        std::cerr << "Failed to open camera." << std::endl;
        return -1;
    }

    cap.set(CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(CAP_PROP_FRAME_HEIGHT, 720);

    VideoWriter writer;
    std::string pipeline = "appsrc ! videoconvert ! jpegenc ! rtpjpegpay ! udpsink host=192.168.1.190 port=5000";

    writer.open(pipeline, CAP_GSTREAMER, 0, 30, Size(1280, 720), true);
    if (!writer.isOpened()) {
        std::cerr << "Failed to open GStreamer pipeline." << std::endl;
        return -1;
    }

    Mat frame, hsv;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        // Saturation filter (convert to HSV, boost saturation, back to BGR)
        cvtColor(frame, hsv, COLOR_BGR2HSV);
        for (int y = 0; y < hsv.rows; y++) {
            for (int x = 0; x < hsv.cols; x++) {
                hsv.at<Vec3b>(y,x)[1] = std::min(255, hsv.at<Vec3b>(y,x)[1] * 2);  // Boost saturation
            }
        }
        cvtColor(hsv, frame, COLOR_HSV2BGR);

        writer.write(frame);
    }

    cap.release();
    writer.release();
    return 0;
}