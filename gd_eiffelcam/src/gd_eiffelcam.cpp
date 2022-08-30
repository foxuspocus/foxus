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

#include "gd_eiffelcam.hpp"

#include <Engine.hpp>
#include <OS.hpp>

#include <unistd.h>

#include <string>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>

#include "opencv2/imgproc.hpp"
#include "opencv2/ximgproc.hpp"

#include "profiler.h"

using namespace godot;

using cclock = std::chrono::system_clock;
using sec = std::chrono::duration<double, std::milli>;

void logDuration(std::string message, const float duration) {
    std::string standardString = message + ": " + std::to_string(duration) + "ms";
    godot::String godotString = godot::String(standardString.c_str());
    Godot::print(godotString);
}

GDEiffelCam* singleton;

extern "C" void GDN_EXPORT godot_gdnative_init(godot_gdnative_init_options *o) {
    PROFILER_INIT();
    Godot::gdnative_init(o);
}

extern "C" void GDN_EXPORT godot_gdnative_terminate(godot_gdnative_terminate_options *o) {
    Godot::gdnative_terminate(o);
}

extern "C" void GDN_EXPORT godot_nativescript_init(void *handle) {
    Godot::nativescript_init(handle);
    register_class<godot::GDEiffelCam>();
}

void jpegErrorExit ( j_common_ptr cinfo )
{
    char jpegLastErrorMsg[JMSG_LENGTH_MAX];
    /* Create the message */
    ( *( cinfo->err->format_message ) ) ( cinfo, jpegLastErrorMsg );

    /* Jump to the setjmp point */
    throw std::runtime_error( jpegLastErrorMsg ); // or your preffered exception ...
}

class ICameraProperty {
public:
    virtual String name() = 0;
    virtual uvc_error_t set(int, bool) = 0;
    virtual int get() = 0;
    virtual int get_max() = 0;
    virtual int get_min() = 0;
    virtual int get_def() = 0;

    virtual Variant get_label() = 0;

    virtual ~ICameraProperty() = 0;

    uvc_device_handle_t* dh;
};

ICameraProperty::~ICameraProperty() {

}
template<typename DT>
class CameraProperty : public ICameraProperty {
public:
    CameraProperty(String name,
                        uvc_device_handle_t* dh,
                        uvc_error_t (*getter)(uvc_device_handle_t*, DT*, uvc_req_code),
                        uvc_error_t (*setter)(uvc_device_handle_t*, DT),
                        String type = "range",
                        Array values = Array(),
                        Array labels = Array()) {
                            this->_name = name;
                            this->dh = dh;
                            get_funcptr = getter;
                            set_funcptr = setter;
                            this->type = type;
                            this->values = values;
                            this->labels = labels;

    }

    ~CameraProperty() {

    }

    String name() {
        return _name;
    }

    uvc_error_t set(int p, bool convert) {
        if (type == "enum" && convert) {
            p = values[p];
        }

        uvc_error_t res = set_funcptr(dh, static_cast<DT>(p));
        return res;
    }

    int get() {
        DT result;
        get_funcptr(dh, &result, uvc_req_code::UVC_GET_CUR);

        int res = static_cast<int>(result);

        if (type == "enum") {
            return values.find(res);
        }

        return res;
    }

    Variant get_label() {
        int index = get();

        if (type == "enum") {
            return labels[index];
        }

        return index;
    }

    int get_max() {
        if (type == "bool") {
            return 1;
        } else if (type == "enum") {
            return labels.size() - 1;
        } else {
            DT result;
            get_funcptr(dh, &result, uvc_req_code::UVC_GET_MAX);
            return static_cast<int>(result);
        }
    }

    int get_min() {
        if (type == "bool" || type == "enum") {
            return 0;
        } else {
            DT result;
            get_funcptr(dh, &result, uvc_req_code::UVC_GET_MIN);
            return static_cast<int>(result);
        }
    }

    int get_def() {
        DT result;
        get_funcptr(dh, &result, uvc_req_code::UVC_GET_DEF);
        return static_cast<int>(result);
    }

private:
    String _name;
    uvc_error_t (*get_funcptr)(uvc_device_handle_t*, DT*, uvc_req_code);
    uvc_error_t (*set_funcptr)(uvc_device_handle_t*, DT);
    String type;

    Array values;
    Array labels;
};

std::map<String, std::unique_ptr<ICameraProperty>> camera_range_properties;

void ImageProcessor::create_decompressor()  {
    jpeg_create_decompress(&cinfo);

    cinfo.dct_method = JDCT_FASTEST;
    cinfo.two_pass_quantize = FALSE;
    cinfo.dither_mode = JDITHER_NONE;
    cinfo.desired_number_of_colors = 1024;
    // cinfo.output_gamma = 1.4;
    cinfo.do_fancy_upsampling = FALSE;
    cinfo.out_color_space = JCS_RGB;

    /* Initialize the JPEG decompression object with default error handling. */
    cinfo.err = jpeg_std_error(&jerr);
    cinfo.err->trace_level = 0;
    jerr.error_exit = jpegErrorExit;
}

ImageProcessor::ImageProcessor(JDIMENSION p_crop_x, JDIMENSION p_crop_y, JDIMENSION p_crop_width, JDIMENSION p_crop_height) {
    crop_x = p_crop_x;
    crop_y = p_crop_y;
    crop_width = p_crop_width;
    crop_height = p_crop_height;

    create_decompressor();
}

ImageProcessor::~ImageProcessor() {
    jpeg_destroy_decompress(&cinfo);
    tjDestroy(jtd);
}

void ImageProcessor::init(Node* cam) {
    eiffelcam = Object::cast_to<GDEiffelCam>(cam);
    TRACE_EVENT("image_processor", "ImageProcessor::init");
    jtd = tjInitDecompress();

    rgb_data.resize(WIDTH * HEIGHT * 6);
    rgb_decoded.resize(WIDTH * HEIGHT * 6);
    yuv_data.resize(HEIGHT * WIDTH * 4);
}

bool ImageProcessor::decode_yuv(PoolByteArray& bytes) {
    TRACE_EVENT("image_processor", "ImageProcessor::decode_yuv");

    PoolByteArray::Write yuv_data_wrt = bytes.write();
    int result = tjDecompressToYUV(jtd, (unsigned char*)inbuffer, insize, yuv_data_wrt.ptr(), TJFLAG_FASTDCT | TJFLAG_NOREALLOC);
    if (result != 0) {
        char* errstr = tjGetErrorStr2(jtd);
        String msg = String("ERROR during JPEG decode: ") + errstr;
        Godot::print(msg);

        return false;
    } else {
        TRACE_EVENT("image_processor", "upload_yuv_to_gpu");

        gtc->update_yuv_frame_array(bytes);

        eiffelcam->emit_signal("frame_index_updated", gtc->get_current_frame_index());

        return true;
    }
}

