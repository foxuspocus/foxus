shader_type spatial;
render_mode unshaded,depth_draw_never;

// Color spaces
const int YUV = 0;
const int RGB = 1;

uniform int color_space;

uniform sampler2D texture_rgb;
uniform vec2 uv_offset;

// Last n frame buffers:
uniform int current_frame_index;
uniform int frame_array_size;

uniform sampler2D texture_y;
uniform sampler2D texture_u;
uniform sampler2D texture_v;

uniform sampler2DArray rgb_frame_array : hint_albedo;

uniform sampler2DArray y_frame_array;
uniform sampler2DArray u_frame_array;
uniform sampler2DArray v_frame_array;

// Remap modes
const int GPU_REMAP = 0;
const int CPU_REMAP = 1;
const int NO_REMAP = 2;

uniform int remap_mode;
const vec3 ERROR_COLOR = vec3(1.0, 0.0, 0.0);

// Undistort maps
uniform sampler2D map_x;
uniform sampler2D map_y;

uniform sampler3D lut;

// Uniforms
uniform sampler2D depth_map;
uniform sampler2D face_map;

uniform bool display_error;
uniform sampler2D error_texture;

const int WIDTH = 1280;
const int HEIGHT = 960;
const int U_BEGIN = 2 * WIDTH * HEIGHT;
const int V_BEGIN = U_BEGIN + WIDTH * HEIGHT;

uniform int frame_diff = 9;

int get_older_index(int index) {
	return (index  + frame_array_size - frame_diff) % frame_array_size;
}

ivec2 calculateDistortedPixel(ivec2 target_pixel) {
	ivec2 distorted_pixel;
	
	distorted_pixel.x = int(round(texelFetch(map_x, target_pixel, 0).x));
	distorted_pixel.y = int(round(texelFetch(map_y, target_pixel, 0).x));
	if (distorted_pixel.x < 0 || distorted_pixel.x > (2 * WIDTH - 1) || distorted_pixel.y < 0 || distorted_pixel.y > (HEIGHT - 1)) {
		return ivec2(0);
	}
	
	return distorted_pixel;
} 

vec2 calculateDistortedUV(vec2 uv, bool right) {
	
	vec2 duv;
	ivec2 target_pixel;
	target_pixel.x = int(round(uv.x * float(WIDTH - 1)));
	target_pixel.y = int(round(uv.y * float(HEIGHT - 1))); 
	duv.x = texelFetch(map_x, target_pixel, 0).x / float(WIDTH - 1);
	duv.y = texelFetch(map_y, target_pixel, 0).x / float(HEIGHT - 1);
	if (duv.x < 0.0 || duv.x > 1.0 || duv.y < 0.0 || duv.y > 1.0) {
		return vec2(-1);
	}
		
	return duv;
}

vec3 ghostingEffectRGB(sampler2DArray frames, vec2 uv, int index){
	vec3 current_frame_pixel = texelFetch(frames, ivec3(int(round(uv.x * float(WIDTH * 2 - 1))), int(round(uv.y * float(HEIGHT - 1))), index), 0).rgb;
	int older_index = get_older_index(index);
	if (older_index == index) {
		return current_frame_pixel;
	}
	vec3 oldest_frame_pixel = texelFetch(frames, ivec3(int(round(uv.x * float(WIDTH * 2 - 1))), int(round(uv.y * float(HEIGHT - 1))), older_index), 0).rgb;
	return mix(current_frame_pixel.rgb, oldest_frame_pixel.rgb, 0.5);
}

vec4 texelFromFrameArray(sampler2DArray frames, int index, int x, int y){
	return texelFetch(frames, ivec3(x, y, index), 0);
}

