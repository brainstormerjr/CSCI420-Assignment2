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

// Width and height of terrain object in world space
float terrainWidth = 5;
float terrainHeight = 5;

// Transformations of the terrain.
float terrainRotate[3] = { -45.0f, 0.0f, 0.0f }; 
// terrainRotate[0] gives the rotation around x-axis (in degrees)
// terrainRotate[1] gives the rotation around y-axis (in degrees)
// terrainRotate[2] gives the rotation around z-axis (in degrees)
float terrainTranslate[3] = { -terrainWidth / 2, -terrainHeight / 2, 0.0f };
float terrainScale[3] = { 1.0f, 1.0f, 1.0f };
float smoothScale = 1.0f;
float smoothExponent = 1.0f;

// Width and height of the OpenGL window, in pixels.
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 Homework 1";

// Stores the image loaded from disk.
ImageIO * heightmapImage;

// Number of vertices in the single triangle (starter code).
int numVertices;

typedef enum { POINTS, WIREFRAME, SURFACE } RENDER_MODE;
bool smoothing = false;
RENDER_MODE renderMode = POINTS;

int screenshotCount = 0;
int targetScreenshotCount = 280;

// CSCI 420 helper classes.
OpenGLMatrix matrix;
PipelineProgram * pipelineProgram = nullptr;
VBO * vboVertices = nullptr;
VBO * vboColors = nullptr;
VBO * vboLeft = nullptr;
VBO * vboRight = nullptr;
VBO * vboUp = nullptr;
VBO * vboDown = nullptr;
VAO * vao = nullptr;

// Compute the neighbors of a given vertex and add them to the array.
void computeNeighbors(
  float * leftPositions, float * rightPositions,
  float * upPositions, float * downPositions,
  int x, int y,
  float xGap, float yGap,
  int& offsetIndex
)
{

  int width = heightmapImage->getWidth();
  int height = heightmapImage->getHeight();

  // Left vertex
  if (x > 0)
  {
    leftPositions[offsetIndex] = (x - 1) * xGap;
    leftPositions[offsetIndex + 1] = y * yGap;
    leftPositions[offsetIndex + 2] = heightmapImage->getPixel(x - 1, y, 0) / 255.0f;
  }
  else
  {
    leftPositions[offsetIndex] = x * xGap;
    leftPositions[offsetIndex + 1] = y * yGap;
    leftPositions[offsetIndex + 2] = heightmapImage->getPixel(x, y, 0) / 255.0f;;
  }

  // Right vertex
  if (x < width - 1)
  {
    rightPositions[offsetIndex] = (x + 1) * xGap;
    rightPositions[offsetIndex + 1] = y * yGap;
    rightPositions[offsetIndex + 2] = heightmapImage->getPixel(x + 1, y, 0) / 255.0f;
  }
  else
  {
    rightPositions[offsetIndex] = x * xGap;
    rightPositions[offsetIndex + 1] = y * yGap;
    rightPositions[offsetIndex + 2] = heightmapImage->getPixel(x, y, 0) / 255.0f;;
  }

  // Up vertex
  if (y < height - 1)
  {
    upPositions[offsetIndex] = x * xGap;
    upPositions[offsetIndex + 1] = (y + 1) * yGap;
    upPositions[offsetIndex + 2] = heightmapImage->getPixel(x, y + 1, 0) / 255.0f;
  }
  else
  {
    upPositions[offsetIndex] = x * xGap;
    upPositions[offsetIndex + 1] = y * yGap;
    upPositions[offsetIndex + 2] = heightmapImage->getPixel(x, y, 0) / 255.0f;;
  }

  // Down vertex
  if (y > 0)
  {
    downPositions[offsetIndex] = x * xGap;
    downPositions[offsetIndex + 1] = (y - 1) * yGap;
    downPositions[offsetIndex + 2] = heightmapImage->getPixel(x, y - 1, 0) / 255.0f;
  }
  else
  {
    downPositions[offsetIndex] = x * xGap;
    downPositions[offsetIndex + 1] = y * yGap;
    downPositions[offsetIndex + 2] = heightmapImage->getPixel(x, y, 0) / 255.0f;;
  }

  offsetIndex += 3;
}

