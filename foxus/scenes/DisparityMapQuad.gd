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

var DISTANCE = 50
var material = get_active_material(0)
var eiffel_camera : Node

func connect_to_camera():
  if eiffel_camera == null:
    eiffel_camera = get_node("/root/Scene/EiffelCamera")

var disparity_map_quad_transform = Transform()

func on_frame_start():
  var viewport = get_viewport()
  var camera = viewport.get_camera()

  disparity_map_quad_transform.origin = camera.project_position(viewport.size / 2, DISTANCE)
  disparity_map_quad_transform.basis = camera.transform.basis

func on_frame_end():
  if disparity_map_quad_transform:
    transform.origin = disparity_map_quad_transform.origin
    transform.basis = disparity_map_quad_transform.basis

func _process(delta):
  if visible:
    on_frame_start()
    on_frame_end()
    material.set_shader_param("disparity_map", eiffel_camera.get_disparity_map())

func _input(event):
  if not visible:
    return
  
  if event is InputEventKey and event.scancode == KEY_SPACE:
    eiffel_camera.save_disparity_images()	#save the next frame's pictures into res://debug_images
    eiffel_camera.get_disparity_map().get_data().save_png("res://debug_images/godot_image.png")
