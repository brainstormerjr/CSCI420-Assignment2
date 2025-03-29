/*
  CSCI 420 Computer Graphics, Computer Science, USC
  Assignment 1: Height Fields with Shaders.
  C/C++ starter code

  Student username: <type your USC username here>
*/

#include "openGLHeader.h"
#include "glutHeader.h"
#include "openGLMatrix.h"
#include "imageIO.h"
#include "pipelineProgram.h"
#include "vbo.h"
#include "vao.h"

#include <iostream>
#include <cstring>

#if defined(WIN32) || defined(_WIN32)
  #ifdef _DEBUG
    #pragma comment(lib, "glew32d.lib")
  #else
    #pragma comment(lib, "glew32.lib")
  #endif
#endif

#if defined(WIN32) || defined(_WIN32)
  char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
  char shaderBasePath[1024] = "../openGLHelper";
#endif

using namespace std;

int mousePos[2]; // x,y screen coordinates of the current mouse position

int leftMouseButton = 0; // 1 if pressed, 0 if not 
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

// Transformations.
float worldRotate[3] = { 0.0f, 0.0f, 0.0f }; 
// worldRotate[0] gives the rotation around x-axis (in degrees)
// worldRotate[1] gives the rotation around y-axis (in degrees)
// worldRotate[2] gives the rotation around z-axis (in degrees)
float worldTranslate[3] = { 0.0f, 0.0f, 0.0f };
float worldScale[3] = { 1.0f, 1.0f, 1.0f };

// Width and height of the OpenGL window, in pixels.
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 Homework 2";

// Number of vertices in the single triangle (starter code).
int numVertices;

// CSCI 420 helper classes.
OpenGLMatrix matrix;
PipelineProgram * pipelineProgram = nullptr;
VBO * vboVertices = nullptr;
VBO * vboColors = nullptr;
VAO * vao = nullptr;

// Coordinate system information
float* coordinates = nullptr;

// Camera speed
const float speed = 0.001f;
int frameNumber = 0;

// Cross section calculation
const float root32 = 0.8660254038f;
float csScale = 1.0f;

// Write a screenshot to the specified filename.
void saveScreenshot(const char * filename)
{
  unsigned char * screenshotData = new unsigned char[windowWidth * windowHeight * 3];
  glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData);

  ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

  if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
    cout << "File " << filename << " saved successfully." << endl;
  else cout << "Failed to save file " << filename << '.' << endl;

  delete [] screenshotData;
}

// Represents one spline control point.
struct Point 
{
  float x, y, z;
};

Point cross(Point a, Point b)
{
  Point out;
  out.x = a.y * b.z - a.z * b.y;
  out.y = a.z * b.x - a.x * b.z;
  out.z = a.x * b.y - a.y * b.x;
  return out;
}

Point unit(Point p)
{
  float length = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
  Point out;
  out.x = p.x / length;
  out.y = p.y / length;
  out.z = p.z / length;
  return out;
}

// Contains the control points of the spline.
struct Spline 
{
  int numControlPoints;
  Point * points;
} spline;

void loadSpline(char * argv) 
{
  FILE * fileSpline = fopen(argv, "r");
  if (fileSpline == NULL) 
  {
    printf ("Cannot open file %s.\n", argv);
    exit(1);
  }

  // Read the number of spline control points.
  fscanf(fileSpline, "%d\n", &spline.numControlPoints);
  printf("Detected %d control points.\n", spline.numControlPoints);

  // Allocate memory.
  spline.points = (Point *) malloc(spline.numControlPoints * sizeof(Point));
  // Load the control points.
  for(int i=0; i<spline.numControlPoints; i++)
  {
    if (fscanf(fileSpline, "%f %f %f", 
           &spline.points[i].x, 
	   &spline.points[i].y, 
	   &spline.points[i].z) != 3)
    {
      printf("Error: incorrect number of control points in file %s.\n", argv);
      exit(1);
    }
  }
}

