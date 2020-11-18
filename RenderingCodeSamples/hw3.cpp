/* **************************
 * CSCI 420
 * Assignment 3 Raytracer
 * Name: Yang Cao
 * *************************
*/

#ifdef WIN32
  #include <windows.h>
#endif

#if defined(WIN32) || defined(linux)
  #include <GL/gl.h>
  #include <GL/glut.h>
#elif defined(__APPLE__)
  #include <OpenGL/gl.h>
  #include <GLUT/glut.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
  #define strcasecmp _stricmp
#endif

#include <imageIO.h>

#define _USE_MATH_DEFINES
#include <math.h>

#define MAX_TRIANGLES 20000
#define MAX_SPHERES 100
#define MAX_LIGHTS 100

char * filename = NULL;

//different display modes
#define MODE_DISPLAY 1
#define MODE_JPEG 2

int mode = MODE_DISPLAY;

//you may want to make these smaller for debugging purposes
#define WIDTH 640 //640
#define HEIGHT 480 //480

//the field of view of the camera
#define fov 60.0
#define eps 0.001

// max recursive depth for reflection color calculation
#define MAX_DEPTH 2

unsigned char buffer[HEIGHT][WIDTH][3];

struct Vertex
{
  double position[3];
  double color_diffuse[3];
  double color_specular[3];
  double normal[3];
  double shininess;
};

struct Triangle
{
  Vertex v[3];
};

struct Sphere
{
  double position[3];
  double color_diffuse[3];
  double color_specular[3];
  double shininess;
  double radius;
};

struct Light
{
  double position[3];
  double color[3];
};

enum ProjectionPlane
{
	XY,
	YZ,
	XZ
};

double x_axis[3] = { 1.0, 0.0, 0.0 };
double y_axis[3] = { 0.0, 1.0, 0.0 };
double z_axis[3] = { 0.0, 0.0, 1.0 };

Triangle triangles[MAX_TRIANGLES];
Sphere spheres[MAX_SPHERES];
Light lights[MAX_LIGHTS];
double ambient_light[3];

int num_triangles = 0;
int num_spheres = 0;
int num_lights = 0;

void plot_pixel_display(int x,int y,unsigned char r,unsigned char g,unsigned char b);
void plot_pixel_jpeg(int x,int y,unsigned char r,unsigned char g,unsigned char b);
void plot_pixel(int x,int y,unsigned char r,unsigned char g,unsigned char b);

double top; double bot; double left; double right;
double pixelSize;

const double const camPos[3] = { 0.0, 0.0, 0.0 };
double intersect_triangle[3];

// barycentric coordinates
double alpha; double beta; double gamma;
double normal_interpolated[3]; double diffuse_interpolated[3]; double specular_interpolated[3]; double shininess_interpolated;

double average(const double * const v, const int size)
{
	double result = 0;
	for (int i = 0; i < size; i++)
	{
		result += v[i];
	}
	return result / size;
}

