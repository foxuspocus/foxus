/*************************************************************************/
/* Copyright (c) 2022 Nolan Consulting Limited.                          */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef GD_EIFFELCAM_HPP
#define GD_EIFFELCAM_HPP
// #define CV_NEON
// #define CV_NEON 1
// #define ENABLE_NEON

#include <Godot.hpp>
#include <Node.hpp>
#include <Image.hpp>
#include <ImageTexture.hpp>
// #include "core/os/os.h"
#include <ProjectSettings.hpp>
#include <TextureArray.hpp>

#include <libusb-1.0/libusb.h>
#include <libuvc/libuvc.h>
#include <opencv2/core.hpp>
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/calib3d.hpp"
#include <iostream>
#include <turbojpeg.h>
#include <stdexcept>

#include <jpeglib.h>
#include <jerror.h>             /* get library error codes too */

#include "godot_texture_components.hpp"

#define WIDTH 1280
#define HEIGHT 960
#define FRAME_WIDTH (WIDTH * 2)

static void uvcCallback(uvc_frame_t *frame, void *ptr);

namespace godot {

class GDEiffelCam;

struct ImageProcessor {
    PoolByteArray rgb_data;
    PoolByteArray rgb_decoded;
    PoolByteArray yuv_data;

    JDIMENSION crop_x, crop_y, crop_width, crop_height;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    unsigned long decodedImageSize = 2 * WIDTH * HEIGHT * 3;

    std::string tag;

    int colorspace = 0;
    int remap_mode = 0;

    enum COLORSPACE {
        COLORSPACE_YUV,
        COLORSPACE_RGB
    };

    enum REMAP_MODE {
        GPU_REMAP,
        CPU_REMAP,
        NO_REMAP
    };

    void create_decompressor ();

    ImageProcessor(JDIMENSION p_crop_x, JDIMENSION p_crop_y, JDIMENSION p_crop_width, JDIMENSION p_crop_height);

    ~ImageProcessor();

    const unsigned char* inbuffer;
    unsigned long insize;
    cv::Mat* mapX;
    cv::Mat* mapY;

    GodotTextureComponents* gtc;

    tjhandle jtd;

    GDEiffelCam* eiffelcam;

    void init(Node* cam);

    bool decode_yuv(PoolByteArray& bytes);

    bool decode_rgb (PoolByteArray& decoded);

    void run();

    Error process(const unsigned char* inbuffer, unsigned long insize, cv::Mat& mapX, cv::Mat& mapY, GodotTextureComponents* gtc, int colorspace, int remap_mode);
};

class GDEiffelCam : public Node {
    GODOT_CLASS(GDEiffelCam, Node)
private:
    std::unique_ptr<ImageProcessor> image_processor = std::make_unique<ImageProcessor>(0, 0, 2 * WIDTH, HEIGHT);

    int colorspace = 0;
    int remap_mode = 0;

    uint32_t vid,pid; // Eiffel Camera vid/pid
    uint32_t streamWidth,streamHeight,streamFps; //dimensions we will negotiate for our stream
    uint32_t frame_array_size;

    // LibUSB / LibUVC handles and control stream
    uvc_context_t* ctx;
    uvc_device_t* dev;
    uvc_device_handle_t* devh;
    uvc_stream_ctrl_t ctrl;
    uvc_stream_handle_t* streamh;
    uvc_frame_t* frame;

    bool isAndroid;
    bool cameraRunning = false;
    bool cameraAttached = false;

    float time_elapsed = 0.0;

    void send_camera_properties();

    void connect_to_camera();
    void start_streaming();
    bool connected_to_camera = false;

    Object* eiffelcamera_singleton = nullptr;

    enum CAMERA_CONNECTION_STATUS {
        NOT_CONNECTED,
        ATTACHED,
        CONNECTED
    };

    bool in_calibration_mode = false;

