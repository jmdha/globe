#include <stdio.h>
#include <math.h>

#define GISL_IMPLEMENTATION
#include "gisl.h"

#define RGFW_NO_X11
#define RGFW_IMPLEMENTATION
#include "RGFW.h"

#include "io.h"

#define WIDTH    1920
#define HEIGHT   1080

#define RADIUS   400

#define ZOOM_MIN 1
#define ZOOM_MAX 1000
#define ZOOMS    10

const uint8_t WHITE[4] = { 255, 255, 255, 255 };
const uint8_t BLACK[4] = {   0,   0,   0, 255 };

enum projection {
	FLAT,
	SPHERE
};

u8*           BUF;
RGFW_window*  WIN;
RGFW_surface* SURFACE;

gisl_shape_t* SHAPES = NULL;
size_t        SHAPE_COUNT = 0;

enum projection PROJECTION = SPHERE;

double x_min, y_min, x_max, y_max;

double rot_x = 0, rot_y = 0;
double cam_x = 0, cam_y = 0;
size_t zoom_step = 1;
double zoom = 1;

double ts(void) {
	struct timespec t;
	timespec_get(&t, TIME_UTC);
	return (double) t.tv_sec * 1e9 + t.tv_nsec;
}

double dt_get(void) {
	static double t0;
	const  double t1 = ts();
	const  double v  = (t1 - t0) / 1e9;
	              t0 = t1;
	return v;
}

void hsv2rgb(u8* r, u8* g, u8* b, float H, float S, float V) {
	float _r, _g, _b;
	
	float h = H / 360;
	float s = S / 100;
	float v = V / 100;
	
	int i = floor(h * 6);
	float f = h * 6 - i;
	float p = v * (1 - s);
	float q = v * (1 - f * s);
	float t = v * (1 - (1 - f) * s);
	
	switch (i % 6) {
		case 0: _r = v, _g = t, _b = p; break;
		case 1: _r = q, _g = v, _b = p; break;
		case 2: _r = p, _g = v, _b = t; break;
		case 3: _r = p, _g = q, _b = v; break;
		case 4: _r = t, _g = p, _b = v; break;
		case 5: _r = v, _g = p, _b = q; break;
	}
	
	*r = _r * 255;
	*g = _g * 255;
	*b = _b * 255;
}

void render_clear(void) {
	memset(BUF, 0, WIDTH * HEIGHT * 4 * sizeof(u8));
}

void render_point(int x, int y, const u8 color[4]) {
	if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT)
		return;
	i32 c = (HEIGHT - y - 1) * (4 * WIDTH) + 4 * x;
	//i32 c = y * (4 * WIDTH) + 4 * x;
	memcpy(&BUF[c], color, 4 * sizeof(u8));
}

void render_line(int x0, int y0, int x1, int y1, const u8 color[4]) {
	if (
		(x0 < 0 && x1 < 0) || (x0 > WIDTH && x1 > WIDTH) ||
		(y0 < 0 && y1 < 0) || (y0 > HEIGHT && y1 > HEIGHT)
	)
		return;
	const float h  = sqrt(pow(x1 - x0, 2) + pow(y1 - y0, 2));
	const float mx = 1 / h * (x1 - x0);
	const float my = 1 / h * (y1 - y0);

	for (size_t i = 0; i < h; i++) {
		i32 x = x0 + i * mx;
		i32 y = y0 + i * my;
		render_point(x, y, color);
	}
}

void render_arc(int x0, int y0, int x1, int y1, const u8 color[4]) {
	const float h  = sqrt(pow(x1 - x0, 2) + pow(y1 - y0, 2));
	const float mx = 1 / h * (x1 - x0);
	const float my = 1 / h * (y1 - y0);

	for (size_t i = 0; i < h; i++) {
		i32 x = x0 + i * mx;
		i32 y = y0 + i * my;
		render_point(x, y, color);
	}
}


int map(double v, double v_min, double v_max, double o_min, double o_max) {
	return ((v - v_min) / (v_max - v_min)) * (o_max - o_min) + o_min;
}

