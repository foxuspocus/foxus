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

extends VBoxContainer

var slider_box_scene : PackedScene = preload("res://scenes/SliderBox.tscn")

onready var eiffel_camera = get_node("/root/Scene/EiffelCamera")

var properties : Dictionary

func _ready():
  eiffel_camera.connect("camera_property_range_changed", self, "on_range_changed")

func on_range_changed(prop, current, minimum, maximum):

  var slider

  if properties.has(prop):
    slider = properties.get(prop)
  else:
    slider = slider_box_scene.instance()
    add_child(slider)

    properties[prop] = slider
    slider.eiffel_camera = eiffel_camera

  slider.set_range(prop, minimum, maximum, current)
