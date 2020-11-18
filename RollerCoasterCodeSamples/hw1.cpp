/*
  CSCI 420 Computer Graphics, USC
  Assignment 2: Roller Coaster
  C++ starter code

  Student username: Yang Cao
  Student USC ID: 7777082764
*/

#include "basicPipelineProgram.h"
#include "openGLMatrix.h"
#include "imageIO.h"
#include "openGLHeader.h"
#include "glutHeader.h"

#include <iostream>
#include <cstring>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "openGLHeader.h"
#include "imageIO.h"
#include <glm/gtc/type_ptr.hpp>

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
  char shaderBasePath[1024] = "../openGLHelper-starterCode";
#endif

using namespace std;

// hw2
// represents one control point along the spline 
struct Point
{
    double x;
    double y;
    double z;
};

// spline struct 
// contains how many control points the spline has, and an array of control points 
struct Spline
{
    int numControlPoints;
    Point* points;
};



// level 0 and 1
GLuint splineVBO;
GLuint splineVAO;

// the spline array 
Spline* splines;
// total number of splines 
int numSplines;

std::vector<glm::vec3> vertices;
const float s = 0.5;
const glm::mat4 basis = glm::mat4(-s, 2 * s, -s, 0, 2 - s, s - 3, 0, 1, s - 2, 3 - 2 * s, s, 0, s, -s, 0, 0);
glm::mat3x4 ctrlMat;
const float maxLineLength = 0.01;
std::vector<glm::mat3x4> ctrlMatrices;


// level 2: the ride
const glm::vec3 v = glm::vec3(1, 0, 0);
glm::vec3 normal;
glm::vec3 binormal;
int splineIndex = 0;
float u = 0;
float uStep = 0.001;
bool pause = false;
bool physics = false;


// level 3: rail cross-section
GLuint trackVBO;
GLuint trackVAO;

std::vector<glm::vec3> crossSectionVertices;
std::vector<glm::vec3> crossSectionNormals;
const float a = 0.025f;
const float tThickness = 0.015f;
const float offset = 0.1f;
const float plankHalfLength = 0.125f;
const float plankHalfWidth = 0.05f;
float travelledDistance = 0;
const float plankDistance = 0.75f;


// level 4: ground
GLuint texVBO;
GLuint texVAO;

BasicPipelineProgram* pipelineProgramGround;
GLuint texHandle;
const float groundSize = 100.0f;
const float groundHeight = -20.0f;
const float groundTextureSize = 2;

// level 5: phong shading
const float lightDirection[3] = { 0, 1, 0 }; // the “Sun” at noon
const float lightDirection4[4] = { lightDirection[0], lightDirection[1], lightDirection[2], 0.0f };
float viewLightDirection[3]; // light direction in the view space
const float La[4] = { 1, 1, 1, 1 };
const float Ld[4] = { 1, 1, 1, 1 };
const float Ls[4] = { 1, 1, 1, 1 };
const float ka[4] = { 0.1f, 0.07f, 0, 1 };
const float kd[4] = { 0.8f, 0.56f, 0, 1 };
const float ks[4] = { 0.9f, 0.63f, 0, 1 };
const float alpha = 50.f;

int mousePos[2]; // x,y coordinate of the mouse position

int leftMouseButton = 0; // 1 if pressed, 0 if not 
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

// state of the world
float landRotate[3] = { 0.0f, 0.0f, 0.0f };
float landTranslate[3] = { 0.0f, 0.0f, 0.0f };
float landScale[3] = { 1.0f, 1.0f, 1.0f };

int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 homework I";


OpenGLMatrix matrix;
BasicPipelineProgram * pipelineProgram;

bool startScreenshot = false;
string file;
int screenshotNum = 0;
int frameCounter = 0;

// write a screenshot to the specified filename
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

void takeScreenshot()
{
    file = "./Screenshots/";
    if (screenshotNum < 10) {
        file += "00" + to_string(screenshotNum);
        file += ".jpg";
        const char* s = file.c_str();
        saveScreenshot(s);
        screenshotNum++;
    }
    else if (screenshotNum < 100) {
        file += "0" + to_string(screenshotNum);
        file += ".jpg";
        const char* s = file.c_str();
        saveScreenshot(s);
        screenshotNum++;
    }
    else if (screenshotNum < 700) {
        file += "" + to_string(screenshotNum);
        file += ".jpg";
        const char* s = file.c_str();
        saveScreenshot(s);
        screenshotNum++;
    }
    else
    {
        startScreenshot = false;
    }
}