void render_polygon(const gisl_point_t* points, const u8 color[4]) {
	for (size_t i = 1;; i++) {
		const double x0 = WIDTH / 2  + cam_x + points[i - 1].x;
		const double y0 = HEIGHT / 2 + cam_y + points[i - 1].y;
		const double x1 = WIDTH / 2  + cam_x + points[i].x;
		const double y1 = HEIGHT / 2 + cam_y + points[i].y;
		render_line(x0, y0, x1, y1, color);
		if (points[i].x == points[0].x && points[i].y == points[0].y)
			break;
	}
}

void render_polygon_arc(const gisl_point_t* points, const u8 color[4]) {
	for (size_t i = 1;; i++) {
		const float _x = points[i].x;
		const float _y = points[i].y;
		const float ay = _x > 0 ? _x : 180 + (180 - fabs(_x));
		const float ax = _y > 0 ? _y : 90  + (90  - fabs(_y));
		const float x  = RADIUS * sin(ax + sin(rot_x)) * cos(ay);
		const float y  = RADIUS * sin(ax - sin(rot_x)) * sin(ay);
		const float z  = RADIUS * sin(ay);
		if (z > 0)
			render_point(x + WIDTH / 2, y + HEIGHT / 2, color);
		if (points[i].x == points[0].x && points[i].y == points[0].y)
			break;
	}
}

void render_map(size_t count, const gisl_shape_t* shapes) {
	for (size_t i = 0; i < 1; i++) {
		const gisl_shape_t* shape = &shapes[i];
		const gisl_polygon_t* polygon = &shape->polygon;
		u8 color[4] = { 0, 0, 0, 255, };
		hsv2rgb(&color[0], &color[1], &color[2], (float) i / count * 360, 100, 100);
		for (size_t t = 1; t < 2; t++) {
			const size_t start   = polygon->parts[t];
			if (PROJECTION == FLAT)
			render_polygon(&polygon->points[start], color);
			else
			render_polygon_arc(&polygon->points[start], color);
		}
	}
}

void input(double dt) {
	RGFW_pollEvents();
	// Projection
	if (RGFW_isKeyPressed(RGFW_F1)) PROJECTION = FLAT;
	if (RGFW_isKeyPressed(RGFW_F2)) PROJECTION = SPHERE;
	// Zoom
	float scroll_x, scroll_y;
	RGFW_getMouseScroll(&scroll_x, &scroll_y);
	if (scroll_y > 0 && zoom_step < ZOOMS) zoom_step++;
	if (scroll_y < 0 && zoom_step > 0)     zoom_step--;
	const float log_min = log(ZOOM_MIN);
	const float log_max = log(ZOOM_MAX);
	const float log_zoom = log_min + (log_max - log_min) * zoom_step / ZOOMS;
	zoom = exp(log_zoom);
	// Pan
	if (RGFW_isKeyDown(RGFW_a)) cam_x += 100 * dt / zoom;
	if (RGFW_isKeyDown(RGFW_d)) cam_x -= 100 * dt / zoom;
	if (RGFW_isKeyDown(RGFW_s)) cam_y += 100 * dt / zoom;
	if (RGFW_isKeyDown(RGFW_w)) cam_y -= 100 * dt / zoom;
}

void render(void) {
	render_clear();
	render_map(SHAPE_COUNT, SHAPES);
	RGFW_window_blitSurface(WIN, SURFACE);
}

int main(int argc, char** argv) {
	WIN     = RGFW_createWindow("", 0, 0, WIDTH, HEIGHT, RGFW_windowCenter);
	BUF     = malloc(WIDTH * HEIGHT * 4);
	SURFACE = RGFW_createSurface(BUF, WIDTH, HEIGHT, RGFW_formatRGBA8);
	RGFW_window_setExitKey(WIN, RGFW_escape);

	char** str = f_open("ne_10m_land.shp");
	dt_get();
	SHAPE_COUNT = gisl_parse(&SHAPES, *str);
	f_close(str);
	gisl_mbr(&x_min, &y_min, &x_max, &y_max, SHAPES, SHAPE_COUNT);
	for (size_t i = 0; i < SHAPE_COUNT; i++)
		printf("%zu: %zu\n", i, SHAPES[i].polygon.part_count);

	while (RGFW_window_shouldClose(WIN) == RGFW_FALSE) {
		const double dt = dt_get();
		input(dt);
		render();
		rot_x += dt;
		rot_y += dt;
	}

	return 0;
}
