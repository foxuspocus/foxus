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

onready var fullscreen_quad = get_tree().get_root().find_node("FullscreenQuad", true, false)

onready var colorspace_items = get_node("Container/ColorspaceContainer/Items")
onready var remap_items = get_node("Container/RemapContainer/Items")

onready var button = get_node("Button")

func _ready():
  colorspace_items.add_item("YUV")
  colorspace_items.add_item("RGB")

  remap_items.add_item("GPU_REMAP")
  remap_items.add_item("CPU_REMAP")
  remap_items.add_item("NO_REMAP")

  button.connect("pressed", self, "on_button_pressed")
  
func on_button_pressed():
  var color_space_arr = colorspace_items.get_selected_items()
  var remap_mode_arr = remap_items.get_selected_items()
  
  if len(color_space_arr) > 0:
    var color_space = color_space_arr[0]
    fullscreen_quad.color_space = color_space

  if len(remap_mode_arr) > 0:
    var remap_mode = remap_mode_arr[0]
    fullscreen_quad.remap_mode = remap_mode

  fullscreen_quad.refresh_options()