// Compute a quad (two triangles) and add them to the positions and colors array
void computeQuad(
  float * positions, float * colors,
  int x, int y,
  float xGap, float yGap,
  int& vertexIndex, int& colorIndex, int& offsetIndex,
  bool smoothing = false,
  float * leftPositions = nullptr, float * rightPositions = nullptr,
  float * upPositions = nullptr, float * downPositions = nullptr
)
{
  // Vertices
  // Triangle 1
  positions[vertexIndex++] = x * xGap;
  positions[vertexIndex++] = y * yGap;
  positions[vertexIndex++] = heightmapImage->getPixel(x, y, 0) / 255.0f;
  if (smoothing)
    computeNeighbors(leftPositions, rightPositions, upPositions, downPositions, x, y, xGap, yGap, offsetIndex);

  positions[vertexIndex++] = (x + 1) * xGap;
  positions[vertexIndex++] = y * yGap;
  positions[vertexIndex++] = heightmapImage->getPixel(x+1, y, 0) / 255.0f;
  if (smoothing)
    computeNeighbors(leftPositions, rightPositions, upPositions, downPositions, x+1, y, xGap, yGap, offsetIndex);

  positions[vertexIndex++] = x * xGap;
  positions[vertexIndex++] = (y + 1) * yGap;
  positions[vertexIndex++] = heightmapImage->getPixel(x, y+1, 0) / 255.0f;
  if (smoothing)
    computeNeighbors(leftPositions, rightPositions, upPositions, downPositions, x, y+1, xGap, yGap, offsetIndex);

  // Triangle 2
  positions[vertexIndex++] = (x + 1) * xGap;
  positions[vertexIndex++] = (y + 1)* yGap;
  positions[vertexIndex++] = heightmapImage->getPixel(x+1, y+1, 0) / 255.0f;
  if (smoothing)
    computeNeighbors(leftPositions, rightPositions, upPositions, downPositions, x+1, y+1, xGap, yGap, offsetIndex);

  positions[vertexIndex++] = (x + 1) * xGap;
  positions[vertexIndex++] = y * yGap;
  positions[vertexIndex++] = heightmapImage->getPixel(x+1, y, 0) / 255.0f;
  if (smoothing)
    computeNeighbors(leftPositions, rightPositions, upPositions, downPositions, x+1, y, xGap, yGap, offsetIndex);

  positions[vertexIndex++] = x * xGap;
  positions[vertexIndex++] = (y + 1) * yGap;
  positions[vertexIndex++] = heightmapImage->getPixel(x, y+1, 0) / 255.0f;
  if (smoothing)
    computeNeighbors(leftPositions, rightPositions, upPositions, downPositions, x, y+1, xGap, yGap, offsetIndex);

  // Colors
  // Triangle 1
  float value = heightmapImage->getPixel(x, y, 0) / 255.0f;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = 1.0f;

  value = heightmapImage->getPixel(x+1, y, 0) / 255.0f;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = 1.0f;

  value = heightmapImage->getPixel(x, y+1, 0) / 255.0f;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = 1.0f;

  // Triangle 2
  value = heightmapImage->getPixel(x+1, y+1, 0) / 255.0f;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = 1.0f;

  value = heightmapImage->getPixel(x+1, y, 0) / 255.0f;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = 1.0f;

  value = heightmapImage->getPixel(x, y+1, 0) / 255.0f;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = value;
  colors[colorIndex++] = 1.0f;
}