int loadSplines(char* argv)
{
    char* cName = (char*)malloc(128 * sizeof(char));
    FILE* fileList;
    FILE* fileSpline;
    int iType, i = 0, j, iLength;

    // load the track file 
    fileList = fopen(argv, "r");
    if (fileList == NULL)
    {
        printf("can't open file\n");
        exit(1);
    }

    // stores the number of splines in a global variable 
    fscanf(fileList, "%d", &numSplines);

    splines = (Spline*)malloc(numSplines * sizeof(Spline));

    // reads through the spline files 
    for (j = 0; j < numSplines; j++)
    {
        i = 0;
        fscanf(fileList, "%s", cName);
        fileSpline = fopen(cName, "r");

        if (fileSpline == NULL)
        {
            printf("can't open file\n");
            exit(1);
        }

        // gets length for spline file
        fscanf(fileSpline, "%d %d", &iLength, &iType);

        // allocate memory for all the points
        splines[j].points = (Point*)malloc(iLength * sizeof(Point));
        splines[j].numControlPoints = iLength;

        // saves the data to the struct
        while (fscanf(fileSpline, "%lf %lf %lf",
            &splines[j].points[i].x,
            &splines[j].points[i].y,
            &splines[j].points[i].z) != EOF)
        {
            i++;
        }
    }

    free(cName);

    return 0;
}

//int initTexture(const char* imageFilename, GLuint textureHandle)
int initTexture(const char* imageFilename)
{
    // read the texture image
    ImageIO img;
    ImageIO::fileFormatType imgFormat;
    ImageIO::errorType err = img.load(imageFilename, &imgFormat);

    if (err != ImageIO::OK)
    {
        printf("Loading texture from %s failed.\n", imageFilename);
        return -1;
    }

    // check that the number of bytes is a multiple of 4
    if (img.getWidth() * img.getBytesPerPixel() % 4)
    {
        printf("Error (%s): The width*numChannels in the loaded image must be a multiple of 4.\n", imageFilename);
        return -1;
    }

    // allocate space for an array of pixels
    int width = img.getWidth();
    int height = img.getHeight();
    unsigned char* pixelsRGBA = new unsigned char[4 * width * height]; // we will use 4 bytes per pixel, i.e., RGBA

    // fill the pixelsRGBA array with the image pixels
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

    // bind the texture
    glBindTexture(GL_TEXTURE_2D, texHandle);

    // initialize the texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRGBA);

    // generate the mipmaps for this texture
    glGenerateMipmap(GL_TEXTURE_2D);

    // set the texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // query support for anisotropic texture filtering
    GLfloat fLargest;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
    printf("Max available anisotropic samples: %f\n", fLargest);
    // set anisotropic texture filtering
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.5f * fLargest);

    // query for any errors
    GLenum errCode = glGetError();
    if (errCode != 0)
    {
        printf("Texture initialization error. Error code: %d.\n", errCode);
        return -1;
    }

    // de-allocate the pixel array -- it is no longer needed
    delete[] pixelsRGBA;

    return 0;
}

