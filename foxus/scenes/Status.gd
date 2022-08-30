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

onready var label: Label = get_node("ConnectionStatusContainer/Label")
onready var rich_text_label: RichTextLabel = get_node("ConnectionStatusContainer/RichTextLabel")
onready var button: Button = get_node("Button")

enum CONNECTION_STATUS {NOT_CONNECTED, ATTACHED, CONNECTED}

const STATUS_TO_MSG : Array = [
  "[color=#FF0000]Not connected[/color]",
  "[color=#0000FF]Attached[/color]",
  "[color=#00FF00]Connected[/color]"
]

onready var eiffel_camera = get_node("/root/Scene/EiffelCamera")

func _ready():
  eiffel_camera.connect("camera_status_changed", self, "on_camera_status_changed")
  button.connect("pressed", self, "on_ask_for_permission_pressed")

func on_camera_status_changed(new_status):
  rich_text_label.bbcode_text = STATUS_TO_MSG[new_status]

  button.disabled = (new_status != CONNECTION_STATUS.ATTACHED)

func on_ask_for_permission_pressed():
  eiffel_camera.connect_to_camera()
  button.disabled = true
