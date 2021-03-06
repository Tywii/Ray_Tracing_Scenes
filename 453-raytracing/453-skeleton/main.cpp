#define _USE_MATH_DEFINES
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>

#include <glm/gtx/vector_query.hpp>

#include "Geometry.h"
#include "GLDebug.h"
#include "Log.h"
#include "ShaderProgram.h"
#include "Shader.h"
#include "Texture.h"
#include "Window.h"
#include "imagebuffer.h"
#include "RayTrace.h"
#include "Scene.h"
#include "Lighting.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"


int hasIntersection(Scene const &scene, Ray ray, int skipID){
	for (auto &shape : scene.shapesInScene) {
		Intersection tmp = shape->getIntersection(ray);
		if(
			shape->id != skipID
			&& tmp.numberOfIntersections!=0
			&& glm::distance(tmp.point, ray.origin) > 0.00001
			&& glm::distance(tmp.point, ray.origin) < glm::distance(ray.origin, scene.lightPosition) - 0.01
		){
			return tmp.id;
		}
	}
	return -1;
}

Intersection getClosestIntersection(Scene const &scene, Ray ray, int skipID){ //get the nearest
	Intersection closestIntersection;
	float min = std::numeric_limits<float>::max();
	for(auto &shape : scene.shapesInScene) {
		if(skipID == shape->id) {
			// Sometimes you need to skip certain shapes. Useful to
			// avoid self-intersection. ;)
			continue;
		}
		Intersection p = shape->getIntersection(ray);
		float distance = glm::distance(p.point, ray.origin);
		if(p.numberOfIntersections !=0 && distance < min){
			min = distance;
			closestIntersection = p;
		}
	}
	return closestIntersection;
}


glm::vec3 raytraceSingleRay(Scene const &scene, Ray const &ray, int level, int source_id) {
	// TODO: Part 3: Somewhere in this function you will need to add the code to determine
	//               if a given point is in shadow or not. Think carefully about what parts
	//               of the lighting equation should be used when a point is in shadow.
	// TODO: Part 4: Somewhere in this function you will need to add the code that does reflections.
	//               NOTE: The ObjectMaterial class already has a parameter to store the object's
	//               reflective properties. Use this parameter + the color coming back from the
	//               reflected array and the color from the phong shading equation.
	Intersection result = getClosestIntersection(scene, ray, source_id); //find intersection
	
	PhongReflection phong;
	phong.ray = ray;
	phong.scene = scene;
	phong.material = result.material;
	phong.intersection = result;
	
	if(result.numberOfIntersections == 0) return glm::vec3(0, 0, 0); // black;

	if (level < 1) {
		phong.material.reflectionStrength = glm::vec3(0);
	}
	
	
	Ray shadowRay = Ray(result.point + 0.01f * (scene.lightPosition - result.point), glm::normalize(scene.lightPosition - result.point));
	Intersection shadowIntersect = getClosestIntersection(scene, shadowRay, source_id);
	if (shadowIntersect.numberOfIntersections == 2 && shadowRay.origin[2] > -11.9f && shadowRay.origin[1] < 2.7f) {
		phong.material.diffuse *= 0.4f;
		phong.material.specular *= 0.4f;
	}
	
	else {
		Ray shadowRay2 = Ray(shadowIntersect.point + 0.01f * (scene.lightPosition - result.point), glm::normalize(scene.lightPosition - result.point));
		Intersection shadowIntersect2 = getClosestIntersection(scene, shadowRay2, source_id);
		if (shadowIntersect.numberOfIntersections + shadowIntersect2.numberOfIntersections == 2 && shadowRay.origin[2] > -11.9f && shadowRay.origin[1] < 2.7f) {
			phong.material.diffuse *= 0.4f;
			phong.material.specular *= 0.4f;
		}
	}

	return phong.I();
}

struct RayAndPixel {
	Ray ray;
	int x;
	int y;
};

