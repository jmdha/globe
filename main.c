#include <stdio.h>
#include <math.h>

#define GEOL_SHAPEFILE_IMPLEMENTATION
#include <geol/geol_shapefile.h>

#define GEOL_PROJ_IMPLEMENTATION
#include <geol/geol_proj.h>

#define RGFW_IMPLEMENTATION
#include <RGFW/RGFW.h>

#include "io.h"

#define WIDTH    1920
#define HEIGHT   1080

#define ZOOM_MIN 0.00001
#define ZOOM_MAX 100
#define ZOOMS    100

const u8 WHITE[4] = { 255, 255, 255, 255 };
const u8 BLACK[4] = {   0,   0,   0, 255 };

u8*           BUF;
RGFW_window*  WIN;
RGFW_surface* SURFACE;

double RX = 0, RY = 0;
size_t ZOOM_STEP = 1;
double ZOOM = 1;

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

void render_clear(void) {
	memset(BUF, 0, WIDTH * HEIGHT * 4 * sizeof(u8));
}

void render_point(int x, int y, const u8 color[4]) {
	if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT)
		return;
	i32 c = (HEIGHT - y - 1) * (4 * WIDTH) + 4 * x;
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

void render_polygon(const double* px, const double* py, size_t count, const u8 color[4]) {
	for (size_t i = 1;; i++) {
		double x, y, z;
		WGS84toECEF_deg(&x, &y, &z, px[i] + RX, py[i] + RY, 0);
		if (x > 0)
			render_point(
				ZOOM * z + WIDTH / 2,
				ZOOM * y + HEIGHT / 2,
				color
			);
		if (px[i] == px[0] && py[i] == py[0])
			break;
	}
}

void render_map(const geol_record_t* records, size_t record_count) {
	for (size_t i = 0; i < record_count; i++) {
		const geol_record_t* record = &records[i];
		for (size_t t = 0; t < record->part_count; t++) {
			const size_t start   = record->parts[t];
			render_polygon(
				&record->px[start], &record->py[start],
				record->point_count,
				WHITE
			);
		}
	}
}

void input(double dt) {
	RGFW_pollEvents();
	float scroll_x, scroll_y;
	RGFW_getMouseScroll(&scroll_x, &scroll_y);
	if (scroll_y > 0 && ZOOM_STEP < ZOOMS) ZOOM_STEP++;
	if (scroll_y < 0 && ZOOM_STEP > 0)     ZOOM_STEP--;
	const float log_min = log(ZOOM_MIN);
	const float log_max = log(ZOOM_MAX);
	const float log_zoom = log_min + (log_max - log_min) * ZOOM_STEP / ZOOMS;
	ZOOM = exp(log_zoom);
	if (RGFW_isKeyDown(RGFW_a)) RX += 0.001 * dt / ZOOM;
	if (RGFW_isKeyDown(RGFW_d)) RX -= 0.001 * dt / ZOOM;
	if (RGFW_isKeyDown(RGFW_s)) RY += 0.001 * dt / ZOOM;
	if (RGFW_isKeyDown(RGFW_w)) RY -= 0.001 * dt / ZOOM;
}

void render(const geol_record_t* records, size_t record_count) {
	render_clear();
	render_map(records, record_count);
	RGFW_window_blitSurface(WIN, SURFACE);
}

int main(int argc, char** argv) {
	WIN     = RGFW_createWindow("", 0, 0, WIDTH, HEIGHT, RGFW_windowCenter);
	BUF     = malloc(WIDTH * HEIGHT * 4);
	SURFACE = RGFW_createSurface(BUF, WIDTH, HEIGHT, RGFW_formatRGBA8);
	RGFW_window_setExitKey(WIN, RGFW_escape);

	char** str = f_open("ne_10m_land.shp");

	geol_record_t* records = NULL;
	size_t         record_count = geol_shp_decode(&records, *str);
	f_close(str);

	dt_get();
	while (RGFW_window_shouldClose(WIN) == RGFW_FALSE) {
		const double dt = dt_get();
		input(dt);
		render(records, record_count);
	}

	return 0;
}