//void setupTexture(const char* imageFilename, GLuint textureHandle)
void setupTexture(const char* imageFilename)
{
    // create ground plane
    glm::vec3* vertices = (glm::vec3*)malloc(sizeof(glm::vec3) * 4);
    glm::vec2* uvCoords = (glm::vec2*)malloc(sizeof(glm::vec2) * 4);

    vertices[0] = glm::vec3(-groundSize, groundHeight, groundSize);
    vertices[1] = glm::vec3(-groundSize, groundHeight, -groundSize);
    vertices[2] = glm::vec3(groundSize, groundHeight, groundSize);
    vertices[3] = glm::vec3(groundSize, groundHeight, -groundSize);

    uvCoords[0] = glm::vec2(0, 0);
    uvCoords[1] = glm::vec2(0, groundTextureSize);
    uvCoords[2] = glm::vec2(groundTextureSize, 0);
    uvCoords[3] = glm::vec2(groundTextureSize, groundTextureSize);

    //////////////////////////////////// Ground (Level 4) ////////////////////////////////////
    pipelineProgramGround = new BasicPipelineProgram;
    int ret = pipelineProgramGround->Init(shaderBasePath, 1);
    if (ret != 0) abort();

    // VBO set up for texture
    glGenBuffers(1, &texVBO);
    glBindBuffer(GL_ARRAY_BUFFER, texVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 4 + sizeof(glm::vec2) * 4, nullptr, GL_STATIC_DRAW);

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * 4, vertices);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 4, sizeof(glm::vec2) * 4, uvCoords);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // VAO set up for texture
    glGenVertexArrays(1, &texVAO);
    glBindVertexArray(texVAO);
    glBindBuffer(GL_ARRAY_BUFFER, texVBO);

    GLuint loc =
        glGetAttribLocation(pipelineProgramGround->GetProgramHandle(), "position");
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    loc = glGetAttribLocation(pipelineProgramGround->GetProgramHandle(), "texCoord");
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, (const void*)(sizeof(glm::vec3) * 4));

    glBindVertexArray(0);

    // Texture handler setup
    glGenTextures(1, &texHandle);

    int code = initTexture(imageFilename);
    if (code != 0)
    {
        printf("Error loading the texture image.\n");
        exit(EXIT_FAILURE);
    }
}

glm::vec3 calculatePosition(float u) {
    return glm::vec4(u * u * u, u * u, u, 1) * basis * ctrlMat;
}

glm::vec3 calculateTangent(float u) {
    return glm::vec4(3 * u * u, 2 * u, 1, 0) * basis * ctrlMat;
}

void displayFunc()
{
  // render some stuff...
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.LoadIdentity();

  // calculate position, tangent, normal, binormal
  if (splineIndex < ctrlMatrices.size())
  {
      ctrlMat = ctrlMatrices[splineIndex];
      glm::vec3 pos = calculatePosition(u);
      glm::vec3 tangentPreNormalize = calculateTangent(u);
      glm::vec3 tangent = glm::normalize(tangentPreNormalize);
      if (u == 0 && splineIndex == 0)
      {
          normal = glm::normalize(glm::cross(tangent, v));
      }
      else if (u <= 1)
      {
          normal = glm::normalize(glm::cross(binormal, tangent));
          //TODO
          uStep = sqrt(2 * 9.8f * (40 - pos[1])) / glm::length(tangentPreNormalize) / 10;
      }
      binormal = glm::normalize(glm::cross(tangent, normal));
      if (!pause)
      {
          if (physics)
          {
              u += uStep;
          }
          else {
              u += 0.001;
          }
      }
      if (u >= 1)
      {
          u = 0;
          splineIndex++;
      }
      matrix.LookAt(pos[0] + normal[0]/2, pos[1] + normal[1]/2, pos[2] + normal[2]/2, pos[0]+tangent[0], pos[1]+tangent[1], pos[2]+tangent[2], normal[0], normal[1], normal[2]);
  }
  else
  {
      matrix.LookAt(0, 0, 0, 0, 0, -1, 0, 1, 0);
  }
  

  float m[16];
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);

  ////////////////// Transformations performed here //////////////////
  /*matrix.Translate(landTranslate[0], landTranslate[1], landTranslate[2]);
  matrix.Rotate(landRotate[0], 1, 0, 0);
  matrix.Rotate(landRotate[1], 0, 1, 0);
  matrix.Rotate(landRotate[2], 0, 0, 1);
  matrix.Scale(landScale[0], landScale[1], landScale[2]);*/
  ////////////////////////////////////////////////////////////////////

  matrix.GetMatrix(m);
  GLboolean isRowMajor = GL_FALSE;
  //glUniformMatrix4fv(GL_MODELVIEW_MATRIX, 1, isRowMajor, m);

  float p[16];
  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.GetMatrix(p);

  // update viewLightDirection
  glm::mat4 viewMatrix = glm::make_mat4(m);
  glm::vec4 tmp = viewMatrix * glm::make_vec4(lightDirection4);
  tmp = glm::normalize(tmp);
  viewLightDirection[0] = tmp[0]; viewLightDirection[1] = tmp[1]; viewLightDirection[2] = tmp[2];

  // normal matrix
  float n[16];
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.GetNormalMatrix(n);

  ///////////////// bind shader for the roller coaster /////////////////
  pipelineProgram->Bind();

  pipelineProgram->SetModelViewMatrix(m);
  pipelineProgram->SetProjectionMatrix(p);

  GLint loc;

  ///////////////// render splines (level 1) /////////////////
  //glBindVertexArray(splineVAO);
  //loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
  //glUniform1i(loc, 0);
  //glDrawArrays(GL_LINES, 0, vertices.size());

  ///////////////// render the roller coaster /////////////////
  glBindVertexArray(trackVAO);
  loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
  glUniform1i(loc, 1);
  // get a handle to the viewLightDirection shader variable
  GLint h_viewLightDirection = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "viewLightDirection");
  glUniform3fv(h_viewLightDirection, 1, viewLightDirection);
  GLint h_normalMatrix = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "normalMatrix");
  glUniformMatrix4fv(h_normalMatrix, 1, isRowMajor, n);
  // set up uniform variables
  loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "La");
  glUniform4fv(loc, 1, La);
  loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "Ld");
  glUniform4fv(loc, 1, Ld);
  loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "Ls");
  glUniform4fv(loc, 1, Ls);
  loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "ka");
  glUniform4fv(loc, 1, ka);
  loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "kd");
  glUniform4fv(loc, 1, kd);
  loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "ks");
  glUniform4fv(loc, 1, ks);
  loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "alpha");
  glUniform1f(loc, alpha);
  glDrawArrays(GL_TRIANGLES, 0, crossSectionVertices.size());


  ///////////////// bind shader for the ground plane /////////////////
  pipelineProgramGround->Bind();

  pipelineProgramGround->SetModelViewMatrix(m);
  pipelineProgramGround->SetProjectionMatrix(p);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texHandle);

  glBindVertexArray(texVAO);
  loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
  glUniform1i(loc, 0);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  ///////////////// take screenshot (every 10th frame) /////////////////
  if (startScreenshot)
  {
      if (frameCounter == 0) {
          takeScreenshot();
          frameCounter++;
      }
      else {
          frameCounter++;
          if (frameCounter > 20) {
              frameCounter = 0;
          }
      }
  }

  glutSwapBuffers();
}

