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

extends Node

onready var fullscreen_quad = get_parent()
onready var eiffel_camera : Node = get_node("/root/Scene/EiffelCamera")

func _ready():
  eiffel_camera.connect("frame_diff_changed", self, "on_frame_diff_changed")
  eiffel_camera.connect("opened", self, "on_opened")

  fullscreen_quad.register_output_texture_rgb("texture_rgb")
  fullscreen_quad.register_output_texture_yuv("texture_y", "texture_u", "texture_v")
  fullscreen_quad.register_output_texture_array_rgb("rgb_frame_array")
  fullscreen_quad.register_output_texture_array_yuv("y_frame_array", "u_frame_array", "v_frame_array")
  fullscreen_quad.register_index_uniform_name("current_frame_index")

func on_opened():
  fullscreen_quad.set_shader_param("frame_array_size", eiffel_camera.getEyeRGBFrameArray().get_depth())
  
func on_frame_diff_changed(value):
  fullscreen_quad.set_shader_param("frame_diff", value)
