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

extends Area

func _ready():
  set_ray_pickable(true)

func _on_Area_input_event(camera, event, position, normal, shape_idx):
  var viewport = find_parent("Scene").get_node("ViewportContainer/Viewport")
  print(viewport.get_mouse_position())

  if event is InputEventMouseButton :
    var e = InputEventMouseButton.new()
    e.button_index = BUTTON_LEFT
    e.position = Vector2(0.5, 0.5) * viewport.size
    e.pressed = true
    viewport.input(e)

    e = InputEventMouseButton.new()
    e.button_index = BUTTON_LEFT
    e.position = Vector2(0.5, 0.5) * viewport.size
    e.pressed = false
    viewport.input(e)