void idleFunc()
{
  // do some stuff... 

  // for example, here, you can save the screenshots to disk (to make the animation)

  // make the screen update 
  glutPostRedisplay();
}

void reshapeFunc(int w, int h)
{
  glViewport(0, 0, w, h);

  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.LoadIdentity();
  matrix.Perspective(54.0f, (float)w / (float)h, 0.01f, 1000.0f);
}

void mouseMotionDragFunc(int x, int y)
{
  // mouse has moved and one of the mouse buttons is pressed (dragging)

  // the change in mouse position since the last invocation of this function
  int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };

  switch (controlState)
  {
    // translate the landscape
    case TRANSLATE:
      if (leftMouseButton)
      {
        // control x,y translation via the left mouse button
        landTranslate[0] += mousePosDelta[0] * 0.01f;
        landTranslate[1] -= mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z translation via the middle mouse button
        landTranslate[2] += mousePosDelta[1] * 0.01f;
      }
      break;

    // rotate the landscape
    case ROTATE:
      if (leftMouseButton)
      {
        // control x,y rotation via the left mouse button
        landRotate[0] += mousePosDelta[1];
        landRotate[1] += mousePosDelta[0];
      }
      if (middleMouseButton)
      {
        // control z rotation via the middle mouse button
        landRotate[2] += mousePosDelta[1];
      }
      break;

    // scale the landscape
    case SCALE:
      if (leftMouseButton)
      {
        // control x,y scaling via the left mouse button
        landScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
        landScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z scaling via the middle mouse button
        landScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
  // mouse has moved
  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
  // a mouse button has has been pressed or depressed

  // keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables
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

  // keep track of whether CTRL and SHIFT keys are pressed
  switch (glutGetModifiers())
  {
    case GLUT_ACTIVE_CTRL:
      controlState = TRANSLATE;
    break;

    case GLUT_ACTIVE_SHIFT:
      controlState = SCALE;
    break;

    // if CTRL and SHIFT are not pressed, we are in rotate mode
    default:
      controlState = ROTATE;
    break;
  }

  // store the new mouse position
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
      // take a screenshot
        startScreenshot = true;
    break;

    case 'z':
        pause = !pause;
        break;

    case 'a':
        physics = !physics;
        break;
  }
}

