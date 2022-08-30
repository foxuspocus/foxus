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

var viewport : Viewport
onready var texture_viewport = get_node("/root/Scene/ViewportContainer/Viewport")

func toggle():
  if visible:
    hide()
  else:
    show()

func show():
  var camera = viewport.get_camera()
  var position = camera.project_position(viewport.size / 2, 1.02)

  var e = camera.transform.basis.get_euler()
  print(e)

  transform.origin = position
  transform.basis = Basis(Vector3(0, e.y, 0)) # camera.transform.basis

  visible = true

func is_visible():
  return !!visible

func hide():
  visible = false

func _ready():
  viewport = get_viewport()

func _input(event):
  if Engine.has_singleton("EiffelCamera") and event is InputEventMouseButton:
    viewport.input(event)
