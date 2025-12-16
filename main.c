#include <stdio.h>
#include <math.h>
#include <time.h>

#define RGFW_IMPLEMENTATION
#include "third_party/RGFW/RGFW.h"

#define W 800           // width
#define H 800           // height
#define Z 0.00006       // zoom
#define S 0.01          // rotation speed
#define A 6378137.0     // earth major axis
#define B 6356752.3142  // earth minor axis
#define F ((A - B) / A) // ellipsoid flatness
#define E (F * (2 - F)) // square of eccentricity

typedef struct point {
	double lat, lon;
} point_t;

typedef struct polygon {
	size_t   len;
	point_t* points;
} polygon_t;

const char COUNTRIES[] = {
	#embed "10m_countries"
};

const u8 WHITE[4] = { 255, 255, 255, 255 };

void WGS84toECEF(double* x, double* y, double* z, double lat, double lon, double h) {
	const double N = A / sqrt(1 - E * pow(lat, 2));

	*x = (h + N) * cos(lat) * cos(lon);
	*y = (h + N) * cos(lat) * sin(lon);
	*z = (h + N * (1 - E)) * sin(lat);
}

void load_polygons(size_t* count, polygon_t** polygons, const char* buf) {
	memcpy(count, buf, sizeof(size_t));
	buf = buf + sizeof(size_t);
	*polygons = malloc(*count * sizeof(polygon_t));
	for (size_t i = 0; i < *count; i++) {
		polygon_t* p = &((*polygons)[i]);
		memcpy(&p->len, buf, sizeof(size_t));
		buf = buf + sizeof(size_t);
		p->points = malloc(p->len * sizeof(point_t));
		for (size_t t = 0; t < p->len; t++) {
			memcpy(&p->points[t], buf, sizeof(point_t));
			buf = buf + sizeof(point_t);
		}
	}
}

void render_point(u8* buf, int x, int y, const u8 color[4]) {
	size_t i = (H - y - 1) * (4 * W) + 4 * x;
	memcpy(&buf[i], color, 4 * sizeof(u8));
}

void render_polygon(u8* buf, const polygon_t* polygon, double r) {
	for (size_t i = 1; i < polygon->len; i++) {
		const point_t* p = &polygon->points[i];
		double x, y, z;
		WGS84toECEF(&x, &y, &z, p->lon, p->lat + r, 0);
		if (x < 0) continue;
		// Technically, this should render a line
		// However, because it is so zoomed out there is no visual difference
		render_point(buf, Z * y + W / 2, Z * z + H / 2, WHITE);
	}
}

void render_polygons(u8* buf, size_t count, const polygon_t* polygons, double r) {
	for (size_t i = 0; i < count; i++)
		render_polygon(buf, &polygons[i], r);
}

int main(int argc, char** argv) {
	size_t     polygon_count;
	polygon_t* polygons;
	load_polygons(&polygon_count, &polygons, COUNTRIES);

	u8*           buf     = malloc(4 * W * H);
	RGFW_window*  win     = RGFW_createWindow("", 0, 0, W, H, RGFW_windowCenter);
	RGFW_surface* surface = RGFW_createSurface(buf, W, H, RGFW_formatBGRA8);

	double rotation = 0;

	while (RGFW_window_shouldClose(win) == RGFW_FALSE) {
		RGFW_pollEvents();
		memset(buf, 0, 4 * W * H * sizeof(u8));
		rotation += S;
		render_polygons(buf, polygon_count, polygons, rotation);
		RGFW_window_blitSurface(win, surface);
	}

	return 0;
}
