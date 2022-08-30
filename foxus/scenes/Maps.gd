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

func _ready():
  $HTTPRequest.connect("request_completed", self, "_on_request_completed")
  $HTTPRequest.request("http://eiffel-voxels.herokuapp.com/intrinsics.json")
  get_node("LoadMap").connect("pressed", self, "load_map")
  get_node("UnloadMap").connect("pressed", self, "unload_map")
  load_map(true)

var intrinsics

func _on_request_completed(result, response_code, headers, body):
  var json = JSON.parse(body.get_string_from_utf8())
  intrinsics = json.result

  $ItemList.clear()

  if json.result != null:
    for i in json.result:
      $ItemList.add_item(i.name)

func unload_map():
  var eiffel_camera = get_node("/root/Scene/EiffelCamera")
  eiffel_camera.unloadMaps()

var fullscreen_quad : MeshInstance
var disparity_map_quad : MeshInstance

func load_map(load_default: bool = false):
  var fudge = get_node('HSlider').value
  var file = File.new()
  var file_name : String

  var selected_items = $ItemList.get_selected_items()

  if not load_default and !selected_items.empty():
    var i = $ItemList.get_selected_items()[0]

    #The output ItemNo is a list of selected items.  Use this to select
    #The matching component from the associated array, ItemListContent.

    var yaml = intrinsics[i]['content']
    print(yaml)

    file_name = "user://stereo_cam.yml"
    file.open(file_name, File.WRITE)
    file.store_line('%YAML:1.0')
    file.store_line('---')
    file.store_string(yaml)
    file.store_line('')
    file.close()
  else:
      # Select existing calibration file:
      if OS.get_name() == "Android":
          if file.open("user://stereo_cam_calibrated.yml", File.READ) == OK:
              print("User calibration not found. Loading default calibration.")
              file_name = "user://stereo_cam_calibrated.yml"
              fudge = 1
          else:
              print("User calibration found. Loading user calibration.")
              var dir = Directory.new()
              dir.copy("res://calibration/stereo_cam.yml", "user://stereo_cam.yml")
              file_name = "user://stereo_cam.yml"
              fudge = 1
      elif OS.get_name() == "X11" or OS.get_name() == "OSX":
          if file.open("res://calibration/stereo_cam_calibrated.yml", File.READ) == OK:
              print("User calibration not found. Loading default calibration.")
              file_name = "res://calibration/stereo_cam_calibrated.yml"
              fudge = 1
          else:
              print("User calibration found. Loading user calibration.")
              var dir = Directory.new()
              dir.copy("res://calibration/stereo_cam.yml", "user://stereo_cam.yml")
              file_name = "user://stereo_cam.yml"
              fudge = 1

  var eiffel_camera = get_node("/root/Scene/EiffelCamera")
  eiffel_camera.loadMaps(file_name, fudge)

  if fullscreen_quad == null:
    fullscreen_quad = get_node("/root/Scene/ARVROrigin/FullscreenQuad")
  fullscreen_quad.connect_to_camera()

  if disparity_map_quad == null:
    disparity_map_quad = get_node("/root/Scene/ARVROrigin/DisparityMapQuad")
  disparity_map_quad.connect_to_camera()
