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

extends MeshInstance

var DISTANCE = 65
var config = ConfigFile.new()

const YUV = 0
const RGB = 1

const GPU_REMAP = 0
const CPU_REMAP = 1
const NO_REMAP = 2

var color_space = RGB
var remap_mode = GPU_REMAP

onready var eiffel_camera : Node = get_node("/root/Scene/EiffelCamera")
onready var error_viewport = get_node("/root/Scene/ErrorViewport")
onready var error_label = error_viewport.get_node("Label")
onready var user_notification_quad = get_node("/root/Scene/ARVROrigin/UserNotificationQuad")
onready var images_tab = find_node("images")

var material = get_active_material(0)
var display_error = true

enum CONNECTION_STATUS {NOT_CONNECTED, ATTACHED, CONNECTED}

const STATUS_TO_MSG : Array = [
  "Please connect your camera",
  "Please give the app permission to use\nyour camera. Press B and go to settings.",
  ""
]

const STANDARD_LUT = 'res://luts/Neutral.png'

var uniform_texture_rgb : String

var uniform_texture_y : String
var uniform_texture_u : String
var uniform_texture_v : String

var uniform_texture_array_rgb : String

var uniform_texture_array_y : String
var uniform_texture_array_u : String
var uniform_texture_array_v : String

var uniform_frame_index : String

var offset = Vector2(0, 0)

func nudge(c: Vector2):
  offset += c * 0.002

func push_pull(i):
  DISTANCE += i

func save_config():
  config.save("user://settings.cfg")

var image_on_hold : bool = false

func _ready():
  var err = config.load("user://settings.cfg")

  eiffel_camera.connect("opened", self, "on_opened")
  eiffel_camera.connect("frame_index_updated", self, "on_frame_index_updated")
  eiffel_camera.connect("camera_status_changed", self, "on_camera_status_changed")

  material.set_shader_param("error_texture", error_viewport.get_texture())

  # Load LUT (standard if unspecified, otherwise from settings.cfg)
  var fn = config.get_value('lut', 'default', STANDARD_LUT)
  self.load_lut(load(fn).get_data())

func on_camera_status_changed(new_state):
  display_error = true

  if new_state == CONNECTION_STATUS.CONNECTED:
    display_error = false

  material.set_shader_param("display_error", display_error)

  error_label.text = STATUS_TO_MSG[new_state]

func on_opened():
  material.set_shader_param(uniform_texture_rgb, eiffel_camera.getEyeTextureRGB())

  material.set_shader_param(uniform_texture_y, eiffel_camera.getEyeTextureY())
  material.set_shader_param(uniform_texture_u, eiffel_camera.getEyeTextureU())
  material.set_shader_param(uniform_texture_v, eiffel_camera.getEyeTextureV())

  material.set_shader_param(uniform_frame_index, eiffel_camera.getCurrentFrameIndex())

  material.set_shader_param(uniform_texture_array_rgb, eiffel_camera.getEyeRGBFrameArray())

  material.set_shader_param(uniform_texture_array_y, eiffel_camera.getEyeYFrameArray())
  material.set_shader_param(uniform_texture_array_u, eiffel_camera.getEyeUFrameArray())
  material.set_shader_param(uniform_texture_array_v, eiffel_camera.getEyeVFrameArray())

  refresh_options()

func set_lut_filename(fn):
  config.set_value('lut', 'default', fn)
  save_config()

  var img = load(fn).get_data()
  self.load_lut(img)

func load_lut(img):
  var t = Texture3D.new()
  t.create(64, 64, 64, Image.FORMAT_RGB8)

  for z in range(0, 63):
    var u = floor(fmod(z, 8.0))
    var v = floor(z / 8)

    var sub = img.get_rect(Rect2(u * 64, v * 64, 64, 64))
    t.set_data_partial(sub, 0, 0, z)

  material.set_shader_param("lut", t)

func refresh_options():
  if color_space == YUV && remap_mode == CPU_REMAP:
    user_notification_quad.popup("Error: YUV texture incompatible with CPU side remapping.", 5)
    return

  material.set_shader_param("color_space", color_space)
  material.set_shader_param("remap_mode", remap_mode)

  eiffel_camera.set_colorspace(color_space)
  eiffel_camera.set_remap_mode(remap_mode)

func connect_to_camera():
  material.set_shader_param("map_x", eiffel_camera.getMapX());
  material.set_shader_param("map_y", eiffel_camera.getMapY());

  if !eiffel_camera.is_connected('frame_start', self, 'on_frame_start'):
    eiffel_camera.connect('frame_start', self, 'on_frame_start')

  if !eiffel_camera.is_connected('frame_end', self, 'on_frame_end'):
    eiffel_camera.connect('frame_end', self, 'on_frame_end')

var fullscreen_quad_transform = Transform()

func on_frame_index_updated(value):
  material.set_shader_param(uniform_frame_index, value)

func on_frame_start():
  var viewport = get_viewport()
  var camera = viewport.get_camera()

  var fullscreen_quad_position = camera.project_position(viewport.size / 2, DISTANCE)
  fullscreen_quad_transform.origin = fullscreen_quad_position
  fullscreen_quad_transform.basis = camera.transform.basis

func on_frame_end():
  if fullscreen_quad_transform:
    transform.origin = fullscreen_quad_transform.origin
    transform.basis = fullscreen_quad_transform.basis

const button_press_delay = 0.2
var current_button_press_delay = 0

func _process(delta):
  if display_error:
    on_frame_start()
    on_frame_end()
  current_button_press_delay -= delta
  material.set_shader_param("uv_offset", offset);

func _unhandled_input(event):

  if current_button_press_delay > 0:
    return
    
  if not event is InputEventKey:
    return
    
  if !event.pressed or event.scancode != KEY_P:
    return
    
  if !eiffel_camera.is_in_calibration_mode():
    user_notification_quad.popup("Images can only be taken in calibration mode.", 5)
    return
  
  if eiffel_camera.is_image_on_hold():
    user_notification_quad.popup("Accept or discard the image on the 'Images' tab.")
  else:
    var calibration_images_left = eiffel_camera.calibration_images_left()
    
    if eiffel_camera.take_picture():
      if calibration_images_left > 0:
        user_notification_quad.popup("Accept or discard the image on the 'Images' tab.")
      current_button_press_delay = button_press_delay
    else:
      if calibration_images_left > 0:
        user_notification_quad.popup("Invalid picture. Try again.")

func register_output_texture_rgb(uniform_name):
  uniform_texture_rgb = uniform_name

func register_output_texture_yuv(y_uniform_name, u_uniform_name, v_uniform_name):
  uniform_texture_y = y_uniform_name
  uniform_texture_u = u_uniform_name
  uniform_texture_v = v_uniform_name

func register_output_texture_array_rgb(uniform_name):
  uniform_texture_array_rgb = uniform_name

func register_output_texture_array_yuv(y_uniform_name, u_uniform_name, v_uniform_name):
  uniform_texture_array_y = y_uniform_name
  uniform_texture_array_u = u_uniform_name
  uniform_texture_array_v = v_uniform_name

func register_index_uniform_name(index_uniform_name):
  uniform_frame_index = index_uniform_name

func set_shader_param(uniform, value):
  material.set_shader_param(uniform, value)
