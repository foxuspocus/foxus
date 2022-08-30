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

onready var slider : HSlider = get_node("BottomRow/Slider")
onready var reset_button : Button = get_node("BottomRow/Reset")
onready var value_label : Label = get_node("LabelRow/Value")
onready var name_label : Label = get_node("LabelRow/Name")

var eiffel_camera

var property_name = ""
var mode = "number"

var enum_values
var enum_names

func _ready():
  slider.connect("value_changed", self, "on_value_changed")
  reset_button.connect("pressed", self, "on_reset_pressed")

func on_reset_pressed():
  if eiffel_camera != null and property_name != "":
    eiffel_camera.reset_camera_setting(property_name)

    slider.value = eiffel_camera.get(property_name)
    value_label.text = str(eiffel_camera.get_camera_setting_label(property_name))

func set_range(property, minimum, maximum, current):

  name_label.text = property

  property_name = property

  slider.min_value = minimum
  slider.max_value = maximum
  slider.value = current

  value_label.text = str(eiffel_camera.get_camera_setting_label(property_name))

func on_value_changed(value):
  if property_name != "" && eiffel_camera:
    eiffel_camera.set(property_name, value)
    value_label.text = str(eiffel_camera.get_camera_setting_label(property_name))