// Multiply C = A * B, where A is a m x p matrix, and B is a p x n matrix.
// All matrices A, B, C must be pre-allocated (say, using malloc or similar).
// The memory storage for C must *not* overlap in memory with either A or B. 
// That is, you **cannot** do C = A * C, or C = C * B. However, A and B can overlap, and so C = A * A is fine, as long as the memory buffer for A is not overlaping in memory with that of C.
// Very important: All matrices are stored in **column-major** format.
// Example. Suppose 
//      [ 1 8 2 ]
//  A = [ 3 5 7 ]
//      [ 0 2 4 ]
//  Then, the storage in memory is
//   1, 3, 0, 8, 5, 2, 2, 7, 4. 
void MultiplyMatrices(int m, int p, int n, const float * A, const float * B, float * C)
{
  for(int i=0; i<m; i++)
  {
    for(int j=0; j<n; j++)
    {
      float entry = 0.0;
      for(int k=0; k<p; k++)
        entry += A[k * m + i] * B[j * p + k];
      C[m * j + i] = entry;
    }
  }
}

int initTexture(const char * imageFilename, GLuint textureHandle)
{
  // Read the texture image.
  ImageIO img;
  ImageIO::fileFormatType imgFormat;
  ImageIO::errorType err = img.load(imageFilename, &imgFormat);

  if (err != ImageIO::OK) 
  {
    printf("Loading texture from %s failed.\n", imageFilename);
    return -1;
  }

  // Check that the number of bytes is a multiple of 4.
  if (img.getWidth() * img.getBytesPerPixel() % 4) 
  {
    printf("Error (%s): The width*numChannels in the loaded image must be a multiple of 4.\n", imageFilename);
    return -1;
  }

  // Allocate space for an array of pixels.
  int width = img.getWidth();
  int height = img.getHeight();
  unsigned char * pixelsRGBA = new unsigned char[4 * width * height]; // we will use 4 bytes per pixel, i.e., RGBA

  // Fill the pixelsRGBA array with the image pixels.
  memset(pixelsRGBA, 0, 4 * width * height); // set all bytes to 0
  for (int h = 0; h < height; h++)
    for (int w = 0; w < width; w++) 
    {
      // assign some default byte values (for the case where img.getBytesPerPixel() < 4)
      pixelsRGBA[4 * (h * width + w) + 0] = 0; // red
      pixelsRGBA[4 * (h * width + w) + 1] = 0; // green
      pixelsRGBA[4 * (h * width + w) + 2] = 0; // blue
      pixelsRGBA[4 * (h * width + w) + 3] = 255; // alpha channel; fully opaque

      // set the RGBA channels, based on the loaded image
      int numChannels = img.getBytesPerPixel();
      for (int c = 0; c < numChannels; c++) // only set as many channels as are available in the loaded image; the rest get the default value
        pixelsRGBA[4 * (h * width + w) + c] = img.getPixel(w, h, c);
    }

  // Bind the texture.
  glBindTexture(GL_TEXTURE_2D, textureHandle);

  // Initialize the texture.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRGBA);

  // Generate the mipmaps for this texture.
  glGenerateMipmap(GL_TEXTURE_2D);

  // Set the texture parameters.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  // Query support for anisotropic texture filtering.
  GLfloat fLargest;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
  printf("Max available anisotropic samples: %f\n", fLargest);
  // Set anisotropic texture filtering.
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.5f * fLargest);

  // Query for any errors.
  GLenum errCode = glGetError();
  if (errCode != 0) 
  {
    printf("Texture initialization error. Error code: %d.\n", errCode);
    return -1;
  }
  
  // De-allocate the pixel array -- it is no longer needed.
  delete [] pixelsRGBA;

  return 0;
}

