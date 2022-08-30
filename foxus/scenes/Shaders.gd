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

var DISCOVERY_PORT = 42069
var udp_peer = PacketPeerUDP.new()

func dir_contents(path):
	var results = []
	var dir = Directory.new()
	if dir.open(path) == OK:
		dir.list_dir_begin()
		var file_name = dir.get_next()
		while file_name != "":
			if !dir.current_is_dir() and file_name.ends_with("png.import"):
				results.append(file_name.rstrip(".import"))
			file_name = dir.get_next()
	else:
		print("An error occurred when trying to access the path.")

	results.sort()

	return results

var luts = []

func _ready():
	$ItemList.clear()
	luts = dir_contents("res://luts/")
	for i in luts:
		$ItemList.add_item(i)
	get_node("ItemList").connect("item_selected", self, "load_shader")

var shaders

func _on_request_completed(result, response_code, headers, body):
	var json = JSON.parse(body.get_string_from_utf8())
	shaders = json.result

	$ItemList.clear()

	if json.result != null:
		for i in json.result:
			$ItemList.add_item('$' + i.price + ' - ' + i.name)

func load_shader(i):
	var fn = luts[i]

	var mesh = get_node("/root/Scene/ARVROrigin/FullscreenQuad")
	mesh.set_lut_filename('res://luts/' + fn)