bool ImageProcessor::decode_rgb (PoolByteArray& decoded) {
    TRACE_EVENT("image_processor", "ImageProcessor::decode_rgb");

    // Wild guess at valid sizes..
    if (insize < 0xFF || insize > 0xFFFFF) {
        std::cout << "Invalid insize\n";
        return false;
    }

    try {
        jpeg_mem_src(&cinfo, inbuffer, insize);

        /* Read file header, set default decompression parameters */
        jpeg_read_header(&cinfo, TRUE);

        /* Start decompressor */
        jpeg_start_decompress(&cinfo);

        JDIMENSION tmp;
        unsigned long decodedOffset = 0;

        /* Check for valid crop dimensions.  We cannot check these values until
        * after jpeg_start_decompress() is called.
        */
        if (crop_x + crop_width > cinfo.output_width ||
            crop_y + crop_height > cinfo.output_height) {
            Godot::print("ERROR: crop dimensions exceed image dimensions {} x {}\n",
                cinfo.output_width, cinfo.output_height);
            return false;
        }
        jpeg_crop_scanline(&cinfo, &crop_x, &crop_width);

        /* Process data */
        if ((tmp = jpeg_skip_scanlines(&cinfo, crop_y)) != crop_y) {
            Godot::print("jpeg_skip_scanlines() returned {} rather than {}\n",
                tmp, crop_y);
            return false;
        }

        // jpeg decode

        {
            PoolByteArray::Write write = decoded.write();

            while (cinfo.output_scanline < crop_y + crop_height) {
                JDIMENSION max_scanlines = (decodedImageSize - decodedOffset) / (2 * WIDTH*3);
                JSAMPLE* row = write.ptr() + cinfo.output_scanline * 3 * WIDTH * 2;
                JDIMENSION num_scanlines = jpeg_read_scanlines(&cinfo, &row, max_scanlines);
                decodedOffset += num_scanlines * WIDTH * 3 * 2;
            }
        }

        {
            if ((tmp =
                jpeg_skip_scanlines(&cinfo,
                                    cinfo.output_height - crop_y - crop_height)) !=
                cinfo.output_height - crop_y - crop_height) {
                Godot::print("jpeg_skip_scanlines() returned {} rather than {}\n",
                    tmp, cinfo.output_height - crop_y - crop_height);
                return false;
            }
        }

        (void)jpeg_finish_decompress(&cinfo);

        return true;
    } catch ( const std::exception & e ) {
        Godot::print("Exception on decode...");

        char pszErr[1024];
        (cinfo.err->format_message)((j_common_ptr)&cinfo, pszErr);

        Godot::print(pszErr);

        // Probably leaks memory like shit when the decompressor dies, plus creates a stall
        // due to flushing all the sweet cached DCT data
        jpeg_destroy_decompress( &cinfo );
        create_decompressor();

        return false;
    }
}

