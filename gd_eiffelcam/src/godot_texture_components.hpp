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

#pragma once

#include <Godot.hpp>
#include <TextureArray.hpp>
#include <ImageTexture.hpp>

#include <opencv2/core.hpp>

#define GRID_HEIGHT 6
#define GRID_WIDTH 9
#define SQUARE_SIZE 24      // NOTE: The actual square width/height in mm on the chessboard you're using. Can vary between users though.
namespace godot {

class GodotTextureComponents {
private:
    int frame_array_size;
    int default_frame_diff = 0;
    int frame_diff = default_frame_diff;
    int current_frame_index = 0;  // always replace the oldest frame
    bool first_update = true;

    Ref<ImageTexture> current_rgb_frame;
    Ref<ImageTexture> current_y_frame;
    Ref<ImageTexture> current_u_frame;
    Ref<ImageTexture> current_v_frame;
    Ref<ImageTexture> current_disparity_map;

    Ref<TextureArray> rgb_frame_array;
    Ref<TextureArray> y_frame_array;
    Ref<TextureArray> u_frame_array;
    Ref<TextureArray> v_frame_array;

    Ref<ImageTexture> left_map_x_texture;
    Ref<ImageTexture> left_map_y_texture;
    Ref<ImageTexture> right_map_x_texture;
    Ref<ImageTexture> right_map_y_texture;
    Ref<ImageTexture> map_x_texture;
    Ref<ImageTexture> map_y_texture;

    Ref<Image> current_rgb_image;

    Ref<ImageTexture> current_left_chessboard_image;
    Ref<ImageTexture> current_right_chessboard_image;
    cv::Mat current_left_calibration_image;
    cv::Mat current_right_calibration_image;

    void update_current_frame_index();

    const int calibration_images_required = 20; // max 20
    int calibration_images_left = 20;
    bool image_on_hold = false;

    PoolByteArray convert_mat_to_pba(cv::Mat rgb_image);
    PoolByteArray draw_detected_chessboard(cv::Mat gray_image, const std::vector<cv::Point2f>& corner_points);

    // Saving calibration data WHILE taking the pictures:
    cv::Mat left_gray_image;
    cv::Mat right_gray_image;
    std::vector<cv::Point2f> left_image_corner_points;
    std::vector<cv::Point2f> right_image_corner_points;
    std::vector<cv::Point3f> object_points_concat;               // Create real world coords. Use your metric.
    std::vector<std::vector<cv::Point3f> >* object_points;       // 3d point in real world space.
    std::vector<std::vector<cv::Point2f> >* left_image_points;   // 2d points in image plane.
    std::vector<std::vector<cv::Point2f> >* right_image_points;  // 2d points in image plane.
    cv::TermCriteria termination_criteria;

    /*
    // For debugging only, comment them if you don't need them:
    
    cv::Mat full_image;
    cv::Mat left_image_untouched;
    cv::Mat right_image_untouched;
    int image_number = 1;
    */

public:

    enum PictureTakenResult{
        SUCCESS = 1,
        INVALID_PICTURE = 2,
        READY_FOR_CALIBRATION = 3
    };

    void init(int frame_array_size);

    void update_yuv_frame_array(const PoolByteArray& yuv_data);
    void update_rgb_frame_array(PoolByteArray& rgb_data);
    void update_disparity_map(PoolByteArray& disparity_map_data);

    void accept_calibration_image();

    int get_current_frame_index(){ return current_frame_index; }
    Ref<ImageTexture> get_current_rgb_frame(){ return current_rgb_frame; }
    Ref<ImageTexture> get_current_y_frame(){ return current_y_frame; }
    Ref<ImageTexture> get_current_u_frame(){ return current_u_frame; }
    Ref<ImageTexture> get_current_v_frame(){ return current_v_frame; }
    Ref<TextureArray> get_rgb_frame_array(){ return rgb_frame_array; }
    Ref<TextureArray> get_y_frame_array(){ return y_frame_array; }
    Ref<TextureArray> get_u_frame_array(){ return u_frame_array; }
    Ref<TextureArray> get_v_frame_array(){ return v_frame_array; }
    Ref<ImageTexture>& get_left_map_x_texture(){ return left_map_x_texture; }
    Ref<ImageTexture>& get_left_map_y_texture(){ return left_map_y_texture; }
    Ref<ImageTexture>& get_right_map_x_texture(){ return right_map_x_texture; }
    Ref<ImageTexture>& get_right_map_y_texture(){ return right_map_y_texture; }
    Ref<ImageTexture>& get_map_x_texture(){ return map_x_texture; }
    Ref<ImageTexture>& get_map_y_texture(){ return map_y_texture; }
    Ref<ImageTexture> get_current_disparity_map(){ return current_disparity_map; }
    Ref<ImageTexture> get_current_left_chessboard_image(){ return current_left_chessboard_image; }
    Ref<ImageTexture> get_current_right_chessboard_image(){ return current_right_chessboard_image; }
    cv::Mat get_current_left_calibration_image(){ return current_left_calibration_image; }
    cv::Mat get_current_right_calibration_image(){ return current_right_calibration_image; }
    int get_frame_diff(){ return frame_diff; }
    void set_frame_diff(int p_frame_diff) { frame_diff = p_frame_diff; }
    int get_default_frame_diff() { return default_frame_diff; }

    void init_calibration_buffer();
    PictureTakenResult take_picture();

    int get_calibration_images_required(){ return calibration_images_required; }
    int get_calibration_images_left(){ return calibration_images_left; }
    void set_image_on_hold(bool value){ image_on_hold = value; }
    bool is_image_on_hold() { return image_on_hold; }
    std::vector<std::vector<cv::Point3f> >* get_object_points(){ return object_points; }
    std::vector<std::vector<cv::Point2f> >* get_left_image_points(){ return left_image_points; }
    std::vector<std::vector<cv::Point2f> >* get_right_image_points(){ return right_image_points; }
    cv::TermCriteria get_termination_criteria(){ return termination_criteria; }

    GodotTextureComponents(){};
    ~GodotTextureComponents(){
        delete object_points;
        delete left_image_points;
        delete right_image_points;
    };
};

}