    void calibrate_single_cameras(
        std::vector< std::vector<cv::Point3f> >* p_object_points,
        std::vector< std::vector<cv::Point2f> >* p_left_image_points,
        std::vector< std::vector<cv::Point2f> >* p_right_image_points,
        cv::Mat& p_left_camera_matrix,
        cv::Mat& p_left_camera_distortion_coefficients,
        cv::Mat& p_right_camera_matrix,
        cv::Mat& p_right_camera_distortion_coefficients);
    bool calibrate_stereo_camera(
        std::vector< std::vector<cv::Point3f> >* p_object_points,
        std::vector< std::vector<cv::Point2f> >* p_left_image_points,
        std::vector< std::vector<cv::Point2f> >* p_right_image_points,
        cv::Mat& p_left_camera_matrix,
        cv::Mat& p_left_camera_distortion_coefficients,
        cv::Mat& p_right_camera_matrix,
        cv::Mat& p_right_camera_distortion_coefficients,
        cv::TermCriteria p_termination_criteria);
    bool calibrate_single_cameras_from_files(
        String& p_calibration_images_folder,
        std::vector< std::vector<cv::Point3f> >* p_object_points,
        std::vector< std::vector<cv::Point2f> >* p_left_image_points,
        std::vector< std::vector<cv::Point2f> >* p_right_image_points,
        cv::Mat& p_left_camera_matrix,
        cv::Mat& p_left_camera_distortion_coefficients,
        cv::Mat& p_right_camera_matrix,
        cv::Mat& p_right_camera_distortion_coefficients,
        cv::TermCriteria p_termination_criteria);

    std::mutex recalibration_mutex;
    bool early_stop_recalibration;

public:
    void on_permission_received(int fd);

    static void _register_methods();
    GDEiffelCam();
    ~GDEiffelCam();
    void emit_error(godot::String);
    bool mapsLoaded = false;
    void loadMaps (godot::String mapsYamlPath, float fudgeFactor);
    void unloadMaps ();
    void undistort(cv::Mat img, void *ptr);
    void _init(); // our initializer called by Godot
    void _ready(); // our initializer called by Godot
    void _process(float delta);

    int getCurrentFrameIndex();

    Ref<ImageTexture> getEyeTextureRGB();

    Ref<TextureArray> getEyeRGBFrameArray();

    Ref<ImageTexture> getEyeTextureY();
    Ref<ImageTexture> getEyeTextureU();
    Ref<ImageTexture> getEyeTextureV();

    Ref<TextureArray> getEyeYFrameArray();
    Ref<TextureArray> getEyeUFrameArray();
    Ref<TextureArray> getEyeVFrameArray();

    Ref<ImageTexture> getLeftMapX();
    Ref<ImageTexture> getLeftMapY();
    Ref<ImageTexture> getRightMapX();
    Ref<ImageTexture> getRightMapY();
    Ref<ImageTexture> getMapX();
    Ref<ImageTexture> getMapY();

    void set_remap_mode(int p_remap) {
        remap_mode = p_remap;
    }
    void set_colorspace(int p_colorspace) {
        colorspace = p_colorspace;
    }

    int get_colorspace() {
        return colorspace;
    }
    int get_remap_mode() {
        return remap_mode;
    }

    Array _get_property_list();

    Variant _get(String property);
    bool _set(const String property, const Variant value);

    void reset_camera_setting(const String property);
    Variant get_camera_setting_label(const String property);

    GodotTextureComponents eyeData;

    cv::Mat leftMapX, leftMapY;
    cv::Mat rightMapX, rightMapY;
    cv::Mat mapX, mapY;

    void enter_calibration_mode();
    bool is_in_calibration_mode(){ return in_calibration_mode; }
    void set_image_on_hold(bool value){ eyeData.set_image_on_hold(value); };
    bool is_image_on_hold(){ return eyeData.is_image_on_hold(); }
    bool take_picture();
    int calibration_images_left();
    bool recalibrate_camera();
    bool recalibrate_camera_from_files();
    void exit_calibration_mode();

    void cancel_recalibration();

    bool disparity_test_mode = false;
    bool save_debug_images = true;
    void set_disparity_test_mode(bool on);
    PoolByteArray create_disparity_map(Ref<ImageTexture> p_image);
    Ref<ImageTexture> get_disparity_map();
    void save_disparity_images();

    void accept_calibration_image();
};

} // namespace godot

#endif