void ImageProcessor::run() {
    TRACE_EVENT("image_processor", "ImageProcessor::run");

    if (colorspace == COLORSPACE::COLORSPACE_YUV) {
        decode_yuv(yuv_data);
    }

    if (colorspace == COLORSPACE::COLORSPACE_RGB) {
        if (decode_rgb(rgb_decoded)) {

            // remap
            if (remap_mode == REMAP_MODE::CPU_REMAP) {
                PoolByteArray::Write decoded_wrt = rgb_decoded.write();
                PoolByteArray::Write data_wrt = rgb_data.write();

                cv::Mat decodedImage { cv::Size(WIDTH * 2, HEIGHT), CV_8UC3, decoded_wrt.ptr() };
                cv::Mat targetFrame { cv::Size(WIDTH * 2, HEIGHT), CV_8UC3, data_wrt.ptr() };

                // try preload the l2 cache
                for (int i = 0 ; i < insize; i += 64) {
                    __builtin_prefetch (inbuffer + i, 0, 1);
                }

                cv::remap(decodedImage, targetFrame, *mapX, *mapY, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
            }

            // upload
            {
                TRACE_EVENT("image_processor", "upload_rgb_to_gpu");
                if (remap_mode == REMAP_MODE::CPU_REMAP) {
                    gtc->update_rgb_frame_array(rgb_data);
                } else {
                    gtc->update_rgb_frame_array(rgb_decoded);
                }
                eiffelcam->emit_signal("frame_index_updated", gtc->get_current_frame_index());
            }
        }
    }
}

Error ImageProcessor::process(const unsigned char* inbuffer, unsigned long insize, cv::Mat& mapX, cv::Mat& mapY, GodotTextureComponents* gtc, int colorspace, int remap_mode) {
    TRACE_EVENT("image_processor", "ImageProcessor::process");

    this->inbuffer = inbuffer;
    this->insize = insize;
    this->mapX = &mapX;
    this->mapY = &mapY;
    this->gtc = gtc;
    this->colorspace = colorspace;
    this->remap_mode = remap_mode;

    run();

    return Error::OK;
}

void GDEiffelCam::_register_methods() {

    register_method("_process", &GDEiffelCam::_process);
    register_method("_ready", &GDEiffelCam::_ready);

    register_method("getCurrentFrameIndex", &GDEiffelCam::getCurrentFrameIndex);

    register_method("getEyeTextureRGB", &GDEiffelCam::getEyeTextureRGB);

    register_method("getEyeRGBFrameArray", &GDEiffelCam::getEyeRGBFrameArray);

    register_method("getEyeTextureY", &GDEiffelCam::getEyeTextureY);
    register_method("getEyeTextureU", &GDEiffelCam::getEyeTextureU);
    register_method("getEyeTextureV", &GDEiffelCam::getEyeTextureV);

    register_method("getEyeYFrameArray", &GDEiffelCam::getEyeYFrameArray);
    register_method("getEyeUFrameArray", &GDEiffelCam::getEyeUFrameArray);
    register_method("getEyeVFrameArray", &GDEiffelCam::getEyeVFrameArray);

    register_method("getLeftMapX", &GDEiffelCam::getLeftMapX);
    register_method("getLeftMapY", &GDEiffelCam::getLeftMapY);

    register_method("getRightMapX", &GDEiffelCam::getRightMapX);
    register_method("getRightMapY", &GDEiffelCam::getRightMapY);

    register_method("getMapX", &GDEiffelCam::getMapX);
    register_method("getMapY", &GDEiffelCam::getMapY);

    register_method("loadMaps", &GDEiffelCam::loadMaps);
    register_method("unloadMaps", &GDEiffelCam::unloadMaps);

    register_method("set_colorspace", &GDEiffelCam::set_colorspace);
    register_method("set_remap_mode", &GDEiffelCam::set_remap_mode);

    register_method("connect_to_camera", &GDEiffelCam::connect_to_camera);

    register_method("_get_property_list", &GDEiffelCam::_get_property_list);
    register_method("_get", &GDEiffelCam::_get);
    register_method("_set", &GDEiffelCam::_set);
    register_method("reset_camera_setting", &GDEiffelCam::reset_camera_setting);
    register_method("get_camera_setting_label", &GDEiffelCam::get_camera_setting_label);

    register_method("on_permission_received", &GDEiffelCam::on_permission_received);
    register_method("enter_calibration_mode", &GDEiffelCam::enter_calibration_mode);
    register_method("is_in_calibration_mode", &GDEiffelCam::is_in_calibration_mode);
    register_method("set_image_on_hold", &GDEiffelCam::set_image_on_hold);
    register_method("is_image_on_hold", &GDEiffelCam::is_image_on_hold);
    register_method("take_picture", &GDEiffelCam::take_picture);
    register_method("calibration_images_left", &GDEiffelCam::calibration_images_left);
    register_method("recalibrate_camera", &GDEiffelCam::recalibrate_camera);
    register_method("recalibrate_camera_from_files", &GDEiffelCam::recalibrate_camera_from_files);
    register_method("exit_calibration_mode", &GDEiffelCam::exit_calibration_mode);
    register_method("set_disparity_test_mode", &GDEiffelCam::set_disparity_test_mode);
    register_method("cancel_recalibration", &GDEiffelCam::cancel_recalibration);
    register_method("get_disparity_map", &GDEiffelCam::get_disparity_map);
    register_method("save_disparity_images", &GDEiffelCam::save_disparity_images);
    register_method("accept_calibration_image", &GDEiffelCam::accept_calibration_image);

    register_signal<GDEiffelCam>((char*)"error");
    register_signal<GDEiffelCam>((char*)"frame_start");
    register_signal<GDEiffelCam>((char*)"frame_end");
    register_signal<GDEiffelCam>((char*)"opened");
    register_signal<GDEiffelCam>((char*)"closed");
    register_signal<GDEiffelCam>((char*)"frame_index_updated", "value", GODOT_VARIANT_TYPE_INT);
    register_signal<GDEiffelCam>("camera_property_range_changed", "property", GODOT_VARIANT_TYPE_STRING, "current", GODOT_VARIANT_TYPE_INT, "min", GODOT_VARIANT_TYPE_INT, "max", GODOT_VARIANT_TYPE_INT);
    register_signal<GDEiffelCam>("camera_status_changed", "status", GODOT_VARIANT_TYPE_INT);
    register_signal<GDEiffelCam>((char*)"ready_for_calibration");
    register_signal<GDEiffelCam>((char*)"frame_diff_changed", "frame_diff", GODOT_VARIANT_TYPE_INT);

    register_signal<GDEiffelCam>((char*)"chessboard_detected", "left", GODOT_VARIANT_TYPE_OBJECT, "right", GODOT_VARIANT_TYPE_OBJECT);
}

Array GDEiffelCam::_get_property_list() {


    Array property_list;

    for (const auto& entry : camera_range_properties) {
        Dictionary dict;
        dict["name"] = entry.second->name();
        dict["type"] = GODOT_VARIANT_TYPE_INT;
        dict["usage"] = GODOT_PROPERTY_USAGE_EDITOR | GODOT_PROPERTY_USAGE_STORAGE;
        dict["hint"] = GODOT_PROPERTY_HINT_RANGE;
        String minimum = std::to_string(entry.second->get_min()).c_str();
        String maximum = std::to_string(entry.second->get_max()).c_str();
        dict["hint_string"] = maximum + "," + minimum + ",1";

        property_list.append(dict);
    }

    return property_list;
}

Variant GDEiffelCam::_get(String property) {


    if (camera_range_properties.find(property) != camera_range_properties.end()) {
        return camera_range_properties[property]->get();
    } else if (property == "frame_diff") {
        return eyeData.get_frame_diff();
    }

    return Variant();

}

bool GDEiffelCam::_set(const String property, const Variant value) {

    if (camera_range_properties.find(property) != camera_range_properties.end()) {
        uvc_error_t res = camera_range_properties[property]->set(value, true);
        return res == uvc_error_t::UVC_SUCCESS;
    } else if (property == "frame_diff") {
        eyeData.set_frame_diff(value);
        emit_signal("frame_diff_changed", eyeData.get_frame_diff());
        return true;
    }

    return false;
}

void GDEiffelCam::reset_camera_setting(const String property) {

    if (camera_range_properties.find(property) != camera_range_properties.end()) {
        int def = camera_range_properties[property]->get_def();
        camera_range_properties[property]->set(def, false);
    } else if (property == "frame_diff") {
        eyeData.set_frame_diff(eyeData.get_default_frame_diff());
        emit_signal("frame_diff_changed", eyeData.get_frame_diff());
    }
}

Variant GDEiffelCam::get_camera_setting_label(const String property) {

    if (camera_range_properties.find(property) != camera_range_properties.end()) {
        return camera_range_properties[property]->get_label();
    } else if (property == "frame_diff") {
        return eyeData.get_frame_diff();
    }

    return Variant();
}

GDEiffelCam::GDEiffelCam() {
}

GDEiffelCam::~GDEiffelCam() {
}

void GDEiffelCam::_init() {

    // USB Vendor ID and Product ID for Eiffel Camera.
    vid=0x32e4;
    pid=0x9750;
    // Stream characteristics we will attempt to use
    streamWidth=2560;
    streamHeight=960;
    streamFps=60;

    frame_array_size = 10;

    singleton = this;

    setenv("JSIMD_FORCENEON", "1", 1);

    cv::setUseOptimized(true);
    Godot::print(cv::getBuildInformation().c_str());

    if (!cv::useOptimized()) {
        Godot::print("MASSIVE PROBLEM: OPENCV IS NOT USING OPTIMISED SIMD CODE");
    }

    if (!cv::checkHardwareSupport(CV_CPU_NEON)) {
        Godot::print("MASSIVE PROBLEM: OPENCV DOES NOT SUPPORT NEON");
    }
}

void GDEiffelCam::emit_error(godot::String msg) {

    Godot::print(msg);
    emit_signal("error", msg);
}

#ifdef __ANDROID__
    #define isAndroid true
#else
    #define isAndroid false
#endif

void GDEiffelCam::connect_to_camera() {

    // If we are running on Android, we must call our Android Plugin that makes
    // the necessary Java API calls to open the device and retrieve the
    // file descriptor that we can wrap with libuvc

    uvc_error_t res;

    if (isAndroid) { // Engine::get_singleton()->has_singleton("EiffelCamera")) {
        Godot::print("Running on android");

        int libusb_FD = -1;

        Godot::print("Calling connectCamera, vid, pid follows:");
        Godot::print(std::to_string(vid).c_str());
        Godot::print(std::to_string(pid).c_str());
        bool b = eiffelcamera_singleton->call("connectCamera",vid,pid);

        Godot::print(b ? "Got camera permissions" : "FAIL: Could not get camera permissions");
    } else {
        //We are running on OSX
        Godot::print("Running on os x");

        res = uvc_init(&ctx, NULL);
        if (res < 0) {
            Godot::print("uvc_init error!\n");
            return;
        }

        res = uvc_find_device(ctx, &dev,vid, pid, NULL); // filter devices: vendor_id, product_id, "serial_num"
        if (res < 0) {
            emit_error("ERROR: cannot find Eiffel Camera device");
            return;
        } else {
            Godot::print("INFO: Eiffel Camera Device found");
            res = uvc_open(dev, &devh);
            if (res < 0) {
              emit_error("ERROR: Cannot open UVC device (Needs sudo?)");
              return;
            } else {
              printf("INFO: Eiffel Camera Device opened\n");

              // Uncomment to list camera formats etc.for debugging
              uvc_print_diag(devh, stderr);

              start_streaming();
            }
        }
    }
}

void GDEiffelCam::on_permission_received(int fd) {

    if (fd < 0 || cameraRunning) {
        return;
    }

    int err = libusb_set_option(NULL, LIBUSB_OPTION_NO_DEVICE_DISCOVERY, NULL);

    uvc_error_t res = uvc_init(&ctx, NULL);
    if (res < 0) {
        emit_error("ERROR: cannot initialise UVC");
        return;
    }

    res = uvc_wrap(fd,ctx,&devh);
    if (res < 0) {
        emit_error("ERROR: cannot find Eiffel Camera device");
        Godot::print(std::to_string(res).c_str());
        return;
    } else {
        Godot::print("INFO: Eiffel Camera Device found");
        dev = uvc_get_device(devh);
    }

    start_streaming();
}

void GDEiffelCam::start_streaming() {

    // Try to negotiate a MJPG stream
    uvc_error_t res = uvc_get_stream_ctrl_format_size(devh, &ctrl, UVC_FRAME_FORMAT_COMPRESSED, streamWidth, streamHeight, streamFps);

    // Uncomment to display stream control for debugging
    uvc_print_stream_ctrl(&ctrl, stderr);

    if (res < 0) {
        emit_error("Cannot get stream size requested");
        return;
    } else {
        res = uvc_stream_open_ctrl(devh, &streamh, &ctrl);
        if (res != UVC_SUCCESS) {
            Godot::print(String("ERROR: Unable to open stream ctrl (") + String(uvc_strerror(res)) + ")");
            return;
        }

        res = uvc_stream_start(streamh, NULL, NULL, 0);
        if (res != UVC_SUCCESS) {
            Godot::print(String("ERROR: Unable to start streaming (") + String(uvc_strerror(res)) + ")");
            return;
        } else {
            uvc_set_ae_mode(devh, 0); // turn off auto exposure
            Godot::print("INFO: setting ae mode to aperture priority");
            cameraRunning = true;
            emit_signal("camera_status_changed", CAMERA_CONNECTION_STATUS::CONNECTED);

            uvc_set_white_balance_temperature_auto(devh, 1);
            // uvc_set_white_balance_temperature(devh, 32000);
        }
    }

    send_camera_properties();
    emit_signal("opened");
}

void GDEiffelCam::_ready() {

    TRACE_EVENT("eiffel_camera", "EiffelCamera::_ready");

    emit_signal("camera_status_changed", CAMERA_CONNECTION_STATUS::NOT_CONNECTED);
    cameraRunning = false;

    // Set up our Godot buffer and Image/Texture wrappers
    eyeData.init(frame_array_size);

    if (Engine::get_singleton()->has_singleton("EiffelCamera")) {
        eiffelcamera_singleton = Engine::get_singleton()->get_singleton("EiffelCamera");
        eiffelcamera_singleton->connect("permission_received", this, "on_permission_received");
    }

    image_processor->init(this);

    if (!isAndroid) {
        connect_to_camera();
        cameraAttached = true;
    }

    emit_signal("frame_diff_changed", eyeData.get_frame_diff());
}

void GDEiffelCam::send_camera_properties() {

    camera_range_properties["brightness"] = std::unique_ptr<ICameraProperty>(new CameraProperty<int16_t>("brightness", devh, &uvc_get_brightness, &uvc_set_brightness));
    camera_range_properties["contrast"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint16_t>("contrast", devh, &uvc_get_contrast, &uvc_set_contrast));
    camera_range_properties["saturation"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint16_t>("saturation", devh, &uvc_get_saturation, &uvc_set_saturation));
    camera_range_properties["hue"] = std::unique_ptr<ICameraProperty>(new CameraProperty<int16_t>("hue", devh, &uvc_get_hue, &uvc_set_hue));
    camera_range_properties["white_balance_temperature_auto"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint8_t>("white_balance_temperature_auto", devh, &uvc_get_white_balance_temperature_auto, &uvc_set_white_balance_temperature_auto, "bool"));
    camera_range_properties["gamma"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint16_t>("gamma", devh, &uvc_get_gamma, &uvc_set_gamma));
    camera_range_properties["gain"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint16_t>("gain", devh, &uvc_get_gain, &uvc_set_gain));
    camera_range_properties["white_balance_temperature"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint16_t>("white_balance_temperature", devh, &uvc_get_white_balance_temperature, &uvc_set_saturation));
    camera_range_properties["sharpness"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint16_t>("sharpness", devh, &uvc_get_sharpness, &uvc_set_sharpness));
    camera_range_properties["backlight_compensation"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint16_t>("backlight_compensation", devh, &uvc_get_backlight_compensation, &uvc_set_backlight_compensation));
    camera_range_properties["exposure_abs"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint32_t>("exposure_abs", devh, &uvc_get_exposure_abs, &uvc_set_exposure_abs));
    camera_range_properties["focus_abs"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint16_t>("focus_abs", devh, &uvc_get_focus_abs, &uvc_set_focus_abs));
    camera_range_properties["focus_auto"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint8_t>("focus_auto", devh, &uvc_get_focus_auto, &uvc_set_focus_auto, "bool"));

    Array power_line_frequency_values, power_line_frequency_names;
    Array ae_mode_values, ae_mode_names;

    {
        power_line_frequency_values.append(0);
        power_line_frequency_values.append(1);
        power_line_frequency_values.append(2);

        power_line_frequency_names.append("Disabled");
        power_line_frequency_names.append("50 Hz");
        power_line_frequency_names.append("60 Hz");
    }

    {
        ae_mode_values.append(1);
        ae_mode_values.append(8);

        ae_mode_names.append("Manual Mode");
        ae_mode_names.append("Aperture Priority Mode");
    }

    camera_range_properties["power_line_frequency"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint8_t>("power_line_frequency", devh, &uvc_get_power_line_frequency, &uvc_set_power_line_frequency, "enum", power_line_frequency_values, power_line_frequency_names));
    camera_range_properties["ae_mode"] = std::unique_ptr<ICameraProperty>(new CameraProperty<uint8_t>("ae_mode", devh, &uvc_get_ae_mode, &uvc_set_ae_mode, "enum", ae_mode_values, ae_mode_names));

    emit_signal("camera_property_range_changed", "frame_diff", 0, 0, std::max(frame_array_size - 1, (uint32_t)0));

    for (const auto& entry : camera_range_properties) {
        emit_signal("camera_property_range_changed",
            entry.second->name(),
            entry.second->get(),
            entry.second->get_min(),
            entry.second->get_max()
        );
    }
}

void GDEiffelCam::_process(float delta) {

    TRACE_EVENT("eiffel_camera", "EiffelCamera::_process", "delta", delta);
    time_elapsed += delta;

    if (isAndroid && !cameraAttached) {

        {
        TRACE_EVENT("eiffel_camera", "isCameraAttached");
        cameraAttached = eiffelcamera_singleton->call("isCameraAttached", vid, pid);
        }

        if (cameraAttached) {
            TRACE_EVENT("eiffel_camera", "camera_attached");
            emit_signal("camera_status_changed", CAMERA_CONNECTION_STATUS::ATTACHED);
            cameraRunning = false;
            connect_to_camera();
        }
    }

    if (cameraRunning && cameraAttached) {
        uvc_error_t ret;

        if (!mapsLoaded) {
            Godot::print("Maps not loaded");
            return;
        }

        {
        TRACE_EVENT("eiffel_camera", "uvc_stream_get_frame");
        ret = uvc_stream_get_frame(streamh, &frame, -1);
        }

        if (ret != UVC_SUCCESS || frame == nullptr) {
            TRACE_EVENT("eiffel_camera", "get_frame_error");
            if (ret == UVC_SUCCESS) {
                Godot::print("ERROR: Unable to get frame");
            } else {
                Godot::print(String("ERROR: ") + String(uvc_strerror(ret)));
            }
            if (isAndroid) {
                cameraAttached = eiffelcamera_singleton->call("isCameraAttached", vid, pid);
                if (!cameraAttached) {
                    TRACE_EVENT("eiffel_camera", "camera_not_attached");
                    emit_signal("camera_status_changed", CAMERA_CONNECTION_STATUS::NOT_CONNECTED);
                }
            }
            return;
        }

        {
        TRACE_EVENT("eiffel_camera", "emit_frame_start");
        emit_signal("frame_start");
        }


        {
            TRACE_EVENT("eiffel_camera", "frame");

            image_processor->process((const unsigned char*) frame->data,
                                    frame->data_bytes,
                                    mapX,
                                    mapY,
                                    &eyeData,
                                    get_colorspace(),
                                    get_remap_mode()
                                );
        }

        if (disparity_test_mode){
            PoolByteArray disparity_map_data = create_disparity_map(eyeData.get_current_rgb_frame());
            eyeData.update_disparity_map(disparity_map_data);
            set_disparity_test_mode(false);
        }

        emit_signal("frame_end");
    }
}

int GDEiffelCam::getCurrentFrameIndex(){
    return eyeData.get_current_frame_index();
}

Ref<ImageTexture> GDEiffelCam::getEyeTextureRGB() {

    return eyeData.get_current_rgb_frame();
}

Ref<TextureArray> GDEiffelCam::getEyeRGBFrameArray(){
    return eyeData.get_rgb_frame_array();
}

Ref<ImageTexture> GDEiffelCam::getEyeTextureY() {

    return eyeData.get_current_y_frame();
}

Ref<ImageTexture> GDEiffelCam::getEyeTextureU() {

    return eyeData.get_current_u_frame();
}

Ref<ImageTexture> GDEiffelCam::getEyeTextureV() {

    return eyeData.get_current_v_frame();
}

Ref<TextureArray> GDEiffelCam::getEyeYFrameArray(){
    return eyeData.get_y_frame_array();
}

Ref<TextureArray> GDEiffelCam::getEyeUFrameArray(){
    return eyeData.get_u_frame_array();
}

Ref<TextureArray> GDEiffelCam::getEyeVFrameArray(){
    return eyeData.get_v_frame_array();
}

Ref<ImageTexture> GDEiffelCam::getLeftMapX() {

    return eyeData.get_left_map_x_texture();
}

Ref<ImageTexture> GDEiffelCam::getLeftMapY() {

    return eyeData.get_left_map_y_texture();
}

Ref<ImageTexture> GDEiffelCam::getRightMapX() {

    return eyeData.get_right_map_x_texture();
}

Ref<ImageTexture> GDEiffelCam::getRightMapY() {

    return eyeData.get_right_map_y_texture();
}

Ref<ImageTexture> GDEiffelCam::getMapX() {
    return eyeData.get_map_x_texture();
}

Ref<ImageTexture> GDEiffelCam::getMapY() {
    return eyeData.get_map_y_texture();
}

cv::Ptr<cv::StereoBM> sbm;

int i;

#define PROCESS_WIDTH 320
#define PROCESS_HEIGHT 240

void print_mat(std::string prefix, cv::Mat& mat) {
    prefix << mat;
    Godot::print(prefix.c_str());
}

void loadMapTexture(Ref<ImageTexture>& tex, cv::Mat& map, int width = WIDTH, int height = HEIGHT) {
    PoolByteArray map_data;
    map_data.resize(width * height * 4);
    {
        PoolByteArray::Write wrt = map_data.write();
        memcpy(wrt.ptr(), map.ptr(), width * height * 4);
    }
    Ref<Image> img = Ref<Image>(Image::_new());
    img->create_from_data(width, height, false, Image::FORMAT_RF, map_data);
    tex = Ref<ImageTexture>(ImageTexture::_new());
    tex->create_from_image(img);
}

void GDEiffelCam::loadMaps (godot::String mapsYamlPath, float fudgeFactor) {

    TRACE_EVENT("eiffel_camera", "EiffelCamera::loadMaps");

    Godot::print("Loading " + mapsYamlPath);

    cv::FileStorage fs;

//const char* username = rawUsername.utf8().get_data();
    auto fpath = ProjectSettings::get_singleton()->globalize_path(mapsYamlPath);

    const char* path = fpath.utf8().get_data();
    if (!fs.open(path, cv::FileStorage::READ)) {
        Godot::print("ERROR: Unable to open fpath");
        return;
    }

    cv::Mat K1, K2, D1, D2, R1, R2, P1, P2;

    // Loads stereo matrix coefficients
    fs["K1"] >> K1;
    fs["K2"] >> K2;

    fs["D1"] >> D1;
    fs["D2"] >> D2;

    fs["R1"] >> R1;
    fs["R2"] >> R2;

    fs["P1"] >> P1;
    fs["P2"] >> P2;

    auto size = cv::Size(WIDTH, HEIGHT);

    double f = fudgeFactor;
    P1.at<double>(0,0) *= f;
    P1.at<double>(1,1) *= f;
    P2.at<double>(0,0) *= f;
    P2.at<double>(1,1) *= f;

    auto M1 = getOptimalNewCameraMatrix(K1, D1, size, 0.8, size);
    auto M2 = getOptimalNewCameraMatrix(K2, D2, size, 0.8, size);

    // Generate the rectification maps
    cv::initUndistortRectifyMap(K1, D1, cv::Mat(), M1, size, CV_32FC1, leftMapX, leftMapY);
    cv::initUndistortRectifyMap(K2, D2, cv::Mat(), M2, size, CV_32FC1, rightMapX, rightMapY);

    // Set up the maps
    loadMapTexture(eyeData.get_left_map_x_texture(), leftMapX);
    loadMapTexture(eyeData.get_left_map_y_texture(), leftMapY);
    loadMapTexture(eyeData.get_right_map_x_texture(), rightMapX);
    loadMapTexture(eyeData.get_right_map_y_texture(), rightMapY);

    cv::Mat leftMapXT, leftMapYT, rightMapXT, rightMapYT;

    cv::transpose(leftMapX, leftMapXT);
    cv::transpose(leftMapY, leftMapYT);
    cv::transpose(rightMapX, rightMapXT);
    cv::transpose(rightMapY, rightMapYT);

    mapX = cv::Mat();
    mapY = cv::Mat();

    mapX.push_back(leftMapXT);
    mapX.push_back(rightMapXT);

    mapY.push_back(leftMapYT);
    mapY.push_back(rightMapYT);

    cv::transpose(mapX, mapX);
    cv::transpose(mapY, mapY);

    loadMapTexture(eyeData.get_map_x_texture(), mapX, WIDTH * 2);
    loadMapTexture(eyeData.get_map_y_texture(), mapY, WIDTH * 2);

    sbm = cv::StereoBM::create(16, 21);

    mapsLoaded = true;
}


void GDEiffelCam::unloadMaps() {

    mapsLoaded = false;
}

void GDEiffelCam::enter_calibration_mode(){
    in_calibration_mode = true;
    eyeData.init_calibration_buffer();
}

void GDEiffelCam::exit_calibration_mode(){
    in_calibration_mode = false;
}

bool GDEiffelCam::take_picture(){
    GodotTextureComponents::PictureTakenResult result = eyeData.take_picture();

    switch (result) {
        case GodotTextureComponents::PictureTakenResult::SUCCESS: {
            emit_signal("chessboard_detected", eyeData.get_current_left_chessboard_image(), eyeData.get_current_right_chessboard_image());
            return true;
        } case GodotTextureComponents::PictureTakenResult::INVALID_PICTURE:
            return false;
        case GodotTextureComponents::PictureTakenResult::READY_FOR_CALIBRATION: {
            emit_signal("ready_for_calibration");
            return false;
        }
    }
}

void GDEiffelCam::accept_calibration_image() {
    eyeData.accept_calibration_image();

    if (eyeData.get_calibration_images_left() == 0) {
        emit_signal("ready_for_calibration");
    }
}

int GDEiffelCam::calibration_images_left(){
    return eyeData.get_calibration_images_left();
}

void GDEiffelCam::calibrate_single_cameras(
    std::vector< std::vector<cv::Point3f> >* p_object_points,
    std::vector< std::vector<cv::Point2f> >* p_left_image_points,
    std::vector< std::vector<cv::Point2f> >* p_right_image_points,
    cv::Mat& p_left_camera_matrix,
    cv::Mat& p_left_camera_distortion_coefficients,
    cv::Mat& p_right_camera_matrix,
    cv::Mat& p_right_camera_distortion_coefficients
    ) {
    
    cv::Size image_size = cv::Size_<int>(WIDTH, HEIGHT);
    cv::Mat left_rvecs;
    cv::Mat left_tvecs;
    cv::Mat right_rvecs;
    cv::Mat right_tvecs;

    cv::calibrateCamera(*p_object_points, *p_left_image_points, image_size, p_left_camera_matrix, p_left_camera_distortion_coefficients, left_rvecs, left_tvecs);
    p_left_camera_matrix = cv::getOptimalNewCameraMatrix(p_left_camera_matrix, p_left_camera_distortion_coefficients, image_size, 1, image_size);
    cv::calibrateCamera(*p_object_points, *p_right_image_points, image_size, p_right_camera_matrix, p_right_camera_distortion_coefficients, right_rvecs, right_tvecs);
    p_right_camera_matrix = cv::getOptimalNewCameraMatrix(p_right_camera_matrix, p_right_camera_distortion_coefficients, image_size, 1, image_size);
}

bool save_stereo_coefficients(
    const cv::String& save_file_path,
    const cv::Mat& K1,
    const cv::Mat& D1,
    const cv::Mat& K2,
    const cv::Mat& D2,
    cv::InputOutputArray R,
    cv::InputOutputArray T,
    cv::OutputArray E,
    cv::OutputArray F,
    cv::OutputArray R1,
    cv::OutputArray R2,
    cv::OutputArray P1,
    cv::OutputArray P2,
    cv::OutputArray Q
    ){
    //Save the stereo coefficients to given path/file.
    cv::FileStorage cv_file = cv::FileStorage();
    bool file_opened = cv_file.open(save_file_path, cv::FileStorage::WRITE);
    if (!file_opened) return false;

    cv_file.write("K1", K1);
    cv_file.write("D1", D1);
    cv_file.write("K2", K2);
    cv_file.write("D2", D2);
    cv_file.write("R", R.getMat());
    cv_file.write("T", T.getMat());
    cv_file.write("E", E.getMat());
    cv_file.write("F", F.getMat());
    cv_file.write("R1", R1.getMat());
    cv_file.write("R2", R2.getMat());
    cv_file.write("P1", P1.getMat());
    cv_file.write("P2", P2.getMat());
    cv_file.write("Q", Q.getMat());
    cv_file.release();
    return true;
}

bool GDEiffelCam::calibrate_stereo_camera(
    std::vector< std::vector<cv::Point3f> >* p_object_points,
    std::vector< std::vector<cv::Point2f> >* p_left_image_points,
    std::vector< std::vector<cv::Point2f> >* p_right_image_points,
    cv::Mat& p_left_camera_matrix,
    cv::Mat& p_left_camera_distortion_coefficients,
    cv::Mat& p_right_camera_matrix,
    cv::Mat& p_right_camera_distortion_coefficients,
    cv::TermCriteria p_termination_criteria
    ) {
    cv::Size image_size = cv::Size(WIDTH, HEIGHT);
    cv::Mat R;
    cv::Vec3d T;
    cv::Mat E;
    cv::Mat F;
    int stereo_calibration_flags = cv::CALIB_FIX_INTRINSIC;
    float re_projection_error = cv::stereoCalibrate(
        *p_object_points,
        *p_left_image_points,
        *p_right_image_points,
        p_left_camera_matrix,
        p_left_camera_distortion_coefficients,
        p_right_camera_matrix,
        p_right_camera_distortion_coefficients,
        image_size, R, T, E, F, stereo_calibration_flags, p_termination_criteria);
    Godot::print(String("Stereo calibration rms: ") + std::to_string(re_projection_error).c_str());

    cv::Mat R1;
    cv::Mat R2;
    cv::Mat P1;
    cv::Mat P2;
    cv::Mat Q;
    cv::stereoRectify(p_left_camera_matrix, p_left_camera_distortion_coefficients, p_right_camera_matrix, p_right_camera_distortion_coefficients, image_size, R, T, R1, R2, P1, P2, Q, cv::CALIB_ZERO_DISPARITY, 1.0, image_size);

    cv::String stereo_camera_calibration_file_path;
    if (OS::get_singleton()->get_name() == "Android") {
        stereo_camera_calibration_file_path = cv::String(ProjectSettings::get_singleton()->globalize_path(String("user://stereo_cam_calibrated.yml")).utf8().get_data());
    } else if (OS::get_singleton()->get_name() == "X11" || OS::get_singleton()->get_name() == "OSX") {
        stereo_camera_calibration_file_path = cv::String(ProjectSettings::get_singleton()->globalize_path(String("res://calibration/stereo_cam_calibrated.yml")).utf8().get_data());
    }

    return save_stereo_coefficients(stereo_camera_calibration_file_path, p_left_camera_matrix, p_left_camera_distortion_coefficients, p_right_camera_matrix, p_right_camera_distortion_coefficients, R, T, E, F, R1, R2, P1, P2, Q);
}

bool GDEiffelCam::recalibrate_camera() {
    bool local_early_stop = false;
    early_stop_recalibration = false;

    cv::Mat K1; //left camera matrix
    cv::Mat D1; //left camera distortion coefficients
    cv::Mat K2; //right camera matrix
    cv::Mat D2; //right camera distortion coefficients

    calibrate_single_cameras(eyeData.get_object_points(), eyeData.get_left_image_points(), eyeData.get_right_image_points(), K1, D1, K2, D2);

    {
        std::unique_lock<std::mutex> lock(recalibration_mutex);
        local_early_stop = early_stop_recalibration;
    }

    if (local_early_stop || !calibrate_stereo_camera(eyeData.get_object_points(), eyeData.get_left_image_points(), eyeData.get_right_image_points(), K1, D1, K2, D2, eyeData.get_termination_criteria())) return false;

    return true;
}

bool GDEiffelCam::calibrate_single_cameras_from_files(
    String& p_calibration_images_folder_path,
    std::vector< std::vector<cv::Point3f> >* p_object_points,
    std::vector< std::vector<cv::Point2f> >* p_left_image_points,
    std::vector< std::vector<cv::Point2f> >* p_right_image_points,
    cv::Mat& p_left_camera_matrix,
    cv::Mat& p_left_camera_distortion_coefficients,
    cv::Mat& p_right_camera_matrix,
    cv::Mat& p_right_camera_distortion_coefficients,
    cv::TermCriteria p_termination_criteria
    ) {

    std::vector<cv::Point3f> object_points_concat;

    for (int i = 0; i < GRID_HEIGHT; i++) {
        for (int j = 0; j < GRID_WIDTH; j++) {
            object_points_concat.push_back(cv::Point3f(j * SQUARE_SIZE, i * SQUARE_SIZE, 0.0));
        }
    }
    
    cv::Mat current_bgr_image;
    cv::Mat current_rgb_image;
    cv::Mat current_rgb_image_copy;
    cv::Rect left_rect = cv::Rect(0, 0, WIDTH, HEIGHT);
    cv::Mat left_image;
    cv::Rect right_rect = cv::Rect(WIDTH, 0, WIDTH, HEIGHT);
    cv::Mat right_image;
    cv::Mat left_gray_image;
    cv::Mat right_gray_image;
    std::vector<cv::Point2f> left_image_corner_points;
    std::vector<cv::Point2f> right_image_corner_points;

    for (int image_number = 1; image_number <= eyeData.get_calibration_images_required(); ++image_number) {
        // Load next image from file:
        current_bgr_image = cv::imread( (p_calibration_images_folder_path + "full_image_" + std::to_string(image_number).c_str() + ".png").utf8().get_data() );
        if (current_bgr_image.data == NULL){
            Godot::print("ERROR: NOT ENOUGH IMAGES FOR CALIBRATION.");
            return false;
        }
        cv::cvtColor(current_bgr_image, current_rgb_image, cv::COLOR_BGR2RGB);

        // Do the single camera calibration steps:
        current_rgb_image_copy = current_rgb_image.clone();

        left_image = current_rgb_image(left_rect);
        right_image = current_rgb_image_copy(right_rect);

        left_gray_image = cv::Mat(HEIGHT, WIDTH, CV_8UC3);
        cv::cvtColor(left_image, left_gray_image, cv::COLOR_RGB2GRAY);

        right_gray_image = cv::Mat(HEIGHT, WIDTH, CV_8UC3);
        cv::cvtColor(right_image, right_gray_image, cv::COLOR_RGB2GRAY);

        left_image_corner_points.clear();
        right_image_corner_points.clear();

        if (!cv::findChessboardCorners(left_gray_image, cv::Size_<int>(GRID_WIDTH, GRID_HEIGHT), left_image_corner_points)){
            Godot::print("ERROR: INVALID CALIBRATION IMAGE FOUND.");
            return false;
        }

        if (!cv::findChessboardCorners(right_gray_image, cv::Size_<int>(GRID_WIDTH, GRID_HEIGHT), right_image_corner_points)){
            Godot::print("ERROR: INVALID CALIBRATION IMAGE FOUND.");
            return false;
        }

        p_object_points->push_back(object_points_concat);

        cv::cornerSubPix(left_gray_image, left_image_corner_points, cv::Size_<int>(11, 11), cv::Size_<int>(-1, -1), p_termination_criteria);
        p_left_image_points->push_back(left_image_corner_points);

        cv::cornerSubPix(right_gray_image, right_image_corner_points, cv::Size_<int>(11, 11), cv::Size_<int>(-1, -1), p_termination_criteria);
        p_right_image_points->push_back(right_image_corner_points);
    }

    cv::Size image_size = cv::Size_<int>(WIDTH, HEIGHT);
    cv::Mat left_rvecs;
    cv::Mat left_tvecs;
    cv::Mat right_rvecs;
    cv::Mat right_tvecs;

    cv::calibrateCamera(*p_object_points, *p_left_image_points, image_size, p_left_camera_matrix, p_left_camera_distortion_coefficients, left_rvecs, left_tvecs);
    p_left_camera_matrix = cv::getOptimalNewCameraMatrix(p_left_camera_matrix, p_left_camera_distortion_coefficients, image_size, 1, image_size);
    cv::calibrateCamera(*p_object_points, *p_right_image_points, image_size, p_right_camera_matrix, p_right_camera_distortion_coefficients, right_rvecs, right_tvecs);
    p_right_camera_matrix = cv::getOptimalNewCameraMatrix(p_right_camera_matrix, p_right_camera_distortion_coefficients, image_size, 1, image_size);

    return true;
}

bool GDEiffelCam::recalibrate_camera_from_files() {
    String calibration_images_folder_path;
    if (OS::get_singleton()->get_name() == "Android") {
        calibration_images_folder_path = ProjectSettings::get_singleton()->globalize_path(String("user://"));
    } else if (OS::get_singleton()->get_name() == "X11" || OS::get_singleton()->get_name() == "OSX") {
        calibration_images_folder_path = ProjectSettings::get_singleton()->globalize_path(String("res://calibration/calibration_images/"));    // Do we need the / at the end?
    }
    std::vector< std::vector<cv::Point3f> >* object_points      = new std::vector< std::vector<cv::Point3f> >;
    std::vector< std::vector<cv::Point2f> >* left_image_points  = new std::vector< std::vector<cv::Point2f> >;
    std::vector< std::vector<cv::Point2f> >* right_image_points = new std::vector< std::vector<cv::Point2f> >;
    cv::Mat K1; //left camera matrix
    cv::Mat D1; //left camera distortion coefficients
    cv::Mat K2; //right camera matrix
    cv::Mat D2; //right camera distortion coefficients

    bool local_early_stop = false;
    early_stop_recalibration = false;

    if (!calibrate_single_cameras_from_files(calibration_images_folder_path, object_points, left_image_points, right_image_points, K1, D1, K2, D2, eyeData.get_termination_criteria())) return false;

    {
        std::unique_lock<std::mutex> lock(recalibration_mutex);
        local_early_stop = early_stop_recalibration;
    }

    if (local_early_stop || !calibrate_stereo_camera(object_points, left_image_points, right_image_points, K1, D1, K2, D2, eyeData.get_termination_criteria())) return false;

    return true;
}

void GDEiffelCam::cancel_recalibration() {

    std::unique_lock<std::mutex> lock(recalibration_mutex);
    early_stop_recalibration = true;
}

void GDEiffelCam::set_disparity_test_mode(bool on){
    disparity_test_mode = on;
}

PoolByteArray GDEiffelCam::create_disparity_map(Ref<ImageTexture> p_image) {
    //Porting historic/research/vendor/calibration/stereo_depth.py to C++:

    PoolByteArray image = p_image->get_data()->get_data();
    uint8_t* frame_data = image.write().ptr();
    cv::Mat frame_original(HEIGHT, WIDTH * 2, CV_8UC3, frame_data);

    cv::Mat frame_remapped(HEIGHT, WIDTH * 2, CV_8UC3);

    cv::remap(frame_original, frame_remapped, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

    // We need grayscale for disparity map.
    cv::Mat frame_gray(HEIGHT, WIDTH * 2, CV_8UC3);

    cv::cvtColor(frame_remapped, frame_gray, cv::COLOR_RGB2GRAY);

    cv::Mat left_frame_gray;
    cv::Mat right_frame_gray;

    cv::Rect left_rect(0, 0, WIDTH, HEIGHT);
    cv::Rect right_rect(WIDTH, 0, WIDTH, HEIGHT);

    left_frame_gray = frame_gray(left_rect);
    right_frame_gray = frame_gray(right_rect);

    int minDisparity = -1;
    int numDisparities = 5*16;          // max_disp has to be dividable by 16 f. E. HH 192, 256
    int window_size = 3;                // wsize default 3; 5; 7 for SGBM reduced size image; 15 for SGBM full size image (1300px and above); 5 Works nicely
    int blockSize = window_size;
    int P1 = 8 * 3 * window_size;
    int P2 = 32 * 3 * window_size;
    int disp12MaxDiff = 12;
    int uniquenessRatio = 10;
    int speckleWindowSize = 50;
    int speckleRange = 32;
    int preFilterCap = 63;
    int mode = cv::StereoSGBM::MODE_SGBM_3WAY;

    cv::Ptr<cv::StereoSGBM> left_matcher = cv::StereoSGBM::create(
        minDisparity,
        numDisparities,
        blockSize,
        P1,
        P2,
        disp12MaxDiff,
        uniquenessRatio,
        speckleWindowSize,
        speckleRange,
        preFilterCap,
        mode
    );

    cv::Ptr<cv::StereoSGBM> right_matcher = cv::ximgproc::createRightMatcher(left_matcher).dynamicCast<cv::StereoSGBM>();
    // FILTER Parameters
    int lambda = 80000;
    float sigma = 1.3;

    cv::Ptr<cv::ximgproc::DisparityWLSFilter> wls_filter = cv::ximgproc::createDisparityWLSFilter(left_matcher);
    wls_filter->setLambda(lambda);
    wls_filter->setSigmaColor(sigma);

    cv::Mat left_disparity_map;
    cv::Mat right_disparity_map;
    left_matcher->compute(left_frame_gray, right_frame_gray, left_disparity_map);
    right_matcher->compute(right_frame_gray, left_frame_gray, right_disparity_map);

    left_disparity_map.convertTo(left_disparity_map, CV_16SC1);
    right_disparity_map.convertTo(right_disparity_map, CV_16SC1);

    cv::Mat filtered_disparity_map;
    wls_filter->filter(left_disparity_map, left_frame_gray, filtered_disparity_map, right_disparity_map);   //NOTE: left_frame_gray can be YUV's Y array.
    cv::normalize(filtered_disparity_map, filtered_disparity_map, 255, 0, cv::NORM_MINMAX);

    PoolByteArray disparity_map_data;
    disparity_map_data.resize(filtered_disparity_map.rows * filtered_disparity_map.cols * 3);

    cv::Mat filtered_8_bit_disparity_map(filtered_disparity_map.rows, filtered_disparity_map.cols, CV_8UC3);
    filtered_disparity_map.convertTo(filtered_8_bit_disparity_map, CV_8UC3);

    cv::Mat filtered_rgb_disparity_map(HEIGHT, WIDTH, CV_8UC3, disparity_map_data.write().ptr());
    cv::cvtColor(filtered_8_bit_disparity_map, filtered_rgb_disparity_map, cv::COLOR_GRAY2RGB);

    if (save_debug_images) {
        cv::imwrite(ProjectSettings::get_singleton()->globalize_path("res://debug_images/4_1_frame_original.png").utf8().get_data(), frame_original);
        cv::imwrite(ProjectSettings::get_singleton()->globalize_path("res://debug_images/4_2_frame_remapped.png").utf8().get_data(), frame_remapped);
        cv::imwrite(ProjectSettings::get_singleton()->globalize_path("res://debug_images/4_3_left_frame_gray.png").utf8().get_data(), left_frame_gray);
        cv::imwrite(ProjectSettings::get_singleton()->globalize_path("res://debug_images/4_4_right_frame_gray.png").utf8().get_data(), right_frame_gray);
        cv::imwrite(ProjectSettings::get_singleton()->globalize_path("res://debug_images/4_5_left_disparity_map.png").utf8().get_data(), left_disparity_map);
        cv::imwrite(ProjectSettings::get_singleton()->globalize_path("res://debug_images/4_6_right_disparity_map.png").utf8().get_data(), right_disparity_map);
        cv::imwrite(ProjectSettings::get_singleton()->globalize_path("res://debug_images/4_7_filtered_disparity_map.png").utf8().get_data(), filtered_rgb_disparity_map);
        save_debug_images = false;
    }

    return disparity_map_data;
}

Ref<ImageTexture> GDEiffelCam::get_disparity_map(){
    return eyeData.get_current_disparity_map();
}

void GDEiffelCam::save_disparity_images(){
    save_debug_images = true;
}