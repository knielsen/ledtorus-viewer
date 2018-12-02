#include <stdlib.h>
#include <stdio.h>

#include <QGLWidget>
#include <QVector3D>

#include <qmath.h>
#include <QObject>
#include <QColor>

#include "io.h"
#include "ledtorus.h"


/*
  Frame buffer. ToDo: use read frames from io instead.
  Indexed as r_or_g_or_b_or_alpha + 4*(y + x*LEDS_Y + a*(LEDS_Y*LEDS_X)).
  Doplet (samme data to gange efter hinanden), så den kan give farve
  both to the starting and ending vertex of a line segment.
*/
static uint8_t *framebuf;
/*
  Vertex buffer for line segments. First all of the starting vertices,
  then all of the ending vertices, to match with raw colour framebuffer.
*/
static float *torus_line_vertices;
/* Indices into framebuf/torus_line_vertices. */
static uint32_t *torus_line_indices;


static QVector<QVector3D> vertices;
static QVector<QVector3D> normals;
static QVector<GLushort> faces;

static float col_red[4] = { 0.55, 0.025, 0.025};

static void
add_face(QVector3D a, QVector3D b, QVector3D c)
{
  QVector3D normal = QVector3D::normal(a, b, c);
  faces.append(vertices.count());
  vertices.append(a);
  normals.append(normal);
  faces.append(vertices.count());
  vertices.append(b);
  normals.append(normal);
  faces.append(vertices.count());
  vertices.append(c);
  normals.append(normal);
}

/* Number of indices for torus line vertexes. */
static int cnt_torus_lines;


void
build_geometry()
{
  /* Allocate memory. */
  framebuf =
    (uint8_t *)malloc(4*LEDS_X*LEDS_Y*LEDS_TANG*2);
  torus_line_vertices =
    (float *)malloc(sizeof(*torus_line_vertices)*2*3*LEDS_X*LEDS_Y*LEDS_TANG);
  torus_line_indices =
    (uint32_t *)malloc(sizeof(*torus_line_indices)*2*LEDS_X*LEDS_Y*LEDS_TANG);
  if (!framebuf || !torus_line_vertices || !torus_line_indices) {
    fprintf(stderr, "Error: Failed to allocate memory for OpenGL buffers\n");
    exit(1);
  }

  /* The red base. */
  QVector3D t1(-0.405, -0.36, -0.405);
  QVector3D t2(-0.405, -0.36, 0.405);
  QVector3D t3(0.405, -0.36, -0.405);
  QVector3D t4(0.405, -0.36, 0.405);
  QVector3D b1(-0.405, -0.495, -0.405);
  QVector3D b2(-0.405, -0.495, 0.405);
  QVector3D b3(0.405, -0.495, -0.405);
  QVector3D b4(0.405, -0.495, 0.405);
  add_face(t1,t2,t3);
  add_face(t3,t2,t4);
  add_face(t2,t1,b1);
  add_face(b2,t2,b1);
  add_face(t1,b3,b1);
  add_face(t1,t3,b3);
  add_face(t3,t4,b4);
  add_face(t3,b4,b3);
  add_face(t4,t2,b2);
  add_face(b2,b4,t4);
  add_face(b1,b3,b2);
  add_face(b3,b4,b2);

  /* Line segments for the LED torus. */
  static const float mm_to_world_factor = 0.54/47.19*(ledtorus_ver == 1 ? 1.0 : 0.7);
  static const float led_dist_mm = (ledtorus_ver == 1 ? 5.5 : 4.0);
  static const float inner_led_to_center_mm = (ledtorus_ver == 1 ? 14.19 : 19.11);
  uint32_t *p = torus_line_indices;
  for (unsigned k = 0; k < LEDS_TANG; ++k)
  {
    float angle1 = 2.0*M_PI*(1.0f-(float)k/(float)LEDS_TANG);
    float angle2 = 2.0*M_PI*(1.0f-(float)(k+1)/(float)LEDS_TANG);

    for (unsigned i= 0; i < LEDS_X; ++i)
    {
      float dist = (inner_led_to_center_mm + led_dist_mm*i)*mm_to_world_factor;

      for (unsigned j= 0; j < LEDS_Y; ++j)
      {
        float height = led_dist_mm*((float)(LEDS_Y-1)/2.0 - (float)j)*mm_to_world_factor;
        if (ledtorus_ver == 1) {
          if ( (i == 0 && (j < 2 || j > 5)) ||
               ((i == 1 || i == 6) && (j == 0 || j == 7)))
            continue;
        } else {
          if ( (i == 0 && (j < 6 || j > 9)) ||
               (i < 5 && (j < 5-i || j > i+10)) )
            continue;
        }
        uint32_t idx = j + i*LEDS_Y + k *(LEDS_X*LEDS_Y);
        torus_line_vertices[3*idx] = dist*sinf(angle1);
        torus_line_vertices[3*idx+1] = height;
        torus_line_vertices[3*idx+2] = dist*cosf(angle1);
        torus_line_vertices[3*idx+(3*LEDS_X*LEDS_Y*LEDS_TANG)] = dist*sinf(angle2);
        torus_line_vertices[3*idx+(3*LEDS_X*LEDS_Y*LEDS_TANG+1)] = height;
        torus_line_vertices[3*idx+(3*LEDS_X*LEDS_Y*LEDS_TANG+2)] = dist*cosf(angle2);
        *p++ = idx;
        *p++ = idx+LEDS_X*LEDS_Y*LEDS_TANG;
      }
    }
  }

  cnt_torus_lines = 2*LEDS_X*LEDS_Y*LEDS_TANG;
}


