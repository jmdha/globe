#ifndef GISL_H
#define GISL_H

#include <stdint.h>

typedef enum gisl_kind {
	GISL_NULL        = 0,
	GISL_POINT       = 1,
	GISL_POLYLINE    = 3,
	GISL_POLYGON     = 5,
	GISL_MULTIPOINT  = 8,
} gisl_kind_t;

typedef struct gisl_point {
	double x, y;
} gisl_point_t;

typedef struct gisl_multipoint {
	double        x_min, y_min, x_max, y_max;
	size_t        point_count;
	gisl_point_t* points;
} gisl_multipoint_t;

typedef struct gisl_polygon {
	double        x_min, y_min, x_max, y_max;
	size_t        part_count;
	size_t        point_count;
	size_t *      parts;
	gisl_point_t* points;
} gisl_polygon_t;

typedef struct gisl_shape {
	gisl_kind_t kind;
	union {
		gisl_point_t      point;
		gisl_multipoint_t multipoint;
		gisl_polygon_t    polygon;
	};
} gisl_shape_t;

size_t gisl_parse(gisl_shape_t** shapes, const char* str);
void gisl_mbr(
      double* x0, double* y0, double* x1, double* y1,
      const gisl_shape_t* shapes, size_t count
);
void gisl_mbr_shape(
      double* x0, double* y0, double* x1, double* y1,
      const gisl_shape_t* shape
);
void gisl_mbr_point(
	double* x0, double* y0, double* x1, double* y1,
	const gisl_point_t* point
);
void gisl_mbr_multipoint(
	double* x0, double* y0, double* x1, double* y1,
	const gisl_multipoint_t* point
);
void gisl_mbr_polygon(
	double* x0, double* y0, double* x1, double* y1,
	const gisl_polygon_t* polygon
);

#ifdef GISL_IMPLEMENTATION

#include <stdlib.h>
#include <memory.h>

int32_t _gisl_parse_int_big(const char* str) {
	return (str[3] & 0xFF)       |
	       (str[2] & 0xFF) << 8  |
	       (str[1] & 0xFF) << 16 |
	       (str[0] & 0xFF) << 24;
}

int32_t _gisl_parse_int_little(const char* str) {
	return (str[0] & 0xFF)       |
	       (str[1] & 0xFF) << 8  |
	       (str[2] & 0xFF) << 16 |
	       (str[3] & 0xFF) << 24;
}

// TODO: Actually force parsing as little endian
double _gisl_parse_double_little(const char* str) {
	double out = 0;
	memcpy(&out, str, 8);
	return out;
}

size_t _gisl_len_from_header(const char* str) {
	return 2 * _gisl_parse_int_big(&str[24]);
}

void _gisl_parse_point(gisl_point_t* point, const char* str) {
	point->x = _gisl_parse_double_little(&str[0]);
	point->y = _gisl_parse_double_little(&str[8]);
}

void _gisl_parse_polygon(gisl_polygon_t* p, const char* str) {
	p->x_min       = _gisl_parse_double_little(&str[0]);
	p->y_min       = _gisl_parse_double_little(&str[8]);
	p->x_max       = _gisl_parse_double_little(&str[16]);
	p->y_max       = _gisl_parse_double_little(&str[24]);
	p->part_count  = _gisl_parse_int_little(&str[32]);
	p->point_count = _gisl_parse_int_little(&str[36]);
	p->parts       = malloc(p->part_count * sizeof(size_t));
	p->points      = malloc(p->point_count * sizeof(gisl_point_t));
	for (size_t i = 0; i < p->part_count; i++) {
		const size_t o   = 40;
		const size_t idx = o + 4 * i;
		p->parts[i] = _gisl_parse_int_little(&str[idx]);
	}
	for (size_t i = 0; i < p->point_count; i++) {
		const size_t o   = 40 + 4 * p->part_count;
		const size_t idx = o + 16 * i;
		_gisl_parse_point(&p->points[i], &str[idx]);
	}
}

size_t _gisl_parse_shape(gisl_shape_t* shape, const char* str) {
	shape->kind = _gisl_parse_int_little(&str[8]);
	switch (shape->kind) {
	case GISL_POINT:   _gisl_parse_point(&shape->point, &str[12]);     break;
	case GISL_POLYGON: _gisl_parse_polygon(&shape->polygon, &str[12]); break;
	default: 
		printf("Unhandled shape %d\n", shape->kind);
		exit(1);
	}
	return 8 + 2 * _gisl_parse_int_big(&str[4]);
}

size_t gisl_parse(gisl_shape_t** shapes, const char* str) {
	size_t count = 0;
	size_t len = _gisl_len_from_header(&str[0]);
	size_t offset = 100;
	while (offset < len) {
		*shapes = realloc(*shapes, (count + 1) * sizeof(gisl_shape_t));
		offset += _gisl_parse_shape(&(*shapes)[count], &str[offset]);
		count++;
	}
	return count;
}

void gisl_mbr(
	double* x0, double* y0, double* x1, double* y1,
	const gisl_shape_t* shapes, size_t count
) {
	gisl_mbr_shape(x0, y0, x1, y1, &shapes[0]);
	for (size_t i = 1; i < count; i++) {
		double s_x0, s_y0, s_x1, s_y1;
		gisl_mbr_shape(&s_x0, &s_y0, &s_x1, &s_y1, &shapes[i]);
		if (s_x0 < *x0) *x0 = s_x0;
		if (s_y0 < *y0) *y0 = s_y0;
		if (s_x1 > *x1) *x1 = s_x1;
		if (s_y1 > *y1) *y1 = s_y1;
	}
}

void gisl_mbr_shape(
      double* x0, double* y0, double* x1, double* y1,
      const gisl_shape_t* s
) {
	switch (s->kind) {
	case GISL_POINT:
		gisl_mbr_point(x0, y0, x1, y1, &s->point); break;
	case GISL_MULTIPOINT:
		gisl_mbr_multipoint(x0, y0, x1, y1, &s->multipoint); break;
	case GISL_POLYLINE:
		gisl_mbr_polygon(x0, y0, x1, y1, &s->polygon); break;
	case GISL_POLYGON:
		gisl_mbr_polygon(x0, y0, x1, y1, &s->polygon); break;
	default:
		exit(1);
	}
}

void gisl_mbr_point(
	double* x0, double* y0, double* x1, double* y1,
	const gisl_point_t* p
) {
	*x0 = p->x;
	*y0 = p->y;
	*x1 = p->x;
	*y1 = p->y;
}

void gisl_mbr_multipoint(
	double* x0, double* y0, double* x1, double* y1,
	const gisl_multipoint_t* mp
) {
	*x0 = mp->x_min;
	*y0 = mp->y_min;
	*x1 = mp->x_max;
	*y1 = mp->y_max;
}

void gisl_mbr_polygon(
	double* x0, double* y0, double* x1, double* y1,
	const gisl_polygon_t* p
) {
	*x0 = p->x_min;
	*y0 = p->y_min;
	*x1 = p->x_max;
	*y1 = p->y_max;
}

#endif
#endif
