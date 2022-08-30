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

const DISTANCE = 49.9
onready var notification_viewport = get_node("NotificationViewport")
onready var notification_message = get_node("NotificationViewport/UserNotificationDialog/NotificationMessage")
#onready var fullscreen_quad = get_node("/root/Scene/ARVROrigin/FullscreenQuad")
onready var timer = get_node("PopupTimer")

func _ready():
  get_active_material(0).set_texture(0, notification_viewport.get_texture())
  self.visible = false
  timer.process_mode = Timer.TIMER_PROCESS_PHYSICS
  timer.connect("timeout", self, "_on_timer_timeout")

var user_notification_quad_transform = Transform()

func on_frame_start():
  var spatial_viewport = get_viewport()
  var camera = spatial_viewport.get_camera()
  
  user_notification_quad_transform.origin = camera.project_position(spatial_viewport.size / 2, DISTANCE)
  user_notification_quad_transform.basis = camera.transform.basis

func on_frame_end():
  if user_notification_quad_transform:
    transform.origin = user_notification_quad_transform.origin
    transform.basis = user_notification_quad_transform.basis

func popup(message: String, waiting_time: int = 0):
  notification_message.set_text(message)
  self.visible = true
  
  if waiting_time > 0:
    timer.set_paused(false)
    timer.start(waiting_time)
  else:
    timer.set_paused(true)

func _process(delta):
  if visible:
    on_frame_start()
    on_frame_end()

func _on_timer_timeout():
  self.visible = false
