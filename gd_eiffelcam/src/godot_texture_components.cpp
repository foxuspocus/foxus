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

#include "godot_texture_components.hpp"
#include "gd_eiffelcam.hpp"
#include "OS.hpp"

using namespace godot;

void GodotTextureComponents::init(int frame_array_size){
    this->frame_array_size = frame_array_size;

    current_rgb_frame = Ref<ImageTexture>(ImageTexture::_new());
    current_y_frame = Ref<ImageTexture>(ImageTexture::_new());
    current_u_frame = Ref<ImageTexture>(ImageTexture::_new());
    current_v_frame = Ref<ImageTexture>(ImageTexture::_new());

    current_disparity_map = Ref<ImageTexture>(ImageTexture::_new());

    rgb_frame_array = Ref<TextureArray>(TextureArray::_new());
    y_frame_array = Ref<TextureArray>(TextureArray::_new());
    u_frame_array = Ref<TextureArray>(TextureArray::_new());
    v_frame_array = Ref<TextureArray>(TextureArray::_new());

    rgb_frame_array->create(WIDTH * 2, HEIGHT, frame_array_size, Image::FORMAT_RGB8);
    y_frame_array->create(WIDTH * 2, HEIGHT, frame_array_size, Image::FORMAT_L8);
    u_frame_array->create(WIDTH, HEIGHT, frame_array_size, Image::FORMAT_L8);
    v_frame_array->create(WIDTH, HEIGHT, frame_array_size, Image::FORMAT_L8);

    left_map_x_texture = Ref<ImageTexture>(ImageTexture::_new());
    left_map_y_texture = Ref<ImageTexture>(ImageTexture::_new());
    right_map_x_texture = Ref<ImageTexture>(ImageTexture::_new());
    right_map_y_texture = Ref<ImageTexture>(ImageTexture::_new());
    map_x_texture = Ref<ImageTexture>(ImageTexture::_new());
    map_y_texture = Ref<ImageTexture>(ImageTexture::_new());

    current_left_chessboard_image = Ref<ImageTexture>(ImageTexture::_new());
    current_right_chessboard_image = Ref<ImageTexture>(ImageTexture::_new());

    Ref<Image> current_rgb_image = Ref<Image>(Image::_new());
    Ref<Image> current_y_image = Ref<Image>(Image::_new());
    Ref<Image> current_u_image = Ref<Image>(Image::_new());
    Ref<Image> current_v_image = Ref<Image>(Image::_new());

    current_rgb_image->create(WIDTH * 2, HEIGHT, false, Image::FORMAT_RGB8);
    current_y_image->create(WIDTH * 2, HEIGHT, false, Image::FORMAT_L8);
    current_u_image->create(WIDTH, HEIGHT, false, Image::FORMAT_L8);
    current_v_image->create(WIDTH, HEIGHT, false, Image::FORMAT_L8);

    current_rgb_frame->create_from_image(current_rgb_image, 0);
    current_y_frame->create_from_image(current_y_image, 0);
    current_u_frame->create_from_image(current_u_image, 0);
    current_v_frame->create_from_image(current_u_image, 0);

    // Preparing for calibration:

    // Apply camera calibration operation for images in the given directory path.
    // Prepare object points:
    for (int i = 0; i < GRID_HEIGHT; i++) {
        for (int j = 0; j < GRID_WIDTH; j++) {
            object_points_concat.push_back(cv::Point3f(j * SQUARE_SIZE, i * SQUARE_SIZE, 0.0));
        }
    }

    object_points      = new std::vector<std::vector<cv::Point3f> >;
    left_image_points  = new std::vector<std::vector<cv::Point2f> >;
    right_image_points = new std::vector<std::vector<cv::Point2f> >;

    termination_criteria = cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.001);
}

void GodotTextureComponents::update_yuv_frame_array(const PoolByteArray& yuv_data){
    update_current_frame_index();

    if (frame_diff == 0) {
        current_y_frame->update_from_data(yuv_data, 0);
        current_u_frame->update_from_data(yuv_data, WIDTH * HEIGHT * 2);
        current_v_frame->update_from_data(yuv_data, WIDTH * HEIGHT * 3);
    } else {
        y_frame_array->set_layer_data_raw(yuv_data, 0, current_frame_index);
        u_frame_array->set_layer_data_raw(yuv_data, WIDTH * HEIGHT * 2, current_frame_index);
        v_frame_array->set_layer_data_raw(yuv_data, WIDTH * HEIGHT * 3, current_frame_index);
    }

}

void GodotTextureComponents::update_rgb_frame_array(PoolByteArray& rgb_data){
    update_current_frame_index();

    current_rgb_frame->update_from_data(rgb_data, 0);
    rgb_frame_array->set_layer_data_raw(rgb_data, 0, current_frame_index);
}