// Computes a point on the Catmull-Rom Spline given the floating point value
Point pointOnSpline(float u)
{
  int numSegments = spline.numControlPoints - 3;
  int segmentIndex = floor(u * numSegments);

  // Localized u
  float t = u * numSegments - segmentIndex;

  Point p0 = spline.points[segmentIndex];
  Point p1 = spline.points[segmentIndex + 1];
  Point p2 = spline.points[segmentIndex + 2];
  Point p3 = spline.points[segmentIndex + 3];

  // Dynamically allocate the matrices in column-major

  // u parameters (1x4)
  // [t^3 t^2 t 1]
  float* tM = new float[4];
  tM[0] = powf(t, 3); tM[1] = powf(t, 2); tM[2] = t; tM[3] = 1;

  // Basis matrix (4x4)
  // Assume tension parameter s=0.5
  // [ -0.5  1.5  -1.5  0.5 ]
  // [  1   -2.5   2   -0.5 ]
  // [ -0.5  0     0.5  0   ]
  // [  0    1     0    0   ]
  float* M = new float[16];
  M[0] = -0.5f; M[4] = 1.5f;  M[8] = -1.5f; M[12] = 0.5f;
  M[1] = 1.0f;  M[5] = -2.5f; M[9] = 2.0f;  M[13] = -0.5f;
  M[2] = -0.5f; M[6] = 0.0f;  M[10] = 0.5f; M[14] = 0.0f;
  M[3] = 0.0f;  M[7] = 1.0f;  M[11] = 0.0f; M[15] = 0.0f;

  // Control matrix (4x3)
  // [ x1   y1   z1 ]
  // [ x2   y2   z2 ]
  // [ x3   y3   z3 ]
  // [ x4   y4   z4 ]
  float* C = new float[12];
  C[0] = p0.x; C[4] = p0.y; C[8] = p0.z;
  C[1] = p1.x; C[5] = p1.y; C[9] = p1.z;
  C[2] = p2.x; C[6] = p2.y; C[10] = p2.z;
  C[3] = p3.x; C[7] = p3.y; C[11] = p3.z;

  // MC = M * C
  float* MC = new float[12]; // (4x3)
  MultiplyMatrices(4, 4, 3, M, C, MC);

  // result = tM * MC = tM * M * C
  float* result = new float[3]; // (1x3)
  MultiplyMatrices(1, 4, 3, tM, MC, result);

  // Construct return value
  Point out;
  out.x = result[0];
  out.y = result[1];
  out.z = result[2];

  // Remember to free the allocated memory later
  delete[] tM;
  delete[] M;
  delete[] C;
  delete[] MC;
  delete[] result;

  return out;
}

// Computes the tangent of a point on the Catmull-Rom Spline given the floating point value
Point tangentOnSpline(float u)
{
  int numSegments = spline.numControlPoints - 3;
  int segmentIndex = floor(u * numSegments);

  // Localized u
  float t = u * numSegments - segmentIndex;

  Point p0 = spline.points[segmentIndex];
  Point p1 = spline.points[segmentIndex + 1];
  Point p2 = spline.points[segmentIndex + 2];
  Point p3 = spline.points[segmentIndex + 3];

  // Dynamically allocate the matrices in column-major

  // u parameters (1x4)
  // [3t^2 2t 1 0]
  float* tM = new float[4];
  tM[0] = 3.0f * powf(t, 2); tM[1] = 2.0f * t; tM[2] = 1.0f; tM[3] = 0.0f;

  // Basis matrix (4x4)
  // Assume tension parameter s=0.5
  // [ -0.5  1.5  -1.5  0.5 ]
  // [  1   -2.5   2   -0.5 ]
  // [ -0.5  0     0.5  0   ]
  // [  0    1     0    0   ]
  float* M = new float[16];
  M[0] = -0.5f; M[4] = 1.5f;  M[8] = -1.5f; M[12] = 0.5f;
  M[1] = 1.0f;  M[5] = -2.5f; M[9] = 2.0f;  M[13] = -0.5f;
  M[2] = -0.5f; M[6] = 0.0f;  M[10] = 0.5f; M[14] = 0.0f;
  M[3] = 0.0f;  M[7] = 1.0f;  M[11] = 0.0f; M[15] = 0.0f;

  // Control matrix (4x3)
  // [ x1   y1   z1 ]
  // [ x2   y2   z2 ]
  // [ x3   y3   z3 ]
  // [ x4   y4   z4 ]
  float* C = new float[12];
  C[0] = p0.x; C[4] = p0.y; C[8] = p0.z;
  C[1] = p1.x; C[5] = p1.y; C[9] = p1.z;
  C[2] = p2.x; C[6] = p2.y; C[10] = p2.z;
  C[3] = p3.x; C[7] = p3.y; C[11] = p3.z;

  // MC = M * C
  float* MC = new float[12]; // (4x3)
  MultiplyMatrices(4, 4, 3, M, C, MC);

  // result = tM * MC = tM * M * C
  float* result = new float[3]; // (1x3)
  MultiplyMatrices(1, 4, 3, tM, MC, result);

  // Construct return value
  Point out;
  out.x = result[0];
  out.y = result[1];
  out.z = result[2];

  // Remember to free the allocated memory later
  delete[] tM;
  delete[] M;
  delete[] C;
  delete[] MC;
  delete[] result;

  return out;
}

