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

onready var eiffel_camera : Node = get_node("/root/Scene/EiffelCamera")

onready var left_image : TextureRect = get_node("LeftImage")
onready var right_image : TextureRect = get_node("RightImage")

onready var discard_button : Button = get_node("Buttons/DiscardButton")
onready var accept_button : Button = get_node("Buttons/AcceptButton")

onready var user_notification_quad : MeshInstance = get_node("/root/Scene/ARVROrigin/UserNotificationQuad")

onready var vrui : MeshInstance = get_node("/root/Scene/VRUI")

onready var tab_container : TabContainer = get_parent()

var tab_index

func _ready():
  eiffel_camera.connect("chessboard_detected", self, "on_chessboard_detected")
  
  discard_button.connect("pressed", self, "on_discard_pressed")
  accept_button.connect("pressed", self, "on_accept_pressed")
  
  tab_index = get_parent().get_children().find(self)
  
func on_accept_pressed():
  eiffel_camera.accept_calibration_image()
  remove_images()
  show_message()
  vrui.hide()
  eiffel_camera.set_image_on_hold(false);

func on_discard_pressed():
  remove_images()
  show_message()
  vrui.hide()
  eiffel_camera.set_image_on_hold(false);

func show_message():
  var calibration_images_left = eiffel_camera.calibration_images_left()
  if calibration_images_left > 0:
    user_notification_quad.popup("Take " + str(calibration_images_left) + " more pictures for calibration.")

func remove_images():
  discard_button.disabled = true
  accept_button.disabled = true
  
  left_image.texture = null
  right_image.texture = null
    
func on_chessboard_detected(left, right):
  discard_button.disabled = false
  accept_button.disabled = false
  eiffel_camera.set_image_on_hold(true)

  if left == null:
    print("ERROR: LEFT CALIBRATION IMAGE IS NULL")
  if right == null:
    print("ERROR: RIGHT CALIBRATION IMAGE IS NULL")
  
  left_image.texture = left
  right_image.texture = right

  vrui.show()
  tab_container.current_tab = tab_index