void GodotTextureComponents::update_disparity_map(PoolByteArray& disparity_map_data){
    Ref<Image> current_disparity_map_image = Ref<Image>(Image::_new());
    current_disparity_map_image->create_from_data(WIDTH, HEIGHT, false, Image::FORMAT_RGB8, disparity_map_data);
    current_disparity_map->create_from_image(current_disparity_map_image, Texture::FLAG_FILTER | Texture::FLAG_VIDEO_SURFACE);
}

void GodotTextureComponents::update_current_frame_index() {
    if (first_update) {
        first_update = false;
        return;
    }

    current_frame_index = (current_frame_index + 1) % frame_array_size;
}

void GodotTextureComponents::init_calibration_buffer(){
    calibration_images_left = calibration_images_required;
    //image_number = 1;

    object_points->clear();
    left_image_points->clear();
    right_image_points->clear();
}

PoolByteArray GodotTextureComponents::convert_mat_to_pba(cv::Mat rgb_image) {

    PoolByteArray pba;
    pba.resize(WIDTH * HEIGHT * 3);

    {
        PoolByteArray::Write wrt = pba.write();
        memcpy(wrt.ptr(), rgb_image.ptr(), WIDTH * HEIGHT * 3);
    }

    return pba;
}

PoolByteArray GodotTextureComponents::draw_detected_chessboard(cv::Mat gray_image, const std::vector<cv::Point2f>& corner_points) {

    cv::Mat rgb_image = cv::Mat(HEIGHT, WIDTH, CV_8UC3);
    cv::cvtColor(gray_image, rgb_image, cv::COLOR_GRAY2RGB);

    cv::drawChessboardCorners(rgb_image, cv::Size_<int>(GRID_WIDTH, GRID_HEIGHT), corner_points, true);

    PoolByteArray pba = convert_mat_to_pba(rgb_image);
    return pba;
}

GodotTextureComponents::PictureTakenResult GodotTextureComponents::take_picture(){
    if (calibration_images_left == 0) return READY_FOR_CALIBRATION;

    // Crop the image using OpenCV:

    Ref<Image> image = current_rgb_frame->get_data();
    const uint8_t* image_data = image->get_data().read().ptr();
    cv::Mat original_image = cv::Mat(HEIGHT, WIDTH * 2, CV_8UC3, (uint8_t*) image_data);
    cv::Mat gray_image;
    cv::cvtColor(original_image, gray_image, cv::COLOR_RGB2GRAY);

    cv::Rect left_rect = cv::Rect(0, 0, WIDTH, HEIGHT);
    left_gray_image = cv::Mat(gray_image, left_rect).clone();

    cv::Rect right_rect = cv::Rect(WIDTH, 0, WIDTH, HEIGHT);
    right_gray_image = cv::Mat(gray_image, right_rect).clone();

    left_image_corner_points.clear();
    right_image_corner_points.clear();

    if (!cv::findChessboardCorners(left_gray_image, cv::Size_<int>(GRID_WIDTH, GRID_HEIGHT), left_image_corner_points)) return INVALID_PICTURE;
    if (!cv::findChessboardCorners(right_gray_image, cv::Size_<int>(GRID_WIDTH, GRID_HEIGHT), right_image_corner_points)) return INVALID_PICTURE;

    current_left_calibration_image = left_gray_image.clone();
    current_right_calibration_image = right_gray_image.clone();

    PoolByteArray left_chessboard_array = draw_detected_chessboard(left_gray_image, left_image_corner_points);
    Ref<Image> left_chessboard_image = Ref<Image>(Image::_new());
    left_chessboard_image->create_from_data(WIDTH, HEIGHT, false, Image::FORMAT_RGB8, left_chessboard_array);

    PoolByteArray right_chessboard_array = draw_detected_chessboard(right_gray_image, right_image_corner_points);
    Ref<Image> right_chessboard_image = Ref<Image>(Image::_new());
    right_chessboard_image->create_from_data(WIDTH, HEIGHT, false, Image::FORMAT_RGB8, right_chessboard_array);

    current_left_chessboard_image->create_from_image(left_chessboard_image, Texture::FLAG_FILTER | Texture::FLAG_VIDEO_SURFACE);
    current_right_chessboard_image->create_from_image(right_chessboard_image, Texture::FLAG_FILTER | Texture::FLAG_VIDEO_SURFACE);

    return SUCCESS;
}

void GodotTextureComponents::accept_calibration_image() {
    object_points->push_back(object_points_concat);

    cv::cornerSubPix(left_gray_image, left_image_corner_points, cv::Size_<int>(11, 11), cv::Size_<int>(-1, -1), termination_criteria);
    left_image_points->push_back(left_image_corner_points);

    cv::cornerSubPix(right_gray_image, right_image_corner_points, cv::Size_<int>(11, 11), cv::Size_<int>(-1, -1), termination_criteria);
    right_image_points->push_back(right_image_corner_points);

    --calibration_images_left;
}