// Re-compute height data depending on the renderMode.
// Should be called after every time the renderMode is changed
void computeHeightData()
{
  if (vboVertices == nullptr)
  {
    delete vboVertices;
    vboVertices = nullptr;
  }
  if (vboColors == nullptr)
  {
    delete vboColors;
    vboColors = nullptr;
  }
  if (vao == nullptr) {
    delete vao;
    vao = nullptr;
  }
  if (vboLeft == nullptr)
  {
    delete vboLeft;
    vboLeft = nullptr;
  }
  if (vboRight == nullptr)
  {
    delete vboRight;
    vboRight = nullptr;
  }
  if (vboUp == nullptr)
  {
    delete vboUp;
    vboUp = nullptr;
  }
  if (vboDown == nullptr)
  {
    delete vboDown;
    vboDown = nullptr;
  }

  int width = heightmapImage->getWidth();
  int height = heightmapImage->getHeight();
  float xGap = terrainWidth / (float) heightmapImage->getWidth() ;
  float yGap = terrainHeight / (float) heightmapImage->getHeight();

  float * positions;
  float * colors;

  float * leftPositions;
  float * rightPositions;
  float * upPositions;
  float * downPositions;

  if (renderMode == POINTS)
  {
    // Prepare the triangle position and color data for the VBO. 

    numVertices = width * height;

    // Vertex positions.
    positions = (float*) malloc (numVertices * 3 * sizeof(float)); // 3 floats per vertex, i.e., x,y,z
    // Vertex colors.
    colors = (float*) malloc (numVertices * 4 * sizeof(float)); // 4 floats per vertex, i.e., r,g,b,a

    int vertexIndex = 0; int colorIndex = 0;
    for (int y = 0; y < heightmapImage->getHeight(); y++)
    {
      for (int x = 0; x < heightmapImage->getWidth(); x++)
      {
        positions[vertexIndex++] = (float) x * xGap; // x position
        positions[vertexIndex++] = (float) y * yGap; // y position
        positions[vertexIndex++] = heightmapImage->getPixel(x, y, 0) / 255.0f; // z position
        
        float value = heightmapImage->getPixel(x, y, 0) / 255.0f;
        colors[colorIndex++] = value; // r value
        colors[colorIndex++] = value; // g value
        colors[colorIndex++] = value; // b value
        colors[colorIndex++] = 1.0; // a value
      }
    }
  }
  else if (renderMode == WIREFRAME)
  {
    numVertices = (width - 1) * height * 2 + (height - 1) * width * 2;

    // Vertex positions for the wireframe
    positions = (float*) malloc(numVertices * 3 * sizeof(float));
    // Vertex colors
    colors = (float*) malloc(numVertices * 4 * sizeof(float));

    int vertexIndex = 0; int colorIndex = 0;
    for (int y = 0; y < height; y++)
    {
      for (int x = 0; x < width - 1; x++)
      {
        // Horizontal line segment
        positions[vertexIndex++] = x * xGap;
        positions[vertexIndex++] = y * yGap;
        positions[vertexIndex++] = heightmapImage->getPixel(x, y, 0) / 255.0f;

        positions[vertexIndex++] = (x + 1) * xGap;
        positions[vertexIndex++] = y * yGap;
        positions[vertexIndex++] = heightmapImage->getPixel(x + 1, y, 0) / 255.0f;

        float value = heightmapImage->getPixel(x, y, 0) / 255.0f;
        colors[colorIndex++] = value;
        colors[colorIndex++] = value;
        colors[colorIndex++] = value;
        colors[colorIndex++] = 1.0f;

        value = heightmapImage->getPixel(x, y+1, 0) / 255.0f;
        colors[colorIndex++] = value;
        colors[colorIndex++] = value;
        colors[colorIndex++] = value;
        colors[colorIndex++] = 1.0f;
      }
    }

    for (int y = 0; y < height - 1; y++)
    {
      for (int x = 0; x < width; x++)
      {
        // Vertical line segment
        positions[vertexIndex++] = x * xGap;
        positions[vertexIndex++] = y * yGap;
        positions[vertexIndex++] = heightmapImage->getPixel(x, y, 0) / 255.0f;

        positions[vertexIndex++] = x * xGap;
        positions[vertexIndex++] = (y + 1) * yGap;
        positions[vertexIndex++] = heightmapImage->getPixel(x, y + 1, 0) / 255.0f;

        float value = heightmapImage->getPixel(x, y, 0) / 255.0f;
        colors[colorIndex++] = value;
        colors[colorIndex++] = value;
        colors[colorIndex++] = value;
        colors[colorIndex++] = 1.0f;

        value = heightmapImage->getPixel(x, y+1, 0) / 255.0f;
        colors[colorIndex++] = value;
        colors[colorIndex++] = value;
        colors[colorIndex++] = value;
        colors[colorIndex++] = 1.0f;
      }
    }
  }
  else if (renderMode == SURFACE)
  {
    numVertices = (width - 1) * (height - 1) * 6; // 2 triangles per quad, 3 vertices per triangle

    // Vertex positions for the wireframe
    positions = (float*) malloc(numVertices * 3 * sizeof(float));
    // Vertex colors
    colors = (float*) malloc(numVertices * 4 * sizeof(float));

    if (smoothing)
    {
      leftPositions = (float*) malloc(numVertices * 3 * sizeof(float));
      rightPositions = (float*) malloc(numVertices * 3 * sizeof(float));
      upPositions = (float*) malloc(numVertices * 3 * sizeof(float));
      downPositions = (float*) malloc(numVertices * 3 * sizeof(float));
    }

    int vertexIndex = 0; int colorIndex = 0; int offsetIndex = 0;
    if (!smoothing)
    {
      for (int y = 0; y < height - 1; y++)
      {
        for (int x = 0; x < width - 1; x++)
        {
          computeQuad(positions, colors, x, y, xGap, yGap, vertexIndex, colorIndex, offsetIndex);
        }
      }
    }
    else
    {
      for (int y = 0; y < height - 1; y++)
      {
        for (int x = 0; x < width - 1; x++)
        {
          computeQuad(positions, colors, x, y, xGap, yGap, vertexIndex, colorIndex, offsetIndex, true, leftPositions, rightPositions, upPositions, downPositions);
        }
      }
    }
  }

  // Create the VBOs.
  vboVertices = new VBO(numVertices, 3, positions, GL_STATIC_DRAW);
  vboColors = new VBO(numVertices, 4, colors, GL_STATIC_DRAW);
  
  if (smoothing)
  {
    vboLeft = new VBO(numVertices, 3, leftPositions, GL_STATIC_DRAW);
    vboRight = new VBO(numVertices, 3, rightPositions, GL_STATIC_DRAW);
    vboUp = new VBO(numVertices, 3, upPositions, GL_STATIC_DRAW);
    vboDown = new VBO(numVertices, 3, downPositions, GL_STATIC_DRAW);
  }

  // Create the VAO.
  vao = new VAO();

  vao->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboVertices, "position");
  vao->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboColors, "color");

  if (smoothing)
  {
    vao->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboLeft, "p_up");
    vao->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboRight, "p_down");
    vao->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboUp, "p_left");
    vao->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, vboDown, "p_right");
  }

  free(positions);
  free(colors);

  if (smoothing)
  {
    free(leftPositions);
    free(rightPositions);
    free(upPositions);
    free(downPositions);
  }
}

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