// Computes all points on the spline and loads them into the VBOs and VAO
// For each point, it also computes the coordinate system, and saves it into the coordinates array
void drawSpline()
{
  if (vboVertices != nullptr)
  {
    delete vboVertices;
    vboVertices = nullptr;
  }
  if (vboColors != nullptr)
  {
    delete vboColors;
    vboColors = nullptr;
  }
  if (vao != nullptr) {
    delete vao;
    vao = nullptr;
  }

  int numPoints = 1001;
  numVertices = (numPoints - 1) * 2; // 2 points per line, 1001-1 lines
  float* positions = new float[numVertices * 3]; int vertexIndex = 0;
  float* colors = new float[numVertices * 4]; int colorIndex = 0;
  
  for (int i = 0; i < numPoints; i++)
  {
    float u = 0.001f * i;
    Point p = pointOnSpline(u);

    int r = (i == 0 || i == 1000) ? 1 : 2;
    for (int j = 0; j < r; j++)
    {
      positions[vertexIndex++] = p.x; positions[vertexIndex++] = p.y; positions[vertexIndex++] = p.z;
      colors[colorIndex++] = 1.0f;
      colors[colorIndex++] = 1.0f;
      colors[colorIndex++] = 1.0f;
      colors[colorIndex++] = 1.0f;
    }
  }

  // Create the VBOs.
  vboVertices = new VBO(numVertices, 3, positions, GL_STATIC_DRAW);
  vboColors = new VBO(numVertices, 4, colors, GL_STATIC_DRAW);

  // Create the VAO.
  vao = new VAO();

  vao->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboVertices, "position");
  vao->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboColors, "color");

  delete[] positions;
  delete[] colors;

  // Compute coordinate system for each point

  // 3 vectors per coordinate system, 3 floats per vector
  // Stored in the order t, n, then b
  coordinates = new float[numVertices * 3 * 3]; int index = 0;
  
  Point v; v.x = 0.0f; v.y = 1.0f; v.z = 0.0f; // Arbitrary v
  Point t; Point n; Point b;

  // First point
  t = unit(tangentOnSpline(0.0f));
  n = unit(cross(t, v));
  b = unit(cross(t, n));
  coordinates[index++] = t.x; coordinates[index++] = t.y; coordinates[index++] = t.z;
  coordinates[index++] = n.x; coordinates[index++] = n.y; coordinates[index++] = n.z;
  coordinates[index++] = b.x; coordinates[index++] = b.y; coordinates[index++] = b.z;

  for (int i = 1; i < numPoints; i++)
  {
    float u = 0.001f * (i + 1);
    // Iterate
    t = unit(tangentOnSpline(u));
    n = unit(cross(b, t));
    b = unit(cross(t, n));

    coordinates[index++] = t.x; coordinates[index++] = t.y; coordinates[index++] = t.z;
    coordinates[index++] = n.x; coordinates[index++] = n.y; coordinates[index++] = n.z;
    coordinates[index++] = b.x; coordinates[index++] = b.y; coordinates[index++] = b.z;
  }
}

void idleFunc()
{
  frameNumber++;
  if (frameNumber > 1000) frameNumber = 1000;

  // Notify GLUT that it should call displayFunc.
  glutPostRedisplay();
}

void reshapeFunc(int w, int h)
{
  glViewport(0, 0, w, h);

  // When the window has been resized, we need to re-set our projection matrix.
  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.LoadIdentity();
  // You need to be careful about setting the zNear and zFar. 
  // Anything closer than zNear, or further than zFar, will be culled.
  const float zNear = 0.1f;
  const float zFar = 10000.0f;
  const float humanFieldOfView = 60.0f;
  matrix.Perspective(humanFieldOfView, 1.0f * w / h, zNear, zFar);
}