void drawLine(glm::vec3 v0, glm::vec3 v1) {
    vertices.push_back(v0);
    vertices.push_back(v1);
}

void drawTriangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2)
{
    crossSectionVertices.push_back(v0);
    crossSectionVertices.push_back(v1);
    crossSectionVertices.push_back(v2);

    glm::vec3 n = glm::normalize(glm::cross((v1 - v0), (v2 - v0)));
    crossSectionNormals.push_back(n);
    crossSectionNormals.push_back(n);
    crossSectionNormals.push_back(n);
}

void drawTrack(glm::vec3 p0, glm::vec3 p1, glm::vec3 n0, glm::vec3 n1, glm::vec3 b0, glm::vec3 b1)
{
    glm::vec3 leftOffset0 = -offset * b0;
    glm::vec3 leftOffset1 = -offset * b1;
    glm::vec3 rightOffset0 = offset * b0;
    glm::vec3 rightOffset1 = offset * b1;

    glm::vec3 v0 = p0 + a * b0 + (a - tThickness) * n0;
    glm::vec3 v1 = p0 + a * (n0 + b0);
    glm::vec3 v2 = p0 + a * (n0 - b0);
    glm::vec3 v3 = p0 - a * b0 + (a - tThickness) * n0;
    glm::vec3 v8 = p0 - a * n0 + tThickness / 2 * b0;
    glm::vec3 v9 = p0 + (a - tThickness) * n0 + tThickness / 2 * b0;
    glm::vec3 v10 = p0 + (a - tThickness) * n0 - tThickness / 2 * b0;
    glm::vec3 v11 = p0 - a * n0 - tThickness / 2 * b0;

    glm::vec3 v4 = p1 + a * b1 + (a - tThickness) * n1;
    glm::vec3 v5 = p1 + a * (n1 + b1);
    glm::vec3 v6 = p1 + a * (n1 - b1);
    glm::vec3 v7 = p1 - a * b1 + (a - tThickness) * n1;
    glm::vec3 v12 = p1 - a * n1 + tThickness / 2 * b1;
    glm::vec3 v13 = p1 + (a - tThickness) * n1 + tThickness / 2 * b1;
    glm::vec3 v14 = p1 + (a - tThickness) * n1 - tThickness / 2 * b1;
    glm::vec3 v15 = p1 - a * n1 - tThickness / 2 * b1;

    // left t track
    drawTriangle(v0 + leftOffset0, v4 + leftOffset1, v1 + leftOffset0);
    drawTriangle(v1 + leftOffset0, v4 + leftOffset1, v5 + leftOffset1);
    drawTriangle(v1 + leftOffset0, v5 + leftOffset1, v2 + leftOffset0);
    drawTriangle(v2 + leftOffset0, v5 + leftOffset1, v6 + leftOffset1);
    drawTriangle(v2 + leftOffset0, v6 + leftOffset1, v3 + leftOffset0);
    drawTriangle(v3 + leftOffset0, v6 + leftOffset1, v7 + leftOffset1);
    drawTriangle(v9 + leftOffset0, v13 + leftOffset1, v0 + leftOffset0);
    drawTriangle(v0 + leftOffset0, v13 + leftOffset1, v4 + leftOffset1);
    drawTriangle(v3 + leftOffset0, v7 + leftOffset1, v10 + leftOffset0);
    drawTriangle(v10 + leftOffset0, v7 + leftOffset1, v14 + leftOffset1);
    drawTriangle(v8 + leftOffset0, v12 + leftOffset1, v9 + leftOffset0);
    drawTriangle(v9 + leftOffset0, v12 + leftOffset1, v13 + leftOffset1);
    drawTriangle(v10 + leftOffset0, v14 + leftOffset1, v11 + leftOffset0);
    drawTriangle(v11 + leftOffset0, v14 + leftOffset1, v15 + leftOffset1);
    drawTriangle(v11 + leftOffset0, v15 + leftOffset1, v8 + leftOffset0);
    drawTriangle(v8 + leftOffset0, v15 + leftOffset1, v12 + leftOffset1);

    // right t track
    drawTriangle(v0 + rightOffset0, v4 + rightOffset1, v1 + rightOffset0);
    drawTriangle(v1 + rightOffset0, v4 + rightOffset1, v5 + rightOffset1);
    drawTriangle(v1 + rightOffset0, v5 + rightOffset1, v2 + rightOffset0);
    drawTriangle(v2 + rightOffset0, v5 + rightOffset1, v6 + rightOffset1);
    drawTriangle(v2 + rightOffset0, v6 + rightOffset1, v3 + rightOffset0);
    drawTriangle(v3 + rightOffset0, v6 + rightOffset1, v7 + rightOffset1);
    drawTriangle(v9 + rightOffset0, v13 + rightOffset1, v0 + rightOffset0);
    drawTriangle(v0 + rightOffset0, v13 + rightOffset1, v4 + rightOffset1);
    drawTriangle(v3 + rightOffset0, v7 + rightOffset1, v10 + rightOffset0);
    drawTriangle(v10 + rightOffset0, v7 + rightOffset1, v14 + rightOffset1);
    drawTriangle(v8 + rightOffset0, v12 + rightOffset1, v9 + rightOffset0);
    drawTriangle(v9 + rightOffset0, v12 + rightOffset1, v13 + rightOffset1);
    drawTriangle(v10 + rightOffset0, v14 + rightOffset1, v11 + rightOffset0);
    drawTriangle(v11 + rightOffset0, v14 + rightOffset1, v15 + rightOffset1);
    drawTriangle(v11 + rightOffset0, v15 + rightOffset1, v8 + rightOffset0);
    drawTriangle(v8 + rightOffset0, v15 + rightOffset1, v12 + rightOffset1);
}