static uint8_t
gamma_correct(uint8_t rgb_component)
{
  static const float gamma = 0.6f;
  static float normalise = 0.0f;
  if (rgb_component == 0)
    return 0;
  if (normalise == 0.0f)
    normalise = 255.0f / powf(255.0f, gamma);
  return (uint8_t)roundf(normalise*powf(rgb_component, gamma));
}


static void
get_led_colours(const uint8_t *frame)
{
  uint32_t i = 0, j = 0;
  while (i < 3*LEDS_X*LEDS_Y*LEDS_TANG)
  {
    uint8_t c_r = framebuf[j++] = gamma_correct(frame[i++]);
    uint8_t c_g = framebuf[j++] = gamma_correct(frame[i++]);
    uint8_t c_b = framebuf[j++] = gamma_correct(frame[i++]);
    /* Make turned-off led transparent, others opaque. */
    framebuf[j++] = ((c_r || c_g || c_b) ? 255 : 0);
  }
  /* Double the colour values. */
  memcpy(&framebuf[j], &framebuf[0], 4*LEDS_X*LEDS_Y*LEDS_TANG);
}

void
draw_ledtorus()
{
  const uint8_t *frame;

  glDisable(GL_LIGHTING);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glVertexPointer(3, GL_FLOAT, 0, torus_line_vertices);
  glEnableClientState(GL_VERTEX_ARRAY);
  frame= get_current_frame();
  get_led_colours(frame);
  release_frame();
  glColorPointer(4, GL_UNSIGNED_BYTE, 0, framebuf);
  glEnableClientState(GL_COLOR_ARRAY);
  glLineWidth(ledtorus_ver == 1 ? 4.0 : 3.0);
  glDrawElements(GL_LINES, cnt_torus_lines, GL_UNSIGNED_INT, torus_line_indices);
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisable(GL_BLEND);
  glEnable(GL_LIGHTING);

  glVertexPointer(3, GL_FLOAT, 0, vertices.constData());
  glEnableClientState(GL_VERTEX_ARRAY);
  glNormalPointer(GL_FLOAT, 0, normals.constData());
  glEnableClientState(GL_NORMAL_ARRAY);
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col_red);
  //glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, faces.constData());
  glDisableClientState(GL_NORMAL_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
}