void mouseMotionDragFunc(int x, int y)
{
  // Mouse has moved, and one of the mouse buttons is pressed (dragging).

  // the change in mouse position since the last invocation of this function
  int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };

  switch (controlState)
  {
    // translate the terrain
    case TRANSLATE:
      if (leftMouseButton)
      {
        // control x,y translation via the left mouse button
        worldTranslate[0] += mousePosDelta[0] * 0.01f;
        worldTranslate[1] -= mousePosDelta[1] * 0.01f;
      }
      if (rightMouseButton)
      {
        // control z translation via the right mouse button
        worldTranslate[2] += mousePosDelta[1] * 0.01f;
      }
      break;

    // rotate the terrain
    case ROTATE:
      if (leftMouseButton)
      {
        // control x,y rotation via the left mouse button
        worldRotate[0] += mousePosDelta[1];
        worldRotate[1] += mousePosDelta[0];
      }
      if (rightMouseButton)
      {
        // control z rotation via the right mouse button
        worldRotate[2] += mousePosDelta[1];
      }
      break;

    // scale the terrain
    case SCALE:
      if (leftMouseButton)
      {
        // control x,y scaling via the left mouse button
        worldScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
        worldScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      if (rightMouseButton)
      {
        // control z scaling via the middle mouse button
        worldScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
  // Mouse has moved.
  // Store the new mouse position.
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
  // A mouse button has has been pressed or depressed.

  // Keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables.
  switch (button)
  {
    case GLUT_LEFT_BUTTON:
      leftMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_MIDDLE_BUTTON:
      middleMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_RIGHT_BUTTON:
      rightMouseButton = (state == GLUT_DOWN);
    break;
  }

  // Keep track of whether CTRL and SHIFT keys are pressed.
  switch (glutGetModifiers())
  {
    case GLUT_ACTIVE_ALT:
      controlState = TRANSLATE;
    break;

    case GLUT_ACTIVE_SHIFT:
      controlState = SCALE;
    break;

    // If CTRL and SHIFT are not pressed, we are in rotate mode.
    default:
      controlState = ROTATE;
    break;
  }

  // Store the new mouse position.
  mousePos[0] = x;
  mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y)
{
  switch (key)
  {
    case 27: // ESC key
      exit(0); // exit the program
    break;

    case ' ':
      cout << "You pressed the spacebar." << endl;
    break;

    case 'x':
      // Take a screenshot.
      saveScreenshot("screenshot.jpg");
    break;

    case 't':
      controlState = TRANSLATE;
    break;
  }
}

void keyboardUpFunc(unsigned char key, int x, int y)
{
  switch (key)
  {
    case 't':
      controlState = ROTATE;
    break;
  }
}

void displayFunc()
{
  // This function performs the actual rendering.

  // First, clear the screen.
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  printf("Frame #%d\n", frameNumber);
  float u = speed * frameNumber;
  Point p = pointOnSpline(u);
  int coordIndex = frameNumber * 9;
  Point t; Point n; Point b;
  t.x = coordinates[coordIndex++]; t.y = coordinates[coordIndex++]; t.z = coordinates[coordIndex++];
  n.x = coordinates[coordIndex++]; n.y = coordinates[coordIndex++]; n.z = coordinates[coordIndex++];
  b.x = coordinates[coordIndex++]; b.y = coordinates[coordIndex++]; b.z = coordinates[coordIndex++];
  Point f;
  f.x = p.x + t.x; f.y = p.y + t.y; f.z = p.z + t.z;

  // Set up the camera position, focus point, and the up vector.
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.LoadIdentity();
  // matrix.LookAt(0.0, 0.0, 5.0,
  //               0.0, 0.0, 0.0,
  //               0.0, 1.0, 0.0);
  matrix.LookAt(p.x, p.y, p.z,
                f.x, f.y, f.z,
                n.x, n.y, n.z);

  // In here, you can do additional modeling on the object, such as performing translations, rotations and scales.
  // ...
  matrix.Rotate(worldRotate[0], 1.0, 0.0, 0.0);
  matrix.Rotate(worldRotate[1], 0.0, 1.0, 0.0);
  matrix.Rotate(worldRotate[2], 0.0, 0.0, 1.0);
  matrix.Translate(worldTranslate[0], worldTranslate[1], worldTranslate[2]);
  matrix.Scale(worldScale[0], worldScale[1], worldScale[2]);

  // Read the current modelview and projection matrices from our helper class.
  // The matrices are only read here; nothing is actually communicated to OpenGL yet.
  float modelViewMatrix[16];
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.GetMatrix(modelViewMatrix);

  float projectionMatrix[16];
  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.GetMatrix(projectionMatrix);

  // Upload the modelview and projection matrices to the GPU. Note that these are "uniform" variables.
  // Important: these matrices must be uploaded to *all* pipeline programs used.
  // In hw1, there is only one pipeline program, but in hw2 there will be several of them.
  // In such a case, you must separately upload to *each* pipeline program.
  // Important: do not make a typo in the variable name below; otherwise, the program will malfunction.
  pipelineProgram->SetUniformVariableMatrix4fv("modelViewMatrix", GL_FALSE, modelViewMatrix);
  pipelineProgram->SetUniformVariableMatrix4fv("projectionMatrix", GL_FALSE, projectionMatrix);

  // Execute the rendering.
  // Bind the VAO that we want to render. Remember, one object = one VAO. 
  vao->Bind();
  glDrawArrays(GL_LINES, 0, numVertices); // Render the VAO, by rendering "numVertices", starting from vertex 0.

  // Swap the double-buffers.
  glutSwapBuffers();
}

void initScene(int argc, char *argv[])
{
  // Set the background color.
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black color.

  // Enable z-buffering (i.e., hidden surface removal using the z-buffer algorithm).
  glEnable(GL_DEPTH_TEST);

  // Create a pipeline program. This operation must be performed BEFORE we initialize any VAOs.
  // A pipeline program contains our shaders. Different pipeline programs may contain different shaders.
  // In this homework, we only have one set of shaders, and therefore, there is only one pipeline program.
  // In hw2, we will need to shade different objects with different shaders, and therefore, we will have
  // several pipeline programs (e.g., one for the rails, one for the ground/sky, etc.).
  pipelineProgram = new PipelineProgram(); // Load and set up the pipeline program, including its shaders.
  // Load and set up the pipeline program, including its shaders.
  if (pipelineProgram->BuildShadersFromFiles(shaderBasePath, "vertexShader.glsl", "fragmentShader.glsl") != 0)
  {
    cout << "Failed to build the pipeline program." << endl;
    throw 1;
  } 
  cout << "Successfully built the pipeline program." << endl;
    
  // Bind the pipeline program that we just created. 
  // The purpose of binding a pipeline program is to activate the shaders that it contains, i.e.,
  // any object rendered from that point on, will use those shaders.
  // When the application starts, no pipeline program is bound, which means that rendering is not set up.
  // So, at some point (such as below), we need to bind a pipeline program.
  // From that point on, exactly one pipeline program is bound at any moment of time.
  pipelineProgram->Bind();

  drawSpline();

  // Check for any OpenGL errors.
  std::cout << "GL error status is: " << glGetError() << std::endl;
}

void closeFunc()
{
  if (coordinates != nullptr)
  {
    delete[] coordinates;
    coordinates = nullptr;
  }
  printf("Program has been closed.\n");
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {  
    printf ("Usage: %s <spline file>\n", argv[0]);
    exit(0);
  }

  // Load spline from the provided filename.
  loadSpline(argv[1]);

  printf("Loaded spline with %d control point(s).\n", spline.numControlPoints);

  cout << "Initializing GLUT..." << endl;
  glutInit(&argc,argv);

  cout << "Initializing OpenGL..." << endl;

  #ifdef __APPLE__
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #else
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #endif

  glutInitWindowSize(windowWidth, windowHeight);
  glutInitWindowPosition(0, 0);  
  glutCreateWindow(windowTitle);

  cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
  cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
  cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

  #ifdef __APPLE__
    // This is needed on recent Mac OS X versions to correctly display the window.
    glutReshapeWindow(windowWidth - 1, windowHeight - 1);
  #endif

  // Tells GLUT to use a particular display function to redraw.
  glutDisplayFunc(displayFunc);
  // Perform animation inside idleFunc.
  glutIdleFunc(idleFunc);
  // callback for mouse drags
  glutMotionFunc(mouseMotionDragFunc);
  // callback for idle mouse movement
  glutPassiveMotionFunc(mouseMotionFunc);
  // callback for mouse button changes
  glutMouseFunc(mouseButtonFunc);
  // callback for resizing the window
  glutReshapeFunc(reshapeFunc);
  // callback for pressing the keys on the keyboard
  glutKeyboardFunc(keyboardFunc);
  // callback to cleanup dynamic data
  atexit(closeFunc);

  // init glew
  #ifdef __APPLE__
    // nothing is needed on Apple
  #else
    // Windows, Linux
    GLint result = glewInit();
    if (result != GLEW_OK)
    {
      cout << "error: " << glewGetErrorString(result) << endl;
      exit(EXIT_FAILURE);
    }
  #endif

  // Perform the initialization.
  initScene(argc, argv);

  // Sink forever into the GLUT loop.
  glutMainLoop();
}