void drawPlank(glm::vec3 p, glm::vec3 t, glm::vec3 n, glm::vec3 b)
{
    glm::vec3 v0 = p - (a + tThickness) * n + plankHalfLength * b - plankHalfWidth * t;
    glm::vec3 v1 = p - a * n + plankHalfLength * b - plankHalfWidth * t;
    glm::vec3 v2 = p - a * n - plankHalfLength * b - plankHalfWidth * t;
    glm::vec3 v3 = p - (a + tThickness) * n - plankHalfLength * b - plankHalfWidth * t;

    glm::vec3 v4 = p - (a + tThickness) * n + plankHalfLength * b + plankHalfWidth * t;
    glm::vec3 v5 = p - a * n + plankHalfLength * b + plankHalfWidth * t;
    glm::vec3 v6 = p - a * n - plankHalfLength * b + plankHalfWidth * t;
    glm::vec3 v7 = p - (a + tThickness) * n - plankHalfLength * b + plankHalfWidth * t;

    drawTriangle(v0, v4, v1);
    drawTriangle(v1, v4, v5);
    drawTriangle(v1, v5, v2);
    drawTriangle(v2, v5, v6);
    drawTriangle(v2, v6, v3);
    drawTriangle(v3, v6, v7);
    drawTriangle(v3, v7, v0);
    drawTriangle(v0, v7, v4);
}

void subdivide(float u0, float u1, float maxlinelength) {
    glm::vec3 v0 = calculatePosition(u0);
    glm::vec3 v1 = calculatePosition(u1);
    if (glm::distance(v0, v1) > maxlinelength) {
        float umid = (u0 + u1) / 2;
        subdivide(u0, umid, maxlinelength);
        subdivide(umid, u1, maxlinelength);
    }
    else {
        drawLine(v0, v1);
    }
}

glm::vec3 BruteForce(bool isFirst, glm::vec3 lastBinormal)
{
    float u = 0;
    float step = 0.01f;
    glm::vec3 p0; glm::vec3 p1; glm::vec3 t0; glm::vec3 t1; glm::vec3 n0; glm::vec3 n1; glm::vec3 b0; glm::vec3 b1;

    p0 = calculatePosition(u);
    t0 = glm::normalize(calculateTangent(u));
    if (isFirst)
    {
        n0 = glm::normalize(glm::cross(t0, v));
    }
    else
    {
        n0 = glm::normalize(glm::cross(lastBinormal, t0));
    }
    b0 = glm::normalize(glm::cross(t0, n0));
    
    u += step;
    while (u <= 1)
    {
        vertices.push_back(p0);

        p1 = calculatePosition(u);
        t1 = glm::normalize(calculateTangent(u));
        n1 = glm::normalize(glm::cross(b0, t1));
        b1 = glm::normalize(glm::cross(t1, n1));

        vertices.push_back(p1);

        drawTrack(p0, p1, n0, n1, b0, b1);

        travelledDistance += glm::distance(p0, p1);
        if (travelledDistance >= plankDistance)
        {
            travelledDistance = 0;
            drawPlank(p1, t1, n1, b1);
        }

        p0 = p1;
        n0 = n1;
        b0 = b1;
        u += step;
    }

    return b1;
}

