// file: stream_camera.cpp

#include <opencv2/opencv.hpp>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <host‑IP> <port>\n"
                  << "Example: " << argv[0] << " 192.168.1.190 5000\n";
        return 1;
    }
    std::string host = argv[1];
    int port       = std::stoi(argv[2]);

    // Open the USB camera (V4L2)
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "ERROR: could not open camera\n";
        return 1;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    cap.set(cv::CAP_PROP_FPS, 30);

    // Build a GStreamer pipeline to send JPEG frames over RTP/UDP
    std::ostringstream oss;
    oss << "appsrc ! videoconvert ! "
           "jpegenc ! "
           "rtpjpegpay ! "
           "udpsink host=" << host
        << " port=" << port;

    cv::VideoWriter writer;
    writer.open(oss.str(), 
                cv::CAP_GSTREAMER, 
                0,       // fourcc (unused for GStreamer)
                30.0,    // fps
                cv::Size(1280, 720), 
                true);   // isColor
    if (!writer.isOpened()) {
        std::cerr << "ERROR: could not open GStreamer pipeline:\n  " 
                  << oss.str() << "\n";
        return 1;
    }

    std::cout << "Streaming to udp://" << host << ":" << port 
              << " — press Ctrl‑C to stop\n";

    cv::Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "ERROR: blank frame grabbed\n";
            break;
        }
        // --- optional saturation boost ---
        // cv::Mat hsv;
        // cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
        // hsv.forEach<cv::Vec3b>([](cv::Vec3b &p, const int *pos) {
        //     p[1] = std::min(255, p[1]*2);
        // });
        // cv::cvtColor(hsv, frame, cv::COLOR_HSV2BGR);
        // ----------------------------------

        writer.write(frame);
    }

    return 0;
}