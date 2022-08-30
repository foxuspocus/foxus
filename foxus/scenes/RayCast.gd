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

extends RayCast

var viewport

var viewport_point

var blueAlbedo = SpatialMaterial.new()
var whiteAlbedo = SpatialMaterial.new()

var vrui : MeshInstance
var controller : ARVRController
var eiffel_camera : Node
onready var user_notification_quad : MeshInstance = get_node("/root/Scene/ARVROrigin/UserNotificationQuad")

func _ready():
  whiteAlbedo.set_albedo(Color(255, 255, 255, 144))
  blueAlbedo.set_albedo(Color(0, 37, 147, 144))

  if is_left():
    $MeshInstance.visible = false

func is_left():
  return self.get_parent().controller_id == 1

func is_right():
  return !is_left()

var menu_pressed = false

const button_press_delay = 0.2
var current_button_press_delay = 0

var trigger_pressed = false

func _process(_delta):
  if vrui == null:
    vrui = find_parent("Scene").get_node("VRUI")
  if controller == null:
    controller = get_parent()
  if eiffel_camera == null:
    eiffel_camera = find_parent("Scene").get_node("EiffelCamera")

  var quad = get_node("/root/Scene/ARVROrigin/FullscreenQuad")

  var c = Vector3(controller.get_joystick_axis(0), controller.get_joystick_axis(1), 0)

  if c.length() > 0.1:
    self.transform.origin += c * 0.002


  if is_left():
    return

  if get_parent().is_button_pressed(JOY_OCULUS_AX):
    if !eiffel_camera.is_in_calibration_mode():
      user_notification_quad.popup("Images can only be taken in calibration mode.", 5)
    elif eiffel_camera.is_image_on_hold():
      user_notification_quad.popup("Accept or discard the image on the 'Images' tab.")
    else:
      var calibration_images_left = eiffel_camera.calibration_images_left()

      if eiffel_camera.take_picture():
        if calibration_images_left > 0:
          user_notification_quad.popup("Accept or discard the image on the 'Images' tab.")
        current_button_press_delay = button_press_delay
      else:
        if calibration_images_left > 0:
          user_notification_quad.popup("Invalid picture. Try again.")

  if !controller.is_button_pressed(JOY_OCULUS_BY) && menu_pressed:
    vrui.toggle()
    menu_pressed = false

  if get_parent().is_button_pressed(JOY_OCULUS_BY):
    menu_pressed = true

  if current_button_press_delay > 0:
    current_button_press_delay -= _delta
    return

  if !vrui.is_visible():
    return

  if get_parent().is_button_pressed(JOY_VR_TRIGGER):
    var raycast_collider = get_collider()
    _try_send_input_to_gui(raycast_collider)
    if viewport_point != null:
      pressTrigger()
      trigger_pressed = true
  elif trigger_pressed:
    if viewport_point != null:
      releaseTrigger()
      trigger_pressed = false


func _try_send_input_to_gui(raycast_collider):

  if raycast_collider == null:
    return

  viewport = raycast_collider.find_parent("Scene").get_node("ViewportContainer/Viewport")
  if not (viewport is Viewport):
    viewport = null
    print('fucken shit auw')
    return # This isn't something we can give input to.

  var collider_transform = raycast_collider.global_transform
  if collider_transform.xform_inv(global_transform.origin).z < 0:
    return # Don't allow pressing if we're behind the GUI.

  # Convert the collision to a relative position. Expects the 2nd child to be a CollisionShape.
  var shape_size = raycast_collider.get_node("CollisionShape").shape.extents * 2
  var collision_point = get_collision_point()
  var collider_scale = collider_transform.basis.get_scale()
  var local_point = collider_transform.xform_inv(collision_point)
  local_point /= (collider_scale * collider_scale)
  local_point /= shape_size
  local_point += Vector3(0.5, -0.5, 0) # X is about 0 to 1, Y is about 0 to -1.

  # Find the viewport position by scaling the relative position by the viewport size. Discard Z.
  viewport_point = Vector2(local_point.x, -local_point.y) * viewport.size

func pressTrigger():
  var event = InputEventMouseButton.new()
  event.button_index = BUTTON_LEFT
  event.position = viewport_point
  event.pressed = true
  viewport.input(event)

func releaseTrigger():
  var event = InputEventMouseButton.new()
  event.button_index = BUTTON_LEFT
  event.position = viewport_point
  event.pressed = false
  viewport.input(event)