void initSpline(Spline s)
{
    glm::vec3 lastBinormal = v;
    for (int i = 0; i < s.numControlPoints - 3; i++) {

        // set up control matrix
        Point p1 = s.points[i];
        Point p2 = s.points[i+1];
        Point p3 = s.points[i+2];
        Point p4 = s.points[i+3];

        float mat[12] = {p1.x, p2.x, p3.x, p4.x, p1.y, p2.y, p3.y, p4.y, p1.z, p2.z, p3.z, p4.z};
        ctrlMat = glm::make_mat3x4(mat);
        ctrlMatrices.push_back(ctrlMat);
        if (i == 0)
        {
            lastBinormal = BruteForce(true, v);
        }
        else
        {
            lastBinormal = BruteForce(false, lastBinormal);
        }
        
        subdivide(0, 1, maxLineLength);
    }
}

void initSplines()
{
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  
  for (int i = 0; i < numSplines; i++)
  {
      initSpline(splines[i]);
      printf("Num control points in spline %d: %d.\n", i, splines[i].numControlPoints);
  }

  pipelineProgram = new BasicPipelineProgram;
  int ret = pipelineProgram->Init(shaderBasePath, 0);
  if (ret != 0) abort();

  //////////////////////////////////// Spline (Level 1) ////////////////////////////////////

  // VBO set up for spline
  glGenBuffers(1, &splineVBO);
  glBindBuffer(GL_ARRAY_BUFFER, splineVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // VAO set up for spline
  glGenVertexArrays(1, &splineVAO);
  glBindVertexArray(splineVAO);
  glBindBuffer(GL_ARRAY_BUFFER, splineVBO);

  GLuint loc =
      glGetAttribLocation(pipelineProgram->GetProgramHandle(), "position");
  glEnableVertexAttribArray(loc);
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

  glBindVertexArray(0);

  //////////////////////////////////// Track/Railroad (Level 2, 3) ////////////////////////////////////

  // VBO set up for track
  glGenBuffers(1, &trackVBO);
  glBindBuffer(GL_ARRAY_BUFFER, trackVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * (crossSectionVertices.size() + crossSectionNormals.size()), nullptr, GL_STATIC_DRAW);

  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * crossSectionVertices.size(), crossSectionVertices.data());
  glBufferSubData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * crossSectionVertices.size(), sizeof(glm::vec3) * crossSectionNormals.size(), crossSectionNormals.data());

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // TODO: VAO set up for track
  glGenVertexArrays(1, &trackVAO);
  glBindVertexArray(trackVAO);
  glBindBuffer(GL_ARRAY_BUFFER, trackVBO);

  loc =
      glGetAttribLocation(pipelineProgram->GetProgramHandle(), "position");
  glEnableVertexAttribArray(loc);
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

  loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "normal");
  glEnableVertexAttribArray(loc);
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, (const void*)(sizeof(glm::vec3) * crossSectionVertices.size()));

  glBindVertexArray(0);


  glEnable(GL_DEPTH_TEST);

  std::cout << "GL error: " << glGetError() << std::endl;
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    cout << "The arguments are incorrect." << endl;
    cout << "usage: ./hw1 <texture file>" << endl;
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

  // tells glut to use a particular display function to redraw 
  glutDisplayFunc(displayFunc);
  // perform animation inside idleFunc
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

    // do initialization
    if (argc < 2)
    {
        printf("usage: %s <trackfile>\n", argv[0]);
        exit(0);
    }

    // load the splines from the provided filename
    loadSplines(argv[1]);

    // set up texture for ground
    setupTexture(argv[2]);

    printf("Loaded %d spline(s).\n", numSplines);
    
    initSplines();
  
  // sink forever into the glut loop
  glutMainLoop();
}