vec3 getYUVFrameColor(int frame_index, vec2 uv) {
	int x = int(round(uv.x * float(2 * WIDTH - 1)));
	int y = int(round(uv.y * float(HEIGHT - 1)));

	float Y, U, V;
	
	if (frame_diff == 0) {
		Y = texelFetch(texture_y, ivec2(x, y), 0).x;
		U = texelFetch(texture_u, ivec2(x / 2, y), 0).x;
		V = texelFetch(texture_v, ivec2(x / 2, y), 0).x;
	} else {
		Y = texelFromFrameArray(y_frame_array, frame_index, x, y).x;
		U = texelFromFrameArray(u_frame_array, frame_index, x / 2, y).x;
		V = texelFromFrameArray(v_frame_array, frame_index, x / 2, y).x;
	}

    Y -= 16.0 / 255.0;
    U -= 0.5;
    V -= 0.5;

    float R = clamp(Y + 1.402 * V, 0.0, 1.0);
    float G = clamp(Y - 0.34414 * U - 0.71414 * V, 0.0, 1.0);
    float B = clamp(Y + 1.772 * U, 0.0, 1.0);

    return vec3(R, G, B);
}

vec3 ghostingEffectYUV(vec2 uv, int index){
	vec3 current_frame_pixel = getYUVFrameColor(index, uv);
	int older_index = get_older_index(index);
	if (older_index == index) {
		return current_frame_pixel;
	}
	vec3 oldest_frame_pixel = getYUVFrameColor(older_index, uv);
	return mix(current_frame_pixel.rgb, oldest_frame_pixel.rgb, 0.5);
}

const mediump vec3 luminanceWeighting = vec3(0.2125, 0.5854, 0.0721);
const float saturation = 1.1;
const float minC = 2.0 / 64.;
const float maxC = 1.0 - (2.0 / 64.);

vec3 get_color_from_eye(vec2 uv, bool right) {
	vec2 nudge = vec2(0., 0.);
	vec3 c = vec3(0.2);
	
	vec2 norm_UV = uv;
	
	norm_UV.x = norm_UV.x / 2.0;
	
	if (right) {
		norm_UV.x += 0.5;
	}
	
	if (color_space == YUV) {
		if (remap_mode == NO_REMAP) {
			c = ghostingEffectYUV(norm_UV - nudge, current_frame_index).rgb;
		} else if (remap_mode == GPU_REMAP) {
			vec2 uvc = calculateDistortedUV(uv, right);
			if (uvc.x < 0.0) {
				c = ERROR_COLOR;
			} else {
				
				uvc.x /= 2.0;
				if (right) {
					uvc.x += 0.5;
				}
				
				c = ghostingEffectYUV(uvc, current_frame_index).rgb;
			}
		}
	} else if (color_space == RGB) {
		if (remap_mode == NO_REMAP || remap_mode == CPU_REMAP){
			c = ghostingEffectRGB(rgb_frame_array, norm_UV, current_frame_index);						//Demonstrating the last n frame array.
		}
		else if (remap_mode == GPU_REMAP){
			vec2 uvc = calculateDistortedUV(uv, right);
			if (uvc.x < 0.0) {
				c = ERROR_COLOR;
			} else {
				
				uvc.x /= 2.0;
				if (right) {
					uvc.x += 0.5;
				}
				
				c = ghostingEffectRGB(rgb_frame_array, uvc, current_frame_index);						//Demonstrating the last n frame array.
			}
		}
	}
	return c;
}

void fragment() {
	vec3 c;
	vec2 nudge = vec2(0., 0.);
	vec2 uv = UV;

	uv += uv_offset;

	if (display_error == true) {
		c = texture(error_texture, uv).rgb;
	} else {
		c = get_color_from_eye(uv, false); // PROJECTION_MATRIX[2][0] > 0.0);
	}

	// c.b = fract(TIME);
	

	// c = c + (c - c2);
	// c = c2;
	
	float luminance = dot(c, luminanceWeighting);
    vec3 greyScaleColor = vec3(luminance);

	c = vec3(mix(greyScaleColor, c, saturation));

	c.x = clamp(c.x, minC, maxC);
	c.y = clamp(c.y, minC, maxC);
	c.z = clamp(c.z, minC, maxC);
	// c.y = 1.0 - c.y;
	ALBEDO = texture(lut, c).xyz;
	// ALBEDO.x = UV.x;
	// ALBEDO.y = VERTEX.y;

	
	// ALBEDO = c;
}