std::vector<RayAndPixel> getRaysForViewpoint(Scene const &scene, ImageBuffer &image, glm::vec3 viewPoint) {
	// This is the function you must implement for part 1
	//
	// This function is responsible for creating the rays that go
	// from the viewpoint out into the scene with the appropriate direction
	// and angles to produce a perspective image.
	int x = 0;
	int y = 0;
	std::vector<RayAndPixel> rays;

	// TODO: Part 1: Currently this is casting rays in an orthographic style.
	//               You need to change this code to project them in a pinhole camera style.
	//for (float i = -1; x < image.Width(); x++) {
	//	y = 0;
	//	for (float j = -1; y < image.Height(); y++) {
	//		glm::vec3 direction(0, 0, -1);
	//		glm::vec3 viewPointOrthographic(i-viewPoint.x, j-viewPoint.y, 0);
	//		Ray r = Ray(viewPointOrthographic, direction);
	//		rays.push_back({r, x, y});
	//		j += 2.f / image.Height();
	//	}
	//	i += 2.f / image.Width();
	//}
	for (float i = -1; x < image.Width(); x++) {
		y = 0;
		for (float j = -1; y < image.Height(); y++) {
			float dirX = (x - image.Width() / 2.0f) / (image.Width() / 2.0f);
			float dirY = (y - image.Height() / 2.0f) / (image.Height() / 2.0f);
			//float dirZ = sqrt(pow(dirX, 2.0f) + pow(dirY, 2.0f));
			glm::vec3 direction(dirX, dirY, -2.0f);
			//glm::vec3 viewPointOrthographic(i - viewPoint.x, j - viewPoint.y, 0);
			//Ray r = Ray(viewPointOrthographic, direction);
			glm::vec3 viewPointOrthographic(0, 0, 0);
			Ray r = Ray(viewPointOrthographic, normalize(direction));

			rays.push_back({ r, x, y });
			j += 2.f / image.Height();
		}
		i += 2.f / image.Width();
	}
	return rays;
}

void raytraceImage(Scene const &scene, ImageBuffer &image, glm::vec3 viewPoint) {
	// Reset the image to the current size of the screen.
	image.Initialize();

	// Get the set of rays to cast for this given image / viewpoint
	std::vector<RayAndPixel> rays = getRaysForViewpoint(scene, image, viewPoint);


	// This loops processes each ray and stores the resulting pixel in the image.
	// final color into the image at the appropriate location.
	//
	// I've written it this way, because if you're keen on this, you can
	// try and parallelize this loop to ensure that your ray tracer makes use
	// of all of your CPU cores
	//
	// Note, if you do this, you will need to be careful about how you render
	// things below too
	// std::for_each(std::begin(rays), std::end(rays), [&] (auto const &r) {
	for (auto const & r : rays) {
		glm::vec3 color = raytraceSingleRay(scene, r.ray, 5, -1);
		image.SetPixel(r.x, r.y, color);
	}
}

// EXAMPLE CALLBACKS
class Assignment5 : public CallbackInterface {

public:
	Assignment5() {
		viewPoint = glm::vec3(0, 0, 0);
		scene = initScene1();
		raytraceImage(scene, outputImage, viewPoint);
	}

	virtual void keyCallback(int key, int scancode, int action, int mods) {
		if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
			shouldQuit = true;
		}

		if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
			scene = initScene1();
			raytraceImage(scene, outputImage, viewPoint);
	
		}

		if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
			scene = initScene2();
			raytraceImage(scene, outputImage, viewPoint);

		}
	}

	bool shouldQuit = false;

	ImageBuffer outputImage;
	Scene scene;
	glm::vec3 viewPoint;

};
// END EXAMPLES


int main() {
	Log::debug("Starting main");

	// WINDOW
	glfwInit();

	// Change your image/screensize here.
	int width = 800;
	int height = 800;
	Window window(width, height, "CPSC 453");

	GLDebug::enable();

	// CALLBACKS
	std::shared_ptr<Assignment5> a5 = std::make_shared<Assignment5>(); // can also update callbacks to new ones
	window.setCallbacks(a5); // can also update callbacks to new ones

	// RENDER LOOP
	while (!window.shouldClose() && !a5->shouldQuit) {
		glfwPollEvents();

		glEnable(GL_FRAMEBUFFER_SRGB);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		a5->outputImage.Render();

		window.swapBuffers();
	}


	// Save image to file:
	// outpuImage.SaveToFile("foo.png")

	glfwTerminate();
	return 0;
}
