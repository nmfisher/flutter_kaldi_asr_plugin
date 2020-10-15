typedef struct {
  double x;
  double y;
} FFIPoint;

typedef struct {
  uint8_t num_points;
  FFIPoint *point;
} FFIStroke;

typedef struct {
  uint32_t hanzi;
  float score;
} Match;

uint8_t lookupFFI(FFIStroke *strokes_ffi, uint8_t num_strokes, Match *matches, uint8_t limit);