void idleFunc()
{

  // Cycle through the four render modes, with 70 frames each.
  // Total: 280 frames (set this global variable at the start of the file)

  if (screenshotCount < targetScreenshotCount)
  {
    terrainRotate[2] += 180.0f / 70.0f; // 180deg rotation in 70 frames
    screenshotCount++;
    char filename[256];
    sprintf(filename, "%03d.jpg", screenshotCount);
    saveScreenshot(filename);
    if (screenshotCount % 70 == 0)
    {
      if (renderMode == POINTS) 
      {
        renderMode = WIREFRAME;
        computeHeightData();
      }
      else if (renderMode == WIREFRAME)
      {
        renderMode = SURFACE;
        computeHeightData();
      }
      else if (renderMode == SURFACE)
      {
        renderMode = SURFACE;
        smoothing = true;
        pipelineProgram->SetUniformVariablei("mode", 1);
        computeHeightData();
      }
    }
  }

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
        terrainTranslate[0] += mousePosDelta[0] * 0.01f;
        terrainTranslate[1] -= mousePosDelta[1] * 0.01f;
      }
      if (rightMouseButton)
      {
        // control z translation via the right mouse button
        terrainTranslate[2] += mousePosDelta[1] * 0.01f;
      }
      break;

    // rotate the terrain
    case ROTATE:
      if (leftMouseButton)
      {
        // control x,y rotation via the left mouse button
        terrainRotate[0] += mousePosDelta[1];
        terrainRotate[1] += mousePosDelta[0];
      }
      if (rightMouseButton)
      {
        // control z rotation via the right mouse button
        terrainRotate[2] += mousePosDelta[1];
      }
      break;

    // scale the terrain
    case SCALE:
      if (leftMouseButton)
      {
        // control x,y scaling via the left mouse button
        terrainScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
        terrainScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      if (rightMouseButton)
      {
        // control z scaling via the middle mouse button
        terrainScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
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

    case '1':
      renderMode = POINTS;
      smoothing = false;
      pipelineProgram->SetUniformVariablei("mode", 0);
      computeHeightData();
      cout << "Render mode : Points" << endl;
    break;

    case '2':
      renderMode = WIREFRAME;
      smoothing = false;
      pipelineProgram->SetUniformVariablei("mode", 0);
      computeHeightData();
      cout << "Render mode : Wireframe" << endl;
    break;

    case '3':
      renderMode = SURFACE;
      smoothing = false;
      pipelineProgram->SetUniformVariablei("mode", 0);
      computeHeightData();
      cout << "Render mode : Surface" << endl;
    break;

    case '4':
      renderMode = SURFACE;
      smoothing = true;
      pipelineProgram->SetUniformVariablei("mode", 1);
      computeHeightData();
      cout << "Render mode : Smooth" << endl;
    break;

    case '=':
      smoothScale *= 2.0f;
      pipelineProgram->SetUniformVariablef("scale", smoothScale);
    break;

    case '-':
      smoothScale /= 2.0f;
      pipelineProgram->SetUniformVariablef("scale", smoothScale);
    break;

    case '9':
      smoothExponent *= 2.0f;
      pipelineProgram->SetUniformVariablef("exponent", smoothExponent);
    break;

    case '0':
      smoothExponent /= 2.0f;
      pipelineProgram->SetUniformVariablef("exponent", smoothExponent);
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

  // Set up the camera position, focus point, and the up vector.
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.LoadIdentity();
  matrix.LookAt(0.0, 0.0, 5.0,
                0.0, 0.0, 0.0,
                0.0, 1.0, 0.0);

  // In here, you can do additional modeling on the object, such as performing translations, rotations and scales.
  // ...
  matrix.Rotate(terrainRotate[0], 1.0, 0.0, 0.0);
  matrix.Rotate(terrainRotate[1], 0.0, 1.0, 0.0);
  matrix.Rotate(terrainRotate[2], 0.0, 0.0, 1.0);
  matrix.Translate(terrainTranslate[0], terrainTranslate[1], terrainTranslate[2]);
  matrix.Scale(terrainScale[0], terrainScale[1], terrainScale[2]);

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
  if (renderMode == POINTS)
  {
    glDrawArrays(GL_POINTS, 0, numVertices); // Render the VAO, by rendering "numVertices", starting from vertex 0.
  }
  else if (renderMode == WIREFRAME)
  {
    glDrawArrays(GL_LINES, 0, numVertices);
  }
  else if (renderMode == SURFACE)
  {
    glDrawArrays(GL_TRIANGLES, 0, numVertices);
  }

  // Swap the double-buffers.
  glutSwapBuffers();
}

void initScene(int argc, char *argv[])
{
  // Load the image from a jpeg disk file into main memory.
  heightmapImage = new ImageIO();
  if (heightmapImage->loadJPEG(argv[1]) != ImageIO::OK)
  {
    cout << "Error reading image " << argv[1] << "." << endl;
    exit(EXIT_FAILURE);
  }

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

  computeHeightData();

  pipelineProgram->SetUniformVariablef("scale", smoothScale);
  pipelineProgram->SetUniformVariablef("exponent", smoothExponent);

  // Check for any OpenGL errors.
  std::cout << "GL error status is: " << glGetError() << std::endl;
}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    cout << "The arguments are incorrect." << endl;
    cout << "usage: ./hw1 <heightmap file>" << endl;
    exit(EXIT_FAILURE);
  }

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