double dot(const double * const v1, const double * const v2)
{
	return v1[0]* v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

double cross2d(const double * const v1, const double * const v2)
{
	return v1[0] * v2[1] - v1[1] * v2[0];
}

double* cross3d(const double * const v1, const double * const v2)
{
	static double result[3];
	result[0] = v1[1] * v2[2] - v1[2] * v2[1];
	result[1] = v1[2] * v2[0] - v1[0] * v2[2];
	result[2] = v1[0] * v2[1] - v1[1] * v2[0];
	return result;
}

double get_magnitude(const double * const v)
{
	return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void normalize(double * const v)
{
	static double length;
	length = get_magnitude(v);
	v[0] = v[0] / length; v[1] = v[1] / length; v[2] = v[2] / length;
}

double calculate_area(const double * const p0, const double * const p1, const double * const p2)
{
	return (1.0 / 2) * ((p1[0] - p0[0]) * (p2[1] - p0[1]) - (p2[0] - p0[0]) * (p1[1] - p0[1]));
}

void create_image_plane()
{
	double y = tan(fov / 2 * M_PI / 180);
	double x = y * WIDTH / HEIGHT;
	top = y; bot = -y; left = -x; right = x;
	pixelSize = 2.0 * y / HEIGHT;
}

double* interpolation3d(const double * const v0, const double * const v1, const double * const v2)
{
	static double result[3];
	result[0] = v0[0] * alpha + v1[0] * beta + v2[0] * gamma;
	result[1] = v0[1] * alpha + v1[1] * beta + v2[1] * gamma;
	result[2] = v0[2] * alpha + v1[2] * beta + v2[2] * gamma;
	return result;
}

void perform_interpolations(const double a, const double b, const double g, const int index)
{
	// get triangle and 3 vertices
	static Triangle tri; static Vertex v0; static Vertex v1; static Vertex v2;
	tri = triangles[index];
	v0 = tri.v[0]; v1 = tri.v[1]; v2 = tri.v[2];

	static double * var = 0;

	var = interpolation3d(v0.normal, v1.normal, v2.normal);
	normal_interpolated[0] = var[0]; normal_interpolated[1] = var[1]; normal_interpolated[2] = var[2];
	normalize(normal_interpolated);
	var = interpolation3d(v0.color_diffuse, v1.color_diffuse, v2.color_diffuse);
	diffuse_interpolated[0] = var[0]; diffuse_interpolated[1] = var[1]; diffuse_interpolated[2] = var[2];
	var = interpolation3d(v0.color_specular, v1.color_specular, v2.color_specular);
	specular_interpolated[0] = var[0]; specular_interpolated[1] = var[1]; specular_interpolated[2] = var[2];
	shininess_interpolated = alpha * v0.shininess + beta * v1.shininess + gamma * v2.shininess;
}

double intersection_triangle(const double * const origin, const double * const directionVec, const int index, bool check_shadow)
{
	// get triangle and 3 vertices
	static Triangle tri; static Vertex v0; static Vertex v1; static Vertex v2;
	tri = triangles[index];
	v0 = tri.v[0]; v1 = tri.v[1]; v2 = tri.v[2];

	// initialize other variables/arrays
	static double vec01[3]; static double vec02[3];
	static double planeNormal[3];

	// check intersection of plane
	vec01[0] = v1.position[0] - v0.position[0]; vec01[1] = v1.position[1] - v0.position[1]; vec01[2] = v1.position[2] - v0.position[2];
	vec02[0] = v2.position[0] - v0.position[0]; vec02[1] = v2.position[1] - v0.position[1]; vec02[2] = v2.position[2] - v0.position[2];
	double * tmp = 0;
	tmp = cross3d(vec01, vec02);
	planeNormal[0] = tmp[0]; planeNormal[1] = tmp[1]; planeNormal[2] = tmp[2];
	normalize(planeNormal);

	double denominator = dot(planeNormal, directionVec);
	if (fabs(denominator) <= eps)
	{
		return -1;
	}
	double d = -dot(planeNormal, v0.position);
	double t = -(dot(planeNormal, origin) + d) / denominator;
	if (t <= eps)
	{
		return -1;
	}


	// if intersects with the plane,
	// 1. find the position of intersection
	// 2. project triangle onto a plane, find the best plane
	// 3. use barycentric coordinates to test inside/outside

	// intersection
	static double intersect_triangle_tmp[3];
	intersect_triangle_tmp[0] = origin[0] + directionVec[0] * t;
	intersect_triangle_tmp[1] = origin[1] + directionVec[1] * t;
	intersect_triangle_tmp[2] = origin[2] + directionVec[2] * t;

	if (!check_shadow)
	{
		intersect_triangle[0] = intersect_triangle_tmp[0]; intersect_triangle[1] = intersect_triangle_tmp[1]; intersect_triangle[2] = intersect_triangle_tmp[2];
	}

	ProjectionPlane plane = ProjectionPlane::XY;
	double cosineTheta = fabs(dot(planeNormal, z_axis));
	double cosineYZ = fabs(dot(planeNormal, x_axis));
	if (cosineYZ > cosineTheta)
	{
		cosineTheta = cosineYZ;
		plane = ProjectionPlane::YZ;
	}
	double cosineXZ = fabs(dot(planeNormal, y_axis));
	if (cosineXZ > cosineTheta)
	{
		cosineTheta = cosineXZ;
		plane = ProjectionPlane::XZ;
	}

	// project
	double v0New[2]; double v1New[2]; double v2New[2]; double intersectionNew[2];

	if (plane == ProjectionPlane::XY)
	{
		v0New[0] = v0.position[0]; v0New[1] = v0.position[1];
		v1New[0] = v1.position[0]; v1New[1] = v1.position[1];
		v2New[0] = v2.position[0]; v2New[1] = v2.position[1];
		intersectionNew[0] = intersect_triangle_tmp[0]; intersectionNew[1] = intersect_triangle_tmp[1];
	}
	else if (plane == ProjectionPlane::YZ)
	{
		v0New[0] = v0.position[2]; v0New[1] = v0.position[1];
		v1New[0] = v1.position[2]; v1New[1] = v1.position[1];
		v2New[0] = v2.position[2]; v2New[1] = v2.position[1];
		intersectionNew[0] = intersect_triangle_tmp[2]; intersectionNew[1] = intersect_triangle_tmp[1];
	}
	else
	{
		v0New[0] = v0.position[0]; v0New[1] = v0.position[2];
		v1New[0] = v1.position[0]; v1New[1] = v1.position[2];
		v2New[0] = v2.position[0]; v2New[1] = v2.position[2];
		intersectionNew[0] = intersect_triangle_tmp[0]; intersectionNew[1] = intersect_triangle_tmp[2];
	}

	// barycentric coordinate
	double totalArea = calculate_area(v0New, v1New, v2New);
	if (fabs(totalArea) > eps)
	{
		alpha = calculate_area(intersectionNew, v1New, v2New) / totalArea;
		beta = calculate_area(v0New, intersectionNew, v2New) / totalArea;
		gamma = calculate_area(v0New, v1New, intersectionNew) / totalArea;
	}
	else
	{
		// degenerate triangles
		return -1;
	}

	// check inside/outside
	if (alpha >= 0 && alpha <= 1 && beta >= 0 && beta && gamma >= 0 && gamma <= 1)
	{
		// inside
		if (!check_shadow)
		{
			perform_interpolations(alpha, beta, gamma, index);
		}
		return t;
	}
	else
	{
		return -1;
	}
}

double intersection_sphere(const double * const origin, const double * const directionVec, Sphere s)
{
	static double a; static double b; static double c;

	a = 1.0;
	b = 2 * (directionVec[0] * (origin[0] - s.position[0]) + directionVec[1] * (origin[1] - s.position[1]) + directionVec[2] * (origin[2] - s.position[2]));
	c = pow(origin[0] - s.position[0], 2) + pow(origin[1] - s.position[1], 2) + pow(origin[2] - s.position[2], 2) - pow(s.radius, 2);

	// check for real number results
	double check = b * b - 4 * c;
	if (check < 0)
	{
		return -1;
	}

	double t0 = (-b - sqrt(check)) / 2.0;
	double t1 = (-b + sqrt(check)) / 2.0;

	if (t0 <= eps)
	{
		t0 = -1;
	}
	if (t1 <= eps)
	{
		t1 = -1;
	}

	if (t0 > 0 && t1 > 0)
	{
		return min(t0, t1);
	}
	else
	{
		if (t0 > 0)
		{
			return t0;
		}
		else if (t1 > 0)
		{
			return t1;
		}
		else
		{
			return -1;
		}
	}
}


// create a shadow array to a light source, check if there's any intersection
// original position, light position
bool shadow_intersection_perLight(const double * const origin, const double * const lightPos)
{
	static double directionVec[3];
	directionVec[0] = lightPos[0] - origin[0]; directionVec[1] = lightPos[1] - origin[1]; directionVec[2] = lightPos[2] - origin[2];
	double vecLength = get_magnitude(directionVec);
	normalize(directionVec);

	// spheres
	for (int i = 0; i < num_spheres; i++)
	{
		double t = intersection_sphere(origin, directionVec, spheres[i]);
		//there is intersection
		if (t > eps)
		{
			if (t > vecLength)
			{
				continue;
			}
			return true;
		}
	}

	// triangles
	for (int i = 0; i < num_triangles; i++)
	{
		double t = intersection_triangle(origin, directionVec, i, true);
		if (t > eps)
		{
			if (t > vecLength)
			{
				continue;
			}
			return true;
		}
	}

	return false;
}

double* phong(const double * const intersection, const double * const normal, const double * const color_diffuse, const double * const color_specular, const double shininess)
{
	static double result[3];

	double r = ambient_light[0];
	double g = ambient_light[1];
	double b = ambient_light[2];

	static double camVec[3]; static double lightVec[3]; static double reflectVec[3];

	camVec[0] = -intersection[0]; camVec[1] = -intersection[1]; camVec[2] = -intersection[2];
	normalize(camVec);

	// iterate through every light source
	for (int i = 0; i < num_lights; i++)
	{
		// if there's intersection, skip this light source
		if (shadow_intersection_perLight(intersection, lights[i].position))
		{
			continue;
		}

		lightVec[0] = lights[i].position[0] - intersection[0]; lightVec[1] = lights[i].position[1] - intersection[1]; lightVec[2] =  lights[i].position[2] - intersection[2];
		normalize(lightVec);

		double LdotN = dot(lightVec, normal);
		reflectVec[0] = 2 * LdotN * normal[0] - lightVec[0]; reflectVec[1] = 2 * LdotN * normal[1] - lightVec[1]; reflectVec[2] = 2 * LdotN * normal[2] - lightVec[2];

		if (LdotN < 0) { LdotN = 0.0; }
		double RdotV = dot(reflectVec, camVec);
		if (RdotV < 0) { RdotV = 0.0; }

		double * reflectionColor;

		r += lights[i].color[0] * (color_diffuse[0] * LdotN + color_specular[0] * pow(RdotV, shininess));
		g += lights[i].color[1] * (color_diffuse[1] * LdotN + color_specular[1] * pow(RdotV, shininess));
		b += lights[i].color[2] * (color_diffuse[2] * LdotN + color_specular[2] * pow(RdotV, shininess));
	}

	// clamp
	if (r < 0)
	{
		r = 0.0;
	}
	if (r > 1)
	{
		r = 1.0;
	}
	if (g < 0)
	{
		g = 0.0;
	}
	if (g > 1)
	{
		g = 1.0;
	}
	if (b < 0)
	{
		b = 0.0;
	}
	if (b > 1)
	{
		b = 1.0;
	}
	result[0] = r; result[1] = g; result[2] = b;
	
	return result;
}


//primary intersection point 
double* intersection(const double * const origin, const unsigned int x, const unsigned int y)
{
	
	static double result[3];

	static double finalR[9];
	static double finalG[9];
	static double finalB[9];

	static double posX_botLeft; static double posY_botLeft;
	static double posX_cur; static double posY_cur;

	static double directionVec[3];
	static double intersect[3];
	static double normalVec[3];

	posX_botLeft = left + pixelSize / 2 + pixelSize * x - pixelSize / 3;
	posY_botLeft = bot + pixelSize / 2 + pixelSize * y - pixelSize / 3;

	int index = 0;
	for (unsigned int i = 0; i < 3; i++)
	{
		posX_cur = posX_botLeft + i * (pixelSize / 3);
		for (unsigned int j = 0; j < 3; j++)
		{
			double minT = -1;

			posY_cur = posY_botLeft + j * (pixelSize / 3);
			directionVec[0] = posX_cur; directionVec[1] = posY_cur; directionVec[2] = -1.0;
			normalize(directionVec);

			// spheres
			for (int i = 0; i < num_spheres; i++)
			{
				double t = intersection_sphere(origin, directionVec, spheres[i]);
				if (t > eps)
				{
					if (t < minT || minT < 0)
					{
						minT = t;
						double r = spheres[i].radius;
						intersect[0] = directionVec[0] * t; intersect[1] = directionVec[1] * t; intersect[2] = directionVec[2] * t;
						normalVec[0] = 1.0 / r * (intersect[0] - spheres[i].position[0]);
						normalVec[1] = 1.0 / r * (intersect[1] - spheres[i].position[1]);
						normalVec[2] = 1.0 / r * (intersect[2] - spheres[i].position[2]);

						double * c;
						c = phong(intersect, normalVec, spheres[i].color_diffuse, spheres[i].color_specular, spheres[i].shininess);
						finalR[index] = c[0] * 255; finalG[index] = c[1] * 255; finalB[index] = c[2] * 255;
					}
				}
			}


			// triangles
			for (int i = 0; i < num_triangles; i++)
			{
				double t = intersection_triangle(origin, directionVec, i, false);
				if (t > eps)
				{
					if (t < minT || minT < 0)
					{
						minT = t;
						// test intersection inside the triangle
						double * color;
						color = phong(intersect_triangle, normal_interpolated, diffuse_interpolated, specular_interpolated, shininess_interpolated);
						finalR[index] = color[0] * 255; finalG[index] = color[1] * 255; finalB[index] = color[2] * 255;
					}
				}

			}



			// check which one is the closest
			if (minT < eps)
			{
				finalR[index] = 255; finalG[index] = 255; finalB[index] = 255;
			}

			index++;
		}
	}

	result[0] = average(finalR, 9); result[1] = average(finalG, 9); result[2] = average(finalB, 9);

	return result;
}

//MODIFY THIS FUNCTION
void draw_scene()
{
	create_image_plane();
	//shoot out arrays
	for (unsigned int x = 0; x < WIDTH; x++)
	{
		glPointSize(2.0);
		glBegin(GL_POINTS);

		double posX = left + pixelSize / 2 + pixelSize * x;
		double posZ = -1.0;
		for (unsigned int y = 0; y < HEIGHT; y++)
		{
			double * pixelColor;
			pixelColor = intersection(camPos, x, y);
			plot_pixel(x, y, pixelColor[0], pixelColor[1], pixelColor[2]);
		}
		glEnd();
		glFlush();
	}
	printf("Done!\n"); fflush(stdout);
}

void plot_pixel_display(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
  glColor3f(((float)r) / 255.0f, ((float)g) / 255.0f, ((float)b) / 255.0f);
  glVertex2i(x,y);
}

void plot_pixel_jpeg(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
  buffer[y][x][0] = r;
  buffer[y][x][1] = g;
  buffer[y][x][2] = b;
}

void plot_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
  plot_pixel_display(x,y,r,g,b);
  if(mode == MODE_JPEG)
    plot_pixel_jpeg(x,y,r,g,b);
}

void save_jpg()
{
  printf("Saving JPEG file: %s\n", filename);

  ImageIO img(WIDTH, HEIGHT, 3, &buffer[0][0][0]);
  if (img.save(filename, ImageIO::FORMAT_JPEG) != ImageIO::OK)
    printf("Error in Saving\n");
  else 
    printf("File saved Successfully\n");
}

void parse_check(const char *expected, char *found)
{
  if(strcasecmp(expected,found))
  {
    printf("Expected '%s ' found '%s '\n", expected, found);
    printf("Parse error, abnormal abortion\n");
    exit(0);
  }
}

void parse_doubles(FILE* file, const char *check, double p[3])
{
  char str[100];
  fscanf(file,"%s",str);
  parse_check(check,str);
  fscanf(file,"%lf %lf %lf",&p[0],&p[1],&p[2]);
  printf("%s %lf %lf %lf\n",check,p[0],p[1],p[2]);
}

void parse_rad(FILE *file, double *r)
{
  char str[100];
  fscanf(file,"%s",str);
  parse_check("rad:",str);
  fscanf(file,"%lf",r);
  printf("rad: %f\n",*r);
}

void parse_shi(FILE *file, double *shi)
{
  char s[100];
  fscanf(file,"%s",s);
  parse_check("shi:",s);
  fscanf(file,"%lf",shi);
  printf("shi: %f\n",*shi);
}

int loadScene(char *argv)
{
  FILE * file = fopen(argv,"r");
  int number_of_objects;
  char type[50];
  Triangle t;
  Sphere s;
  Light l;
  fscanf(file,"%i", &number_of_objects);

  printf("number of objects: %i\n",number_of_objects);

  parse_doubles(file,"amb:",ambient_light);

  for(int i=0; i<number_of_objects; i++)
  {
    fscanf(file,"%s\n",type);
    printf("%s\n",type);
    if(strcasecmp(type,"triangle")==0)
    {
      printf("found triangle\n");
      for(int j=0;j < 3;j++)
      {
        parse_doubles(file,"pos:",t.v[j].position);
        parse_doubles(file,"nor:",t.v[j].normal);
        parse_doubles(file,"dif:",t.v[j].color_diffuse);
        parse_doubles(file,"spe:",t.v[j].color_specular);
        parse_shi(file,&t.v[j].shininess);
      }

      if(num_triangles == MAX_TRIANGLES)
      {
        printf("too many triangles, you should increase MAX_TRIANGLES!\n");
        exit(0);
      }
      triangles[num_triangles++] = t;
    }
    else if(strcasecmp(type,"sphere")==0)
    {
      printf("found sphere\n");

      parse_doubles(file,"pos:",s.position);
      parse_rad(file,&s.radius);
      parse_doubles(file,"dif:",s.color_diffuse);
      parse_doubles(file,"spe:",s.color_specular);
      parse_shi(file,&s.shininess);

      if(num_spheres == MAX_SPHERES)
      {
        printf("too many spheres, you should increase MAX_SPHERES!\n");
        exit(0);
      }
      spheres[num_spheres++] = s;
    }
    else if(strcasecmp(type,"light")==0)
    {
      printf("found light\n");
      parse_doubles(file,"pos:",l.position);
      parse_doubles(file,"col:",l.color);

      if(num_lights == MAX_LIGHTS)
      {
        printf("too many lights, you should increase MAX_LIGHTS!\n");
        exit(0);
      }
      lights[num_lights++] = l;
    }
    else
    {
      printf("unknown type in scene description:\n%s\n",type);
      exit(0);
    }
  }
  return 0;
}

void display()
{
}

void init()
{
  glMatrixMode(GL_PROJECTION);
  glOrtho(0,WIDTH,0,HEIGHT,1,-1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0,0,0,0);
  glClear(GL_COLOR_BUFFER_BIT);
}

void idle()
{
  //hack to make it only draw once
  static int once=0;
  if(!once)
  {
    draw_scene();
    if(mode == MODE_JPEG)
      save_jpg();
  }
  once=1;
}

int main(int argc, char ** argv)
{
  if ((argc < 2) || (argc > 3))
  {  
    printf ("Usage: %s <input scenefile> [output jpegname]\n", argv[0]);
    exit(0);
  }
  if(argc == 3)
  {
    mode = MODE_JPEG;
    filename = argv[2];
  }
  else if(argc == 2)
    mode = MODE_DISPLAY;

  glutInit(&argc,argv);
  loadScene(argv[1]);

  glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
  glutInitWindowPosition(0,0);
  glutInitWindowSize(WIDTH,HEIGHT);
  int window = glutCreateWindow("Ray Tracer");
  #ifdef __APPLE__
    // This is needed on recent Mac OS X versions to correctly display the window.
    glutReshapeWindow(WIDTH - 1, HEIGHT - 1);
  #endif
  glutDisplayFunc(display);
  glutIdleFunc(idle);
  init();
  glutMainLoop();
}

