#
# Copyright (c) 2022 Nolan Consulting Limited.
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
#  the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

extends Control

onready var use_calibrated_button : Button = get_node("UseCalibrated")
onready var test_disparity_map_button : Button = get_node("TestDisparityMap")
onready var exit_disparity_map_button : Button = get_node("ExitDisparityMap")
onready var enter_calibration_mode_button : Button = get_node("EnterCalibrationMode")
onready var cancel_calibration_button : Button = get_node("CancelCalibration")
onready var finish_calibration_button : Button = get_node("FinishCalibration")
onready var calibrate_from_files_button : Button = get_node("CalibrateFromFiles")

onready var eiffel_camera : Node = get_node("/root/Scene/EiffelCamera")
onready var fullscreen_quad : MeshInstance = get_node("/root/Scene/ARVROrigin/FullscreenQuad")
onready var disparity_map_quad : MeshInstance = get_node("/root/Scene/ARVROrigin/DisparityMapQuad")
onready var user_notification_quad : MeshInstance = get_node("/root/Scene/ARVROrigin/UserNotificationQuad")
onready var vrui : MeshInstance = get_node("/root/Scene/VRUI")

var user_calibration_file_name_android = "user://stereo_cam_calibrated.yml"
var user_calibration_file_name_pc = "res://calibration/stereo_cam_calibrated.yml"
var current_user_calibration_file_name : String

var thread: Thread = null

func _ready():
  if OS.get_name() == "Android":
    current_user_calibration_file_name = user_calibration_file_name_android
    use_calibrated_button.visible = true
  if OS.get_name() == "X11" or OS.get_name() == "OSX":
    current_user_calibration_file_name = user_calibration_file_name_pc
    use_calibrated_button.visible = true
  use_calibrated_button.connect("pressed", self, "_load_user_calibration")
  test_disparity_map_button.connect("pressed", self, "_test_disparity_map")
  exit_disparity_map_button.connect("pressed", self, "_exit_disparity_map")
  enter_calibration_mode_button.connect("pressed", self, "_on_enter_calibration_mode")
  cancel_calibration_button.connect("pressed", self, "_on_cancel_calibration")
  finish_calibration_button.connect("pressed", self, "_on_finish_calibration")
  calibrate_from_files_button.connect("pressed", self, "_on_calibrate_from_files")
  eiffel_camera.connect("ready_for_calibration", self, "_on_front_camera_calibration_buffer_full")

func _load_user_calibration():
    if File.new().open(current_user_calibration_file_name, File.READ) == OK:
        var fudge = 1
        eiffel_camera.loadMaps(current_user_calibration_file_name, fudge)

        fullscreen_quad.connect_to_camera()
        disparity_map_quad.connect_to_camera()
    else:
        user_notification_quad.popup("Error: No user calibrations found. Please make one.", 5)
func _test_disparity_map():
  if fullscreen_quad.display_error:
    user_notification_quad.popup("Error: Camera not connected.", 5)
    return

  if fullscreen_quad.color_space != fullscreen_quad.RGB:
    user_notification_quad.popup("Error: Set colorspace to RGB before testing disparity map.", 5)
    return
  
  if  fullscreen_quad.remap_mode != fullscreen_quad.CPU_REMAP:
    user_notification_quad.popup("Error: Disparity maps only available with CPU remapping.", 5)
    return
  
  fullscreen_quad.visible = false
  eiffel_camera.set_disparity_test_mode(true)
  disparity_map_quad.visible = true
  exit_disparity_map_button.visible = true

func _exit_disparity_map():
  eiffel_camera.set_disparity_test_mode(false)
  disparity_map_quad.visible = false
  fullscreen_quad.visible = true
  exit_disparity_map_button.visible = false

func _on_enter_calibration_mode():
  if fullscreen_quad.display_error:
    user_notification_quad.popup("Error: Camera not connected.", 5)
    return
  
  if fullscreen_quad.color_space != fullscreen_quad.RGB:
    user_notification_quad.popup("Error: Set colorspace to RGB before calibrating.", 5)
    return
  
  finish_calibration_button.visible = false
  eiffel_camera.enter_calibration_mode()
  enter_calibration_mode_button.visible = false
  cancel_calibration_button.visible = true
  vrui.hide()
  
  var calibration_images_left = eiffel_camera.calibration_images_left()
  user_notification_quad.popup("Take " + str(calibration_images_left) + " more pictures for calibration.")

func _on_front_camera_calibration_buffer_full():
  user_notification_quad.popup("Enough pictures taken. Click 'Finish Calibration' to continue.")
  finish_calibration_button.visible = true

func _on_cancel_calibration():
  if thread != null and thread.is_alive():
    eiffel_camera.cancel_recalibration()
  else: 
    enter_calibration_mode_button.visible = true
    cancel_calibration_button.visible = false
    finish_calibration_button.visible = false

  eiffel_camera.exit_calibration_mode()
  user_notification_quad.popup("Camera calibration cancelled.", 5)

func _on_finish_calibration():
  finish_calibration_button.visible = false
  
  user_notification_quad.popup("Calibrating camera. Please wait...")
  
  if thread != null:
    thread.wait_to_finish()

  eiffel_camera.exit_calibration_mode()

  thread = Thread.new()
  thread.start(self, "recalibrate_camera")

func recalibrate_camera():
  var calibration_successful = eiffel_camera.recalibrate_camera()

  if calibration_successful:
    user_notification_quad.popup("Calibration successful.", 5)
    use_calibrated_button.visible = true
  else:
    user_notification_quad.popup("Calibration failed. Try again with better pictures.", 5)

  enter_calibration_mode_button.visible = true
  cancel_calibration_button.visible = false
  finish_calibration_button.visible = false

func _on_calibrate_from_files():
  user_notification_quad.popup("Calibrating camera. Please wait...")

  if thread != null:
    thread.wait_to_finish()

  eiffel_camera.exit_calibration_mode()

  thread = Thread.new()
  thread.start(self, "recalibrate_camera_from_files")

func recalibrate_camera_from_files():
  var calibration_successful = eiffel_camera.recalibrate_camera_from_files();

  if calibration_successful:
    user_notification_quad.popup("Calibration successful.", 5)
    use_calibrated_button.visible = true
  else:
    user_notification_quad.popup("Calibration failed. Try again with better pictures.", 5)
